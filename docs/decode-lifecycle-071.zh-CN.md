# Decode Stack 0.7.1-test1：上游工作流修正与退出保护

这版将四项有具体提交依据的 Codec2 修正纳入现有 Decode Stack，并增加忙碌缓冲池的生命周期取消保护。独立开关 `codec2_lifecycle` 默认关闭，依赖 `codec2_sdk_next`；保存后重启生效。当前仍是限定验证的本地实验版本，未发布正式版本。

## 来源和实现

参考的是 [Lineage-Rockchip Codec2](https://github.com/Lineage-Rockchip/android_hardware_rockchip_codec2) 中由 Rockchip 作者署名的后续修复。其历史包含本项目使用的 `e3c30156793f...`。本版保留 1.14.16_[0] 核心和 API32/HIDL 适配，未整套移植新版 SDK。

| 提交 | 采用范围 |
| --- | --- |
| [894f5b7ca773](https://github.com/Lineage-Rockchip/android_hardware_rockchip_codec2/commit/894f5b7ca773c646758ba91fbdea6eb5370f17e4) | `takeBlock()` 转移 Surface 输出块所有权，清除旧包装内的持有引用 |
| [e577f69d04d7](https://github.com/Lineage-Rockchip/android_hardware_rockchip_codec2/commit/e577f69d04d7435a8e4a1088337ff2ed92a3a7e1) | 仅回移普通空输入 work 的完成处理；未混入其余初始化及 EOS pending 重构 |
| [f5e25f2fbf34](https://github.com/Lineage-Rockchip/android_hardware_rockchip_codec2/commit/f5e25f2fbf34ee41dbd083af6f58d6c6b22156cb) | 仅回移 flush 标志与 pending work 取出的锁排序，避免 flush 索引遗失；未混入 tunnel/drop 重构 |
| [9581b4ac11fa](https://github.com/Lineage-Rockchip/android_hardware_rockchip_codec2/commit/9581b4ac11fae9ee81467bec3cc542466593bee8) | 在 release handler 排队前设置 flushing 状态，返回后清理 |

本地配套实现让不断返回 `C2_BLOCKING` 的缓冲池在 flush/stop/release 后停止重试，并让输入端包队列在退出时停止等待。快速返回的忙碌池最多增加约 1ms 退避；API32 BufferQueue 已有的约 10ms 等待不再叠加完整退避。合法暂停没有固定超时，分配和 fence 同步保留。该机制只能在底层调用返回后检查取消，不能中断驱动内部永不返回的调用。

关闭开关保留以前的重试、空输入和所有权策略；这意味着旧空输入问题也会恢复。已有完整十位、HDR 元数据、AFBC 开关和原版应用继续保留。没有降位深、降分辨率、降帧率或修改播放器计数。

## 新版本专项验证

| 场景 | 结果 |
| --- | --- |
| 既有核心：先送 32 个普通空输入，再送视频 | 12 秒内只接受 21 个空输入，0 个真实视频包、0 帧输出；正常停止，没有伪造完成 |
| 新修复开启：相同空输入及 60 帧 AV1 十位 | 32 个空输入完成，60/60 真实帧，60 个图像，EOS、停止通过 |
| 新修复开启：空输入后 flush/seek | flush 前 12 帧，重新送入空输入及视频后 60/60 帧、EOS、停止通过 |
| 实际 Surface 客户端持有输出再 stop | AV1 十位 6 轮、AVC 八位 3 轮；9 轮均正常退出，stop 约 2.8–9.3ms，日志中实际触发取消保护 9 次 |
| UHD PQ HDR 十位精度 | 6 帧，74,649,600 个样本与独立软件参考完全一致 |
| 4096×2160 八位精度 | 3 帧，39,813,120 个样本与独立软件参考完全一致 |
| 控制层 | 15 项隔离 Android shell 断言、49 项 WebUI 交互断言通过 |
| 本地取消 helper | ARM32/64 各 16 项检查；模拟忙碌池收到取消后约 30–32ms 返回，关闭时等池在约 220ms 后可用 |

精度合计 9 帧、114,462,720 个样本完全一致。使用实际 AFBC GPU 消费端缓冲，经同步 RGA 解压、DMA 读取同步后对比同一媒体的 FFmpeg 参考；逐缓冲 crop 按 PTS 匹配。八位值按 uint8 与 uint16 数值比较，不做字节重解释。测试图形组合为 Mali Global 0.3.1-test1 的 g18 用户态、配对 next 内核/CSF，GPU 检测周期 20ms。

持有输出测试保留 MediaCodec 输出索引，直到显式 stop。单纯不读取 ImageReader 可能只导致异步丢弃旧图像，不能作为缓冲池已经耗尽的证据；该早期测试不计入上述 9 轮取消保护结果。

上述退出耗时没有对应旧版持帧性能 A/B，因此仅说明本次限定场景通过。模拟 helper 耗时也不是视频或游戏性能数据。本版没有通用 FPS 提升、所有格式/应用、完整 CTS/VTS、长期压力或 HDMI 线上精度结论。此前原版播放器 UHD HDR 的取帧超时未在本轮宣称解决。

## 开关和回退

在 Decode Stack WebUI 开启“Codec2 工作流与退出保护（实验）”，保存并重启。开启会补齐 MPP/RGA/Codec2 新核心依赖；关闭上级保留该项选择并暂时抑制生效。

```sh
sh /data/adb/modules/rk3588_decode_stack/mode-control.sh linked codec2_lifecycle on
# 重启后生效
sh /data/adb/modules/rk3588_decode_stack/mode-control.sh codec2_lifecycle off
# 重启后恢复此前工作流
```

实机已验证关闭并重启：同一核心恢复了仅接受 21 个空输入、无法继续真实视频包的旧行为；再次开启并重启后恢复 32 个空输入及 60/60 帧完成。最终开关已开启，无待重启。

此开关不撤销 AV1 纹理 AFBC 或 Mali 动态响应。整包停用/卸载并重启恢复原固件。不同的内核或 API/ABI 仍需独立验证；安装检查不会因回移补丁而放宽。

候选核心 SHA256：`36b7b3e385f725067412fcf5cb5405c06ecb2eb0e149512c70c4927b8950960f`。回退与最终状态见随包 `VALIDATION.json` 和 [结构化记录](validation/decode-lifecycle-071.json)。其他开源项目的筛选依据见 [上游核查](upstream-upgrade-audit-20261018.zh-CN.md)。原始日志、设备地址、媒体、固件原库及签名密钥不进入交付包。
