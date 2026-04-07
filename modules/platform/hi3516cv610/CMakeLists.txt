add_library(platform_hi STATIC
    src/hi_video_pipeline.cpp
    src/hi_venc_param.cpp
    src/hi_stream_fetcher.cpp
)
target_include_directories(platform_hi PUBLIC
    include
    ${CMAKE_SOURCE_DIR}/modules/interface/include
    ${CMAKE_SOURCE_DIR}/modules/core/include
    ${CMAKE_SOURCE_DIR}/modules/stream/include
    ${CMAKE_SOURCE_DIR}/thirdparty/zlog/include
    ${CMAKE_SOURCE_DIR}/thirdparty/hi3516cv610_mpp/include
)
target_link_directories(platform_hi PRIVATE
    ${CMAKE_SOURCE_DIR}/thirdparty/zlog/lib
    ${CMAKE_SOURCE_DIR}/thirdparty/hi3516cv610_mpp/lib
)
target_link_libraries(platform_hi PUBLIC interface PRIVATE
    pthread
    ${THIRDPARTY_LIB}
)
