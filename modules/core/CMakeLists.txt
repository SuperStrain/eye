add_library(core STATIC
    src/logger.cpp
    src/global_constants.cpp
)
target_include_directories(core PUBLIC
    include
    ${CMAKE_SOURCE_DIR}/thirdparty/zlog/include
)
target_link_libraries(core PUBLIC zlog)
