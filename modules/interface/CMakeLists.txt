add_library(interface INTERFACE)
target_include_directories(interface INTERFACE 
    include
    ${CMAKE_SOURCE_DIR}/modules/core/include
)
target_link_libraries(interface INTERFACE core)
