# src/posix/CMakeLists.txt

file(GLOB POSIX_SOURCES *.cpp)
file(GLOB POSIX_HEADERS *.h)

add_library(intercept_posix_interface_client ${POSIX_SOURCES})
target_include_directories(intercept_posix_interface_client PUBLIC 
    ${CMAKE_CURRENT_SOURCE_DIR}
)
target_link_libraries(intercept_posix_interface_client PUBLIC
    intercept_registry_client
)
target_compile_options(intercept_posix_interface_client PUBLIC -DCLIENT_BUILD -fPIC)
