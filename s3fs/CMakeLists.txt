SET(EXECUTABLE_OUTPUT_PATH ${PROJECT_BINARY_DIR}/bin)

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -O3 -D_FILE_OFFSET_BITS=64 -U_FORTIFY_SOURCE -D_FORTIFY_SOURCE=3")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -O3 -D_FILE_OFFSET_BITS=64 -U_FORTIFY_SOURCE -D_FORTIFY_SOURCE=3")

file(GLOB_RECURSE ALL_SOURCES CONFIGURE_DEPENDS "*.cpp")
list(REMOVE_ITEM ALL_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/s3fs_lib.cpp")
add_executable(s3fs ${ALL_SOURCES})
target_include_directories(s3fs PRIVATE /usr/include/fuse /usr/include/libxml2)
target_link_libraries(s3fs PUBLIC hybridcache_local madfs_global -lfuse -pthread -lcurl -lxml2 -lcrypto -ldl)

file(GLOB_RECURSE LIB_SOURCES CONFIGURE_DEPENDS "*.cpp")
list(REMOVE_ITEM LIB_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/s3fs.cpp")
add_library(s3fs_lib STATIC ${LIB_SOURCES})
target_include_directories(s3fs_lib PRIVATE /usr/include/fuse /usr/include/libxml2)
target_link_libraries(s3fs_lib PUBLIC hybridcache_local madfs_global -pthread -lcurl -lxml2 -lcrypto -ldl)
