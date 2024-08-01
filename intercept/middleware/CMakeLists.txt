# src/middleware/CMakeLists.txt

find_library(ICEORYX_POSH_LIB NAMES iceoryx_posh PATHS ../../thirdparties/iceoryx/lib)
find_library(ICEORYX_HOOFS_LIB NAMES iceoryx_hoofs PATHS ../../thirdparties/iceoryx/lib)

file(GLOB MIDDLEWARE_SOURCES *.cpp)
file(GLOB MIDDLEWARE_HEADERS *.h)

add_library(intercept_middleware ${MIDDLEWARE_SOURCES})
target_include_directories(intercept_middleware PUBLIC 
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/../../thirdparties/iceoryx/include
)
target_link_libraries(intercept_middleware PUBLIC 
    intercept_internal
    intercept_filesystem
    ${ICEORYX_HOOFS_LIB}
    ${ICEORYX_POSH_LIB}
    
)


set(CMAKE_FIND_LIBRARY_SUFFIXES ".so" ".a")

find_library(ICEORYX_POSH_LIB NAMES iceoryx_posh PATHS ../../thirdparties/iceoryx/lib)
find_library(ICEORYX_HOOFS_LIB NAMES iceoryx_hoofs PATHS ../../thirdparties/iceoryx/lib)
find_library(ICEORYX_PLATFORM_LIB NAMES iceoryx_hoofs PATHS ../../thirdparties/iceoryx/lib)

file(GLOB CLIENT_MIDDLEWARE_SOURCES *.cpp)
file(GLOB CLIENT_MIDDLEWARE_HEADERS *.h)

add_library(intercept_middleware_client ${CLIENT_MIDDLEWARE_SOURCES})
target_include_directories(intercept_middleware_client PUBLIC 
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/../../thirdparties/iceoryx/include
)
target_link_libraries(intercept_middleware_client PUBLIC
    -lrt
    intercept_internal_client
    intercept_filesystem_client
    ${ICEORYX_POSH_LIB}
    ${ICEORYX_HOOFS_LIB} 
    ${ICEORYX_PLATFORM_LIB} 
)
target_compile_options(intercept_middleware_client PUBLIC -DCLIENT_BUILD -fPIC )