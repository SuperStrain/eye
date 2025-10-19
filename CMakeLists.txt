cmake_minimum_required(VERSION 3.15)

project(eye VERSION 0.1 LANGUAGES C CXX)

# 指定 C/C++ 标准
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# 默认构建类型设置成release
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

# 默认生成静态库
option(BUILD_SHARED_LIBS "Build shared libraries" OFF)

# 增加警告严格性 显式禁用​​关于“函数参数未被使用”的特定警告
set(DEFAULT_WARNING_FLAGS "-Wall -Wextra -Wno-unused-parameter")
set(DEFAULT_OPT_FLAGS_RELEASE "-O2 -DNDEBUG")
set(DEFAULT_OPT_FLAGS_DEBUG "-O0 -g")

if(CMAKE_BUILD_TYPE STREQUAL "Release")
  set(CMAKE_C_FLAGS   "${CMAKE_C_FLAGS} ${DEFAULT_OPT_FLAGS_RELEASE}")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${DEFAULT_OPT_FLAGS_RELEASE}")
  set(CMAKE_EXE_LINKER_FLAGS_RELEASE "${CMAKE_EXE_LINKER_FLAGS_RELEASE} -s")
else()
  set(CMAKE_C_FLAGS   "${CMAKE_C_FLAGS} ${DEFAULT_OPT_FLAGS_DEBUG}")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${DEFAULT_OPT_FLAGS_DEBUG}")
endif()

option(ENABLE_PEDANTIC "Enable more pedantic warnings" OFF)
if(ENABLE_PEDANTIC)
# 向编译器添加 -Wpedantic选项，​​启用严格的标准符合性检查
  add_compile_options(-Wpedantic)
endif()

# 引入交叉编译配置
if(NOT DEFINED CMAKE_TOOLCHAIN_FILE)
    message(FATAL_ERROR "Please use -DCMAKE_TOOLCHAIN_FILE=./aarch64-none-linux-gnu-toolchain.cmake")
endif()

set (THIRDPARTY ${CMAKE_SOURCE_DIR}/thirdparty)
set (target_out ${PROJECT_NAME})
set(HI_MPP_LIB
    ss_mpi_sysmem
    ss_mpi
    )
set (THIRDPARTY_LIB
    securec
    ${HI_MPP_LIB}
    zlog
    )
 
file(GLOB_RECURSE DIR_SRCS "./source/*.c" "./source/*.cpp")

add_executable(${target_out} ${DIR_SRCS})

target_include_directories(
    ${target_out}
    PRIVATE
    ./include
    ${THIRDPARTY}/zlog/include
    ${THIRDPARTY}/hi3516cv610_mpp/include
)

target_link_directories(
    ${target_out}
    PRIVATE
    ${THIRDPARTY}/zlog/lib
    ${THIRDPARTY}/hi3516cv610_mpp/lib
)

target_link_libraries(${target_out} PRIVATE
    pthread
    ${THIRDPARTY_LIB}
)

# ------------------------------------------------------------------
# 默认安装到当前用户主目录 ~/eyeOut（仅当用户没有用 -DCMAKE_INSTALL_PREFIX 指定时）
# ------------------------------------------------------------------
if(CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
  # 通过环境变量 HOME 作为用户主目录（Unix 平台）
  if(DEFINED ENV{HOME})
    set(default_install_dir "$ENV{HOME}/eyeOut")
  else()
    set(default_install_dir "${CMAKE_SOURCE_DIR}/eyeOut")  # 兜底
  endif()
  set(CMAKE_INSTALL_PREFIX "${default_install_dir}" CACHE PATH "Default install path" FORCE)
endif()

install(CODE [[
    message(STATUS "准备清理安装目录：${CMAKE_INSTALL_PREFIX}")
    file(REMOVE_RECURSE "${CMAKE_INSTALL_PREFIX}")
]])

# 递归查找 thirdparty 下所有 .so 文件，包括版本号
file(GLOB_RECURSE THIRD_PARTY_SOS
    "${CMAKE_SOURCE_DIR}/thirdparty/*.so*"
)

# 排除芯片库目录
set(FILTERED_SOS "")
foreach(lib ${THIRD_PARTY_SOS})
    if(NOT lib MATCHES ".*/hi3516cv610_mpp/.*")
        list(APPEND FILTERED_SOS ${lib})
    endif()
endforeach()

install(FILES ${FILTERED_SOS}
        DESTINATION lib)

install (TARGETS ${target_out} 
         RUNTIME DESTINATION bin
        #  LIBRARY DESTINATION lib
         )

install(DIRECTORY ${CMAKE_SOURCE_DIR}/conf/ DESTINATION conf)

message(STATUS "C flags: ${CMAKE_C_FLAGS}")
message(STATUS "CXX flags: ${CMAKE_CXX_FLAGS}")
message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "Source dir: ${CMAKE_SOURCE_DIR}")
message(STATUS "Default install prefix: ${CMAKE_INSTALL_PREFIX}")
message(STATUS "Third-party libs to install: ${FILTERED_SOS}")

