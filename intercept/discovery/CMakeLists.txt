# discovery/CMakeLists.txt

file(GLOB DISCOVERY_SOURCES *.cpp)

find_library(ICEORYX_POSH_LIB NAMES iceoryx_posh PATHS ../../thirdparties/iceoryx/lib)
find_library(ICEORYX_HOOFS_LIB NAMES iceoryx_hoofs PATHS ../../thirdparties/iceoryx/lib)
find_library(ICEORYX_PLATFORM_LIB NAMES iceoryx_platform PATHS ../../thirdparties/iceoryx/lib)

add_library(intercept_discovery ${DISCOVERY_SOURCES})
target_include_directories(intercept_discovery PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/../../thirdparties/iceoryx/include
)
target_link_libraries(intercept_discovery PUBLIC intercept_internal ${ICEORYX_POSH_LIB})

add_library(intercept_discovery_client ${DISCOVERY_SOURCES})
target_include_directories(intercept_discovery_client PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/../../thirdparties/iceoryx/include
)
target_compile_options(intercept_discovery_client PUBLIC -fPIC )
target_link_libraries(intercept_discovery_client PUBLIC -lrt intercept_internal_client ${ICEORYX_POSH_LIB}  ${ICEORYX_HOOFS_LIB} ${ICEORYX_PLATFORM_LIB} )