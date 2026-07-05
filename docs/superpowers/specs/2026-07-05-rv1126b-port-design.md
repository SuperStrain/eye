# RV1126B Platform Port Design

## Goal

Port `eye` to the Rockchip RV1126B platform while keeping the existing Hi3516CV610 platform supported from the same codebase.

The RV1126B build must use the Buildroot SDK installed under `/opt/aarch64-buildroot-linux-gnu_sdk-buildroot`, with the `aarch64-buildroot-linux-gnu-` compiler prefix and its sysroot. The generated install tree remains `$HOME/eyeOut` so the RV1126B SDK defconfig `07_atk_dlrv1126b_automipi_eye_defconfig` can package it into the `/app` partition.

## Current State

The repository already has a platform directory mechanism through `TARGET_PLATFORM`:

- `modules/platform/hi3516cv610` contains the current HiSilicon MPP implementation.
- `modules/core`, `modules/interface`, `modules/stream`, and `modules/rtsp` are mostly platform independent.
- `app/main.cpp` directly includes `hi_video_pipeline.h` and `hi_stream_fetcher.h`.
- `app/CMakeLists.txt` directly links `platform_hi`.
- `test/test.cpp` is compiled into the production `eye` binary and directly includes HiSilicon MPP headers.

The main portability blockers are the direct HiSilicon dependencies in `app/main.cpp`, `app/CMakeLists.txt`, and `test/test.cpp`.

## Recommended Approach

Use a platform-isolated implementation with a small platform factory.

Each platform owns its MPP/Rockit code under `modules/platform/<platform>`, while application code creates platform objects through a common factory interface. This keeps platform differences out of `app/main.cpp` and preserves Hi3516CV610 behavior.

Alternatives considered:

- Add platform `#ifdef` blocks directly in `main.cpp` and tests. This is smaller initially but spreads platform coupling into application code.
- Split the app into platform-specific entry points. This is fast but forks business logic and increases long-term maintenance cost.

The factory approach is preferred because the project already has interface abstractions and a platform directory boundary.

## Build Design

Add `cmake/toolchain-rv1126b.cmake` with:

- SDK root default: `/opt/aarch64-buildroot-linux-gnu_sdk-buildroot`
- compiler prefix: `aarch64-buildroot-linux-gnu-`
- sysroot: `${SDK_ROOT}/aarch64-buildroot-linux-gnu/sysroot`
- compiler paths: `${SDK_ROOT}/bin/aarch64-buildroot-linux-gnu-gcc` and `g++`
- CMake find root modes configured so libraries, includes, and packages resolve through the sysroot.

The installed `/opt` SDK has been verified to include:

- `bin/aarch64-buildroot-linux-gnu-gcc`
- `bin/aarch64-buildroot-linux-gnu-g++`
- `aarch64-buildroot-linux-gnu/sysroot/usr/include/rk_mpi_venc.h`
- `aarch64-buildroot-linux-gnu/sysroot/usr/lib/librockit.so`

The RV1126B platform CMake should link Rockit from sysroot with `rockit` and `pthread`. It should not reference the SDK source tree for Rockit headers or libraries unless a future SDK install is incomplete.

`build/set.sh` should support selecting the platform. A minimal interface is:

```sh
./set.sh hi3516cv610
./set.sh rv1126b
```

For compatibility, no argument should keep the existing Hi3516CV610 default.

## Platform Library Design

Normalize the platform target name used by `app`.

Current state:

- Hi3516CV610 builds `platform_hi`.
- `app` links `platform_hi` directly.

Target state:

- The selected platform directory exports a common CMake target such as `platform_impl`.
- `app` links `platform_impl`.
- `modules/platform/hi3516cv610/CMakeLists.txt` can keep its internal source layout but exports the common target.
- `modules/platform/rv1126b/CMakeLists.txt` exports the same target name.

This avoids conditional platform linking in `app/CMakeLists.txt`.

## Application Factory

Add a platform factory API under `modules/interface/include/platform_factory.h`, implemented by the selected platform library:

- `IVideoPipeline& platform_video_pipeline()`
- `std::unique_ptr<IStreamProvider> create_stream_fetcher(VencChannel, StreamType, CodecType, StreamDistributor&)`

`app/main.cpp` should depend on this factory and the existing interfaces, not on platform-specific headers.

This preserves the existing application sequence:

1. initialize logging
2. initialize video pipeline
3. register three stream fetchers
4. start RTSP server
5. register RTSP consumers
6. run existing test hook
7. stay resident

The first implementation can preserve the current lifecycle shape. A later cleanup can replace `pause()` with a signal-driven shutdown path, but that is not required for the RV1126B port.

## RV1126B Video Pipeline

Implement `modules/platform/rv1126b` with Rockit MPI APIs:

- `RK_MPI_SYS_Init()` / `RK_MPI_SYS_Exit()`
- `RK_MPI_VI_*` for camera input
- `RK_MPI_VPSS_*` for scaling and fan-out when needed
- `RK_MPI_VENC_*` for H.265, H.264, and MJPEG encoding
- `RK_MPI_SYS_Bind()` / `RK_MPI_SYS_UnBind()` for module links

Reference sources:

- `/home/yangyang/projects/rv1126b_linux_build/app/rkipc/src/rv1126b_ipc/video/video.c`
- `/home/yangyang/projects/rv1126b_linux_build/external/samples/example/venc/sample_vi_vpss_osd_venc.c`
- `/home/yangyang/projects/rv1126b_linux_build/external/rockit/mpi/example/mod/test_mpi_vi.cpp`
- Rockit headers in the installed sysroot.

Initial stream mapping should preserve eye semantics:

- `VencChannel::CHN0` / `VIDEO_MAIN`: H.265 main stream
- `VencChannel::CHN1` / `VIDEO_SUB`: H.264 sub stream
- `VencChannel::CHN2` / `VIDEO_MJPEG`: MJPEG stream

The exact sensor dimensions and VPSS channel dimensions should be taken from the working RV1126B IPC reference and the active SDK configuration. The initial target is a working visible video output on RV1126B, not a feature-for-feature clone of every RKIPC option.

## RV1126B Stream Fetcher

Implement a Rockit version of `IStreamProvider` using the same pull-and-distribute model as the current HiSilicon implementation.

The fetcher should:

- allocate one `VENC_PACK_S` per fetch initially, matching common Rockit examples
- call `RK_MPI_VENC_GetStream(chn, &stream, timeout_ms)`
- convert `RK_MPI_MB_Handle2VirAddr(pack->pMbBlk)` to `FramePack::data`
- use `pack->u32Len` for `FramePack::len`
- map H.264/H.265 IDR/P/B NALU types into `NaluType`
- call `RK_MPI_VENC_ReleaseStream(chn, &stream)` before the next fetch
- push frames through `StreamDistributor`, preserving existing RTSP and recorder consumers

`StreamFrame` already deep-copies pack data in its constructor, so the RV1126B fetcher may create `StreamFrame` from Rockit pack pointers and then call `RK_MPI_VENC_ReleaseStream()` before pushing the shared frame to `StreamDistributor`. The implementation should preserve this order and avoid exposing raw Rockit buffer pointers beyond the `StreamFrame` constructor.

## Test Hook Compatibility

`test/test.cpp` currently contains HiSilicon-specific VPSS frame dumping code and is compiled into the production `eye` target.

For RV1126B compatibility:

- Guard HiSilicon-only code with platform macros, or move it behind a platform-specific helper.
- Keep `stream_test()` available because it consumes platform-neutral stream output.
- Avoid compiling HiSilicon MPP headers in RV1126B builds.

This keeps the production target buildable for both platforms without removing existing HiSilicon diagnostics.

## Deployment

The install prefix remains `$HOME/eyeOut` by default.

For RV1126B SDK packaging:

- select `07_atk_dlrv1126b_automipi_eye_defconfig` in the SDK
- build and install `eye` into `$HOME/eyeOut`
- keep RV1126B runtime libraries in the SDK rootfs instead of installing copies into `$HOME/eyeOut/lib`
- run the SDK packaging path that creates `app.img` from `eyeout`

The runtime path remains compatible with current assumptions:

- binary: `/app/bin/eye`
- config: `/app/conf`
- logs: governed by existing zlog configuration

## Verification

PC-side verification:

- Configure Hi3516CV610 with the existing toolchain and confirm it still configures.
- Configure RV1126B with `cmake/toolchain-rv1126b.cmake` and `-DTARGET_PLATFORM=rv1126b`.
- Build RV1126B with the `/opt` toolchain.
- Install to `$HOME/eyeOut`.

SDK-side packaging verification:

- Confirm SDK output config uses `07_atk_dlrv1126b_automipi_eye_defconfig`.
- Generate or refresh `app.img` from `$HOME/eyeOut`.

Board-side verification:

- run `/app/bin/eye`
- confirm Rockit system and VI/VPSS/VENC initialization logs are successful
- confirm RTSP main stream is available
- confirm RTSP sub stream is available
- confirm MJPEG stream file output from `stream_test()` if enabled
- confirm process remains resident without repeated VENC timeouts

## Risks

- RV1126B sensor and pipeline configuration may require board-specific values from the working RKIPC `.ini` and DTS. Do not invent sensor parameters.
- Rockit buffer lifetime may differ from HiSilicon. The design relies on the existing `StreamFrame` deep copy and should not bypass it.
- The current application has no orderly deinit path after `pause()`. The RV1126B port should not rely on local process exit behavior for correctness.
- `test/test.cpp` being compiled into production may continue to create platform coupling if not isolated.

## Scope Exclusions

- Audio capture/encoding is not part of the first RV1126B port unless required by a later test case.
- OSD, IVS, NPU, GDC/FEC, UVC, RTMP, and storage features from RKIPC are reference material only and are not required for the first `eye` port.
- No local execution of the ARM binary is expected or valid.
