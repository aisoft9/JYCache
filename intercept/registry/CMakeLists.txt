# src/registry/CMakeLists.txt

find_library(ICEORYX_POSH_LIB NAMES iceoryx_posh PATHS ../../thirdparties/iceoryx/lib)
find_library(ICEORYX_HOOFS_LIB iceoryx_hoofs PATHS ../thirdparties/iceoryx/lib)
find_library(ICEORYX_PLATFORM_LIB iceoryx_platform PATHS ../thirdparties/iceoryx/lib)

file(GLOB REGISTRY_SOURCES *.cpp)
file(GLOB REGISTRY_HEADERS *.h)

add_library(intercept_registry ${REGISTRY_SOURCES})
target_include_directories(intercept_registry PUBLIC 
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/../../thirdparties/iceoryx/include
)
target_link_libraries(intercept_registry PUBLIC 
    intercept_middleware
    intercept_discovery
    ${ICEORYX_HOOFS_LIB} 
    ${ICEORYX_PLATFORM_LIB}
    ${ICEORYX_POSH_LIB}
)


file(GLOB CLIENT_REGISTRY_SOURCES *.cpp)
file(GLOB CLIENT_REGISTRY_HEADERS *.h)

add_library(intercept_registry_client ${CLIENT_REGISTRY_SOURCES})
target_include_directories(intercept_registry_client PUBLIC 
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/../../thirdparties/iceoryx/include
)
target_link_libraries(intercept_registry_client PUBLIC
    intercept_middleware_client
    intercept_discovery_client
    ${ICEORYX_POSH_LIB}
    ${ICEORYX_HOOFS_LIB} 
    ${ICEORYX_PLATFORM_LIB}
    -lrt
)
target_compile_options(intercept_registry_client PUBLIC -DCLIENT_BUILD )