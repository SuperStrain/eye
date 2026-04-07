add_executable(eye main.cpp)
target_include_directories(eye PRIVATE
    ${CMAKE_SOURCE_DIR}/app
    ${CMAKE_SOURCE_DIR}/modules/core/include
    ${CMAKE_SOURCE_DIR}/modules/interface/include
    ${CMAKE_SOURCE_DIR}/modules/stream/include
    ${CMAKE_SOURCE_DIR}/modules/platform/hi3516cv610/include
    ${CMAKE_SOURCE_DIR}/thirdparty/zlog/include
    ${CMAKE_SOURCE_DIR}/thirdparty/hi3516cv610_mpp/include
)
target_link_directories(eye PRIVATE
    ${CMAKE_SOURCE_DIR}/thirdparty/zlog/lib
    ${CMAKE_SOURCE_DIR}/thirdparty/hi3516cv610_mpp/lib
)
target_link_libraries(eye PRIVATE platform_hi stream core pthread)
install(TARGETS eye RUNTIME DESTINATION bin)
