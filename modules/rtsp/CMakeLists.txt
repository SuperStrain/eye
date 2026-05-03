set(LIVE555_ROOT ${THIRDPARTY}/live555)

add_library(rtsp STATIC
    src/rtsp_frame_queue.cpp
    src/rtsp_nal_parser.cpp
    src/rtsp_parameter_set_cache.cpp
    src/rtsp_stream_source.cpp
    src/rtsp_server_media_subsession.cpp
    src/rtsp_server.cpp
)
target_include_directories(rtsp PUBLIC include)
target_include_directories(rtsp PRIVATE
    ${LIVE555_ROOT}/include/liveMedia
    ${LIVE555_ROOT}/include/groupsock
    ${LIVE555_ROOT}/include/BasicUsageEnvironment
    ${LIVE555_ROOT}/include/UsageEnvironment
)
target_link_libraries(rtsp PUBLIC
    stream
    ${LIVE555_ROOT}/lib/libliveMedia.a
    ${LIVE555_ROOT}/lib/libgroupsock.a
    ${LIVE555_ROOT}/lib/libBasicUsageEnvironment.a
    ${LIVE555_ROOT}/lib/libUsageEnvironment.a
)
