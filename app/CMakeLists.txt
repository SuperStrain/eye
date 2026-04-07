add_executable(eye main.cpp)
target_link_libraries(eye PRIVATE platform_hi stream core)
install(TARGETS eye RUNTIME DESTINATION bin)
