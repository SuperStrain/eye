file(GLOB STREAMSRC
    "${CMAKE_CURRENT_SOURCE_DIR}/source/stream/*.cpp"
)

add_library(stream OBJECT ${STREAMSRC})

target_include_directories(stream PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/thirdparty/hi3516cv610_mpp/include
)
