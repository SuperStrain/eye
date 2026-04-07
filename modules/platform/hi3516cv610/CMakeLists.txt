add_library(platform_hi STATIC
    src/hi_video_pipeline.cpp
    src/hi_venc_param.cpp
    src/hi_stream_fetcher.cpp
)
target_include_directories(platform_hi PUBLIC include)
target_link_libraries(platform_hi PUBLIC interface ${THIRDPARTY_LIB})
