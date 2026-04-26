add_library(stream STATIC
    src/stream_frame.cpp
    src/stream_distributor.cpp
    src/stream_consumer_manager.cpp
    src/recorder_consumer.cpp
)
target_include_directories(stream PUBLIC include)
target_link_libraries(stream PUBLIC interface pthread)
