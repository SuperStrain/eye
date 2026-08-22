option(USE_GSTREAMER "Use GStreamer media backend (default: rockit)" OFF)

if(USE_GSTREAMER)
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(GST REQUIRED IMPORTED_TARGET gstreamer-1.0 gstreamer-app-1.0)

    add_library(platform_impl STATIC
        src/gst_video_pipeline.cpp
        src/gst_stream_fetcher.cpp
        src/rv_platform_factory.cpp
    )
    target_include_directories(platform_impl PUBLIC
        include
        ${CMAKE_SOURCE_DIR}/modules/stream/include
    )
    target_link_libraries(platform_impl
        PUBLIC interface
        PRIVATE PkgConfig::GST pthread
    )
    target_compile_definitions(platform_impl PUBLIC PLATFORM_RV1126B=1 USE_GSTREAMER=1)
else()
    find_path(ROCKIT_INCLUDE_DIR rk_mpi_sys.h REQUIRED)
    find_library(ROCKIT_LIBRARY rockit REQUIRED)

    add_library(platform_impl STATIC
        src/rv_video_pipeline.cpp
        src/rv_stream_fetcher.cpp
        src/rv_platform_factory.cpp
    )
    target_include_directories(platform_impl PUBLIC
        include
        ${CMAKE_SOURCE_DIR}/modules/stream/include
        ${ROCKIT_INCLUDE_DIR}
    )
    target_link_libraries(platform_impl
        PUBLIC interface
        PRIVATE ${ROCKIT_LIBRARY} pthread
    )
    target_compile_definitions(platform_impl PUBLIC PLATFORM_RV1126B=1)
endif()
