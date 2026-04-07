# 目录结构重组与硬件抽象层设计

## 目标

1. 将芯片原厂（海思）SDK 依赖集中隔离到独立模块
2. 引入平台抽象接口，使更换芯片平台时只需新增实现、不改动已有代码
3. 按功能模块重组目录结构，提升可维护性和可扩展性

## 约束

- 通用类型使用标准 C/C++ 类型（uint32_t 等），完全脱离海思 td_* 类型
- 平台抽象拆分为三个接口：IVideoPipeline、IVideoEncoder、IStreamProvider
- StreamConsumerManager 等全局单例保持现状（全局作用域、register_consumer 模式）
- 命名空间 hiMppMedia 保持不变
- 构建：交叉编译，构建命令 `cd build && ./set.sh && make` 不变

## 两阶段推进策略

**阶段一：代码抽象**（在当前目录下重构代码，不移动文件位置）
- 定义平台无关的通用类型和接口
- 改造现有代码消除散落的 SDK 依赖
- 每步确保可编译

**阶段二：目录重组**（基于抽象后的代码移动文件）
- 按模块 + 平台分层组织目录
- 改造 CMake 构建系统

---

## 阶段一：代码抽象

### 1.1 新增通用类型文件

新建 `include/common_types.h`：

```cpp
#ifndef COMMON_TYPES_H
#define COMMON_TYPES_H

#include <cstdint>
#include <cstddef>

struct Size {
    uint32_t width;
    uint32_t height;
};

enum class NaluType : uint8_t {
    IDR_SLICE,
    P_SLICE,
    B_SLICE,
    OTHER,
};

struct FramePack {
    const uint8_t* data;
    uint32_t len;
    NaluType nalu_type;
};

struct FrameData {
    static constexpr uint32_t MAX_PACKS = 8;
    FramePack packs[MAX_PACKS];
    uint32_t pack_count;
    uint32_t seq;
};

#endif
```

### 1.2 修改 global_constants.h/.cpp

- `ot_size` → `Size`（自定义结构体）
- `PIC_BUTT` → 自定义枚举边界
- 分辨率枚举 `ot_pic_size` 改为自定义枚举 `PicSize`，与海思解耦
- 映射表从 `ot_size[]` 改为 `Size[]`

### 1.3 新增平台抽象接口

新建 `include/i_video_pipeline.h`：

```cpp
class IVideoPipeline {
public:
    virtual ~IVideoPipeline() = default;
    virtual int init() = 0;
    virtual int deinit() = 0;
    virtual int start() = 0;
    virtual int stop() = 0;
};
```

新建 `include/i_video_encoder.h`：

```cpp
class IVideoEncoder {
public:
    virtual ~IVideoEncoder() = default;
    virtual int createChannel(int chn, CodecType codec, Size resolution) = 0;
    virtual int destroyChannel(int chn) = 0;
    virtual int startChannel(int chn) = 0;
    virtual int stopChannel(int chn) = 0;
};
```

新建 `include/i_stream_provider.h`：

```cpp
class IStreamProvider {
public:
    virtual ~IStreamProvider() = default;
    virtual int start(VencChannel chn) = 0;
    virtual int stop(VencChannel chn) = 0;
    virtual int fetchFrame(VencChannel chn, FrameData& frame) = 0;
    virtual int releaseFrame(VencChannel chn) = 0;
};
```

### 1.4 改造 StreamFrame

- `stream_frame.h`：成员变量从 `ot_venc_stream` / `ot_venc_pack` 改为 `FrameData`
- `stream_frame.cpp`：构造函数接收 `FrameData`，`is_idr()` 根据 `NaluType` 判断
- 消除对 `<ot_common_venc.h>` 的依赖

### 1.5 改造 StreamFetcher

- 重命名为 `HiStreamFetcher`（文件名暂不变），实现 `IStreamProvider`
- `fetchFrame()` 内部调用 `ss_mpi_venc_get_stream`，将 `ot_venc_stream` 转换为 `FrameData` 返回
- `releaseFrame()` 内部调用 `ss_mpi_venc_release_stream`

### 1.6 改造 recorder_consumer

- `is_idr()` 检查通过 `StreamFrame::is_idr()` 获取结果
- 不再直接引用 `OT_VENC_H264_NALU_IDR_SLICE` 等枚举

### 1.7 改造 video_process_hi / video_venc_hi

- `video_process_hi` 类实现 `IVideoPipeline` 和 `IVideoEncoder`
- `video_venc_hi` 的编码参数填充逻辑作为 `video_process_hi` 的内部实现细节
- 接口参数使用标准类型和自定义类型

### 1.8 改造 main.cpp

- 通过接口指针调用，不直接依赖具体实现类
- 具体平台实现的选择可通过工厂函数或编译期条件决定

---

## 阶段二：目录重组

### 目标结构

```
eye/
├── cmake/
│   └── toolchain-arm-v01c02.cmake
├── conf/
│   └── zlog.conf
├── modules/
│   ├── core/
│   │   ├── include/
│   │   │   ├── logger.h
│   │   │   ├── global_constants.h
│   │   │   └── common_types.h
│   │   ├── src/
│   │   │   ├── logger.cpp
│   │   │   └── global_constants.cpp
│   │   └── CMakeLists.txt
│   ├── interface/
│   │   ├── include/
│   │   │   ├── i_video_pipeline.h
│   │   │   ├── i_video_encoder.h
│   │   │   └── i_stream_provider.h
│   │   └── CMakeLists.txt          # INTERFACE 库
│   ├── stream/
│   │   ├── include/
│   │   │   ├── stream_common.h
│   │   │   ├── stream_frame.h
│   │   │   ├── stream_distributor.h
│   │   │   ├── stream_consumer_manager.h
│   │   │   └── recorder_consumer.h
│   │   ├── src/
│   │   │   ├── stream_frame.cpp
│   │   │   ├── stream_distributor.cpp
│   │   │   ├── stream_consumer_manager.cpp
│   │   │   └── recorder_consumer.cpp
│   │   └── CMakeLists.txt
│   └── platform/
│       └── hi3516cv610/
│           ├── include/
│           │   ├── hi_video_pipeline.h
│           │   ├── hi_video_encoder.h
│           │   ├── hi_stream_fetcher.h
│           │   └── hi_venc_param.h
│           ├── src/
│           │   ├── hi_video_pipeline.cpp
│           │   ├── hi_video_encoder.cpp
│           │   ├── hi_stream_fetcher.cpp
│           │   └── hi_sensor_sc4336p.cpp
│           └── CMakeLists.txt
├── app/
│   ├── main.cpp
│   └── CMakeLists.txt
├── test/
│   ├── test.cpp
│   ├── test.h
│   └── stream/
│       ├── stream_test.h
│       └── stream_test.cpp
├── thirdparty/
└── CMakeLists.txt
```

### 文件移动映射

| 原路径 | 新路径 |
|--------|--------|
| `include/logger.h` | `modules/core/include/logger.h` |
| `include/global_constants.h` | `modules/core/include/global_constants.h` |
| `include/common_types.h`（新） | `modules/core/include/common_types.h` |
| `source/logger.cpp` | `modules/core/src/logger.cpp` |
| `source/global_constants.cpp` | `modules/core/src/global_constants.cpp` |
| `include/i_video_pipeline.h`（新） | `modules/interface/include/i_video_pipeline.h` |
| `include/i_video_encoder.h`（新） | `modules/interface/include/i_video_encoder.h` |
| `include/i_stream_provider.h`（新） | `modules/interface/include/i_stream_provider.h` |
| `include/stream_common.h` | `modules/stream/include/stream_common.h` |
| `source/stream/stream_frame.h` | `modules/stream/include/stream_frame.h` |
| `source/stream/stream_distributor.h` | `modules/stream/include/stream_distributor.h` |
| `source/stream/stream_consumer_manager.h` | `modules/stream/include/stream_consumer_manager.h` |
| `source/stream/recorder_consumer.h` | `modules/stream/include/recorder_consumer.h` |
| `source/stream/stream_frame.cpp` | `modules/stream/src/stream_frame.cpp` |
| `source/stream/stream_distributor.cpp` | `modules/stream/src/stream_distributor.cpp` |
| `source/stream/stream_consumer_manager.cpp` | `modules/stream/src/stream_consumer_manager.cpp` |
| `source/stream/recorder_consumer.cpp` | `modules/stream/src/recorder_consumer.cpp` |
| `include/video_process_hi.h` | `modules/platform/hi3516cv610/include/hi_video_pipeline.h` |
| `include/video_venc_hi.h` | `modules/platform/hi3516cv610/include/hi_venc_param.h` |
| `source/video_process_hi.cpp` | `modules/platform/hi3516cv610/src/hi_video_pipeline.cpp` |
| `source/video_venc_hi.cpp` | `modules/platform/hi3516cv610/src/hi_venc_param.cpp` |
| `source/stream/stream_fetcher.h` | `modules/platform/hi3516cv610/include/hi_stream_fetcher.h` |
| `source/stream/stream_fetcher.cpp` | `modules/platform/hi3516cv610/src/hi_stream_fetcher.cpp` |
| `source/main.cpp` | `app/main.cpp` |
| `include/test.h` | `test/test.h` |

### CMake 结构

根 CMakeLists.txt：
```cmake
cmake_minimum_required(VERSION 3.15)
project(eye VERSION 0.1 LANGUAGES C CXX)

# 编译标准、警告 flags、toolchain 等配置保持不变
# ...

set(THIRDPARTY ${CMAKE_SOURCE_DIR}/thirdparty)
set(HI_MPP_LIB ...)
set(THIRDPARTY_LIB ...)

option(TARGET_PLATFORM "hi3516cv610" "Target platform")

add_subdirectory(modules/core)
add_subdirectory(modules/interface)
add_subdirectory(modules/stream)
add_subdirectory(modules/platform/${TARGET_PLATFORM})
add_subdirectory(app)

# thirdparty .so 安装等规则保持不变
```

`modules/core/CMakeLists.txt`：
```cmake
add_library(core STATIC
    src/logger.cpp
    src/global_constants.cpp
)
target_include_directories(core PUBLIC include)
target_link_libraries(core PUBLIC zlog)
```

`modules/interface/CMakeLists.txt`：
```cmake
add_library(interface INTERFACE)
target_include_directories(interface INTERFACE include)
target_link_libraries(interface INTERFACE core)
```

`modules/stream/CMakeLists.txt`：
```cmake
add_library(stream STATIC
    src/stream_frame.cpp
    src/stream_distributor.cpp
    src/stream_consumer_manager.cpp
    src/recorder_consumer.cpp
)
target_include_directories(stream PUBLIC include)
target_link_libraries(stream PUBLIC interface pthread)
```

`modules/platform/hi3516cv610/CMakeLists.txt`：
```cmake
add_library(platform_hi STATIC
    src/hi_video_pipeline.cpp
    src/hi_venc_param.cpp
    src/hi_stream_fetcher.cpp
)
target_include_directories(platform_hi PUBLIC include)
target_link_libraries(platform_hi PUBLIC interface ${THIRDPARTY_LIB})
```

`app/CMakeLists.txt`：
```cmake
add_executable(eye main.cpp)
target_link_libraries(eye PRIVATE platform_hi stream core)
install(TARGETS eye RUNTIME DESTINATION bin)
```

---

## 新增芯片平台的工作量

以新增 Rockchip RK3588 为例：

1. 创建 `modules/platform/rk3588/` 目录
2. 实现 `RkVideoPipeline`（IVideoPipeline）
3. 实现 `RkVideoEncoder`（IVideoEncoder）
4. 实现 `RkStreamFetcher`（IStreamProvider）
5. 编写 `modules/platform/rk3588/CMakeLists.txt`
6. 根 CMakeLists.txt 中 `-DTARGET_PLATFORM=rk3588`

**不需要改动的模块**：core、interface、stream、app

---

## 风险与注意事项

1. `ot_pic_size` 枚举被 video_process_hi.cpp 大量使用作为数组索引，需在 global_constants 中保留映射关系
2. `video_process_hi.cpp` 是最大的改动点，包含 SYS/VB/VI/VPSS/ISP/MIPI 全部初始化逻辑，需仔细拆分
3. 阶段一每步应确保可编译，建议每完成一个子步骤即编译验证
4. StreamFrame 从持有 SDK 类型改为持有 FrameData，涉及数据拷贝，需评估性能影响（当前已在做 memcpy，影响不大）
