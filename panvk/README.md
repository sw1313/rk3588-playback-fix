# PanVK RC3 Test8.1 · optional in-app Vulkan driver / 可选 应用内驱动

**Experimental, firmware-specific.** Test8.1 is the first public packaging of
local Test8, with updated documentation, licenses, source/build tools and
metadata. Its two runtime `.so` files are byte-identical to tested Test8.
It is imported in the **the tested application driver manager**, not installed in Magisk.
It does not replace Android's system GPU driver or modify the application's APK.

**实验版本，仅有限实测。** Test8.1 补齐本地 Test8 的公开说明、许可、源码构建工具和
元数据；两个运行库与已测试 Test8 逐字节一致。这是 **the tested app 导入的驱动 ZIP**，不是
面具模块，不改原版应用 APK、不替换系统驱动或 CSF 固件、不刷 boot。

## Changes / 改动

- Preserve the complete 64-bit GPU address of the provoking-vertex FBD helper.
  RC3 stored it in `uint32_t`; a live BO above 4 GiB then yielded an invalid CSF
  CALL target. The source fix is included. Its binary counterpart widens two
  AArch64 instructions in **G610 / PAN_ARCH10 only**, with SHA256, ELF and exact
  instruction/context guards. **This is not a complete Mesa rebuild.** The
  original ELF build ID remains; use the published SHA256 to identify this copy.
- Schedule vertex/tiler, fragment and compute subqueues at CSI0/1/2 in one CSG,
  instead of three independent CSGs. This is a userspace scheduling workaround
  for the remaining intermittent `CS_UNRECOVERABLE` / `VK_ERROR_DEVICE_LOST`.
  Cross-group waiting is a working hypothesis, not a proven firmware root cause.
  The adapter changes only this payload's ioctl/close imports, with reference
  counted group destruction and fd-close cleanup. Real errors propagate.
- Keep the API32 ROM-mapper bridge: real format/plane/offset/stride/modifier,
  one RGB plane, preserve AFBC; reject unsupported layouts rather than guess.
- Consume Android rendering waits once, wait for real GPU fence completion,
  then return completed-buffer fd `-1`. No fake completion or dropped rendering.
  This synchronous presentation bridge still has overhead even though the
  Test7 `PANVK_DEBUG=sync` submission diagnostic is removed.
- Set `PANVK_BC_AFRC=0` for exact BC decoding. Geometry shaders/multiple streams,
  resolution, textures and quality settings are not reduced.

地址修正解决了已确认的 32 位截断；源码补丁与两条指令的严格校验补丁均公开，未声称
完整重编译 Mesa。CSF 调度调整是对剩余随机闪退的规避方案，跨组等待仍属于待确认的
原因。保留真实绘制与完成等待、无损 BC 解码和 AFBC；不关闭 geometryStreams 或降画质。

## Scope and evidence / 范围与验证

Tested: RK3588 / Mali-G610, Android 12 API32, Linux 5.10.66, the already-enabled
matched Mali g18p0 kernel/CSF pair. System components stayed unchanged during
these PanVK trials. Compatibility with the factory kernel/CSF, other ROMs,
other Mali generations or all games has **not** been established. Changing
package minApi to 32 does not establish general API32 support.

- 4,480 offline emulated address/index/stride cases passed.
- 1,000 compiled C group-engine cycles / 9,000 create-bind-terminate operations
  passed, plus failure/retry/capacity/fd/thread isolation/close checks. These
  offline group tests use mocked kernel calls.
- 23 geometry and 17 transform-feedback cases passed on the box, including
  exact multiple-stream readback. Native AFBC import/render/readback/release
  and the deferred provoking-vertex regression passed.
- The uncorrected address regression returned DEVICE_LOST; the corrected one
  rendered the expected red/blue pixels. Ten additional real context lifecycle
  cycles passed alongside the game.
- The user confirmed characters/scenes now render and reported no crash in a
  short the tested title title/start/touch-overlay retest. This is **not** a long soak,
  all-game guarantee or final FPS benchmark.

实测只覆盖上述既有 g18p0 组合。Test6 仍发生过标题闪退；Test7 强制提交同步仍报
`0x41`，并发生次级诊断解码器崩溃，因此不发布这两个对照包。Test8 短测改善，但
长期稳定性仍待观察。当前该游戏临时使用 **JIT** 对照：NCE 停滞在系统驱动下也出现过，
属于另一个问题；全局 CPU 设置未改。本驱动 ZIP **不会**自动更改 应用的 CPU 后端，
也不包含游戏配置、游戏文件、密钥、存档或原始日志。帧率不是最终性能结论。

## Install and roll back / 安装与回退

1. Stop the game. In the tested application, open its GPU driver manager and import
   `rk3588-panvk-rc3-test8.1.zip`; select the Test8.1 entry.
2. Reopen the game. Keep your existing quality settings. CPU backend selection
   is separate; this ZIP does not configure it. Review the JIT limitation above.
3. To roll back, stop the game, choose **System GPU Driver**, and reopen it.
   No reboot, root-module removal or system-driver replacement is required.

退出游戏后，在原版应用的 GPU 驱动管理中导入并选择 Test8.1，再重新进入游戏。
回退只需退出游戏、改选“系统 GPU 驱动”再重开；不需要重启盒子或卸载系统模块。
现有解码模块、Yamby 和 LSPosed 均无需更新。

## Source, build and licenses / 源码、构建与许可

Project source: `panvk/` at release tag `v0.6.9`. Upstream RC3 revision and Mesa
pin are in `PROVENANCE.txt`. Original adapter code is MIT; upstream Mesa,
Khronos, AOSP, kbase UAPI and NDK material retain their original licenses.
See `LICENSES/`, root `THIRD_PARTY_NOTICES.md` and `SOURCE_CODE.md`.

Requirements: Python 3.10+, NDK **28.2.13676358**, capstone **5.0.9**, unicorn
**2.1.4**. Obtain the official **Android aarch64** core from the pinned release
and verify original SHA256
`b46b9f0416f207a05482349dffb37b6f3d84c20762151c623774831ce3939445`.
The glibc binary, lowmem variant or an arbitrary newer build will be rejected.

```sh
python -m pip install -r panvk/requirements-validation.txt
# Download this exact upstream asset separately (not proprietary firmware):
gh release download panvk-kbase-v0.1.0-rc3 --repo zenithblue-oss/panvk-kbase-android --pattern libvulkan_panfrost-android-aarch64.so --dir /inputs/panvk
python tools/build_panvk.py --payload /inputs/panvk/libvulkan_panfrost-android-aarch64.so --ndk /inputs/android-ndk-r28c --work /work/panvk-test8 --output /outputs/rk3588-panvk-rc3-test8.1.zip
```

Use a new empty work directory. The builder runs address emulation and the
compiled group-engine tests, builds the adapter, and rejects runtime hashes
that differ from the tested Test8 libraries. Exact runtime hash matching is
validated for the documented Windows NDK toolchain; a different host/toolchain
may differ and is not silently accepted. Offline success alone does not prove
device compatibility. This build never connects to ADB or alters a device.

To reconstruct the upstream core source, use upstream commit
`fd6ed91eff1c8d3a83307a2348b959bb93797011`, its `sources.lock` Mesa pin
`5a07217f034b3e50d8c7c7794f97a2df1742613b`, and its documented ordered patch
workflow (`g615-v11-csf` profile). Apply
`fix-provoking-vertex-address-width.patch` afterward. This is source context
for the fix; the public runtime is the explicitly documented binary counterpart,
not an assertion that this recipe produces the distributed core bit for bit.
