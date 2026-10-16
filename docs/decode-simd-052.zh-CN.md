# decode-stack 0.5.2：十位转换加速与格式实测

> 历史研发阶段记录；当前发布与最终 AV1 播放结果见 [0.6.3 报告](decode-av1-063.zh-CN.md)。

## 改动与边界

目标设备为经哈希检查的 H96 Max V58 / RK3588、Android 12 API32、原有
5.10.66 内核。优化位于 Codec2 的 packed ten-bit/NV15 → P010 输出转换，
同时覆盖 SDK 核心与旧核心的对应实现。MPP、RGA、软件解码器及 HDR 桥保留
0.5.1 已验证版本。Yamby 原版组件、LSPosed 0.3.3、全局 Mali g18p0、
Frame Guard、boot、内核和硬件时钟不变。

- **SIMD**：NEON 每批将 20 字节解包为 16 个十位样本，读写边界明确；不足
  一批的尾部使用标量处理，不跨行或跨保护页读取。
- **批量写入**：先在 64 字节对齐的可缓存行缓冲中完成转换，再将可见行整体
  `memcpy` 到映射后的 gralloc 缓冲，避免逐像素写入这类映射的开销。
- **缓存与同步**：保留调用方 mapper 的 lock/unlock、DMA 同步及原来的
  buffer usage/format。没有通过跳过 fence 或缓存一致性操作换速度。
- **画质**：保留全部十位有效值，不抖动、不截成八位，不改变分辨率、帧率、
  码率、色彩范围或 HDR 元数据。关闭优化仍使用原来的完整十位标量转换。

实现源在 [RKPacked10.h](../tools/pixel-conversion/RKPacked10.h)，两套构建器
检查补丁里的头文件与该源一致。独立精度测试和实时 Surface 诊断的源代码
分别在 [pixel-conversion](../tools/pixel-conversion/README.md) 与
[decode-performance](../tools/decode-performance/README.md)。

## 性能对照

同一设备、同一份 3840×2160 60fps 十位 AV1 输入，比较同一新二进制的开关：

| 测试 | 关闭优化 | 开启优化 |
| --- | ---: | ---: |
| ARM32 可缓存内存转换，12 帧均值 | 22.256 ms/帧 | 7.361 ms/帧 |
| ARM64 可缓存内存转换，12 帧均值 | 20.812 ms/帧 | 6.786 ms/帧 |
| 解码与非限速 Surface 输出，360 帧 | 8.612 秒 / 41.80fps | 5.319 秒 / 67.68fps |

两边均收到全部 360 帧及 EOS，停止成功。以上是有界样片测试，不代表所有
片源、长时间负载或流媒体应用的性能。测试信号本身的最早版本没有 HDR
transfer 标记，因此上述对照只证明十位转换性能；HDR 另用明确标记的信号验证。

ARM32/ARM64 各 140 项独立测试覆盖开关两条路径、全部 1024 个样本值、
非整批宽度、步长、非对齐输出、保护页、padding 和非法输入拒绝。原生
AV1/HEVC/VP9 十位输出与软件参考逐样本一致；14 组 Surface 回归截图与
0.5.1 的 RGB 像素完全一致。这些检查不能代替对所有真实片源的视觉确认。

## 不同格式与实景样片

实景源来自 Xiph 的 [Aerial](https://media.xiph.org/video/derf/webm/Netflix_Aerial_4096x2160_60fps_10bit_420.webm)
和 [RollerCoaster](https://media.xiph.org/video/derf/webm/Netflix_RollerCoaster_4096x2160_60fps_10bit_420.webm)。
虽然文件名含 `10bit`，下载的 WebM 预览经探测实际为 **VP9 Profile0 八位 SDR**。
下面十位转码用于检验格式和负载，不将八位升位伪称为原生十位 HDR 画质测试。
媒体及设备原始日志不随模块发布。

异步 MediaCodec 按时间戳调度 Surface，首帧预留 200ms；记录完整帧数、EOS、
输出超过目标时刻 20ms 的次数及停止结果。厂商 render callback 有可能回传
请求时间戳，不能只凭其间隔整齐就判断流畅。

| 样片路径 | 分辨率 / 帧率 | 结果 |
| --- | --- | --- |
| 两段原始 VP9 八位实景 | 4096×2160 / 60 | 各 1200 帧完整，输出迟到计数 0 |
| AVC 八位，HEVC 八位/十位，AV1 八位，VP9 十位实景转码 | 4096×2160 / 60 | 各 360 帧完整，迟到计数 0 |
| AV1 十位实景转码 | 3840×2160 / 60 | 360 帧完整，迟到计数 0 |
| AV1 十位实景转码 | 4096×2160 / 60 | 360 帧完整，但 238 帧迟到，最迟约 646ms，仍可能卡顿 |
| VP8、MPEG-2，系统软件 MPEG-4 | 1920×1080 / 60 | 各 360 帧完整，迟到计数 0 |
| H.263 | 352×288 / 30 | 180 帧完整，迟到计数 0 |
| AV1 十位测试图案、明确 BT.2020/PQ 与 BT.2020/HLG 标记的测试信号 | 3840×2160 / 60 | 各 360 帧完整，迟到计数 0 |

共 16 条可用路径全部解码完整并成功停止，其中 15 条无上述迟到，1 条仍有
迟到。4096 宽十位 AV1 同片纯 MPP 测得约 60.53fps，吞吐余量很小，输出
转换及片源复杂度仍可能影响实时性。这不是已证明无法继续优化的硬件极限，
也不能保证所有 3840 宽片源都能稳定 60fps。模块没有为此强行裁切或降帧。

额外探测的 `c2.rk.m4v.decoder` 初始化失败，但它并未出现在 Android 实际
Codec 列表中，原厂回退也没有该入口；不能据内部组件名认定系统支持此硬件
入口。已使用系统实际可选的 MPEG-4 软件解码器完成上述测试。

原厂与新模块的 AV1 能力查询均存在旧框架不识别 level、尺寸范围回报失真的
问题（查询只有 512 上限，实际可解 4K）。这是回退对照确认的既有能力描述
问题，本版未修改其通告；它不等同于本次实测的输出转换耗时。

本地样片排除了网络饥饿，但 SmartTube 的实际网络流仍有明显丢帧。
同一 COSTA RICA、AV1 itag701、3840×2160@60 HDR、约 26.16Mbps，优化开启时
一组约十秒截图的累计 dropped/rendered 从 1992/610 变为 2632/705，新增
640 个丢弃、95 个呈现；缓冲从 23 秒变为 16 秒，播放器为 ready。
因此本版只确认转换性能提升，**没有修复该实际播放器场景的流畅度**。
不能以本地 Surface 的测试结果覆盖这一失败。转换线程约占半个 CPU 核，
采样周期中约 19% 落在内核缓存 clean/invalidate，约 13% 落在 memcpy；
这些占比提示后续调查方向，不足以单独证明最终丢帧原因。

## 后续升级候选，尚未集成

公开 Khadas `khadas-edge-2l-android14` 分支的
[`e3c30156793f`](https://github.com/khadas/android_vendor_rockchip_hardware_interfaces_codec2/tree/e3c30156793f)
报告 Codec2 **1.14.16_[0]**，比本版使用的 1.14.3_[2] 核心多 153 个提交，
最新提交日期为 2025-10-11。值得隔离兼容测试的改动包括：

- [可缓存缓冲及显式 DMA 同步](https://github.com/khadas/android_vendor_rockchip_hardware_interfaces_codec2/commit/ba27dd90a517f8aba1127cf8853cef0fddba5668)。
- [输出延迟通告修正](https://github.com/khadas/android_vendor_rockchip_hardware_interfaces_codec2/commit/8ce7e526e604214b506bc7ba2aec93c57f6a3e76)。
- [标准输入输出 work 流程](https://github.com/khadas/android_vendor_rockchip_hardware_interfaces_codec2/commit/4a270d77986a9c487cdcbf5a4c89941377920b0b)。

这些是待验证的移植候选，不是本版已生效功能，不能保证解决 SmartTube。
须保留 API32/HIDL 适配、完整十位精度、HDR 桥和独立回退，再验证旧内核
分配器、缓存同步与停止排空。[上游十位 RGA 加速改动](https://github.com/khadas/android_vendor_rockchip_hardware_interfaces_codec2/commit/8719e432f908)
对 P010 仍保留 CPU 转换，其他分支使用八位 NV12，不能作为十位无损加速直接照搬。
真正绕过 CPU 解包还需证明该固件的硬件输出及 gralloc/HWC 能完整兼容十位
输出格式，当前没有通过此项验证。现有 MPP 已是 2026 年的 1.1.0 加相关
后续修复；尚未发现把最新版整库替换即可解决此场景的证据。

## HDR 与回退

六次 HDR10/HLG/SDR 与客户端合成切换通过，实际 HDMI EOTF 及颜色状态符合
预期；三组标准 HDR10+ 元数据传递/逐帧清除检查通过。该桥不代表新增电视
端 HDR10+ HDMI 协议或 Dolby Vision 能力。

```sh
# 关闭转换加速，仍保留完整十位画质；重启生效
su -c 'sh /data/adb/modules/rk3588_decode_stack/mode-control.sh p010_fast off'
# 恢复转换加速；重启生效
su -c 'sh /data/adb/modules/rk3588_decode_stack/mode-control.sh p010_fast on'
# 整包回到固件原始组件；重启生效
su -c 'sh /data/adb/modules/rk3588_decode_stack/mode-control.sh all off'
# 按保存的组件选择重新启用；重启生效
su -c 'sh /data/adb/modules/rk3588_decode_stack/mode-control.sh all on'
```

保存开关不会热替换运行中进程的库。关闭 Codec2 时有效加速开关也被抑制，
但用户选择会保留。完整安装包沿用组件校验、依赖检查和一次性失败回退。
最终构建哈希及实际回退结果记录于 [VALIDATION.json](../magisk-decode/VALIDATION.json)。
