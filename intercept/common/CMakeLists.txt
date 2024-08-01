file(GLOB COMMON_SOURCES *.cpp)

add_library(common_lib ${COMMON_SOURCES})
target_include_directories(common_lib PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

add_library(common_lib_client ${COMMON_SOURCES})
target_compile_options(common_lib_client PUBLIC -fPIC -DCLIENT_BUILD)
target_include_directories(common_lib_client PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
