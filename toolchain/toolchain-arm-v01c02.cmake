# cmake -DCMAKE_TOOLCHAIN_FILE=toolchain-arm-v01c02.cmake

set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR arm)

# ---- 调整下面两项为你的实际路径/前缀 ----
# 如果你的交叉编译器在 /opt/x-tools/arm/bin/ 且编译器名为 arm-v01c02-linux-musleabi-gcc:
#   set(CROSS_TOOLCHAIN_PATH "/opt/x-tools/arm/bin")
#   set(CROSS_PREFIX "arm-v01c02-linux-musleabi-")
# 或者仅用前缀（假设编译器在 PATH 中）：

# set(CROSS_TOOLCHAIN_PATH "/opt/x-tools/arm/bin")
set(CROSS_PREFIX "arm-v01c02-linux-musleabi-")
if(CROSS_TOOLCHAIN_PATH)
  set(CMAKE_C_COMPILER   "${CROSS_TOOLCHAIN_PATH}/${CROSS_PREFIX}gcc")
  set(CMAKE_CXX_COMPILER "${CROSS_TOOLCHAIN_PATH}/${CROSS_PREFIX}g++")
  set(CMAKE_ASM_COMPILER "${CROSS_TOOLCHAIN_PATH}/${CROSS_PREFIX}gcc")
  set(CMAKE_AR            "${CROSS_TOOLCHAIN_PATH}/${CROSS_PREFIX}ar")
  set(CMAKE_RANLIB        "${CROSS_TOOLCHAIN_PATH}/${CROSS_PREFIX}ranlib")
else()
  set(CMAKE_C_COMPILER   "${CROSS_PREFIX}gcc")
  set(CMAKE_CXX_COMPILER "${CROSS_PREFIX}g++")
  set(CMAKE_ASM_COMPILER "${CROSS_PREFIX}gcc")
  set(CMAKE_AR           "${CROSS_PREFIX}ar")
  set(CMAKE_RANLIB       "${CROSS_PREFIX}ranlib")
endif()

# 浮点运算单元和neon
option(ENABLE_CPU_FLAGS "Enable -mcpu/-mfloat-abi/-mfpu flags for this chip" ON)
# 海思cv610平台
set(DEFAULT_CPU_FLAGS "-mcpu=cortex-a7 -mfloat-abi=softfp -mfpu=neon-vfpv4")

if(ENABLE_CPU_FLAGS)
  set(CMAKE_C_FLAGS   "${CMAKE_C_FLAGS} ${DEFAULT_CPU_FLAGS} ${DEFAULT_WARNING_FLAGS}")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${DEFAULT_CPU_FLAGS} ${DEFAULT_WARNING_FLAGS}")
else()
  set(CMAKE_C_FLAGS   "${CMAKE_C_FLAGS} ${DEFAULT_WARNING_FLAGS}")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${DEFAULT_WARNING_FLAGS}")
endif()

# # 可选：指定 sysroot（如果有根文件系统）
# set(CMAKE_SYSROOT "" CACHE PATH "Sysroot for cross compilation (optional)")
# if(CMAKE_SYSROOT)
#   set(CMAKE_FIND_ROOT_PATH ${CMAKE_SYSROOT})
# endif()
# # 查找策略：程序在主机查找，库/头在sysroot查找（典型）
# set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
# set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
# set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
# set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)

# 让 CMake 输出更详细（按需）
set(CMAKE_VERBOSE_MAKEFILE ON)
