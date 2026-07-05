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
target_compile_definitions(rtsp PUBLIC NO_OPENSSL=1 NO_STD_LIB=1)
target_link_libraries(rtsp PUBLIC stream)

if(TARGET_PLATFORM STREQUAL "rv1126b")
    target_compile_definitions(rtsp PUBLIC RTSP_USE_SYSROOT_LIVE555=1)
    target_include_directories(rtsp PUBLIC
        ${CMAKE_SYSROOT}/usr/include/liveMedia
        ${CMAKE_SYSROOT}/usr/include/groupsock
        ${CMAKE_SYSROOT}/usr/include/BasicUsageEnvironment
        ${CMAKE_SYSROOT}/usr/include/UsageEnvironment
    )
    find_library(LIVEMEDIA_LIB liveMedia REQUIRED)
    find_library(GROUPSOCK_LIB groupsock REQUIRED)
    find_library(BASICUSAGE_LIB BasicUsageEnvironment REQUIRED)
    find_library(USAGEENV_LIB UsageEnvironment REQUIRED)
    target_link_libraries(rtsp PUBLIC
        ${LIVEMEDIA_LIB}
        ${GROUPSOCK_LIB}
        ${BASICUSAGE_LIB}
        ${USAGEENV_LIB}
    )
else()
    target_include_directories(rtsp PUBLIC
        ${LIVE555_ROOT}/include/liveMedia
        ${LIVE555_ROOT}/include/groupsock
        ${LIVE555_ROOT}/include/BasicUsageEnvironment
        ${LIVE555_ROOT}/include/UsageEnvironment
    )
    target_link_libraries(rtsp PUBLIC
        ${LIVE555_ROOT}/lib/libliveMedia.a
        ${LIVE555_ROOT}/lib/libgroupsock.a
        ${LIVE555_ROOT}/lib/libBasicUsageEnvironment.a
        ${LIVE555_ROOT}/lib/libUsageEnvironment.a
    )
endif()
