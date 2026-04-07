add_library(interface INTERFACE)
target_include_directories(interface INTERFACE include)
target_link_libraries(interface INTERFACE core)
