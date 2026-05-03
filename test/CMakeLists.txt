add_executable(rtsp_unit_tests
    rtsp/rtsp_unit_tests.cpp
)

target_link_libraries(rtsp_unit_tests PRIVATE rtsp)
