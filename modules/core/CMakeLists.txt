add_library(core STATIC
    src/logger.cpp
    src/global_constants.cpp
)
target_include_directories(core PUBLIC include)
if(TARGET_PLATFORM STREQUAL "rv1126b")
    target_include_directories(core PUBLIC ${CMAKE_SYSROOT}/usr/include)
    find_library(ZLOG_LIBRARY zlog REQUIRED)
    target_link_libraries(core PUBLIC ${ZLOG_LIBRARY})
else()
    target_include_directories(core PUBLIC
        ${CMAKE_SOURCE_DIR}/thirdparty/zlog/include
    )
    target_link_directories(core PUBLIC
        ${CMAKE_SOURCE_DIR}/thirdparty/zlog/lib
    )
    target_link_libraries(core PUBLIC zlog)
endif()
