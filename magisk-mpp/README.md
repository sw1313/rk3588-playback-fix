# RK3588 MPP userspace upgrade — experimental

This separate Magisk module overlays `/vendor/lib64/libmpp.so` and `/vendor/lib/libmpp.so` with Android builds of Rockchip MPP **1.1.0**, revision **c08762ebfadeb4e986d2fed993bc7a54862d3ebe**. It keeps the original kernel, Codec2/OMX components, clocks, display settings and LSPosed module. The library is shared by decoders, encoders and some JPEG consumers; the upgrade is not limited to AV1.

## Compatibility

The installer requires RK3588, Android 12 / API 32, ARM64 with ARM32 vendor libraries, and the exact tested vendor consumer hashes in `contracts/vendor.sha256`. The stock MPP revision is `e103e6e8` (2023-03-28). Different firmware is rejected rather than assumed compatible.

Both Android libraries keep the `libmpp.so` SONAME and require only `liblog.so`, `libm.so`, `libdl.so` and `libc.so`. No shared C++ runtime is added. The ARM64/ARM32 probes verify loading, the expected revision, all MPP symbols imported by the tested vendor consumers, and basic MPP context creation/destruction. Matching symbols do not establish compatibility of every runtime behavior.

## Install and rollback

1. Install `rk3588-mpp-upgrade-0.1.0-mpp1.1.0.zip` using Magisk's **Install from storage**, then reboot.
2. Test actual H.264, HEVC, AV1 and other media in the apps you use, including seeking and subtitles. Installation and full Codec2 playback after the system overlay have not yet been tested.
3. To roll back, press this module's **Action** button or disable it in Magisk, then reboot. Uninstalling and rebooting also restores the firmware originals. Processes keep already-loaded libraries until restarted; the Action button does not hot-swap a running decoder.

The late-start script records library-load checks in the module's `status.txt`. Failure disables the module for the next reboot without rebooting automatically. This is not a Codec2 crash monitor or boot-loop recovery mechanism; it only runs after boot completion. The module never writes the real vendor partition.

## Validation performed

- Built ARM64 and ARM32 with Android NDK r29, API 32, release optimization and static C++ runtime selection.
- All MPP imports from the tested Codec2, VPU, hardware JPEG and ARM32 Rockit consumer libraries remain available: **86 unique ARM64 symbols and 96 ARM32 symbols**. Stock/new MppApi sizes and versions match: ARM64 184 bytes, ARM32 128 bytes, version 0.
- Loaded both packaged libraries on the physical RK3588 Android 12 box in isolated processes. Existing system libraries remained active.
- Each ABI hardware-decoded 12 generated 640×360 frames for H.264 8-bit, HEVC 10-bit, AV1 8-bit and AV1 10-bit. Valid active-image output samples matched FFmpeg software-decoded references exactly in all eight cases. This establishes basic decoding, not 4K/8K throughput, HDR behavior or correctness on arbitrary media.
- Installer preflight passed on the tested vendor stack and rejected a different Android API. Android shell syntax checks passed. The helper validates library imports, not the entire Android Codec2 service.

## Rebuild

From the repository root, with CMake, Ninja, Git, Python 3 and a complete Android NDK:

```sh
python3 tools/build_mpp.py --ndk /path/to/android-ndk
```

Set `ANDROID_NDK_HOME` instead of `--ndk` if preferred. The script fetches the pinned MPP tag and verifies its revision, builds both architectures and probes, copies upstream license texts, and packages into `dist/`. Generated libraries and probes are not committed. Source paths are remapped in the compiled binaries. The vendor hash/symbol contracts describe the tested firmware; they must not be changed merely to bypass an incompatible installation.

MPP source: https://github.com/rockchip-linux/mpp/tree/c08762ebfadeb4e986d2fed993bc7a54862d3ebe

MPP uses Apache-2.0 or MIT licensing; both upstream texts are in `licenses/`. The module wrapper/helper uses this repository's Apache-2.0 license. The ZIP contains newly built MPP, not firmware vendor binaries, sample media, private logs, screenshots or device addresses.

## 中文

这是独立的面具实验模块，把当前 2023 年版用户态 `libmpp` 更新为 MPP 1.1.0，提供 32／64 位两套库。内核、Codec2、时钟和现有 LSPosed 修正保持原样。安装时核对当前测试固件的库指纹并检查加载与符号，其他固件会被拒绝。

安装 ZIP 后重启生效；回退时点“操作”或停用／卸载模块，再重启恢复原库。操作按钮不会立即替换播放器已经加载的库。启动后的加载检查失败会为下次启动停用模块，但它不是启动失败的自动救援工具。

两套库在盒子的独立进程中通过了 H.264、HEVC 10-bit、AV1 8／10-bit 的短样本硬解测试，输出与软件参考逐像素一致。真实播放器、系统覆盖后的 Codec2、编码功能、高分辨率性能和 HDR 仍待验证。
