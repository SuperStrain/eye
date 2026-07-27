TARGET_PLATFORM ?= hi3516cv610

ifeq ($(TARGET_PLATFORM),hi3516cv610)
  TOOLCHAIN := cmake/toolchain-arm-v01c02.cmake
else ifeq ($(TARGET_PLATFORM),rv1126b)
  TOOLCHAIN := cmake/toolchain-rv1126b.cmake
else
  $(error Unsupported TARGET_PLATFORM: $(TARGET_PLATFORM). Use hi3516cv610 or rv1126b)
endif

# 根据命令行目标决定构建类型，支持 make debug install 这类组合
ifneq (,$(filter debug,$(MAKECMDGOALS)))
  BUILD_TYPE := Debug
else ifneq (,$(filter release,$(MAKECMDGOALS)))
  BUILD_TYPE := Release
else
  BUILD_TYPE ?= Release
endif

BUILD_DIR := build/$(TARGET_PLATFORM)-$(shell echo $(BUILD_TYPE) | tr A-Z a-z)

CMAKE_CFG := cmake -S . -B $(BUILD_DIR) \
    -DCMAKE_TOOLCHAIN_FILE=$(TOOLCHAIN) \
    -DTARGET_PLATFORM=$(TARGET_PLATFORM) \
    -DCMAKE_BUILD_TYPE=$(BUILD_TYPE)

NJOBS ?= $(shell nproc)
# 命令行 -jN 经 jobserver 传导至子 make（须在 recipe 内检测 MAKEFLAGS，解析期不可见 -j）

.PHONY: all release debug build install clean distclean help

all: build

# release/debug 仅声明构建类型，实际编译委托给 build（可与 install 组合，如 make debug install）
release debug: build

build:
	@echo "==> 配置 ($(TARGET_PLATFORM), $(BUILD_TYPE)) -> $(BUILD_DIR)"
	$(CMAKE_CFG)
	@echo "==> 编译 ($(BUILD_TYPE), $(if $(findstring -j,$(MAKEFLAGS)),jobserver,-j$(NJOBS)))"
	$(MAKE) -C $(BUILD_DIR) $(if $(findstring -j,$(MAKEFLAGS)),,-j$(NJOBS))

install: build
	@echo "==> 安装 ($(BUILD_TYPE))"
	cmake --install $(BUILD_DIR)

clean:
	rm -rf $(BUILD_DIR)

distclean:
	rm -rf build/hi3516cv610-release build/hi3516cv610-debug \
	       build/rv1126b-release build/rv1126b-debug

help:
	@echo "用法："
	@echo "  make                    编译 Release（默认，-O2）"
	@echo "  make debug              编译 Debug（-O0 -g，perf/gdb 调试用）"
	@echo "  make install            编译 Release 并安装"
	@echo "  make debug install      编译 Debug 并安装"
	@echo "  make clean              清当前 build 目录"
	@echo "  make distclean          清所有 build 目录"
	@echo "变量："
	@echo "  TARGET_PLATFORM=hi3516cv610|rv1126b   (默认 hi3516cv610)"
	@echo "  NJOBS=N                               编译并行度（默认 nproc=$(NJOBS)）"
	@echo "  -jN                                    命令行 -jN 经 jobserver 控制编译并行度"
	@echo "  组合示例：make debug install -j4 TARGET_PLATFORM=rv1126b"
