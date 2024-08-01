link_libraries(-lrt)

find_library(ICEORYX_POSH_LIB iceoryx_posh PATHS ../thirdparties/iceoryx/lib)
find_library(ICEORYX_HOOFS_LIB iceoryx_hoofs PATHS ../thirdparties/iceoryx/lib)
find_library(ICEORYX_PLATFORM_LIB iceoryx_platform PATHS ../thirdparties/iceoryx/lib)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -mavx2")

include_directories(${CMAKE_CURRENT_SOURCE_DIR})
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../thirdparties/spdlog/include)

add_subdirectory(common)
add_subdirectory(internal)
add_subdirectory(discovery)
add_subdirectory(filesystem)
add_subdirectory(registry)
add_subdirectory(middleware)
add_subdirectory(posix)

add_executable(intercept_server server.cpp)
target_link_libraries(intercept_server  PUBLIC intercept_discovery intercept_internal  common_lib intercept_filesystem intercept_middleware intercept_registry hybridcache_local madfs_global s3fs_lib ${THIRD_PARTY_LIBRARIES} ${ICEORYX_POSH_LIB} ${ICEORYX_HOOFS_LIB} ${ICEORYX_PLATFORM_LIB} -pthread -lcurl -lxml2 -lcrypto -ldl -laio -lrt -lacl)

add_library(intercept_client SHARED client.cpp)
target_link_libraries(intercept_client PUBLIC
    intercept_posix_interface_client
    -lsyscall_intercept
    -pthread
    ${ICEORYX_POSH_LIB}
    ${ICEORYX_HOOFS_LIB} 
    ${ICEORYX_PLATFORM_LIB}  
    -lrt
    -L${CMAKE_CURRENT_SOURCE_DIR}/../thirdparties/intercept/lib
)
target_compile_options(intercept_client PUBLIC -DCLIENT_BUILD -mavx2)
