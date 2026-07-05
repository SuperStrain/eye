add_library(platform_impl STATIC
    src/rv_video_pipeline.cpp
    src/rv_stream_fetcher.cpp
    src/rv_platform_factory.cpp
)

target_include_directories(platform_impl PUBLIC
    include
    ${CMAKE_SOURCE_DIR}/modules/stream/include
)

target_link_libraries(platform_impl
    PUBLIC interface
    PRIVATE rockit pthread
)

target_compile_definitions(platform_impl PUBLIC PLATFORM_RV1126B=1)
