# internal/CMakeLists.txt

file(GLOB INTERNAL_SOURCES *.cpp)

add_library(intercept_internal ${INTERNAL_SOURCES})
target_include_directories(intercept_internal PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(intercept_internal PUBLIC common_lib)

add_library(intercept_internal_client ${INTERNAL_SOURCES})
target_include_directories(intercept_internal_client PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_options(intercept_internal_client PUBLIC -fPIC)
target_link_libraries(intercept_internal_client PUBLIC common_lib_client)
