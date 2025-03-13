# src/posix/CMakeLists.txt

file(GLOB POSIX_SOURCES *.cpp)
file(GLOB POSIX_HEADERS *.h)

find_library(ICEORYX_POSH_LIB iceoryx_posh PATHS ../../thirdparties/iceoryx/lib)
find_library(ICEORYX_HOOFS_LIB iceoryx_hoofs PATHS ../../thirdparties/iceoryx/lib)
find_library(ICEORYX_PLATFORM_LIB iceoryx_platform PATHS ../../thirdparties/iceoryx/lib)

add_library(intercept_posix_interface_client SHARED ${POSIX_SOURCES})
target_include_directories(intercept_posix_interface_client PUBLIC 
    ${CMAKE_CURRENT_SOURCE_DIR}
)
target_link_libraries(intercept_posix_interface_client PUBLIC
    intercept_registry_client
    ${ICEORYX_POSH_LIB}
    ${ICEORYX_HOOFS_LIB}
    ${ICEORYX_PLATFORM_LIB}    
    "-Wl,--exclude-libs,ALL" -static-libstdc++ -static-libgcc "-Wl,--exclude-libs,ALL"
)

target_compile_options(intercept_posix_interface_client PUBLIC -DCLIENT_BUILD -fPIC)
