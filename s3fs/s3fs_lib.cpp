#include <algorithm>
#include <atomic>
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <set>
#include <unistd.h>
#include <dirent.h>
#include <sys/types.h>
#include <getopt.h>
#include <sys/stat.h>
#include <unordered_map>
#include <fstream>
#include <sstream>

#include "common.h"
//#include "s3fs.h"
#include "s3fs_lib.h"
#include "s3fs_logger.h"
#include "metaheader.h"
#include "fdcache.h"
#include "fdcache_auto.h"
#include "fdcache_stat.h"
#include "curl.h"
#include "curl_multi.h"
#include "s3objlist.h"
#include "cache.h"
#include "addhead.h"
#include "sighandlers.h"
#include "s3fs_xml.h"
#include "string_util.h"
#include "s3fs_auth.h"
#include "s3fs_cred.h"
#include "s3fs_help.h"
#include "s3fs_util.h"
#include "mpu_util.h"
#include "threadpoolman.h"
#include "autolock.h"


//-------------------------------------------------------------------
// Symbols
//-------------------------------------------------------------------
#if !defined(ENOATTR)
#define ENOATTR                   ENODATA
#endif

enum class dirtype : int8_t {
    UNKNOWN = -1,
    NEW = 0,
    OLD = 1,
    FOLDER = 2,
    NOOBJ = 3,
};

struct PosixContext {
    uid_t 	uid;
    gid_t 	gid;
    pid_t 	pid;
};

using Ino = uint64_t;
struct S3DirStream {
    Ino ino;
    uint64_t fh;
    uint64_t offset;
};

enum class FileType {
    DIR = 1,
    FILE = 2,
};

struct Fileinfo {
    uint64_t fd; 
    int flags;
    Ino ino;
    // off64_t read_offset;
    // off64_t write_offset;
    off64_t offset;
};

struct PosixS3Info { 
     std::string filename;
     FileType type; //0 is file, 1 is dir
     Fileinfo fileinfo;
     S3DirStream dirinfo;
};


//-------------------------------------------------------------------
// Static variables
//-------------------------------------------------------------------
static uid_t mp_uid               = 0;    // owner of mount point(only not specified uid opt)
static gid_t mp_gid               = 0;    // group of mount point(only not specified gid opt)
static mode_t mp_mode             = 0;    // mode of mount point
static mode_t mp_umask            = 0;    // umask for mount point
static bool is_mp_umask           = false;// default does not set.
static std::string mountpoint;
static std::unique_ptr<S3fsCred> ps3fscred; // using only in this file
static std::string mimetype_file;
static bool nocopyapi             = false;
static bool norenameapi           = false;
static bool nonempty              = false;
static bool allow_other           = false;
static uid_t s3fs_uid             = 0;
static gid_t s3fs_gid             = 0;
static mode_t s3fs_umask          = 0;
static bool is_s3fs_uid           = false;// default does not set.
static bool is_s3fs_gid           = false;// default does not set.
static bool is_s3fs_umask         = false;// default does not set.
static bool is_remove_cache       = false;
static bool is_use_xattr          = false;
static off_t multipart_threshold  = 25 * 1024 * 1024;
static int64_t singlepart_copy_limit = 512 * 1024 * 1024;
static bool is_specified_endpoint = false;
static int s3fs_init_deferred_exit_status = 0;
static bool support_compat_dir    = false;// default does not support compatibility directory type
static int max_keys_list_object   = 1000;// default is 1000
static off_t max_dirty_data       = 5LL * 1024LL * 1024LL * 1024LL;
static bool use_wtf8              = false;
static off_t fake_diskfree_size   = -1; // default is not set(-1)
static int max_thread_count       = 5;  // default is 5
static bool update_parent_dir_stat= false;  // default not updating parent directory stats
static fsblkcnt_t bucket_block_count;                       // advertised block count of the bucket
static unsigned long s3fs_block_size = 16 * 1024 * 1024;    // s3fs block size is 16MB
std::string newcache_conf;

static std::unordered_map<int,  PosixS3Info> fdtofile(1000);
static struct PosixContext posixcontext;
//-------------------------------------------------------------------
// Global functions : prototype
//-------------------------------------------------------------------
int put_headers(const char* path, headers_t& meta, bool is_copy, bool use_st_size = true);       // [NOTE] global function because this is called from FdEntity class



//-------------------------------------------------------------------
// Static functions : prototype
//-------------------------------------------------------------------
static int init_config(std::string configpath);

static bool is_special_name_folder_object(const char* path);
static int chk_dir_object_type(const char* path, std::string& newpath, std::string& nowpath, std::string& nowcache, headers_t* pmeta = nullptr, dirtype* pDirType = nullptr);
static int remove_old_type_dir(const std::string& path, dirtype type);
static int get_object_attribute(const char* path, struct stat* pstbuf, headers_t* pmeta = nullptr, bool overcheck = true, bool* pisforce = nullptr, bool add_no_truncate_cache = false);
static int check_object_access(const char* path, int mask, struct stat* pstbuf);
static int check_object_owner(const char* path, struct stat* pstbuf);
static int check_parent_object_access(const char* path, int mask);
static int get_local_fent(AutoFdEntity& autoent, FdEntity **entity, const char* path, int flags = O_RDONLY, bool is_load = false);
static bool multi_head_callback(S3fsCurl* s3fscurl, void* param);
static std::unique_ptr<S3fsCurl> multi_head_retry_callback(S3fsCurl* s3fscurl);
//static int readdir_multi_head(const char* path, const S3ObjList& head, void* buf, fuse_fill_dir_t filler);
static int readdir_multi_head(const char* path, const S3ObjList& head, char* data, int offset, int maxread, ssize_t* realbytes, int* realnum);
static int list_bucket(const char* path, S3ObjList& head, const char* delimiter, bool check_content_only = false);
static int directory_empty(const char* path);
static int rename_large_object(const char* from, const char* to);
static int create_file_object(const char* path, mode_t mode, uid_t uid, gid_t gid);
static int create_directory_object(const char* path, mode_t mode, const struct timespec& ts_atime, const struct timespec& ts_mtime, const struct timespec& ts_ctime, uid_t uid, gid_t gid, const char* pxattrvalue);
static int rename_object(const char* from, const char* to, bool update_ctime);
static int rename_object_nocopy(const char* from, const char* to, bool update_ctime);
static int clone_directory_object(const char* from, const char* to, bool update_ctime, const char* pxattrvalue);
static int rename_directory(const char* from, const char* to);
static int update_mctime_parent_directory(const char* _path);
static int remote_mountpath_exists(const char* path, bool compat_dir);
static bool get_meta_xattr_value(const char* path, std::string& rawvalue);
static bool get_parent_meta_xattr_value(const char* path, std::string& rawvalue);
static bool get_xattr_posix_key_value(const char* path, std::string& xattrvalue, bool default_key);
static bool build_inherited_xattr_value(const char* path, std::string& xattrvalue);
static bool parse_xattr_keyval(const std::string& xattrpair, std::string& key, std::string* pval);
static size_t parse_xattrs(const std::string& strxattrs, xattrs_t& xattrs);
static std::string raw_build_xattrs(const xattrs_t& xattrs);
static std::string build_xattrs(const xattrs_t& xattrs);
static int s3fs_check_service();
static bool set_mountpoint_attribute(struct stat& mpst);
static int set_bucket(const char* arg);
static int my_fuse_opt_proc(void* data, const char* arg, int key, struct fuse_args* outargs);
static fsblkcnt_t parse_bucket_size(char* value);
static bool is_cmd_exists(const std::string& command);
static int print_umount_message(const std::string& mp, bool force);




//-------------------------------------------------------------------
// Classes
//-------------------------------------------------------------------
//
// A flag class indicating whether the mount point has a stat
//
// [NOTE]
// The flag is accessed from child threads, so This class is used for exclusive control of flags.
// This class will be reviewed when we organize the code in the future.
//
class MpStatFlag
{
    private:
        std::atomic<bool>       has_mp_stat;

    public:
        MpStatFlag() = default;
        MpStatFlag(const MpStatFlag&) = delete;
        MpStatFlag(MpStatFlag&&) = delete;
        ~MpStatFlag() = default;
        MpStatFlag& operator=(const MpStatFlag&) = delete;
        MpStatFlag& operator=(MpStatFlag&&) = delete;

        bool Get();
        bool Set(bool flag);
};

bool MpStatFlag::Get()
{
    return has_mp_stat;
}

bool MpStatFlag::Set(bool flag)
{
    return has_mp_stat.exchange(flag);
}

// whether the stat information file for mount point exists
static MpStatFlag* pHasMpStat     = nullptr;



//
// A synchronous class that calls the fuse_fill_dir_t function that processes the readdir data
//

typedef int (*fill_dir_t) (void *buf, const char *name,
				const struct stat *stbuf, off_t off);

class SyncFiller
{
    private:
        mutable pthread_mutex_t filler_lock;
        bool                    is_lock_init = false;
        void*                   filler_buff;
        fill_dir_t         filler_func;
        std::set<std::string>   filled;

    public:
        explicit SyncFiller(void* buff = nullptr, fill_dir_t filler = nullptr);
        SyncFiller(const SyncFiller&) = delete;
        SyncFiller(SyncFiller&&) = delete;
        ~SyncFiller();
        SyncFiller& operator=(const SyncFiller&) = delete;
        SyncFiller& operator=(SyncFiller&&) = delete;

        int Fill(const char *name, const struct stat *stbuf, off_t off);
        int SufficiencyFill(const std::vector<std::string>& pathlist);
};

SyncFiller::SyncFiller(void* buff, fill_dir_t filler) : filler_buff(buff), filler_func(filler)
{
    if(!filler_buff || !filler_func){
        S3FS_PRN_CRIT("Internal error: SyncFiller constructor parameter is critical value.");
        abort();
    }

    pthread_mutexattr_t attr;
    pthread_mutexattr_init(&attr);
#if S3FS_PTHREAD_ERRORCHECK
    pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_ERRORCHECK);
#endif

    int result;
    if(0 != (result = pthread_mutex_init(&filler_lock, &attr))){
        S3FS_PRN_CRIT("failed to init filler_lock: %d", result);
        abort();
    }
    is_lock_init = true;
}

SyncFiller::~SyncFiller()
{
    if(is_lock_init){
        int result;
        if(0 != (result = pthread_mutex_destroy(&filler_lock))){
            S3FS_PRN_CRIT("failed to destroy filler_lock: %d", result);
            abort();
        }
        is_lock_init = false;
    }
}

//
// See. prototype fuse_fill_dir_t in fuse.h
//
int SyncFiller::Fill(const char *name, const struct stat *stbuf, off_t off)
{
    AutoLock auto_lock(&filler_lock);

    int result = 0;
    if(filled.insert(name).second){
        result = filler_func(filler_buff, name, stbuf, off);
    }
    return result;
}

int SyncFiller::SufficiencyFill(const std::vector<std::string>& pathlist)
{
    AutoLock auto_lock(&filler_lock);

    int result = 0;
    for(std::vector<std::string>::const_iterator it = pathlist.begin(); it != pathlist.end(); ++it) {
        if(filled.insert(*it).second){
            if(0 != filler_func(filler_buff, it->c_str(), nullptr, 0)){
                result = 1;
            }
        }
    }
    return result;
}



//-------------------------------------------------------------------
// Functions
//-------------------------------------------------------------------
static bool IS_REPLACEDIR(dirtype type)
{
    return dirtype::OLD == type || dirtype::FOLDER == type || dirtype::NOOBJ == type;
}

static bool IS_RMTYPEDIR(dirtype type)
{
    return dirtype::OLD == type || dirtype::FOLDER == type;
}

static bool IS_CREATE_MP_STAT(const char* path)
{
    // [NOTE]
    // pHasMpStat->Get() is set in get_object_attribute()
    //
    return (path && 0 == strcmp(path, "/") && !pHasMpStat->Get());
}

int put_headers(const char* path, headers_t& meta, bool is_copy, bool use_st_size)
{
    int         result;
    S3fsCurl    s3fscurl(true);
    off_t       size;
    std::string strpath;

    S3FS_PRN_INFO2("[path=%s]", path);

    if(0 == strcmp(path, "/") && mount_prefix.empty()){
        strpath = "//";     // for the mount point that is bucket root, change "/" to "//".
    }else{
        strpath = path;
    }

    // files larger than 5GB must be modified via the multipart interface
    // call use_st_size as false when the file does not exist(ex. rename object)
    if(use_st_size && '/' != *strpath.rbegin()){     // directory object("dir/") is always 0(Content-Length = 0)
        struct stat buf;
        if(0 != (result = get_object_attribute(path, &buf))){
          return result;
        }
        size = buf.st_size;
    }else{
        size = get_size(meta);
    }

    if(!nocopyapi && !nomultipart && size >= multipart_threshold){
        if(0 != (result = s3fscurl.MultipartHeadRequest(strpath.c_str(), size, meta, is_copy))){
            return result;
        }
    }else{
        if(0 != (result = s3fscurl.PutHeadRequest(strpath.c_str(), meta, is_copy))){
            return result;
        }
    }
    return 0;
}


static int directory_empty(const char* path)
{
    int result;
    S3ObjList head;

    if((result = list_bucket(path, head, "/", true)) != 0){
        S3FS_PRN_ERR("list_bucket returns error.");
        return result;
    }
    if(!head.IsEmpty()){
        return -ENOTEMPTY;
    }
    return 0;
}

//
// Get object attributes with stat cache.
// This function is base for s3fs_getattr().
//
// [NOTICE]
// Checking order is changed following list because of reducing the number of the requests.
// 1) "dir"
// 2) "dir/"
// 3) "dir_$folder$"
//
// Special two case of the mount point directory:
//  [Case 1] the mount point is the root of the bucket:
//           1) "/"
//
//  [Case 2] the mount point is a directory path(ex. foo) below the bucket:
//           1) "foo"
//           2) "foo/"
//           3) "foo_$folder$"
//
static int get_object_attribute(const char* path, struct stat* pstbuf, headers_t* pmeta, bool overcheck, bool* pisforce, bool add_no_truncate_cache)
{
    int          result = -1;
    struct stat  tmpstbuf;
    struct stat* pstat = pstbuf ? pstbuf : &tmpstbuf;
    headers_t    tmpHead;
    headers_t*   pheader = pmeta ? pmeta : &tmpHead;
    std::string  strpath;
    S3fsCurl     s3fscurl;
    bool         forcedir = false;
    bool         is_mountpoint = false;             // path is the mount point
    bool         is_bucket_mountpoint = false;      // path is the mount point which is the bucket root
    std::string::size_type Pos;

    S3FS_PRN_DBG("[path=%s]", path);

    if(!path || '\0' == path[0]){
        return -ENOENT;
    }

    memset(pstat, 0, sizeof(struct stat));

    // check mount point
    if(0 == strcmp(path, "/") || 0 == strcmp(path, ".")){
        is_mountpoint = true;
        if(mount_prefix.empty()){
            is_bucket_mountpoint = true;
        }
        // default stat for mount point if the directory stat file is not existed.
        pstat->st_mode  = mp_mode;
        pstat->st_uid   = is_s3fs_uid ? s3fs_uid : mp_uid;
        pstat->st_gid   = is_s3fs_gid ? s3fs_gid : mp_gid;
    }

    // Check cache.
    pisforce    = (nullptr != pisforce ? pisforce : &forcedir);
    (*pisforce) = false;
    strpath     = path;
    if(support_compat_dir && overcheck && std::string::npos != (Pos = strpath.find("_$folder$", 0))){
        strpath.erase(Pos);
        strpath += "/";
    }
    // [NOTE]
    // For mount points("/"), the Stat cache key name is "/".
    //
    if(StatCache::getStatCacheData()->GetStat(strpath, pstat, pheader, overcheck, pisforce)){
        if(is_mountpoint){
            // if mount point, we need to set this.
            pstat->st_nlink = 1; // see fuse faq
        }
        return 0;
    }
    if(StatCache::getStatCacheData()->IsNoObjectCache(strpath)){
        // there is the path in the cache for no object, it is no object.
        return -ENOENT;
    }

    // set query(head request) path
    if(is_bucket_mountpoint){
        // [NOTE]
        // This is a special process for mount point
        // The path is "/" for mount points.
        // If the bucket mounted at a mount point, we try to find "/" object under
        // the bucket for mount point's stat.
        // In this case, we will send the request "HEAD // HTTP /1.1" to S3 server.
        //
        // If the directory under the bucket is mounted, it will be sent
        // "HEAD /<directories ...>/ HTTP/1.1", so we do not need to change path at
        // here.
        //
        strpath = "//";         // strpath is "//"
    }else{
        strpath = path;
    }

    if(use_newcache && accessor->UseGlobalCache()){
        size_t realSize = 0;
        std::map<std::string, std::string> headers;
        result = accessor->Head(strpath, realSize, headers);
        if(0 == result){
            headers["Content-Length"] = std::to_string(realSize);
            for(auto& it : headers) {
                pheader->insert(std::make_pair(it.first, it.second));
            }
        }
    } else {
        result      = s3fscurl.HeadRequest(strpath.c_str(), (*pheader));
        s3fscurl.DestroyCurlHandle();
    }


    // if not found target path object, do over checking
    if(-EPERM == result){
        // [NOTE]
        // In case of a permission error, it exists in directory
        // file list but inaccessible. So there is a problem that
        // it will send a HEAD request every time, because it is
        // not registered in the Stats cache.
        // Therefore, even if the file has a permission error, it
        // should be registered in the Stats cache. However, if
        // the response without modifying is registered in the
        // cache, the file permission will be 0644(umask dependent)
        // because the meta header does not exist.
        // Thus, set the mode of 0000 here in the meta header so
        // that s3fs can print a permission error when the file
        // is actually accessed.
        // It is better not to set meta header other than mode,
        // so do not do it.
        //
        (*pheader)["x-amz-meta-mode"] = "0";

    }else if(0 != result){
        if(overcheck && !is_bucket_mountpoint){
            // when support_compat_dir is disabled, strpath maybe have "_$folder$".
            if('/' != *strpath.rbegin() && std::string::npos == strpath.find("_$folder$", 0)){
                // now path is "object", do check "object/" for over checking
                strpath    += "/";
                result      = s3fscurl.HeadRequest(strpath.c_str(), (*pheader));
                s3fscurl.DestroyCurlHandle();
            }
            if(support_compat_dir && 0 != result){
                // now path is "object/", do check "object_$folder$" for over checking
                strpath.erase(strpath.length() - 1);
                strpath    += "_$folder$";
                result      = s3fscurl.HeadRequest(strpath.c_str(), (*pheader));
                s3fscurl.DestroyCurlHandle();

              if(0 != result){
                  // cut "_$folder$" for over checking "no dir object" after here
                  if(std::string::npos != (Pos = strpath.find("_$folder$", 0))){
                      strpath.erase(Pos);
                  }
              }
            }
        }
        if(0 != result && std::string::npos == strpath.find("_$folder$", 0)){
            // now path is "object" or "object/", do check "no dir object" which is not object but has only children.
            //
            // [NOTE]
            // If the path is mount point and there is no Stat information file for it, we need this process.
            //
            if('/' == *strpath.rbegin()){
                strpath.erase(strpath.length() - 1);
            }
            if(-ENOTEMPTY == directory_empty(strpath.c_str())){
                // found "no dir object".
                strpath  += "/";
                *pisforce = true;
                result    = 0;
            }
        }
    }else{
        if('/' != *strpath.rbegin() && std::string::npos == strpath.find("_$folder$", 0) && is_need_check_obj_detail(*pheader)){
            // check a case of that "object" does not have attribute and "object" is possible to be directory.
            if(-ENOTEMPTY == directory_empty(strpath.c_str())){
                // found "no dir object".
                strpath  += "/";
                *pisforce = true;
                result    = 0;
            }
        }
    }

    // set headers for mount point from default stat
    if(is_mountpoint){
        if(0 != result || pheader->empty()){
            pHasMpStat->Set(false);

            // [NOTE]
            // If mount point and no stat information file, create header
            // information from the default stat.
            //
            (*pheader)["Content-Type"]     = S3fsCurl::LookupMimeType(strpath);
            (*pheader)["x-amz-meta-uid"]   = std::to_string(pstat->st_uid);
            (*pheader)["x-amz-meta-gid"]   = std::to_string(pstat->st_gid);
            (*pheader)["x-amz-meta-mode"]  = std::to_string(pstat->st_mode);
            (*pheader)["x-amz-meta-atime"] = std::to_string(pstat->st_atime);
            (*pheader)["x-amz-meta-ctime"] = std::to_string(pstat->st_ctime);
            (*pheader)["x-amz-meta-mtime"] = std::to_string(pstat->st_mtime);

            result = 0;
        }else{
            pHasMpStat->Set(true);
        }
    }

    // [NOTE]
    // If the file is listed but not allowed access, put it in
    // the positive cache instead of the negative cache.
    // 
    // When mount points, the following error does not occur.
    // 
    if(0 != result && -EPERM != result){
        // finally, "path" object did not find. Add no object cache.
        strpath = path;  // reset original
        StatCache::getStatCacheData()->AddNoObjectCache(strpath);
        return result;
    }

    // set cache key
    if(is_bucket_mountpoint){
        strpath = "/";
    }else if(std::string::npos != (Pos = strpath.find("_$folder$", 0))){
        // if path has "_$folder$", need to cut it.
        strpath.erase(Pos);
        strpath += "/";
    }

    // Set into cache
    //
    // [NOTE]
    // When add_no_truncate_cache is true, the stats is always cached.
    // This cached stats is only removed by DelStat().
    // This is necessary for the case to access the attribute of opened file.
    // (ex. getxattr() is called while writing to the opened file.)
    //
    if(add_no_truncate_cache || 0 != StatCache::getStatCacheData()->GetCacheSize()){
        // add into stat cache
        if(!StatCache::getStatCacheData()->AddStat(strpath, (*pheader), forcedir, add_no_truncate_cache)){
            S3FS_PRN_ERR("failed adding stat cache [path=%s]", strpath.c_str());
            return -ENOENT;
        }
        if(!StatCache::getStatCacheData()->GetStat(strpath, pstat, pheader, overcheck, pisforce)){
            // There is not in cache.(why?) -> retry to convert.
            if(!convert_header_to_stat(strpath.c_str(), (*pheader), pstat, forcedir)){
                S3FS_PRN_ERR("failed convert headers to stat[path=%s]", strpath.c_str());
                return -ENOENT;
            }
        }
    }else{
        // cache size is Zero -> only convert.
        if(!convert_header_to_stat(strpath.c_str(), (*pheader), pstat, forcedir)){
            S3FS_PRN_ERR("failed convert headers to stat[path=%s]", strpath.c_str());
            return -ENOENT;
        }
    }

    if(is_mountpoint){
        // if mount point, we need to set this.
        pstat->st_nlink = 1; // see fuse faq
    }

    return 0;
}

bool get_object_sse_type(const char* path, sse_type_t& ssetype, std::string& ssevalue)
{
    if(!path){
        return false;
    }

    headers_t meta;
    if(0 != get_object_attribute(path, nullptr, &meta)){
        S3FS_PRN_ERR("Failed to get object(%s) headers", path);
        return false;
    }

    ssetype = sse_type_t::SSE_DISABLE;
    ssevalue.clear();
    for(headers_t::iterator iter = meta.begin(); iter != meta.end(); ++iter){
        std::string key = (*iter).first;
        if(0 == strcasecmp(key.c_str(), "x-amz-server-side-encryption") && 0 == strcasecmp((*iter).second.c_str(), "AES256")){
            ssetype  = sse_type_t::SSE_S3;
        }else if(0 == strcasecmp(key.c_str(), "x-amz-server-side-encryption-aws-kms-key-id")){
            ssetype  = sse_type_t::SSE_KMS;
            ssevalue = (*iter).second;
        }else if(0 == strcasecmp(key.c_str(), "x-amz-server-side-encryption-customer-key-md5")){
            ssetype  = sse_type_t::SSE_C;
            ssevalue = (*iter).second;
        }
    }
    return true;
}


//
// Check the object uid and gid for write/read/execute.
// The param "mask" is as same as access() function.
// If there is not a target file, this function returns -ENOENT.
// If the target file can be accessed, the result always is 0.
//
// path:   the target object path
// mask:   bit field(F_OK, R_OK, W_OK, X_OK) like access().
// stat:   nullptr or the pointer of struct stat.
//
static int check_object_access(const char* path, int mask, struct stat* pstbuf)
{
    //return 0;
    int result;
     struct stat st;
     struct stat* pst = (pstbuf ? pstbuf : &st);
    // struct fuse_context* pcxt;

    // S3FS_PRN_DBG("[path=%s]", path);

    // if(nullptr == (pcxt = fuse_get_context())){
    //     return -EIO;
    // }
    // S3FS_PRN_DBG("[pid=%u,uid=%u,gid=%u]", (unsigned int)(pcxt->pid), (unsigned int)(pcxt->uid), (unsigned int)(pcxt->gid));

    if(0 != (result = get_object_attribute(path, pst))){
        // If there is not the target file(object), result is -ENOENT.
        return result;
    }
    // if(0 == pcxt->uid){
    //     // root is allowed all accessing.
    //     return 0;
    // }
    // if(is_s3fs_uid && s3fs_uid == pcxt->uid){
    //     // "uid" user is allowed all accessing.
    //     return 0;
    // }
    // if(F_OK == mask){
    //     // if there is a file, always return allowed.
    //     return 0;
    // }

    // // for "uid", "gid" option
    // uid_t  obj_uid = (is_s3fs_uid ? s3fs_uid : pst->st_uid);
    // gid_t  obj_gid = (is_s3fs_gid ? s3fs_gid : pst->st_gid);

    // // compare file mode and uid/gid + mask.
    // mode_t mode;
    // mode_t base_mask = S_IRWXO;
    // if(is_s3fs_umask){
    //     // If umask is set, all object attributes set ~umask.
    //     mode = ((S_IRWXU | S_IRWXG | S_IRWXO) & ~s3fs_umask);
    // }else{
    //     mode = pst->st_mode;
    // }
    // if(pcxt->uid == obj_uid){
    //     base_mask |= S_IRWXU;
    // }
    // if(pcxt->gid == obj_gid){
    //     base_mask |= S_IRWXG;
    // } else if(1 == is_uid_include_group(pcxt->uid, obj_gid)){
    //     base_mask |= S_IRWXG;
    // }
    // mode &= base_mask;

    // if(X_OK == (mask & X_OK)){
    //     if(0 == (mode & (S_IXUSR | S_IXGRP | S_IXOTH))){
    //         return -EACCES;
    //     }
    // }
    // if(W_OK == (mask & W_OK)){
    //     if(0 == (mode & (S_IWUSR | S_IWGRP | S_IWOTH))){
    //         return -EACCES;
    //     }
    // }
    // if(R_OK == (mask & R_OK)){
    //     if(0 == (mode & (S_IRUSR | S_IRGRP | S_IROTH))){
    //         return -EACCES;
    //     }
    // }
    // if(0 == mode){
    //     return -EACCES;
    // }
    return 0;
}

static bool check_region_error(const char* pbody, size_t len, std::string& expectregion)
{
    if(!pbody){
        return false;
    }

    std::string code;
    if(!simple_parse_xml(pbody, len, "Code", code) || code != "AuthorizationHeaderMalformed"){
        return false;
    }

    if(!simple_parse_xml(pbody, len, "Region", expectregion)){
        return false;
    }

    return true;
}

static bool check_endpoint_error(const char* pbody, size_t len, std::string& expectendpoint)
{
    if(!pbody){
        return false;
    }

    std::string code;
    if(!simple_parse_xml(pbody, len, "Code", code) || code != "PermanentRedirect"){
        return false;
    }

    if(!simple_parse_xml(pbody, len, "Endpoint", expectendpoint)){
        return false;
    }

    return true;
}

static bool check_invalid_sse_arg_error(const char* pbody, size_t len)
{
    if(!pbody){
        return false;
    }

    std::string code;
    if(!simple_parse_xml(pbody, len, "Code", code) || code != "InvalidArgument"){
        return false;
    }
    std::string argname;
    if(!simple_parse_xml(pbody, len, "ArgumentName", argname) || argname != "x-amz-server-side-encryption"){
        return false;
    }
    return true;
}

static bool check_error_message(const char* pbody, size_t len, std::string& message)
{
    message.clear();
    if(!pbody){
        return false;
    }
    if(!simple_parse_xml(pbody, len, "Message", message)){
        return false;
    }
    return true;
}



// [NOTE]
// This function checks if the bucket is accessible when s3fs starts.
//
// The following patterns for mount points are supported by s3fs:
// (1) Mount the bucket top
// (2) Mount to a directory(folder) under the bucket. In this case:
//     (2A) Directories created by clients other than s3fs
//     (2B) Directory created by s3fs
//
// Both case of (1) and (2) check access permissions to the mount point
// path(directory).
// In the case of (2A), if the directory(object) for the mount point does
// not exist, the check fails. However, launching s3fs with the "compat_dir"
// option avoids this error and the check succeeds. If you do not specify
// the "compat_dir" option in case (2A), please create a directory(object)
// for the mount point before launching s3fs.
//
static int s3fs_check_service()
{
    S3FS_PRN_INFO("check services.");

    // At first time for access S3, we check IAM role if it sets.
    if(!ps3fscred->CheckIAMCredentialUpdate()){
        S3FS_PRN_CRIT("Failed to initialize IAM credential.");
        return EXIT_FAILURE;
    }

    S3fsCurl s3fscurl;
    int      res;
    bool     force_no_sse = false;

    while(0 > (res = s3fscurl.CheckBucket(get_realpath("/").c_str(), support_compat_dir, force_no_sse))){
        // get response code
        bool do_retry     = false;
        long responseCode = s3fscurl.GetLastResponseCode();

        // check wrong endpoint, and automatically switch endpoint
        if(300 <= responseCode && responseCode < 500){

            // check region error(for putting message or retrying)
            const std::string* body = s3fscurl.GetBodyData();
            std::string expectregion;
            std::string expectendpoint;

            // Check if any case can be retried
            if(check_region_error(body->c_str(), body->size(), expectregion)){
                // [NOTE]
                // If endpoint is not specified(using us-east-1 region) and
                // an error is encountered accessing a different region, we
                // will retry the check on the expected region.
                // see) https://docs.aws.amazon.com/AmazonS3/latest/dev/UsingBucket.html#access-bucket-intro
                //
                if(s3host != "http://s3.amazonaws.com" && s3host != "https://s3.amazonaws.com"){
                    // specified endpoint for specified url is wrong.
                    if(is_specified_endpoint){
                        S3FS_PRN_CRIT("The bucket region is not '%s'(specified) for specified url(%s), it is correctly '%s'. You should specify url(http(s)://s3-%s.amazonaws.com) and endpoint(%s) option.", endpoint.c_str(), s3host.c_str(), expectregion.c_str(), expectregion.c_str(), expectregion.c_str());
                    }else{
                        S3FS_PRN_CRIT("The bucket region is not '%s'(default) for specified url(%s), it is correctly '%s'. You should specify url(http(s)://s3-%s.amazonaws.com) and endpoint(%s) option.", endpoint.c_str(), s3host.c_str(), expectregion.c_str(), expectregion.c_str(), expectregion.c_str());
                    }

                }else if(is_specified_endpoint){
                    // specified endpoint is wrong.
                    S3FS_PRN_CRIT("The bucket region is not '%s'(specified), it is correctly '%s'. You should specify endpoint(%s) option.", endpoint.c_str(), expectregion.c_str(), expectregion.c_str());

                }else if(S3fsCurl::GetSignatureType() == signature_type_t::V4_ONLY || S3fsCurl::GetSignatureType() == signature_type_t::V2_OR_V4){
                    // current endpoint and url are default value, so try to connect to expected region.
                    S3FS_PRN_CRIT("Failed to connect region '%s'(default), so retry to connect region '%s' for url(http(s)://s3-%s.amazonaws.com).", endpoint.c_str(), expectregion.c_str(), expectregion.c_str());

                    // change endpoint
                    endpoint = expectregion;

                    // change url
                    if(s3host == "http://s3.amazonaws.com"){
                        s3host = "http://s3-" + endpoint + ".amazonaws.com";
                    }else if(s3host == "https://s3.amazonaws.com"){
                        s3host = "https://s3-" + endpoint + ".amazonaws.com";
                    }

                    // Retry with changed host
                    s3fscurl.DestroyCurlHandle();
                    do_retry = true;

                }else{
                    S3FS_PRN_CRIT("The bucket region is not '%s'(default), it is correctly '%s'. You should specify endpoint(%s) option.", endpoint.c_str(), expectregion.c_str(), expectregion.c_str());
                }

            }else if(check_endpoint_error(body->c_str(), body->size(), expectendpoint)){
                // redirect error
                if(pathrequeststyle){
                    S3FS_PRN_CRIT("S3 service returned PermanentRedirect (current is url(%s) and endpoint(%s)). You need to specify correct url(http(s)://s3-<endpoint>.amazonaws.com) and endpoint option with use_path_request_style option.", s3host.c_str(), endpoint.c_str());
                }else{
                    S3FS_PRN_CRIT("S3 service returned PermanentRedirect with %s (current is url(%s) and endpoint(%s)). You need to specify correct endpoint option.", expectendpoint.c_str(), s3host.c_str(), endpoint.c_str());
                }
                return EXIT_FAILURE;

            }else if(check_invalid_sse_arg_error(body->c_str(), body->size())){
                // SSE argument error, so retry it without SSE
                S3FS_PRN_CRIT("S3 service returned InvalidArgument(x-amz-server-side-encryption), so retry without adding x-amz-server-side-encryption.");

                // Retry without sse parameters
                s3fscurl.DestroyCurlHandle();
                do_retry     = true;
                force_no_sse = true;
            }
        }

        // Try changing signature from v4 to v2
        //
        // [NOTE]
        // If there is no case to retry with the previous checks, and there
        // is a chance to retry with signature v2, prepare to retry with v2.
        //
        if(!do_retry && (responseCode == 400 || responseCode == 403) && S3fsCurl::GetSignatureType() == signature_type_t::V2_OR_V4){
            // switch sigv2
            S3FS_PRN_CRIT("Failed to connect by sigv4, so retry to connect by signature version 2. But you should to review url and endpoint option.");

            // retry to check with sigv2
            s3fscurl.DestroyCurlHandle();
            do_retry = true;
            S3fsCurl::SetSignatureType(signature_type_t::V2_ONLY);
        }

        // check errors(after retrying)
        if(!do_retry && responseCode != 200 && responseCode != 301){
            // parse error message if existed
            std::string errMessage;
            const std::string* body = s3fscurl.GetBodyData();
            check_error_message(body->c_str(), body->size(), errMessage);

            if(responseCode == 400){
                S3FS_PRN_CRIT("Failed to check bucket and directory for mount point : Bad Request(host=%s, message=%s)", s3host.c_str(), errMessage.c_str());
            }else if(responseCode == 403){
                S3FS_PRN_CRIT("Failed to check bucket and directory for mount point : Invalid Credentials(host=%s, message=%s)", s3host.c_str(), errMessage.c_str());
            }else if(responseCode == 404){
                if(mount_prefix.empty()){
                    S3FS_PRN_CRIT("Failed to check bucket and directory for mount point : Bucket or directory not found(host=%s, message=%s)", s3host.c_str(), errMessage.c_str());
                }else{
                    S3FS_PRN_CRIT("Failed to check bucket and directory for mount point : Bucket or directory(%s) not found(host=%s, message=%s) - You may need to specify the compat_dir option.", mount_prefix.c_str(), s3host.c_str(), errMessage.c_str());
                }
            }else{
                S3FS_PRN_CRIT("Failed to check bucket and directory for mount point : Unable to connect(host=%s, message=%s)", s3host.c_str(), errMessage.c_str());
            }
            return EXIT_FAILURE;
        }
    }
    s3fscurl.DestroyCurlHandle();

    // make sure remote mountpath exists and is a directory
    if(!mount_prefix.empty()){
        if(remote_mountpath_exists("/", support_compat_dir) != 0){
            S3FS_PRN_CRIT("Remote mountpath %s not found, this may be resolved with the compat_dir option.", mount_prefix.c_str());
            return EXIT_FAILURE;
        }
    }
    S3FS_MALLOCTRIM(0);

    return EXIT_SUCCESS;
}

//
// Check accessing the parent directories of the object by uid and gid.
//
static int check_parent_object_access(const char* path, int mask)
{
    std::string parent;
    int result;

    S3FS_PRN_DBG("[path=%s]", path);

    if(0 == strcmp(path, "/") || 0 == strcmp(path, ".")){
        // path is mount point.
        return 0;
    }
    if(X_OK == (mask & X_OK)){
        for(parent = mydirname(path); !parent.empty(); parent = mydirname(parent)){
            if(parent == "."){
                parent = "/";
            }
            if(0 != (result = check_object_access(parent.c_str(), X_OK, nullptr))){
                return result;
            }
            if(parent == "/" || parent == "."){
                break;
            }
        }
    }
    mask = (mask & ~X_OK);
    if(0 != mask){
        parent = mydirname(path);
        if(parent == "."){
            parent = "/";
        }
        if(0 != (result = check_object_access(parent.c_str(), mask, nullptr))){
            return result;
        }
    }
    return 0;
}

static int list_bucket(const char* path, S3ObjList& head, const char* delimiter, bool check_content_only)
{
    std::string s3_realpath;
    std::string query_delimiter;
    std::string query_prefix;
    std::string query_maxkey;
    std::string next_continuation_token;
    std::string next_marker;
    bool truncated = true;
    S3fsCurl  s3fscurl;

    S3FS_PRN_INFO1("[path=%s]", path);

    if(delimiter && 0 < strlen(delimiter)){
        query_delimiter += "delimiter=";
        query_delimiter += delimiter;
        query_delimiter += "&";
    }

    query_prefix += "&prefix=";
    s3_realpath = get_realpath(path);
    if(s3_realpath.empty() || '/' != *s3_realpath.rbegin()){
        // last word must be "/"
        query_prefix += urlEncodePath(s3_realpath.substr(1) + "/");
    }else{
        query_prefix += urlEncodePath(s3_realpath.substr(1));
    }
    if (check_content_only){
        // Just need to know if there are child objects in dir
        // For dir with children, expect "dir/" and "dir/child"
        query_maxkey += "max-keys=2";
    }else{
        query_maxkey += "max-keys=" + std::to_string(max_keys_list_object);
    }

    while(truncated){
        // append parameters to query in alphabetical order
        std::string each_query;
        if(!next_continuation_token.empty()){
            each_query += "continuation-token=" + urlEncodePath(next_continuation_token) + "&";
            next_continuation_token = "";
        }
        each_query += query_delimiter;
        if(S3fsCurl::IsListObjectsV2()){
            each_query += "list-type=2&";
        }
        if(!next_marker.empty()){
            each_query += "marker=" + urlEncodePath(next_marker) + "&";
            next_marker = "";
        }
        each_query += query_maxkey;
        each_query += query_prefix;

        // request
        int result; 
        if(0 != (result = s3fscurl.ListBucketRequest(path, each_query.c_str()))){
            S3FS_PRN_ERR("ListBucketRequest returns with error.");
            return result;
        }
        const std::string* body = s3fscurl.GetBodyData();

        // [NOTE]
        // CR code(\r) is replaced with LF(\n) by xmlReadMemory() function.
        // To prevent that, only CR code is encoded by following function.
        // The encoded CR code is decoded with append_objects_from_xml(_ex).
        //
        std::string encbody = get_encoded_cr_code(body->c_str());

        // xmlDocPtr
        std::unique_ptr<xmlDoc, decltype(&xmlFreeDoc)> doc(xmlReadMemory(encbody.c_str(), static_cast<int>(encbody.size()), "", nullptr, 0), xmlFreeDoc);
        if(nullptr == doc){
            S3FS_PRN_ERR("xmlReadMemory returns with error.");
            return -EIO;
        }
        if(0 != append_objects_from_xml(path, doc.get(), head)){
            S3FS_PRN_ERR("append_objects_from_xml returns with error.");
            return -EIO;
        }
        if(true == (truncated = is_truncated(doc.get()))){
            auto tmpch = get_next_continuation_token(doc.get());
            if(nullptr != tmpch){
                next_continuation_token = reinterpret_cast<const char*>(tmpch.get());
            }else if(nullptr != (tmpch = get_next_marker(doc.get()))){
                next_marker = reinterpret_cast<const char*>(tmpch.get());
            }

            if(next_continuation_token.empty() && next_marker.empty()){
                // If did not specify "delimiter", s3 did not return "NextMarker".
                // On this case, can use last name for next marker.
                //
                std::string lastname;
                if(!head.GetLastName(lastname)){
                    S3FS_PRN_WARN("Could not find next marker, thus break loop.");
                    truncated = false;
                }else{
                    next_marker = s3_realpath.substr(1);
                    if(s3_realpath.empty() || '/' != *s3_realpath.rbegin()){
                        next_marker += "/";
                    }
                    next_marker += lastname;
                }
            }
        }

        // reset(initialize) curl object
        s3fscurl.DestroyCurlHandle();

        if(check_content_only){
            break;
        }
    }
    S3FS_MALLOCTRIM(0);

    return 0;
}

static int remote_mountpath_exists(const char* path, bool compat_dir)
{
    struct stat stbuf;
    int result;

    S3FS_PRN_INFO1("[path=%s]", path);

    // getattr will prefix the path with the remote mountpoint
    if(0 != (result = get_object_attribute(path, &stbuf, nullptr))){
        return result;
    }

    // [NOTE]
    // If there is no mount point(directory object) that s3fs can recognize,
    // an error will occur.
    // A mount point with a directory path(ex. "<bucket>/<directory>...")
    // requires that directory object.
    // If the directory or object is created by a client other than s3fs,
    // s3fs may not be able to recognize it. If you specify such a directory
    // as a mount point, you can avoid the error by starting with "compat_dir"
    // specified.
    //
    if(!compat_dir && !pHasMpStat->Get()){
        return -ENOENT;
    }
    return 0;
}

//
// Check & Set attributes for mount point.
//
static bool set_mountpoint_attribute(struct stat& mpst)
{
    mp_uid  = geteuid();
    mp_gid  = getegid();
    mp_mode = S_IFDIR | (allow_other ? (is_mp_umask ? (~mp_umask & (S_IRWXU | S_IRWXG | S_IRWXO)) : (S_IRWXU | S_IRWXG | S_IRWXO)) : S_IRWXU);

// In MSYS2 environment with WinFsp, it is not supported to change mode of mount point.
// Doing that forcely will occurs permission problem, so disabling it.
#ifdef __MSYS__
    return true;
#else
    S3FS_PRN_INFO2("PROC(uid=%u, gid=%u) - MountPoint(uid=%u, gid=%u, mode=%04o)",
           (unsigned int)mp_uid, (unsigned int)mp_gid, (unsigned int)(mpst.st_uid), (unsigned int)(mpst.st_gid), mpst.st_mode);

    // check owner
    if(0 == mp_uid || mpst.st_uid == mp_uid){
        return true;
    }
    // check group permission
    if(mpst.st_gid == mp_gid || 1 == is_uid_include_group(mp_uid, mpst.st_gid)){
        if(S_IRWXG == (mpst.st_mode & S_IRWXG)){
            return true;
        }
    }
    // check other permission
    if(S_IRWXO == (mpst.st_mode & S_IRWXO)){
        return true;
    }
    return false;
#endif
}

//
// Set bucket and mount_prefix based on passed bucket name.
//
static int set_bucket(const char* arg)
{
    // TODO: Mutates input.  Consider some other tokenization.
    char *bucket_name = const_cast<char*>(arg);
    if(strstr(arg, ":")){
        if(strstr(arg, "://")){
            S3FS_PRN_EXIT("bucket name and path(\"%s\") is wrong, it must be \"bucket[:/path]\".", arg);
            return -1;
        }
        if(!S3fsCred::SetBucket(strtok(bucket_name, ":"))){
            S3FS_PRN_EXIT("bucket name and path(\"%s\") is wrong, it must be \"bucket[:/path]\".", arg);
            return -1;
        }
        char* pmount_prefix = strtok(nullptr, "");
        if(pmount_prefix){
            if(0 == strlen(pmount_prefix) || '/' != pmount_prefix[0]){
                S3FS_PRN_EXIT("path(%s) must be prefix \"/\".", pmount_prefix);
                return -1;
            }
            mount_prefix = pmount_prefix;
            // Trim the last consecutive '/'
            mount_prefix = trim_right(mount_prefix, "/");
        }
    }else{
        if(!S3fsCred::SetBucket(arg)){
            S3FS_PRN_EXIT("bucket name and path(\"%s\") is wrong, it must be \"bucket[:/path]\".", arg);
            return -1;
        }
    }
    return 0;
}

static int print_umount_message(const std::string& mp, bool force)
{
    std::string cmd;
    if (is_cmd_exists("fusermount")){
        if (force){
            cmd = "fusermount -uz " + mp;
        } else {
            cmd = "fusermount -u " + mp;
        }
    }else{
        if (force){
            cmd = "umount -l " + mp;
        } else {
            cmd = "umount " + mp;
        }
    }

    S3FS_PRN_EXIT("MOUNTPOINT %s is stale, you could use this command to fix: %s", mp.c_str(), cmd.c_str());

    return 0;
}

static bool is_cmd_exists(const std::string& command)
{
    // The `command -v` is a POSIX-compliant method for checking the existence of a program.
    std::string cmd = "command -v " + command + " >/dev/null 2>&1";
    int result = system(cmd.c_str());
    return (result !=-1 && WIFEXITED(result) && WEXITSTATUS(result) == 0);
}

static int update_mctime_parent_directory(const char* _path)
{
    if(!update_parent_dir_stat){
        // Disable updating parent directory stat.
        S3FS_PRN_DBG("Updating parent directory stats is disabled");
        return 0;
    }

    WTF8_ENCODE(path)
    int             result;
    std::string     parentpath;     // parent directory path
    std::string     nowpath;        // now directory object path("dir" or "dir/" or "xxx_$folder$", etc)
    std::string     newpath;        // directory path for the current version("dir/")
    std::string     nowcache;
    headers_t       meta;
    struct stat     stbuf;
    struct timespec mctime;
    struct timespec atime;
    dirtype         nDirType = dirtype::UNKNOWN;

    S3FS_PRN_INFO2("[path=%s]", path);

    // get parent directory path
    parentpath = mydirname(path);

    // check & get directory type
    if(0 != (result = chk_dir_object_type(parentpath.c_str(), newpath, nowpath, nowcache, &meta, &nDirType))){
        return result;
    }

    // get directory stat
    //
    // [NOTE]
    // It is assumed that this function is called after the operation on
    // the file is completed, so there is no need to check the permissions
    // on the parent directory.
    //
    if(0 != (result = get_object_attribute(parentpath.c_str(), &stbuf))){
        // If there is not the target file(object), result is -ENOENT.
        return result;
    }
    if(!S_ISDIR(stbuf.st_mode)){
        S3FS_PRN_ERR("path(%s) is not parent directory.", parentpath.c_str());
        return -EIO;
    }

    // make atime/mtime/ctime for updating
    s3fs_realtime(mctime);
    set_stat_to_timespec(stbuf, stat_time_type::ATIME, atime);

    if(0 == atime.tv_sec && 0 == atime.tv_nsec){
        atime = mctime;
    }

    if(nocopyapi || IS_REPLACEDIR(nDirType) || IS_CREATE_MP_STAT(parentpath.c_str())){
        // Should rebuild directory object(except new type)
        // Need to remove old dir("dir" etc) and make new dir("dir/")
        std::string xattrvalue;
        const char* pxattrvalue;
        if(get_meta_xattr_value(path, xattrvalue)){
            pxattrvalue = xattrvalue.c_str();
        }else{
            pxattrvalue = nullptr;
        }

        // At first, remove directory old object
        if(!nowpath.empty()){
            if(0 != (result = remove_old_type_dir(nowpath, nDirType))){
                return result;
            }
        }
        if(!nowcache.empty()){
            StatCache::getStatCacheData()->DelStat(nowcache);
        }

        // Make new directory object("dir/")
        if(0 != (result = create_directory_object(newpath.c_str(), stbuf.st_mode, atime, mctime, mctime, stbuf.st_uid, stbuf.st_gid, pxattrvalue))){
            return result;
        }
    }else{
        std::string strSourcePath              = (mount_prefix.empty() && "/" == nowpath) ? "//" : nowpath;
        headers_t   updatemeta;
        updatemeta["x-amz-meta-mtime"]         = str(mctime);
        updatemeta["x-amz-meta-ctime"]         = str(mctime);
        updatemeta["x-amz-meta-atime"]         = str(atime);
        updatemeta["x-amz-copy-source"]        = urlEncodePath(service_path + S3fsCred::GetBucket() + get_realpath(strSourcePath.c_str()));
        updatemeta["x-amz-metadata-directive"] = "REPLACE";

        merge_headers(meta, updatemeta, true);

        // upload meta for parent directory.
        if(0 != (result = put_headers(nowpath.c_str(), meta, true))){
            return result;
        }
        StatCache::getStatCacheData()->DelStat(nowcache);
    }
    S3FS_MALLOCTRIM(0);

    return 0;
}

static int create_directory_object(const char* path, mode_t mode, const struct timespec& ts_atime, const struct timespec& ts_mtime, const struct timespec& ts_ctime, uid_t uid, gid_t gid, const char* pxattrvalue)
{
    S3FS_PRN_INFO1("[path=%s][mode=%04o][atime=%s][mtime=%s][ctime=%s][uid=%u][gid=%u]", path, mode, str(ts_atime).c_str(), str(ts_mtime).c_str(), str(ts_ctime).c_str(), (unsigned int)uid, (unsigned int)gid);

    if(!path || '\0' == path[0]){
        return -EINVAL;
    }
    std::string tpath = path;
    if('/' != *tpath.rbegin()){
        tpath += "/";
    }else if("/" == tpath && mount_prefix.empty()){
        tpath = "//";       // for the mount point that is bucket root, change "/" to "//".
    }

    headers_t meta;
    meta["x-amz-meta-uid"]   = std::to_string(uid);
    meta["x-amz-meta-gid"]   = std::to_string(gid);
    meta["x-amz-meta-mode"]  = std::to_string(mode);
    meta["x-amz-meta-atime"] = str(ts_atime);
    meta["x-amz-meta-mtime"] = str(ts_mtime);
    meta["x-amz-meta-ctime"] = str(ts_ctime);

    if(pxattrvalue){
        S3FS_PRN_DBG("Set xattrs = %s", urlDecode(pxattrvalue).c_str());
        meta["x-amz-meta-xattr"] = pxattrvalue;
    }

    S3fsCurl s3fscurl;
    return s3fscurl.PutRequest(tpath.c_str(), meta, -1);    // fd=-1 means for creating zero byte object.
}

// [NOTE]
// Converts and returns the POSIX ACL default(system.posix_acl_default) value of
// the parent directory as a POSIX ACL(system.posix_acl_access) value.
// Returns false if the parent directory has no POSIX ACL defaults.
//
static bool build_inherited_xattr_value(const char* path, std::string& xattrvalue)
{
    S3FS_PRN_DBG("[path=%s]", path);

    xattrvalue.clear();

    if(0 == strcmp(path, "/") || 0 == strcmp(path, ".")){
        // path is mount point, thus does not have parent.
        return false;
    }

    std::string parent = mydirname(path);
    if(parent.empty()){
        S3FS_PRN_ERR("Could not get parent path for %s.", path);
        return false;
    }

    // get parent's "system.posix_acl_default" value(base64'd).
    std::string parent_default_value;
    if(!get_xattr_posix_key_value(parent.c_str(), parent_default_value, true)){
        return false;
    }

    // build "system.posix_acl_access" from parent's default value
    std::string raw_xattr_value;
    raw_xattr_value  = "{\"system.posix_acl_access\":\"";
    raw_xattr_value += parent_default_value;
    raw_xattr_value += "\"}";

    xattrvalue = urlEncodePath(raw_xattr_value);
    return true;
}

static bool get_xattr_posix_key_value(const char* path, std::string& xattrvalue, bool default_key)
{
    xattrvalue.clear();

    std::string rawvalue;
    if(!get_meta_xattr_value(path, rawvalue)){
        return false;
    }

    xattrs_t xattrs;
    if(0 == parse_xattrs(rawvalue, xattrs)){
        return false;
    }

    std::string targetkey;
    if(default_key){
        targetkey = "system.posix_acl_default";
    }else{
        targetkey = "system.posix_acl_access";
    }

    xattrs_t::iterator iter;
    if(xattrs.end() == (iter = xattrs.find(targetkey))){
        return false;
    }

    // convert value by base64
    xattrvalue = s3fs_base64(reinterpret_cast<const unsigned char*>(iter->second.c_str()), iter->second.length());

    return true;
}

static bool get_meta_xattr_value(const char* path, std::string& rawvalue)
{
    if(!path || '\0' == path[0]){
        S3FS_PRN_ERR("path is empty.");
        return false;
    }
    S3FS_PRN_DBG("[path=%s]", path);

    rawvalue.clear();

    headers_t meta;
    if(0 != get_object_attribute(path, nullptr, &meta)){
        S3FS_PRN_ERR("Failed to get object(%s) headers", path);
        return false;
    }

    headers_t::const_iterator iter;
    if(meta.end() == (iter = meta.find("x-amz-meta-xattr"))){
        return false;
    }
    rawvalue = iter->second;
    return true;
}

static size_t parse_xattrs(const std::string& strxattrs, xattrs_t& xattrs)
{
    xattrs.clear();

    // decode
    std::string jsonxattrs = urlDecode(strxattrs);

    // get from "{" to "}"
    std::string restxattrs;
    {
        size_t startpos;
        size_t endpos = std::string::npos;
        if(std::string::npos != (startpos = jsonxattrs.find_first_of('{'))){
            endpos = jsonxattrs.find_last_of('}');
        }
        if(startpos == std::string::npos || endpos == std::string::npos || endpos <= startpos){
            S3FS_PRN_WARN("xattr header(%s) is not json format.", jsonxattrs.c_str());
            return 0;
        }
        restxattrs = jsonxattrs.substr(startpos + 1, endpos - (startpos + 1));
    }

    // parse each key:val
    for(size_t pair_nextpos = restxattrs.find_first_of(','); !restxattrs.empty(); restxattrs = (pair_nextpos != std::string::npos ? restxattrs.substr(pair_nextpos + 1) : ""), pair_nextpos = restxattrs.find_first_of(',')){
        std::string pair = pair_nextpos != std::string::npos ? restxattrs.substr(0, pair_nextpos) : restxattrs;
        std::string key;
        std::string val;
        if(!parse_xattr_keyval(pair, key, &val)){
            // something format error, so skip this.
            continue;
        }
        xattrs[key] = val;
    }
    return xattrs.size();
}


static bool parse_xattr_keyval(const std::string& xattrpair, std::string& key, std::string* pval)
{
    // parse key and value
    size_t pos;
    std::string tmpval;
    if(std::string::npos == (pos = xattrpair.find_first_of(':'))){
        S3FS_PRN_ERR("one of xattr pair(%s) is wrong format.", xattrpair.c_str());
        return false;
    }
    key    = xattrpair.substr(0, pos);
    tmpval = xattrpair.substr(pos + 1);

    if(!takeout_str_dquart(key) || !takeout_str_dquart(tmpval)){
        S3FS_PRN_ERR("one of xattr pair(%s) is wrong format.", xattrpair.c_str());
        return false;
    }

    *pval = s3fs_decode64(tmpval.c_str(), tmpval.size());

    return true;
}

static bool get_parent_meta_xattr_value(const char* path, std::string& rawvalue)
{
    if(0 == strcmp(path, "/") || 0 == strcmp(path, ".")){
        // path is mount point, thus does not have parent.
        return false;
    }

    std::string parent = mydirname(path);
    if(parent.empty()){
        S3FS_PRN_ERR("Could not get parent path for %s.", path);
        return false;
    }
    return get_meta_xattr_value(parent.c_str(), rawvalue);
}

struct multi_head_notfound_callback_param
{
    pthread_mutex_t list_lock;
    s3obj_list_t    notfound_list;
};

int posix_s3fs_create(const char* _path, int flags, mode_t mode) {
    WTF8_ENCODE(path)
    int result;

    S3FS_PRN_INFO("craete file [path=%s][mode=%04o][flags=0x%x]", path, mode, flags);

    // check parent directory attribute.
    if(0 != (result = check_parent_object_access(path, X_OK))){
        return result;
    }
    struct stat statbuf;
    memset(&statbuf, 0, sizeof(struct stat));
    result = check_object_access(path, W_OK, &statbuf);
    if (statbuf.st_size > 0) {
        // 文件已经存在，打开即可
        return -EEXIST;
    }
    if(-ENOENT == result){
        if(0 != (result = check_parent_object_access(path, W_OK))){
            return result;
        }
    }else if(0 != result){
        return result;
    }

    std::string strnow = s3fs_str_realtime();
    headers_t   meta;
    meta["Content-Length"] = "0";
    meta["x-amz-meta-uid"]   = std::to_string(posixcontext.uid);
    meta["x-amz-meta-gid"]   = std::to_string(posixcontext.gid);
    meta["x-amz-meta-mode"]  = std::to_string(mode);
    meta["x-amz-meta-atime"] = strnow;
    meta["x-amz-meta-mtime"] = strnow;
    meta["x-amz-meta-ctime"] = strnow;

    std::string xattrvalue;
    if(build_inherited_xattr_value(path, xattrvalue)){
        S3FS_PRN_DBG("Set xattrs = %s", urlDecode(xattrvalue).c_str());
        meta["x-amz-meta-xattr"] = xattrvalue;
    }

    // [NOTE] set no_truncate flag
    // At this point, the file has not been created(uploaded) and
    // the data is only present in the Stats cache.
    // The Stats cache should not be deleted automatically by
    // timeout. If this stats is deleted, s3fs will try to get it
    // from the server with a Head request and will get an
    // unexpected error because the result object does not exist.
    //
    if(!StatCache::getStatCacheData()->AddStat(path, meta, false, true)){
        return -EIO;
    }

    AutoFdEntity autoent;
    FdEntity*    ent;
    int error = 0;
    if(nullptr == (ent = autoent.Open(path, &meta, 0, S3FS_OMIT_TS, flags, false, true, false, AutoLock::NONE, &error))){
        StatCache::getStatCacheData()->DelStat(path);
        return error;
    }
    ent->MarkDirtyNewFile();
    int fd = autoent.Detach();       // KEEP fdentity open;

    S3FS_MALLOCTRIM(0);
    if (fd > 0) {
        PosixS3Info info;
        info.fileinfo.fd = fd;
        info.fileinfo.flags = flags;
        //info.fileinfo.read_offset = 0;
        //info.fileinfo.write_offset = 0;
        info.fileinfo.offset = 0;
        info.filename = path;
        fdtofile[fd] = info;
    }
    return fd;
}

int posix_s3fs_open(const char* _path, int flags, mode_t mode)
{
    if (flags & O_CREAT) {
        int ret =  posix_s3fs_create(_path, flags, mode);
        if (ret != -EEXIST) {
            return ret;
        }
    }
    WTF8_ENCODE(path)
    int result;
    struct stat st;
    bool needs_flush = false;

    S3FS_PRN_INFO("[path=%s][flags=0x%x]", path, flags);

    if ((flags & O_ACCMODE) == O_RDONLY && flags & O_TRUNC) {
        return -EACCES;
    }

    // [NOTE]
    // Delete the Stats cache only if the file is not open.
    // If the file is open, the stats cache will not be deleted as
    // there are cases where the object does not exist on the server
    // and only the Stats cache exists.
    //
    if(StatCache::getStatCacheData()->HasStat(path)){
        if(!FdManager::HasOpenEntityFd(path)){
            StatCache::getStatCacheData()->DelStat(path);
        }
    }

    int mask = (O_RDONLY != (flags & O_ACCMODE) ? W_OK : R_OK);
    if(0 != (result = check_parent_object_access(path, X_OK))){
        return result;
    }

    result = check_object_access(path, mask, &st);
    if(-ENOENT == result){
        if(0 != (result = check_parent_object_access(path, W_OK))){
            return result;
        }
    }else if(0 != result){
        return result;
    }

    AutoFdEntity autoent;
    FdEntity*    ent;
    headers_t    meta;

    if((unsigned int)flags & O_TRUNC){
        if(0 != st.st_size){
            st.st_size = 0;
            needs_flush = true;
        }
    }else{
        // [NOTE]
        // If the file has already been opened and edited, the file size in
        // the edited state is given priority.
        // This prevents the file size from being reset to its original size
        // if you keep the file open, shrink its size, and then read the file
        // from another process while it has not yet been flushed.
        //
        if(nullptr != (ent = autoent.OpenExistFdEntity(path)) && ent->IsModified()){
            // sets the file size being edited.
            ent->GetSize(st.st_size);
        }
    }
    if(!S_ISREG(st.st_mode) || S_ISLNK(st.st_mode)){
        st.st_mtime = -1;
    }

    if(0 != (result = get_object_attribute(path, nullptr, &meta, true, nullptr, true))){    // no truncate cache
      return result;
    }

    struct timespec st_mctime;
    set_stat_to_timespec(st, stat_time_type::MTIME, st_mctime);

    if(nullptr == (ent = autoent.Open(path, &meta, st.st_size, st_mctime, flags, false, true, false, AutoLock::NONE))){
        StatCache::getStatCacheData()->DelStat(path);
        return -EIO;
    }

    if (needs_flush){
        struct timespec ts;
        s3fs_realtime(ts);
        ent->SetMCtime(ts, ts);

        if(0 != (result = ent->RowFlush(autoent.GetPseudoFd(), path, AutoLock::NONE, true))){
            S3FS_PRN_ERR("could not upload file(%s): result=%d", path, result);
            StatCache::getStatCacheData()->DelStat(path);
            return result;
        }
    }
    int fd = autoent.Detach();       // KEEP fdentity open;
    S3FS_MALLOCTRIM(0);
    if (fd > 0) {
        PosixS3Info info;
        info.fileinfo.fd = fd;
        info.fileinfo.flags = flags;
        //info.fileinfo.read_offset = 0;
        //info.fileinfo.write_offset = 0;
        info.fileinfo.offset = 0;
        info.filename = path;
        info.type =  FileType::FILE;
        //info.fileinfo.ino = ent->GetInode();
        info.fileinfo.ino =  fd;// 暂时赋值为fd
        fdtofile[fd] = info;
    }
    return fd;
}

int posix_s3fs_multiread(int fd, void* buf, size_t size, off_t file_offset) {
    //WTF8_ENCODE(path)
    S3FS_PRN_INFO("read [pseudo_fd=%llu]", (unsigned long long)fd);
    if (fdtofile.find(fd) == fdtofile.end()) {
        S3FS_PRN_ERR("readop could not find opened pseudo_fd(=%llu) ", (unsigned long long)(fd));
        return -EIO;
    }
    auto& info = fdtofile[fd];
    const char* path = info.filename.c_str();
    ssize_t res;

    // ! 注意这个偏移
    //off_t offset = info.fileinfo.read_offset;
    off_t offset = info.fileinfo.offset + file_offset;
    S3FS_PRN_INFO("[path=%s][size=%zu][offset=%lld][pseudo_fd=%llu]", path, size, (long long)offset, (unsigned long long)fd);

    AutoFdEntity autoent;
    FdEntity*    ent;
    if(nullptr == (ent = autoent.GetExistFdEntity(path, fd))){
        S3FS_PRN_ERR("could not find opened pseudo_fd(=%llu) for path(%s)", (unsigned long long)(fd), path);
        return -EIO;
    }

    // check real file size
    off_t realsize = 0;
    if(!ent->GetSize(realsize) || 0 == realsize){
        S3FS_PRN_DBG("file size is 0, so break to read.");
        return 0;
    }
    
    if(0 > (res = ent->Read(fd, (char*)buf, offset, size, false))){
        S3FS_PRN_WARN("failed to read file(%s). result=%zd", path, res);
    }
    // 不更新offset 在调用层统一更新
    // if(0 < res){
    //     info.fileinfo.offset += res;
    // }
    return static_cast<int>(res);
}

int posix_s3fs_read(int fd, void* buf, size_t size)
{
    //WTF8_ENCODE(path)
    S3FS_PRN_INFO("read [pseudo_fd=%llu]", (unsigned long long)fd);
    if (fdtofile.find(fd) == fdtofile.end()) {
        S3FS_PRN_ERR("readop could not find opened pseudo_fd(=%llu) ", (unsigned long long)(fd));
        return -EIO;
    }
    auto& info = fdtofile[fd];
    const char* path = info.filename.c_str();
    ssize_t res;

    // ! 注意这个偏移
    //off_t offset = info.fileinfo.read_offset;
    off_t offset = info.fileinfo.offset;
    S3FS_PRN_INFO("[path=%s][size=%zu][offset=%lld][pseudo_fd=%llu]", path, size, (long long)offset, (unsigned long long)fd);

    AutoFdEntity autoent;
    FdEntity*    ent;
    if(nullptr == (ent = autoent.GetExistFdEntity(path, fd))){
        S3FS_PRN_ERR("could not find opened pseudo_fd(=%llu) for path(%s)", (unsigned long long)(fd), path);
        return -EIO;
    }

    // check real file size
    off_t realsize = 0;
    if(!ent->GetSize(realsize) || 0 == realsize){
        S3FS_PRN_DBG("file size is 0, so break to read.");
        return 0;
    }
    
    if(0 > (res = ent->Read(fd, (char*)buf, offset, size, false))){
        S3FS_PRN_WARN("failed to read file(%s). result=%zd", path, res);
    }
    if(0 < res){
        //info.fileinfo.read_offset += res;
        info.fileinfo.offset += res;
    }
    return static_cast<int>(res);
}

int posix_s3fs_multiwrite(int fd, const void* buf, size_t size, off_t file_offset) {
    S3FS_PRN_INFO("multithread write [pseudo_fd=%llu]", (unsigned long long)fd);
    if (fdtofile.find(fd) == fdtofile.end()) {
        S3FS_PRN_ERR("writeop could not find opened pseudo_fd(=%llu) ", (unsigned long long)(fd));
        return -EIO;
    }
    auto& info = fdtofile[fd];
    const char* path = info.filename.c_str();
    //uint64_t offset = info.fileinfo.write_offset;
    //uint64_t offset = info.fileinfo.offset;
    uint64_t offset = info.fileinfo.offset + file_offset;
    ssize_t res;

    S3FS_PRN_DBG("multiwrite [path=%s][size=%zu][offset=%lld][pseudo_fd=%llu]", path, size, static_cast<long long int>(offset), (unsigned long long)(fd));

    AutoFdEntity autoent;
    FdEntity*    ent;
    if(nullptr == (ent = autoent.GetExistFdEntity(path, static_cast<int>(fd)))){
        S3FS_PRN_ERR("could not find opened pseudo_fd(%llu) for path(%s)", (unsigned long long)(fd), path);
        return -EIO;
    }

    if(0 > (res = ent->Write(static_cast<int>(fd), (const char*)buf, offset, size))){
        S3FS_PRN_WARN("failed to write file(%s). result=%zd", path, res);
    }

    if(max_dirty_data != -1 && ent->BytesModified() >= max_dirty_data && !use_newcache){
        int flushres;
        if(0 != (flushres = ent->RowFlush(static_cast<int>(fd), path, AutoLock::NONE, true))){
            S3FS_PRN_ERR("could not upload file(%s): result=%d", path, flushres);
            StatCache::getStatCacheData()->DelStat(path);
            return flushres;
        }
        // Punch a hole in the file to recover disk space.
        if(!ent->PunchHole()){
            S3FS_PRN_WARN("could not punching HOLEs to a cache file, but continue.");
        }
    }
    // 不更新offset 在调用层统一更新
    // if (0 < res) {
    //     //info.fileinfo.write_offset += res;
    //     info.fileinfo.offset += res;
    // }
    return static_cast<int>(res);
}


int posix_s3fs_write(int fd, const void* buf, size_t size) {
    S3FS_PRN_INFO("write [pseudo_fd=%llu]", (unsigned long long)fd);
    if (fdtofile.find(fd) == fdtofile.end()) {
        S3FS_PRN_ERR("writeop could not find opened pseudo_fd(=%llu) ", (unsigned long long)(fd));
        return -EIO;
    }
    auto& info = fdtofile[fd];
    const char* path = info.filename.c_str();
    //uint64_t offset = info.fileinfo.write_offset;
    uint64_t offset = info.fileinfo.offset;
    ssize_t res;

    S3FS_PRN_DBG("[path=%s][size=%zu][offset=%lld][pseudo_fd=%llu]", path, size, static_cast<long long int>(offset), (unsigned long long)(fd));

    AutoFdEntity autoent;
    FdEntity*    ent;
    if(nullptr == (ent = autoent.GetExistFdEntity(path, static_cast<int>(fd)))){
        S3FS_PRN_ERR("could not find opened pseudo_fd(%llu) for path(%s)", (unsigned long long)(fd), path);
        return -EIO;
    }

    if(0 > (res = ent->Write(static_cast<int>(fd), (const char*)buf, offset, size))){
        S3FS_PRN_WARN("failed to write file(%s). result=%zd", path, res);
    }

    if(max_dirty_data != -1 && ent->BytesModified() >= max_dirty_data && !use_newcache){
        int flushres;
        if(0 != (flushres = ent->RowFlush(static_cast<int>(fd), path, AutoLock::NONE, true))){
            S3FS_PRN_ERR("could not upload file(%s): result=%d", path, flushres);
            StatCache::getStatCacheData()->DelStat(path);
            return flushres;
        }
        // Punch a hole in the file to recover disk space.
        if(!ent->PunchHole()){
            S3FS_PRN_WARN("could not punching HOLEs to a cache file, but continue.");
        }
    }
    if (0 < res) {
        //info.fileinfo.write_offset += res;
        info.fileinfo.offset += res;
    }
    return static_cast<int>(res);
}

off_t posix_s3fs_lseek(int fd, off_t offset, int whence) {
    S3FS_PRN_INFO("lseek [pseudo_fd=%llu, offset=%llu, whence=%d]", (unsigned long long)fd, offset, whence);
    if (fdtofile.find(fd) == fdtofile.end()) {
        S3FS_PRN_ERR("lseekop could not find opened pseudo_fd(=%llu) ", (unsigned long long)(fd));
        return -EIO;
    }
    auto& info = fdtofile[fd];
    long new_pos = -1;

    FdEntity* ent = nullptr;
    //ent = FdManager::get()->GetFdEntity(info.filename.c_str(), fd, false, AutoLock::ALREADY_LOCKED);
    ent = FdManager::get()->GetFdEntity(info.filename.c_str(), fd, false, AutoLock::NONE);
    if (ent == nullptr) {
        S3FS_PRN_ERR("get stat failed in lseek....");
        return -1;
    }
    struct stat st;
    ent->GetStats(st);

    switch (whence) {
        case SEEK_SET:
            new_pos = offset;
            break;
        case SEEK_CUR:
            new_pos = info.fileinfo.offset + offset;
            break;
        case SEEK_END:
            new_pos = st.st_size + offset;
            break;
        default:
            errno = EINVAL;
            return -1;
    }
    S3FS_PRN_INFO("lseek , filesize[%d],  newpos[%d]", st.st_size,  new_pos);

    // if (new_pos < 0 || new_pos > file->size) {
    if (new_pos < 0) {
        errno = EINVAL;
        S3FS_PRN_ERR("lseek wrong new_pos, new_pos[%d]", new_pos);
        return -1;
    }
    info.fileinfo.offset = new_pos;
    return new_pos;
}

int posix_s3fs_close(int fd) {
    S3FS_PRN_INFO("close [pseudo_fd=%llu]", (unsigned long long)fd);
    if (fdtofile.find(fd) == fdtofile.end()) {
        S3FS_PRN_ERR("could not find opened pseudo_fd(=%llu) ", (unsigned long long)(fd));
        return -EIO;
    }
    const auto& info = fdtofile[fd];
    const char* path = info.filename.c_str();
    {   // scope for AutoFdEntity
        AutoFdEntity autoent;

        // [NOTE]
        // The pseudo fd stored in fi->fh is attached to AutoFdEntry so that it can be
        // destroyed here.
        //
        FdEntity* ent;
        if(nullptr == (ent = autoent.Attach(path, static_cast<int>(fd)))){
            S3FS_PRN_ERR("could not find pseudo_fd(%llu) for path(%s)", (unsigned long long)(fd), path);
            return -EIO;
        }

        // [NOTE]
        // There are cases when s3fs_flush is not called and s3fs_release is called.
        // (There have been reported cases where it is not called when exported as NFS.)
        // Therefore, Flush() is called here to try to upload the data.
        // Flush() will only perform an upload if the file has been updated.
        //
        int result;
        if(ent->IsModified()){
            if(0 != (result = ent->Flush(static_cast<int>(fd), AutoLock::NONE, false))){
                S3FS_PRN_ERR("failed to upload file contentsfor pseudo_fd(%llu) / path(%s) by result(%d)", (unsigned long long)(fd), path, result);
                return result;
            }
        }

        // [NOTE]
        // All opened file's stats is cached with no truncate flag.
        // Thus we unset it here.
        StatCache::getStatCacheData()->ChangeNoTruncateFlag(path, false);

        // [NOTICE]
        // At first, we remove stats cache.
        // Because fuse does not wait for response from "release" function. :-(
        // And fuse runs next command before this function returns.
        // Thus we call deleting stats function ASAP.
        //
        if((info.fileinfo.flags & O_RDWR) || (info.fileinfo.flags & O_WRONLY)){
            StatCache::getStatCacheData()->DelStat(path);
        }

        bool is_new_file = ent->IsDirtyNewFile();

        if(0 != (result = ent->UploadPending(static_cast<int>(fd), AutoLock::NONE))){
            S3FS_PRN_ERR("could not upload pending data(meta, etc) for pseudo_fd(%llu) / path(%s)", (unsigned long long)(fd), path);
            return result;
        }

        if(is_new_file){
            // update parent directory timestamp
            int update_result;
            if(0 != (update_result = update_mctime_parent_directory(path))){
                S3FS_PRN_ERR("succeed to create the file(%s), but could not update timestamp of its parent directory(result=%d).", path, update_result);
            }
        }
    }

    // check - for debug
    if(S3fsLog::IsS3fsLogDbg()){
        if(FdManager::HasOpenEntityFd(path)){
            S3FS_PRN_DBG("file(%s) is still opened(another pseudo fd is opend).", path);
        }
    }
    S3FS_MALLOCTRIM(0);
    fdtofile.erase(fd);
    return 0;
}

int posix_s3fs_stat(const char* _path, struct stat* stbuf) {
    WTF8_ENCODE(path)
    int result;

#if defined(__APPLE__)
    S3FS_PRN_INFO("stat [path=%s]", path);
#else
    S3FS_PRN_INFO("stat [path=%s]", path);
#endif

    // check parent directory attribute.
    if(0 != (result = check_parent_object_access(path, X_OK))){
        return result;
    }
    if(0 != (result = check_object_access(path, F_OK, stbuf))){
        return result;
    }
    // If has already opened fd, the st_size should be instead.
    // (See: Issue 241)
    if(stbuf){
        AutoFdEntity autoent;
        const FdEntity*  ent;
        if(nullptr != (ent = autoent.OpenExistFdEntity(path))){
            struct stat tmpstbuf;
            if(ent->GetStats(tmpstbuf)){
                stbuf->st_size = tmpstbuf.st_size;
            }
        }
        if(0 == strcmp(path, "/")){
            stbuf->st_size = 4096;
        }
        stbuf->st_blksize = BLOCK_SIZE;
        stbuf->st_blocks  = get_blocks(stbuf->st_size);

        S3FS_PRN_DBG("stat [path=%s] uid=%u, gid=%u, mode=%04o", path, (unsigned int)(stbuf->st_uid), (unsigned int)(stbuf->st_gid), stbuf->st_mode);
    }
    S3FS_MALLOCTRIM(0);

    return result;
}


int posix_s3fs_fstat(int fd, struct stat* stbuf) {
    // sleep(3);
    const char* path = fdtofile[fd].filename.c_str();
    return posix_s3fs_stat(path, stbuf);
}

int posix_s3fs_mkdir(const char* _path, mode_t mode)
{
    WTF8_ENCODE(path)
    int result;

    S3FS_PRN_INFO("mkdir [path=%s][mode=%04o]", path, mode);

    // check parent directory attribute.
    if(0 != (result = check_parent_object_access(path, W_OK | X_OK))){
        return result;
    }
    if(-ENOENT != (result = check_object_access(path, F_OK, nullptr))){
        if(0 == result){
            result = -EEXIST;
        }
        return result;
    }

    std::string xattrvalue;
    const char* pxattrvalue;
    if(get_parent_meta_xattr_value(path, xattrvalue)){
        pxattrvalue = xattrvalue.c_str();
    }else{
        pxattrvalue = nullptr;
    }

    struct timespec now;
    s3fs_realtime(now);
    result = create_directory_object(path, mode, now, now, now, posixcontext.uid, posixcontext.gid, pxattrvalue);

    StatCache::getStatCacheData()->DelStat(path);

    // update parent directory timestamp
    int update_result;
    if(0 != (update_result = update_mctime_parent_directory(path))){
        S3FS_PRN_ERR("succeed to create the directory(%s), but could not update timestamp of its parent directory(result=%d).", path, update_result);
    }

    S3FS_MALLOCTRIM(0);

    return result;
}

int posix_s3fs_opendir(const char* _path, S3DirStream* dirstream) {
    int flags = O_DIRECTORY;
    int mode = 0777;
    int ret =  posix_s3fs_open(_path, flags, mode);
    fdtofile[ret].type = FileType::DIR;
    fdtofile[ret].dirinfo.fh = ret;
    fdtofile[ret].dirinfo.offset = 0;
    dirstream->fh = ret;
    dirstream->offset = 0;
    dirstream->ino = fdtofile[ret].fileinfo.ino;
    return ret;
}

// cppcheck-suppress unmatchedSuppression
// cppcheck-suppress constParameterCallback
static bool multi_head_callback(S3fsCurl* s3fscurl, void* param)
{
    if(!s3fscurl){
        return false;
    }

    // Add stat cache
    std::string saved_path = s3fscurl->GetSpecialSavedPath();
    if(!StatCache::getStatCacheData()->AddStat(saved_path, *(s3fscurl->GetResponseHeaders()))){
        S3FS_PRN_ERR("failed adding stat cache [path=%s]", saved_path.c_str());
        return false;
    }

    // Get stats from stats cache(for converting from meta), and fill
    std::string bpath = mybasename(saved_path);
    if(use_wtf8){
        bpath = s3fs_wtf8_decode(bpath);
    }
    if(param){
        SyncFiller* pcbparam = reinterpret_cast<SyncFiller*>(param);
        struct stat st;
        if(StatCache::getStatCacheData()->GetStat(saved_path, &st)){
            pcbparam->Fill(bpath.c_str(), &st, 0);
        }else{
            S3FS_PRN_INFO2("Could not find %s file in stat cache.", saved_path.c_str());
            pcbparam->Fill(bpath.c_str(), nullptr, 0);
        }
    }else{
        S3FS_PRN_WARN("param(multi_head_callback_param*) is nullptr, then can not call filler.");
    }

    return true;
}


static bool multi_head_notfound_callback(S3fsCurl* s3fscurl, void* param)
{
    if(!s3fscurl){
        return false;
    }
    S3FS_PRN_INFO("HEAD returned NotFound(404) for %s object, it maybe only the path exists and the object does not exist.", s3fscurl->GetPath().c_str());

    if(!param){
        S3FS_PRN_WARN("param(multi_head_notfound_callback_param*) is nullptr, then can not call filler.");
        return false;
    }

    // set path to not found list
    struct multi_head_notfound_callback_param* pcbparam = reinterpret_cast<struct multi_head_notfound_callback_param*>(param);

    AutoLock auto_lock(&(pcbparam->list_lock));
    pcbparam->notfound_list.push_back(s3fscurl->GetBasePath());

    return true;
}

static std::unique_ptr<S3fsCurl> multi_head_retry_callback(S3fsCurl* s3fscurl)
{
    if(!s3fscurl){
        return nullptr;
    }
    size_t ssec_key_pos= s3fscurl->GetLastPreHeadSeecKeyPos();
    int retry_count = s3fscurl->GetMultipartRetryCount();

    // retry next sse key.
    // if end of sse key, set retry master count is up.
    ssec_key_pos = (ssec_key_pos == static_cast<size_t>(-1) ? 0 : ssec_key_pos + 1);
    if(0 == S3fsCurl::GetSseKeyCount() || S3fsCurl::GetSseKeyCount() <= ssec_key_pos){
        if(s3fscurl->IsOverMultipartRetryCount()){
            S3FS_PRN_ERR("Over retry count(%d) limit(%s).", s3fscurl->GetMultipartRetryCount(), s3fscurl->GetSpecialSavedPath().c_str());
            return nullptr;
        }
        ssec_key_pos = -1;
        retry_count++;
    }

    std::unique_ptr<S3fsCurl> newcurl(new S3fsCurl(s3fscurl->IsUseAhbe()));
    std::string path       = s3fscurl->GetBasePath();
    std::string base_path  = s3fscurl->GetBasePath();
    std::string saved_path = s3fscurl->GetSpecialSavedPath();

    if(!newcurl->PreHeadRequest(path, base_path, saved_path, ssec_key_pos)){
        S3FS_PRN_ERR("Could not duplicate curl object(%s).", saved_path.c_str());
        return nullptr;
    }
    newcurl->SetMultipartRetryCount(retry_count);

    return newcurl;
}


static int readdir_multi_head(const char* path, const S3ObjList& head, char* data, int offset, int maxread, ssize_t* realbytes, int* realnum)
{   //TODO : for newcache

    S3fsMultiCurl curlmulti(S3fsCurl::GetMaxMultiRequest(), true);      // [NOTE] run all requests to completion even if some requests fail.
    s3obj_list_t  headlist;
    int           result = 0;
    *realnum = 0;

    S3FS_PRN_INFO1("readdir_multi_head [path=%s][list=%zu]", path, headlist.size());

    // Make base path list.
    head.GetNameList(headlist, true, false);                                        // get name with "/".
    StatCache::getStatCacheData()->GetNotruncateCache(std::string(path), headlist); // Add notruncate file name from stat cache

    // Initialize S3fsMultiCurl
    curlmulti.SetSuccessCallback(multi_head_callback);
    curlmulti.SetRetryCallback(multi_head_retry_callback);

    // Success Callback function parameter(SyncFiller object)
    // SyncFiller syncfiller(buf, filler);
    // curlmulti.SetSuccessCallbackParam(reinterpret_cast<void*>(&syncfiller));

    // Not found Callback function parameter
    struct multi_head_notfound_callback_param notfound_param;
    if(support_compat_dir){
        pthread_mutexattr_t attr;
        pthread_mutexattr_init(&attr);
        #if S3FS_PTHREAD_ERRORCHECK
        pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_ERRORCHECK);
        #endif

        if(0 != (result = pthread_mutex_init(&(notfound_param.list_lock), &attr))){
            S3FS_PRN_CRIT("failed to init notfound_param.list_lock: %d", result);
            abort();
        }
        curlmulti.SetNotFoundCallback(multi_head_notfound_callback);
        curlmulti.SetNotFoundCallbackParam(reinterpret_cast<void*>(&notfound_param));
    }

    // Make single head request(with max).
    int nowPos = 0;
    for(s3obj_list_t::iterator iter = headlist.begin() + offset; headlist.end() != iter; ++iter){
        struct dirent64 * dirent = (struct dirent64*) (data + nowPos);
        ssize_t entryLen = sizeof(dirent64);

        
        std::string disppath = path + (*iter);
        std::string etag     = head.GetETag((*iter).c_str());
        struct stat st;

        strncpy(dirent->d_name, disppath.c_str(), sizeof(dirent->d_name));
        dirent->d_name[sizeof(dirent->d_name) - 1] = '\0'; 
        dirent->d_reclen = entryLen;
        // TODO: stat的赋值处理
        dirent->d_ino = 999999; // 暂时将d_ino初始化为999999
        if (head.IsDir(disppath.c_str())) {
            dirent->d_type = DT_DIR;
        } else {
            dirent->d_type = DT_REG;  

        }
        dirent->d_off = nowPos;
        nowPos += dirent->d_reclen;
        (*realnum)++;

        // [NOTE]
        // If there is a cache hit, file stat is filled by filler at here.
        //
        if(StatCache::getStatCacheData()->HasStat(disppath, &st, etag.c_str())){
            std::string bpath = mybasename(disppath);
            if(use_wtf8){
                bpath = s3fs_wtf8_decode(bpath);
            }
            
            //syncfiller.Fill(bpath.c_str(), &st, 0);
            //dirent->d_ino = st.st_ino;
            continue;
        }

        // First check for directory, start checking "not SSE-C".
        // If checking failed, retry to check with "SSE-C" by retry callback func when SSE-C mode.
        std::unique_ptr<S3fsCurl> s3fscurl(new S3fsCurl());
        if(!s3fscurl->PreHeadRequest(disppath, disppath, disppath)){  // target path = cache key path.(ex "dir/")
            S3FS_PRN_WARN("Could not make curl object for head request(%s).", disppath.c_str());
            continue;
        }

        if(!curlmulti.SetS3fsCurlObject(std::move(s3fscurl))){
            S3FS_PRN_WARN("Could not make curl object into multi curl(%s).", disppath.c_str());
            continue;
        }
    }
    *realbytes = nowPos;
    headlist.clear();

    // Multi request
    if(0 != (result = curlmulti.Request())){
        // If result is -EIO, it is something error occurred.
        // This case includes that the object is encrypting(SSE) and s3fs does not have keys.
        // So s3fs set result to 0 in order to continue the process.
        if(-EIO == result){
            S3FS_PRN_WARN("error occurred in multi request(errno=%d), but continue...", result);
            result = 0;
        }else{
            S3FS_PRN_ERR("error occurred in multi request(errno=%d).", result);
            return result;
        }
    }

    // [NOTE]
    // Objects that could not be found by HEAD request may exist only
    // as a path, so search for objects under that path.(a case of no dir object)
    //
    if(!support_compat_dir){
        //TODO
        //syncfiller.SufficiencyFill(head.common_prefixes);
    }
    if(support_compat_dir && !notfound_param.notfound_list.empty()){      // [NOTE] not need to lock to access this here.
        // dummy header
        mode_t dirmask = umask(0);      // macos does not have getumask()
        umask(dirmask);

        headers_t   dummy_header;
        dummy_header["Content-Type"]     = "application/x-directory";          // directory
        dummy_header["x-amz-meta-uid"]   = std::to_string(is_s3fs_uid ? s3fs_uid : geteuid());
        dummy_header["x-amz-meta-gid"]   = std::to_string(is_s3fs_gid ? s3fs_gid : getegid());
        dummy_header["x-amz-meta-mode"]  = std::to_string(S_IFDIR | (~dirmask & (S_IRWXU | S_IRWXG | S_IRWXO)));
        dummy_header["x-amz-meta-atime"] = "0";
        dummy_header["x-amz-meta-ctime"] = "0";
        dummy_header["x-amz-meta-mtime"] = "0";

        for(s3obj_list_t::iterator reiter = notfound_param.notfound_list.begin(); reiter != notfound_param.notfound_list.end(); ++reiter){
            int dir_result;
            std::string dirpath = *reiter;
            if(-ENOTEMPTY == (dir_result = directory_empty(dirpath.c_str()))){
                // Found objects under the path, so the path is directory.

                // Add stat cache
                if(StatCache::getStatCacheData()->AddStat(dirpath, dummy_header, true)){    // set forcedir=true
                    // Get stats from stats cache(for converting from meta), and fill
                    std::string base_path = mybasename(dirpath);
                    if(use_wtf8){
                        base_path = s3fs_wtf8_decode(base_path);
                    }

                    struct stat st;
                    if(StatCache::getStatCacheData()->GetStat(dirpath, &st)){
                        // TODO
                        //syncfiller.Fill(base_path.c_str(), &st, 0);
                    }else{
                        S3FS_PRN_INFO2("Could not find %s directory(no dir object) in stat cache.", dirpath.c_str());
                        // TODO
                        //syncfiller.Fill(base_path.c_str(), nullptr, 0);
                    }
                }else{
                    S3FS_PRN_ERR("failed adding stat cache [path=%s], but dontinue...", dirpath.c_str());
                }
            }else{
                S3FS_PRN_WARN("%s object does not have any object under it(errno=%d),", reiter->c_str(), dir_result);
            }
        }
    }

    return result;
}

int posix_s3fs_getdents(S3DirStream* dirstream, char* contents, size_t maxread, ssize_t* realbytes) {

    //WTF8_ENCODE(path)
    const char* path = fdtofile[dirstream->fh].filename.c_str();
    S3ObjList head;
    int result;
    S3FS_PRN_INFO("getdents [path=%s]", path);

    if(0 != (result = check_object_access(path, R_OK, nullptr))){
        return result;
    }

    // get a list of all the objects
    if((result = list_bucket(path, head, "/")) != 0){
        S3FS_PRN_ERR("list_bucket returns error(%d).", result);
        return result;
    }

    if(head.IsEmpty()){
        return 0;
    }

    // Send multi head request for stats caching.
    std::string strpath = path;
    if(strcmp(path, "/") != 0){
        strpath += "/";
    }
    int readnum = 0;
    if(0 != (result = readdir_multi_head(strpath.c_str(), head, contents, dirstream->offset, maxread, realbytes, &readnum))){
        S3FS_PRN_ERR("readdir_multi_head returns error(%d).", result);
    }
    dirstream->offset += readnum;
    S3FS_PRN_DBG("the dirstream offset: %d, realbytes: %d", dirstream->offset, *realbytes);
    S3FS_MALLOCTRIM(0);

    return result;
}

int posix_s3fs_closedir(S3DirStream* dirstream) {
    S3FS_PRN_INFO("closedir [pseudo_fd=%llu]", (unsigned long long)dirstream->fh);
    return posix_s3fs_close(dirstream->fh);
}

int posix_s3fs_unlink(const char* _path)
{
    WTF8_ENCODE(path)
    int result;

    S3FS_PRN_INFO(" delete [path=%s]", path);

    if(0 != (result = check_parent_object_access(path, W_OK | X_OK))){
        return result;
    }
    if(use_newcache){
        result = accessor->Delete(path);
    }else{
        S3fsCurl s3fscurl;
        result = s3fscurl.DeleteRequest(path);
        FdManager::DeleteCacheFile(path);
    }

    StatCache::getStatCacheData()->DelStat(path);
    StatCache::getStatCacheData()->DelSymlink(path);

    // update parent directory timestamp
    int update_result;
    if(0 != (update_result = update_mctime_parent_directory(path))){
        S3FS_PRN_ERR("succeed to remove the file(%s), but could not update timestamp of its parent directory(result=%d).", path, update_result);
    }
    S3FS_MALLOCTRIM(0);
    return result;
}

static void* s3fs_init()
{
    S3FS_PRN_INIT_INFO("init v%s(commit:%s) with %s, credential-library(%s)", VERSION, COMMIT_HASH_VAL, s3fs_crypt_lib_name(), ps3fscred->GetCredFuncVersion(false));

    // cache(remove cache dirs at first)
    if(is_remove_cache && (!CacheFileStat::DeleteCacheFileStatDirectory() || !FdManager::DeleteCacheDirectory())){
        S3FS_PRN_DBG("Could not initialize cache directory.");
    }

    // check loading IAM role name
    if(!ps3fscred->LoadIAMRoleFromMetaData()){
        S3FS_PRN_CRIT("could not load IAM role name from meta data.");
        return nullptr;
    }

    // Check Bucket
    {
        int result;
        if(EXIT_SUCCESS != (result = s3fs_check_service())){
            return nullptr;
        }
    }

    if(!ThreadPoolMan::Initialize(max_thread_count)){
        S3FS_PRN_CRIT("Could not create thread pool(%d)", max_thread_count);
    }

    // Signal object
    if(!S3fsSignals::Initialize()){
        S3FS_PRN_ERR("Failed to initialize signal object, but continue...");
    }

    return nullptr;
}
static void s3fs_destroy()
{
    S3FS_PRN_INFO("destroy");

    // Signal object
    if(!S3fsSignals::Destroy()){
        S3FS_PRN_WARN("Failed to clean up signal object.");
    }

    ThreadPoolMan::Destroy();

    // cache(remove at last)
    if(is_remove_cache && (!CacheFileStat::DeleteCacheFileStatDirectory() || !FdManager::DeleteCacheDirectory())){
        S3FS_PRN_WARN("Could not remove cache directory.");
    }
}

// 初始化配置
static int init_config(std::string configpath) {
    std::cout << "init_config: " << configpath << std::endl;
    std::unordered_map<std::string, std::string> config;
    std::ifstream file(configpath);
    std::string line = "";

    if (!file.is_open()) {
        std::cerr << "Could not open configuration file" << std::endl;
    }

    while (std::getline(file, line)) {
        // Ignore comments and empty lines
        if (line.empty() || line[0] == '#') {
            continue;
        }

        std::istringstream iss(line);
        std::string key, value;

        // Split line into key and value
        if (std::getline(iss, key, '=') && std::getline(iss, value)) {
            // Remove whitespace from the key and value
            key.erase(key.find_last_not_of(" \t\n\r\f\v") + 1);
            key.erase(0, key.find_first_not_of(" \t\n\r\f\v"));
            value.erase(value.find_last_not_of(" \t\n\r\f\v") + 1);
            value.erase(0, value.find_first_not_of(" \t\n\r\f\v"));

            config[key] = value;
        }
    }

    // log level
    if (config.find("log_level") != config.end()) {
        std::cout << "set loglevel: " << config["log_level"] << std::endl;
        if(config["log_level"] == "debug") {
            S3fsLog::SetLogLevel(S3fsLog::LEVEL_DBG);
        } else if (config["log_level"] == "info") {
            S3fsLog::SetLogLevel(S3fsLog::LEVEL_INFO);
        } else if (config["log_level"] == "warning") {
            S3fsLog::SetLogLevel(S3fsLog::LEVEL_WARN);
        } else if (config["log_level"] == "error") {
            S3fsLog::SetLogLevel(S3fsLog::LEVEL_ERR);
        } else if (config["log_level"] == "critical") {
            S3fsLog::SetLogLevel(S3fsLog::LEVEL_CRIT);
        }
    }

    // bucket config
    if(S3fsCred::GetBucket().empty()) {
        int ret = set_bucket(config["bucket"].c_str());
        std::cout << "set_bucket: " << ret << std::endl;
    }

    // mountpoint config
    // the second NONOPT option is the mountpoint(not utility mode)
    if(mountpoint.empty() && utility_incomp_type::NO_UTILITY_MODE == utility_mode){
        // save the mountpoint and do some basic error checking
        mountpoint = config["mountpoint"];
        struct stat stbuf;

// In MSYS2 environment with WinFsp, it is not needed to create the mount point before mounting.
// Also it causes a conflict with WinFsp's validation, so disabling it.
#ifdef __MSYS__
        memset(&stbuf, 0, sizeof stbuf);
        set_mountpoint_attribute(stbuf);
#else
        if(stat(mountpoint.c_str(), &stbuf) == -1){
            // check stale mountpoint
            if(errno == ENOTCONN){
                print_umount_message(mountpoint, true);
            } else {
                S3FS_PRN_EXIT("unable to access MOUNTPOINT %s: %s", mountpoint.c_str(), strerror(errno));
            }
            return -1;
        }
        if(!(S_ISDIR(stbuf.st_mode))){
            S3FS_PRN_EXIT("MOUNTPOINT: %s is not a directory.", mountpoint.c_str());
            return -1;
        }
        if(!set_mountpoint_attribute(stbuf)){
            S3FS_PRN_EXIT("MOUNTPOINT: %s permission denied.", mountpoint.c_str());
            return -1;
        }

        if(!nonempty){
            const struct dirent *ent;
            DIR *dp = opendir(mountpoint.c_str());
            if(dp == nullptr){
                S3FS_PRN_EXIT("failed to open MOUNTPOINT: %s: %s", mountpoint.c_str(), strerror(errno));
                return -1;
            }
            while((ent = readdir(dp)) != nullptr){
                if(strcmp(ent->d_name, ".") != 0 && strcmp(ent->d_name, "..") != 0){
                    closedir(dp);
                    S3FS_PRN_EXIT("MOUNTPOINT directory %s is not empty. if you are sure this is safe, can use the 'nonempty' mount option.", mountpoint.c_str());
                    return -1;
                }
            }
            closedir(dp);
        }
#endif
    }

    // passwd_file
    std::string passwd_filename = config["passwd_file"];
    passwd_filename = "passwd_file=" +  passwd_filename;
    int ret = ps3fscred->DetectParam(passwd_filename.c_str());
    if (0 > ret) {
        std::cerr << "Failed to parse passwd_file=" << passwd_filename << ": " << strerror(-ret);
        return -1;
    }

    // url
    s3host = config["url"];
    // strip the trailing '/', if any, off the end of the host
    // std::string
    size_t found, length;
    found  = s3host.find_last_of('/');
    length = s3host.length();
    while(found == (length - 1) && length > 0){
        s3host.erase(found);
        found  = s3host.find_last_of('/');
        length = s3host.length();
    }
    // Check url for http / https protocol std::string
    if(!is_prefix(s3host.c_str(), "https://") && !is_prefix(s3host.c_str(), "http://")){
        S3FS_PRN_EXIT("option url has invalid format, missing http / https protocol");
        return -1;
    }

    if (config.find("use_path_request_style") != config.end()) {
        pathrequeststyle = true;
        std::cout << "use path reqeust style" << std::endl;
    } else {
        std::cout << "use virtual host style" << std::endl;
    }

    // newcache
    if(config.find("newcache_conf") != config.end()) {
        newcache_conf = config["newcache_conf"];
        if (!newcache_conf.empty()) {
            use_newcache = true;
        }
    }

    return 0;
}

S3fsLog singletonLog;
void s3fs_global_init() { 
//static __attribute__((constructor)) void Init(void) {
    static bool is_called = false;
    if (is_called) {
        std::cout << "global init has called";
        return;
    }
    int ch;
    int option_index = 0; 
    time_t incomp_abort_time = (24 * 60 * 60);
   
    
    S3fsLog::SetLogLevel(S3fsLog::LEVEL_DBG);
    S3fsLog::SetLogfile("./log/posix_s3fs.log");
    //S3fsLog::debug_level = S3fsLog::LEVEL_DBG;
    std::string configpath = "./conf/posix_s3fs.conf";

    posixcontext.uid = geteuid();
    posixcontext.gid = getegid();
    S3FS_PRN_INFO("set the uid:%d , gid:%d", posixcontext.uid, posixcontext.gid);


    // init bucket_block_size
#if defined(__MSYS__)
    bucket_block_count = static_cast<fsblkcnt_t>(INT32_MAX);
#elif defined(__APPLE__)
    bucket_block_count = static_cast<fsblkcnt_t>(INT32_MAX);
#else
    bucket_block_count = ~0U;
#endif

    // init xml2
    xmlInitParser();
    LIBXML_TEST_VERSION

    init_sysconf_vars();

    // get program name - emulate basename
    program_name = "posixs3fs";

    // set credential object
    //
    ps3fscred.reset(new S3fsCred());
    if(!S3fsCurl::InitCredentialObject(ps3fscred.get())){
        S3FS_PRN_EXIT("Failed to setup credential object to s3fs curl.");
        exit(EXIT_FAILURE);
    }

    // Load SSE environment
    if(!S3fsCurl::LoadEnvSse()){
        S3FS_PRN_EXIT("something wrong about SSE environment.");
        exit(EXIT_FAILURE);
    }

    // ssl init
    if(!s3fs_init_global_ssl()){
        S3FS_PRN_EXIT("could not initialize for ssl libraries.");
        exit(EXIT_FAILURE);
    }

    // mutex for xml
    if(!init_parser_xml_lock()){
        S3FS_PRN_EXIT("could not initialize mutex for xml parser.");
        s3fs_destroy_global_ssl();
        exit(EXIT_FAILURE);
    }

    // mutex for basename/dirname
    if(!init_basename_lock()){
        S3FS_PRN_EXIT("could not initialize mutex for basename/dirname.");
        s3fs_destroy_global_ssl();
        destroy_parser_xml_lock();
        exit(EXIT_FAILURE);
    }

    // init curl (without mime types)
    //
    // [NOTE]
    // The curl initialization here does not load mime types.
    // The mime types file parameter are dynamic values according
    // to the user's environment, and are analyzed by the my_fuse_opt_proc
    // function.
    // The my_fuse_opt_proc function is executed after this curl
    // initialization. Because the curl method is used in the
    // my_fuse_opt_proc function, then it must be called here to
    // initialize. Fortunately, the processing using mime types
    // is only PUT/POST processing, and it is not used until the
    // call of my_fuse_opt_proc function is completed. Therefore,
    // the mime type is loaded just after calling the my_fuse_opt_proc
    // function.
    // 
    if(!S3fsCurl::InitS3fsCurl()){
        S3FS_PRN_EXIT("Could not initiate curl library.");
        s3fs_destroy_global_ssl();
        destroy_parser_xml_lock();
        destroy_basename_lock();
        exit(EXIT_FAILURE);
    }

    if(0 != init_config(configpath)){
        S3fsCurl::DestroyS3fsCurl();
        s3fs_destroy_global_ssl();
        destroy_parser_xml_lock();
        destroy_basename_lock();
        exit(EXIT_FAILURE);
    }

    // init newcache
    if(use_newcache){
        HybridCache::HybridCacheConfig cfg;
        HybridCache::GetHybridCacheConfig(newcache_conf, cfg);
        accessor = std::make_shared<HybridCacheAccessor4S3fs>(cfg);
    }

    // init mime types for curl
    if(!S3fsCurl::InitMimeType(mimetype_file)){
        S3FS_PRN_WARN("Missing MIME types prevents setting Content-Type on uploaded objects.");
    }

    // [NOTE]
    // exclusive option check here.
    //
    if(strcasecmp(S3fsCurl::GetStorageClass().c_str(), "REDUCED_REDUNDANCY") == 0 && !S3fsCurl::IsSseDisable()){
        S3FS_PRN_EXIT("use_sse option could not be specified with storage class reduced_redundancy.");
        S3fsCurl::DestroyS3fsCurl();
        s3fs_destroy_global_ssl();
        destroy_parser_xml_lock();
        destroy_basename_lock();
        exit(EXIT_FAILURE);
    }
    if(!S3fsCurl::FinalCheckSse()){
        S3FS_PRN_EXIT("something wrong about SSE options.");
        S3fsCurl::DestroyS3fsCurl();
        s3fs_destroy_global_ssl();
        destroy_parser_xml_lock();
        destroy_basename_lock();
        exit(EXIT_FAILURE);
    }

    if(S3fsCurl::GetSignatureType() == signature_type_t::V2_ONLY && S3fsCurl::GetUnsignedPayload()){
        S3FS_PRN_WARN("Ignoring enable_unsigned_payload with sigv2");
    }

    if(!FdEntity::GetNoMixMultipart() && max_dirty_data != -1){
        S3FS_PRN_WARN("Setting max_dirty_data to -1 when nomixupload is enabled");
        max_dirty_data = -1;
    }

    //
    // Check the combination of parameters for credential
    //
    if(!ps3fscred->CheckAllParams()){
        S3fsCurl::DestroyS3fsCurl();
        s3fs_destroy_global_ssl();
        destroy_parser_xml_lock();
        destroy_basename_lock();
        exit(EXIT_FAILURE);
    }

    // The second plain argument is the mountpoint
    // if the option was given, we all ready checked for a
    // readable, non-empty directory, this checks determines
    // if the mountpoint option was ever supplied
    if(utility_incomp_type::NO_UTILITY_MODE == utility_mode){
        if(mountpoint.empty()){
            S3FS_PRN_EXIT("missing MOUNTPOINT argument.");
            show_usage();
            S3fsCurl::DestroyS3fsCurl();
            s3fs_destroy_global_ssl();
            destroy_parser_xml_lock();
            destroy_basename_lock();
            exit(EXIT_FAILURE);
        }
    }

    // check tmp dir permission
    if(!FdManager::CheckTmpDirExist()){
        S3FS_PRN_EXIT("temporary directory doesn't exists.");
        S3fsCurl::DestroyS3fsCurl();
        s3fs_destroy_global_ssl();
        destroy_parser_xml_lock();
        destroy_basename_lock();
        exit(EXIT_FAILURE);
    }

    // check cache dir permission
    if(!FdManager::CheckCacheDirExist() || !FdManager::CheckCacheTopDir() || !CacheFileStat::CheckCacheFileStatTopDir()){
        S3FS_PRN_EXIT("could not allow cache directory permission, check permission of cache directories.");
        S3fsCurl::DestroyS3fsCurl();
        s3fs_destroy_global_ssl();
        destroy_parser_xml_lock();
        destroy_basename_lock();
        exit(EXIT_FAILURE);
    }

    // set fake free disk space
    if(-1 != fake_diskfree_size){
        FdManager::InitFakeUsedDiskSize(fake_diskfree_size);
    }

    // Set default value of free_space_ratio to 10%
    if(FdManager::GetEnsureFreeDiskSpace()==0){
        //int ratio = 10;
        int ratio = 5;
        
        off_t dfsize = FdManager::GetTotalDiskSpaceByRatio(ratio);
        S3FS_PRN_INFO("Free space ratio default to %d %%, ensure the available disk space is greater than %.3f MB", ratio, static_cast<double>(dfsize) / 1024 / 1024);

        if(dfsize < S3fsCurl::GetMultipartSize()){
            S3FS_PRN_WARN("specified size to ensure disk free space is smaller than multipart size, so set multipart size to it.");
            dfsize = S3fsCurl::GetMultipartSize();
        }
        FdManager::SetEnsureFreeDiskSpace(dfsize);
    }

    // set user agent
    S3fsCurl::InitUserAgent();

    if(utility_incomp_type::NO_UTILITY_MODE != utility_mode){
        int exitcode = s3fs_utility_processing(incomp_abort_time);

        S3fsCurl::DestroyS3fsCurl();
        s3fs_destroy_global_ssl();
        destroy_parser_xml_lock();
        destroy_basename_lock();
        exit(exitcode);
    }

    // Check multipart / copy api for mix multipart uploading
    if(nomultipart || nocopyapi || norenameapi){
        FdEntity::SetNoMixMultipart();
        max_dirty_data = -1;
    }

    // check free disk space
    if(!FdManager::IsSafeDiskSpace(nullptr, S3fsCurl::GetMultipartSize() * S3fsCurl::GetMaxParallelCount())){
        // clean cache dir and retry
        S3FS_PRN_WARN("No enough disk space for s3fs, try to clean cache dir");
        FdManager::get()->CleanupCacheDir();

        if(!FdManager::IsSafeDiskSpaceWithLog(nullptr, S3fsCurl::GetMultipartSize() * S3fsCurl::GetMaxParallelCount())){
            S3fsCurl::DestroyS3fsCurl();
            s3fs_destroy_global_ssl();
            destroy_parser_xml_lock();
            destroy_basename_lock();
            exit(EXIT_FAILURE);
        }
    }

    // set mp stat flag object
    
    pHasMpStat = new MpStatFlag();
    s3fs_init();
    is_called = true;
    std::cout << "finish s3fs global init" << std::endl;
}

void s3fs_global_uninit() {
}

static __attribute__((destructor)) void Clean(void) {   
    // Destroy curl
    s3fs_destroy();

    if(!S3fsCurl::DestroyS3fsCurl()){
        S3FS_PRN_WARN("Could not release curl library.");
    }
    s3fs_destroy_global_ssl();
    destroy_parser_xml_lock();
    destroy_basename_lock();
    delete pHasMpStat;

    // cleanup xml2
    xmlCleanupParser();
    S3FS_MALLOCTRIM(0);
    
    if(use_newcache){
        accessor.reset();
    }
}