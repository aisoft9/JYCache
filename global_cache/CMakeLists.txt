include(FindThreads)
include(FindProtobuf)
protobuf_generate_cpp(PROTO_SRC PROTO_HEADER gcache.proto)
include_directories(${CMAKE_CURRENT_BINARY_DIR} /usr/local/include/jerasure)

add_library(madfs_global
        Common.h
        Common.cpp
        FileSystemDataAdaptor.h
        EtcdClient.h
        Placement.h
        GlobalCacheClient.h
        GlobalCacheClient.cpp
        S3DataAdaptor.h
        S3DataAdaptor.cpp
        GlobalDataAdaptor.h
        GlobalDataAdaptor.cpp
        ReadCacheClient.h
        ReadCacheClient.cpp
        ReplicationWriteCacheClient.h
        ReplicationWriteCacheClient.cpp
        GlobalCacheServer.h
        GlobalCacheServer.cpp
        ReadCache.h
        ReadCache.cpp
        WriteCache.h
        WriteCache.cpp
        WriteCacheClient.h
        ErasureCodingWriteCacheClient.h
        ErasureCodingWriteCacheClient.cpp
        ${PROTO_SRC} 
        ${PROTO_HEADER}
)

option(ENABLE_EC "Enable Erasure Coding" OFF)
target_link_libraries(madfs_global PUBLIC hybridcache_local aio)
if(ENABLE_EC)
    add_definitions(-DCONFIG_JERASURE)
    target_link_libraries(madfs_global PUBLIC Jerasure)
endif()

add_executable(madfs_global_server GlobalCacheServerMain.cpp)
target_link_libraries(madfs_global_server PUBLIC madfs_global)

add_executable(madfs_gc GarbageCollectorMain.cpp)
target_link_libraries(madfs_gc PUBLIC madfs_global)
