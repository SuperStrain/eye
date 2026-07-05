add_library(platform_impl STATIC
    src/hi_video_pipeline.cpp
    src/hi_venc_param.cpp
    src/hi_stream_fetcher.cpp
    src/hi_platform_factory.cpp
)
target_include_directories(platform_impl PUBLIC
    include
    ${CMAKE_SOURCE_DIR}/modules/stream/include
    ${CMAKE_SOURCE_DIR}/thirdparty/hi3516cv610_mpp/include
)
target_link_directories(platform_impl PUBLIC
    ${CMAKE_SOURCE_DIR}/thirdparty/hi3516cv610_mpp/lib
)
target_link_libraries(platform_impl
    PUBLIC interface
    PRIVATE pthread ${HI_MPP_LIB} securec
)
target_compile_definitions(platform_impl PUBLIC PLATFORM_HI3516CV610=1)
