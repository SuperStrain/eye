add_executable(eye main.cpp 
    ${CMAKE_SOURCE_DIR}/test/test.cpp
    ${CMAKE_SOURCE_DIR}/test/stream/stream_test.cpp
)
target_include_directories(eye PRIVATE
    ${CMAKE_SOURCE_DIR}/app
    ${CMAKE_SOURCE_DIR}/test
    ${CMAKE_SOURCE_DIR}/test/stream
)
target_link_libraries(eye PRIVATE platform_hi stream rtsp)
install(TARGETS eye RUNTIME DESTINATION bin)
