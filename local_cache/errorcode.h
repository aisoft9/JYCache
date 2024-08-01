/*
 * Project: HybridCache
 * Created Date: 24-3-18
 * Author: lshb
 */
#ifndef HYBRIDCACHE_ERRORCODE_H_
#define HYBRIDCACHE_ERRORCODE_H_

namespace HybridCache {

enum ErrCode {
    SUCCESS                 = 0,
    PAGE_NOT_FOUND          = -1,
    PAGE_DEL_FAIL           = -2,
    ADAPTOR_NOT_FOUND       = -3,
    REMOTE_FILE_NOT_FOUND   = -4,
};

}  // namespace HybridCache

#endif // HYBRIDCACHE_ERRORCODE_H_
