cmake_minimum_required(VERSION 3.15)

project(eye VERSION 0.1 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

option(BUILD_SHARED_LIBS "Build shared libraries" OFF)
option(ENABLE_PEDANTIC "Enable more pedantic warnings" OFF)

set(DEFAULT_WARNING_FLAGS "-Wall -Wextra -Wno-unused-parameter")
set(DEFAULT_OPT_FLAGS_RELEASE "-O2 -DNDEBUG")
set(DEFAULT_OPT_FLAGS_DEBUG "-O0 -g")

if(CMAKE_BUILD_TYPE STREQUAL "Release")
  set(CMAKE_C_FLAGS   "${CMAKE_C_FLAGS} ${DEFAULT_OPT_FLAGS_RELEASE}")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${DEFAULT_OPT_FLAGS_RELEASE}")
else()
  set(CMAKE_C_FLAGS   "${CMAKE_C_FLAGS} ${DEFAULT_OPT_FLAGS_DEBUG}")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${DEFAULT_OPT_FLAGS_DEBUG}")
endif()

if(ENABLE_PEDANTIC)
  add_compile_options(-Wpedantic)
endif()

if(NOT DEFINED CMAKE_TOOLCHAIN_FILE)
    message(FATAL_ERROR "Please use -DCMAKE_TOOLCHAIN_FILE=./cmake/toolchain-arm-v01c02.cmake")
endif()

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT DEFINED TARGET_PLATFORM)
    set(TARGET_PLATFORM "hi3516cv610")
endif()

set(THIRDPARTY ${CMAKE_SOURCE_DIR}/thirdparty)
set(target_out ${PROJECT_NAME})
if(TARGET_PLATFORM STREQUAL "hi3516cv610")
    set(HI_MPP_LIB
        ss_mpi_sysmem
        ss_mpi
        ir_auto
        dehaze
        acs
        drc
        ldci
        bnr
        calcflicker
        extend_stats
        ot_mpi_isp
        ss_mpi_isp
        ss_mpi_ae
        ss_mpi_awb
        ss_mpi_sysbind
        sns_sc4336p
    )
endif()
set(THIRDPARTY_LIB
    securec
    ${HI_MPP_LIB}
    )

add_subdirectory(modules/core)
add_subdirectory(modules/interface)
add_subdirectory(modules/stream)
add_subdirectory(modules/platform/${TARGET_PLATFORM})
add_subdirectory(modules/rtsp)
add_subdirectory(test)
add_subdirectory(app)

if(CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
  if(DEFINED ENV{HOME})
    set(default_install_dir "$ENV{HOME}/eyeOut/${TARGET_PLATFORM}")
  else()
    set(default_install_dir "${CMAKE_SOURCE_DIR}/eyeOut/${TARGET_PLATFORM}")
  endif()
  set(CMAKE_INSTALL_PREFIX "${default_install_dir}" CACHE PATH "Default install path" FORCE)
endif()

install(CODE [[message(STATUS "准备清理安装目录：${CMAKE_INSTALL_PREFIX}")]])
file(REMOVE_RECURSE "${CMAKE_INSTALL_PREFIX}")

if(TARGET_PLATFORM STREQUAL "rv1126b")
    set(FILTERED_SOS "")
    install(PROGRAMS ${CMAKE_SOURCE_DIR}/scripts/rk_dumpsys_compat.sh DESTINATION bin RENAME rk_dumpsys)
else()
    file(GLOB_RECURSE THIRD_PARTY_SOS "${CMAKE_SOURCE_DIR}/thirdparty/*.so*")
    set(FILTERED_SOS "")
    foreach(lib ${THIRD_PARTY_SOS})
        if(NOT lib MATCHES ".*/hi3516cv610_mpp/.*" AND NOT lib MATCHES ".*/rv1126b_mpp/.*")
            list(APPEND FILTERED_SOS ${lib})
        endif()
    endforeach()
    install(FILES ${FILTERED_SOS} DESTINATION lib)
endif()

install(DIRECTORY ${CMAKE_SOURCE_DIR}/conf/ DESTINATION conf)

message(STATUS "C flags: ${CMAKE_C_FLAGS}")
message(STATUS "CXX flags: ${CMAKE_CXX_FLAGS}")
message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "Source dir: ${CMAKE_SOURCE_DIR}")
message(STATUS "Default install prefix: ${CMAKE_INSTALL_PREFIX}")
message(STATUS "Runtime libs to install: ${FILTERED_SOS}")
