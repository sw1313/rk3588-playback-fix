# 解码组件升级核查：0.3.2

核查日期：2026-10-15。固定原 Android 12/API32、5.10.66 内核和系统 ABI；
本轮实际运行文件的变化只涉及 HWC 修正层，下面的新增后端／上游补丁尚未安装。
Yamby 的原版组件和 LSPosed 0.3.3 保留。候选需要独立编译、接口校验、像素和播放
测试后才能合并；不能把找到提交写成已在盒子上兼容。

## 优先候选

| 部件 | 目前模块 | 新证据／可做内容 | 判断 |
| --- | --- | --- | --- |
| libyuv | `58d817ed81e8...`，1974 API 兼容构建 | 官方后续 `0b9e5c922dee...` 修正缩放函数有符号整数溢出，直接父提交就是当前版本 | 最小的升级候选；只涉及缩放实现和测试，不引入新 ABI；仍需原有 59 个兼容 API、JPEG 与十位缩放回归 |
| MPP 解码器 | 1.1.0 `c08762e`，已回移动态 HDR 生命周期修正 | VP9 概率更新、reset 后 extra data 重放、HEVC 首任务 PS/RPS 更新，以及 AV1 元数据边界修正 | 优先挑选相关修正；不把新增芯片、KMPP 编码功能或其他寄存器配置整批换入 |
| libvpx 软件后端 | 1.17.0 | 官方 main 有 VP8 线程重启失败、参考帧控制、尺寸／stride、损坏帧与线程错误处理等后续修正 | 保留当前共享库和编码器 ABI，只在现有 Codec2 解码包装内更新静态后端；须补损坏流、动态分辨率与 VP9 十位测试 |
| dav1d 可选 AV1 软件解码 | 尚未集成 | AOSP 已有 `C2SoftDav1dDec` 与 P010／HDR 静态元数据处理可参考 | 可以研究 API32 包装移植，作为独立可回退的软件后端；保留 MPP 硬解优先，不承诺 RK3588 上的具体加速幅度 |

libyuv 的修正位于 `source/scale.cc`、`scale_argb.cc`、`scale_common.cc`、
`scale_uv.cc`；本次源码差异没有改公共头或导出名。这是接口兼容的依据之一，
不代替原生执行和质量测试。

MPP 1.1.0 到本次 develop 取样 `14729dd578e5...` 的 GitHub 比较共 46 个提交，
其中最初两个动态 HDR 修正已经在模块内，不能再次计为新增升级。其余既有解码修正，
也有编码、测试和新芯片内容；共享上下文变化需要连同依赖审查。
AV1 RPU 缓冲边界修正是解析安全处理，不是重新开启 Dolby 输出。

## 保持当前版本的部分

- **RGA**：官方仓库当前仍为 1.10.6_[3]，与已验证组件一致。没有找到可确认更高版本。
- **Rockchip Codec2 Android 12 分支**：HEAD 仍是已使用的 `1e9d80a10b09...`。
  Android 13／14 分支存在，但依赖不同平台接口，适合挑选相关修正，不能整库替换。
- **libgav1**：已使用 `085032331f60...`，提交日期 2026-09-24，含 frame-buffer offset
  整数溢出修正。本次公开 main 取样为 2026-06-30 的 `c1deec657b32...`；没有取得
  可确认高于当前 pin 的候选，不用这个较早分支头覆盖现有版本。
- **libvpu／JPEG／IEP**：继续保留已经完成的 MPP 包装兼容与原有回归结果。
  没有核实可直接替换且更优的配套系统 ABI，不能仅凭同名库升级。
- **VDPP、gralloc、完整 HWC／Mali 驱动**：仍涉及厂商 ABI、内核驱动和设备能力配套。
  本次没有足够证据把其他固件里的整套二进制安全换入。0.3.2 的小范围 HWC 适配
  不等于显示栈整体升级，也没有恢复原系统八位 GPU 合成已经损失的精度。

## 直接来源

后续图形专项补充：原厂 Mali 已扩展为双架构全局 g18p0 用户态升级，系统合成器和
开关重启回退已验证，见[独立组件记录](../magisk-mali-global/README.md)。它保留原
allocator／mapper、CSF 固件和内核，不属于解码模块 0.3.2 的内容。

- [libyuv 缩放溢出修正](https://chromium.googlesource.com/libyuv/libyuv/+/0b9e5c922deec7c9921ae385f6242a9a27754d6e)
- [MPP 1.1.0 到 develop 比较](https://github.com/rockchip-linux/mpp/compare/c08762ebfadeb4e986d2fed993bc7a54862d3ebe...14729dd578e570e5f00fd1dd2113f5429012d64b)
- [MPP VP9 概率修正](https://github.com/rockchip-linux/mpp/commit/9db99ac77505)
- [MPP reset 后恢复 extra data](https://github.com/rockchip-linux/mpp/commit/c5b8620973c7)
- [MPP HEVC PS/RPS 更新](https://github.com/rockchip-linux/mpp/commit/8f88b50df0af)
- [libvpx main 修订记录](https://chromium.googlesource.com/webm/libvpx/+log/63bcd588b50bd514c4590edaeae3d70813233ae7)
- [AOSP dav1d Codec2 实现与 P010 路径](https://android.googlesource.com/platform/frameworks/av/+/104c252b9dca9849ea4d006f2b5e4903f7fd0db2%5E2..104c252b9dca9849ea4d006f2b5e4903f7fd0db2/)
- [RGA 官方版本说明](https://github.com/airockchip/librga)
- [Codec2 官方派生仓库分支](https://github.com/khadas/android_vendor_rockchip_hardware_interfaces_codec2/branches)
- [已使用的 libgav1 修订](https://chromium.googlesource.com/codecs/libgav1/+/085032331f60c10055d32ab52932bd7bd77e6f53)
