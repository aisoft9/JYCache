project(hybridcache)

cmake_minimum_required(VERSION 3.7)
cmake_policy(SET CMP0079 NEW)
set(CMAKE_CXX_STANDARD 17)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fno-PIE")
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -fno-PIE") 

list(APPEND CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/thirdparties)
list(APPEND CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/thirdparties/CmakeFiles)
include(ThirdPartyConfig)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DNDEBUG -O3 -g -D__const__=__unused__ -pipe -W -Wno-deprecated -Wno-sign-compare -Wno-unused-parameter -fPIC")

include_directories(AFTER ${CMAKE_SOURCE_DIR}/local_cache ${CMAKE_SOURCE_DIR}/global_cache)
include_directories(AFTER ${CMAKE_BINARY_DIR}/local_cache ${CMAKE_BINARY_DIR}/global_cache)

# subdirectory
add_subdirectory(local_cache)
add_subdirectory(global_cache)
add_subdirectory(s3fs)
add_subdirectory(intercept)
add_subdirectory(test)
