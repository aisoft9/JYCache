find_library(ICEORYX_POSH_LIB iceoryx_posh PATHS ../thirdparties/iceoryx/lib)
find_library(ICEORYX_HOOFS_LIB iceoryx_hoofs PATHS ../thirdparties/iceoryx/lib)
find_library(ICEORYX_PLATFORM_LIB iceoryx_platform PATHS ../thirdparties/iceoryx/lib)

file(GLOB FILESYSTEM_SOURCES *.cpp)
file(GLOB FILESYSTEM_HEADERS *.h)

add_library(intercept_filesystem ${FILESYSTEM_SOURCES})
target_include_directories(intercept_filesystem PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(intercept_filesystem PUBLIC
    ${ICEORYX_POSH_LIB} ${ICEORYX_HOOFS_LIB} ${ICEORYX_PLATFORM_LIB}
    hybridcache_local madfs_global s3fs_lib ${THIRD_PARTY_LIBRARIES} common_lib
    -pthread
    -lcurl
    -lxml2
    -lcrypto
    -ldl
    -laio
    -lrt
)

add_library(intercept_filesystem_client INTERFACE)
target_include_directories(intercept_filesystem_client INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(intercept_filesystem_client INTERFACE
    common_lib_client
    -lrt
)
target_compile_options(intercept_filesystem_client INTERFACE -DCLIENT_BUILD)