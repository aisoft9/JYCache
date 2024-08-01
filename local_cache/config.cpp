#include <algorithm>
#include <fstream>
#include <iostream>
#include <sstream>

#include "gflags/gflags.h"
#include "glog/logging.h"

#include "common.h"
#include "config.h"

namespace HybridCache {

std::vector<std::string> SplitString(const std::string &input) {
    std::vector<std::string> result;
    std::stringstream ss(input);
    std::string item;
    while (std::getline(ss, item, ',')) {
        result.push_back(item);
    }
    return result;
}

bool GetHybridCacheConfig(const std::string& file, HybridCacheConfig& cfg) {
    Configuration conf;
    if (!conf.LoadConfig(file)) return false;

    // ReadCache
    conf.GetValueFatalIfFail("ReadCacheConfig.CacheConfig.CacheName",
                             cfg.ReadCacheCfg.CacheCfg.CacheName);
    conf.GetValueFatalIfFail("ReadCacheConfig.CacheConfig.MaxCacheSize",
                             cfg.ReadCacheCfg.CacheCfg.MaxCacheSize);
    conf.GetValueFatalIfFail("ReadCacheConfig.CacheConfig.PageBodySize",
                             cfg.ReadCacheCfg.CacheCfg.PageBodySize);
    conf.GetValueFatalIfFail("ReadCacheConfig.CacheConfig.PageMetaSize",
                             cfg.ReadCacheCfg.CacheCfg.PageMetaSize);
    conf.GetValueFatalIfFail("ReadCacheConfig.CacheConfig.EnableCAS",
                             cfg.ReadCacheCfg.CacheCfg.EnableCAS);
    conf.GetValueFatalIfFail("ReadCacheConfig.CacheConfig.CacheLibConfig.EnableNvmCache",
                             cfg.ReadCacheCfg.CacheCfg.CacheLibCfg.EnableNvmCache);
    if (cfg.ReadCacheCfg.CacheCfg.CacheLibCfg.EnableNvmCache) {
        conf.GetValueFatalIfFail("ReadCacheConfig.CacheConfig.CacheLibConfig.RaidPath",
                                 cfg.ReadCacheCfg.CacheCfg.CacheLibCfg.RaidPath);
        conf.GetValueFatalIfFail("ReadCacheConfig.CacheConfig.CacheLibConfig.RaidFileNum",
                                 cfg.ReadCacheCfg.CacheCfg.CacheLibCfg.RaidFileNum);
        conf.GetValueFatalIfFail("ReadCacheConfig.CacheConfig.CacheLibConfig.RaidFileSize",
                                 cfg.ReadCacheCfg.CacheCfg.CacheLibCfg.RaidFileSize);
        conf.GetValueFatalIfFail("ReadCacheConfig.CacheConfig.CacheLibConfig.DataChecksum",
                                 cfg.ReadCacheCfg.CacheCfg.CacheLibCfg.DataChecksum);
    }
    conf.GetValueFatalIfFail("ReadCacheConfig.DownloadNormalFlowLimit",
                             cfg.ReadCacheCfg.DownloadNormalFlowLimit);
    conf.GetValueFatalIfFail("ReadCacheConfig.DownloadBurstFlowLimit",
                             cfg.ReadCacheCfg.DownloadBurstFlowLimit);

    // WriteCache
    conf.GetValueFatalIfFail("WriteCacheConfig.CacheConfig.CacheName",
                             cfg.WriteCacheCfg.CacheCfg.CacheName);
    conf.GetValueFatalIfFail("WriteCacheConfig.CacheConfig.MaxCacheSize",
                             cfg.WriteCacheCfg.CacheCfg.MaxCacheSize);
    conf.GetValueFatalIfFail("WriteCacheConfig.CacheConfig.PageBodySize",
                             cfg.WriteCacheCfg.CacheCfg.PageBodySize);
    conf.GetValueFatalIfFail("WriteCacheConfig.CacheConfig.PageMetaSize",
                             cfg.WriteCacheCfg.CacheCfg.PageMetaSize);
    conf.GetValueFatalIfFail("WriteCacheConfig.CacheConfig.EnableCAS",
                             cfg.WriteCacheCfg.CacheCfg.EnableCAS);
    conf.GetValueFatalIfFail("WriteCacheConfig.CacheSafeRatio",
                             cfg.WriteCacheCfg.CacheSafeRatio);

    // GlobalCache
    conf.GetValueFatalIfFail("UseGlobalCache", cfg.UseGlobalCache);
    if (cfg.UseGlobalCache) {
        conf.GetValueFatalIfFail("GlobalCacheConfig.EnableWriteCache",
                                 cfg.GlobalCacheCfg.EnableWriteCache);
        conf.GetValueFatalIfFail("GlobalCacheConfig.EtcdAddress",
                                 cfg.GlobalCacheCfg.EtcdAddress);
        std::string servers;
        conf.GetValueFatalIfFail("GlobalCacheConfig.GlobalServers",
                                 servers);
        cfg.GlobalCacheCfg.GlobalServers = std::move(SplitString(servers));
        conf.GetValueFatalIfFail("GlobalCacheConfig.GflagFile",
                                 cfg.GlobalCacheCfg.GflagFile);
    }

    conf.GetValueFatalIfFail("ThreadNum", cfg.ThreadNum);
    conf.GetValueFatalIfFail("BackFlushCacheRatio", cfg.BackFlushCacheRatio);
    conf.GetValueFatalIfFail("UploadNormalFlowLimit", cfg.UploadNormalFlowLimit);
    conf.GetValueFatalIfFail("UploadBurstFlowLimit", cfg.UploadBurstFlowLimit);
    conf.GetValueFatalIfFail("LogPath", cfg.LogPath);
    conf.GetValueFatalIfFail("LogLevel", cfg.LogLevel);
    conf.GetValueFatalIfFail("EnableLog", cfg.EnableLog);
    conf.GetValueFatalIfFail("FlushToRead", cfg.FlushToRead);
    conf.GetValueFatalIfFail("CleanCacheByOpen", cfg.CleanCacheByOpen);

    conf.PrintConfig();
    return CheckConfig(cfg);
}

bool CheckConfig(const HybridCacheConfig& cfg) {
    if (cfg.WriteCacheCfg.CacheCfg.CacheLibCfg.EnableNvmCache) {
        LOG(FATAL) << "Config error. Write Cache not support nvm cache!";
        return false;
    }

    if (cfg.ReadCacheCfg.CacheCfg.PageBodySize % BYTE_LEN ||
            cfg.WriteCacheCfg.CacheCfg.PageBodySize % BYTE_LEN) {
        LOG(FATAL) << "Config error. Page body size must be a multiple of " << BYTE_LEN;
        return false;
    }

    return true;
}

bool ParseFlagFromFile(const std::string& file) {
    std::ifstream config_file(file);
    if (config_file.is_open()) {
        std::string line;
        std::vector<std::string> args;
        args.push_back("hybridcache");
        while (std::getline(config_file, line)) {
            args.push_back(line);
        }
        char* dummy_argv[args.size()];
        for (size_t i = 0; i < args.size(); ++i) {
            dummy_argv[i] = const_cast<char*>(args[i].c_str());
        }
        int size = args.size();
        char** tmp = const_cast<char**>(dummy_argv);
        google::ParseCommandLineFlags(&size, &tmp, true);
        config_file.close();
    } else {
        LOG(ERROR) << "Unable to open gflag file '" << file << "' failed: "
                   << strerror(errno);
        return false;
    }
    return true;
}

bool Configuration::LoadConfig(const std::string& file) {
    confFile_ = file;
    std::ifstream cFile(confFile_);

    if (cFile.is_open()) {
        std::string line;
        while (getline(cFile, line)) {
            // FIXME: may not remove middle spaces
            line.erase(std::remove_if(line.begin(), line.end(), isspace),
                       line.end());
            if (line[0] == '#' || line.empty())
                continue;

            int delimiterPos = line.find("=");
            std::string key = line.substr(0, delimiterPos);
            int commentPos = line.find("#");
            std::string value = line.substr(delimiterPos + 1,
                                            commentPos - delimiterPos - 1);
            config_[key] = value;
        }
    } else {
        LOG(ERROR) << "Open config file '" << confFile_ << "' failed: "
                   << strerror(errno);
        return false;
    }

    return true;
}

void Configuration::PrintConfig() {
    LOG(INFO) << std::string(30, '=') << "BEGIN" << std::string(30, '=');
    for (auto &item : config_) {
        LOG(INFO) << item.first << std::string(60 - item.first.size(), ' ')
                  << ": " << item.second;
    }
    LOG(INFO) << std::string(31, '=') << "END" << std::string(31, '=');
}

template <class T>
void Configuration::GetValueFatalIfFail(const std::string& key, T& value) {
    if (config_.find(key) != config_.end()) {
        std::stringstream sstream(config_[key]);
        sstream >> value;
        return;
    }
    LOG(FATAL) << "Get " << key << " from " << confFile_ << " fail";
}

}  // namespace HybridCache
