# RK3588 动态 HDR 与 HDR10+ HDMI 集成调查

> 后续实现：0.3.0 已补充 HDR10 静态元数据和标准 Codec2 HDR10+ 桥，
> 请以[实现与实测记录](hdr-metadata-030.zh-CN.md)为准。本文保留前期调查，
> 其中“尚未实现／未安装”的描述仅适用于调查阶段。


日期：2026-10-15。设备范围：已验证的 RK3588 Android 12/API32 固件、原版
5.10.66 内核。统一解码模块 0.2.0、Yamby 原生组件和 LSPosed 保持原样。
本次完成源代码核对、实机显示接口枚举和独立缓冲测试；没有安装显示补丁或重启。

## 结论

| 能力 | 本机证据 | 当前集成判断 |
| --- | --- | --- |
| MPP 动态 HDR 解析与元数据生命周期 | 0.2.0 已通过先前逐帧与引用计数测试 | 已集成；不能代替最终 HDMI 输出 |
| Rockchip Codec2 `hdrMetaCap` 私有路径 | 本机 mapper 不支持私有元数据偏移 | 保持关闭；改能力位不会补齐下游 |
| 标准 Codec2 HDR10+ 元数据输出 | API32 框架有标准接口；mapper 声明支持标准类型 | 可单独开发、验证；本次未实现，不宣称 HDMI 生效 |
| HDR10 静态元数据完整传递 | 内核有静态输出属性，原 HWC 未传完整数值 | 用户态修复候选；仍需 HWC 兼容实现和端到端验证 |
| HDR10+ HDMI 输出 | 原 HWC 不报告支持；缺少已核实的新动态输出属性 | 当前内核／显示栈下没有已验证的可集成方案 |

## 为什么不能只把 hdrMetaCap 改为 1

[公开 Rockchip Codec2 能力表](https://github.com/khadas/android_vendor_rockchip_hardware_interfaces_codec2/blob/khadas-edge2-android13/component/osal/C2RKChipCapDef.cpp)
中，RK3588 的 `hdrMetaCap=0`；RK3528／RK3576 的取值不同。这不是所有芯片通用的
“HDR 总开关”。相应代码仅在 Surface 模式启用 MPP `base:enable_hdr_meta`，申请
`GRALLOC_USAGE_DYNAMIC_HDR`，将元数据写入图形缓冲尾部，再把偏移通过
`rk.graphics.RkMetadataType` 的类型 1 交给 mapper，供匹配的 HWC 消费。

本机使用 mapper 4.0。实机枚举 22 种元数据类型，只有标准类型和 ARM 平面 FD 扩展，
没有上述 Rockchip 类型。为排除枚举不完整的可能，又分配了独立 64×64 RGBA 测试缓冲：

| 操作 | 返回 |
| --- | --- |
| 初始化缓冲 | 成功 |
| 读取标准 SMPTE2086、CTA861_3、SMPTE2094_40 | `NONE`，新缓冲内容为空 |
| 读取 Rockchip 动态 HDR 偏移 | `UNSUPPORTED`（7） |
| 设置 Rockchip 动态 HDR 偏移为 0 | `UNSUPPORTED`（7） |

测试缓冲从未提交显示，释放后不留下设置。标准元数据的完整写入／读回尚未测试。
因此，即使 Codec2 开启私有能力位，本机也没有可用的私有偏移传输接口。额外缓冲分配、
元数据边界／同步、复用时清理以及 HWC 消费方都需要配套，不能仅去掉条件判断。

## 标准元数据接口可以研究，但与 HDMI 输出分开验收

API32 `CCodecBufferChannel` 已支持 `C2StreamHdr10PlusInfo::output`，可将它附加到
`QueueBufferInput` 的 `HdrMetadata::HDR10PLUS`。本机 mapper 也声明标准
SMPTE2094_40 类型可读写。这提供了避开 Rockchip 私有缓冲尾部机制的用户态候选：

`MPP 每帧动态元数据 → Codec2 标准 HDR10+ info → BufferQueue／SurfaceFlinger`

需要处理一个具体格式差异：当前 MPP HEVC 解析器已消耗 T.35 国家码、提供方码、
提供方定向码和应用标识后，才保存 HDR10+ 的剩余数据。标准接口不能未经格式核对就
直接复制这些字节。应重建并校验标准载荷，验证 POC／PTS 对应、重排、seek／flush、
空元数据帧与缓冲复用，确认像素、色彩和 10 位低位不变。

这一路径尚未实现或加入模块。即便元数据到达 SurfaceFlinger，原 HWC 的
`hdr10plus=false`、`metadata=0` 仍是下游缺口。[Android HDR 平台要求](https://source.android.com/docs/core/display/hdr)
也明确涉及解码、显示合成和显示设备配合。

## 原内核缺少哪些接口

直接用 DRM GET ioctls 枚举本机所有 connector、CRTC 和 plane，得到：

- 有可写的 `HDR_OUTPUT_METADATA`，这是 HDR10 静态元数据接口。
- `NEXT_HDR_SINK_DATA` 为不可变接收端信息，当前 blob 为空，不能用它发送动态元数据。
- VP0 的旧 `FEATURE` 表含 `NEXT_HDR`，但没有新版 `DOVI` 功能项。这个标志不证明杜比可用。
- plane `EOTF` 范围为 0–5；没有新版使用的 HDR10+／Vivid／DOVI 扩展值。
- 未发现 `VSIF_DATA`、`HDR_EXT_DATA`、`DOVI_INPUT_TYPE`、`HDR10_PLUS_VSDB`。
- 显示端本次读取的 EDID 与先前记录一致，未宣告 HDR10+；可见 Dolby 厂商块不代表发送端具备杜比链路。

[较新版 Rockchip HDMI 源码](https://github.com/rockchip-linux/kernel/blob/95a3ad83f7f90e0e672b9c99b0858b4964318667/drivers/gpu/drm/rockchip/dw_hdmi-rockchip.c)
已有 `VSIF_DATA` 等接口，但这份新版源码不能当作本机已编入的功能。当前
`CONFIG_DRM_ROCKCHIP=y`，显示驱动编入内核；常规 Magisk 文件覆盖不能为现有驱动增加
这些接口。对 HDMI 寄存器直接写入也不能替代逐帧同步、热插拔和退出清理。

HWC 的先前播放日志显示静态 HDR 数值到达 SurfaceFlinger 后，HDMI mastering／
MaxCLL／MaxFALL 为零。[公开 Android 12 HWC 源码](https://github.com/TinkerBoard-Android/rockchip-android-hardware-rk35-hwcomposer2/blob/d6e29b4094898ef540a86890a78be69f33141c22/drm/drmconnector.cpp)
中的相似路径将输出结构清零后只设置 EOTF，与观察一致。它是修复线索，不能据此宣称
该公开源码与本机二进制完全一致。优先研究静态元数据传递，比强开无消费者的动态开关更具体。

## 本次交付范围

调查证据摘要在 `tools/hdr-probes/verified-api32.json`，探针源码和参数化构建脚本在同目录。
不分发原厂二进制、完整 EDID、个人日志或媒体。原始诊断数据仅保留本地。
已安装 0.2.0 运行文件和开关没有修改；此调查不增加未完成的 HDR 开关，也不改变能力上报。
