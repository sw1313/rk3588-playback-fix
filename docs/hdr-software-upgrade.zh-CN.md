# HDR 输出与系统软件解码升级

> 后续实现：0.3.0 已补充 HDR10 静态元数据和标准 Codec2 HDR10+ 桥，
> 请以[实现与实测记录](hdr-metadata-030.zh-CN.md)为准。本文保留前期调查，
> 其中“尚未实现／未安装”的描述仅适用于调查阶段。


范围：已验证哈希的 RK3588 Android 12/API32 固件，保留原系统、5.10.66 内核、
原版 Yamby 原生库和 LSPosed 0.3.3。测试信号和原始日志不随公开安装包分发。

## HDR10 与 HLG

实际播放 HEVC Main10 测试信号时，从 HDMI 驱动状态读取到：

| 阶段 | 位深 | 色彩 | EOTF |
| --- | --- | --- | --- |
| 普通桌面 | 8 位 | BT.709 | SDR／Off |
| HDR10 测试播放 | 10 位 | BT.2020 | ST2084 |
| HLG 测试播放 | 10 位 | BT.2020 | HLG |
| 退出测试 | 8 位 | BT.709 | SDR |

两段视频均完成暂停／恢复、拖动／flush 和 EOS。驱动状态证明系统已请求相应输出；
没有 HDMI 协议分析仪或显示端测量，因此不能用它证明线缆上所有字段、面板实际位深
和光学表现均正确。

HDR10 的 mastering／MaxCLL／MaxFALL 数据已到达 MediaCodec 和 SurfaceFlinger，
但 HDMI 驱动读取的这些字段全为零，HWC 的逐帧元数据能力也为零。这是原厂显示链路
的待解决缺口，不能把当前 HDR10 路径写成完整元数据透传已经通过。HLG 测试不要求
这组 HDR10 静态元数据。没有盲换 HWC／gralloc，也没有伪造显示能力。

## MPP 动态 HDR

回移 [MPP 上游 #966 的最终两个修正](https://github.com/rockchip-linux/mpp/issues/966)：
帧复制使用独立引用计数元数据，移除解析器旧元数据池，并补足 duplicate、自复制和
相同指针设置的安全处理。32／64 位各通过两万次生命周期测试。

公开合成 HDR10+ 样本有 12 帧，仅 POC 0／7／8 携带动态 SEI。旧版还在 POC 4／5／10
残留元数据，新版只在正确的三帧返回数据。HDR Vivid A／B 交替样本也只在携带 SEI
的帧返回元数据。未携带 SEI 时返回空是该接口的正确语义，不等于显示端动态映射失败。

## HDR10+ 与 Dolby Vision 的边界

后续已完成实机私有 mapper／DRM 接口测试：
见[动态 HDR 集成调查](hdr-integration-feasibility.zh-CN.md)。
该调查不改变下述 0.2.0 的已验证功能范围。

当前 Android／HWC 只报告 HDR10、HLG；HDR10+ 和 Dolby Vision 均未报告支持。
现有连接的 EDID 可见 Dolby 厂商块，但接收端宣告能力不等于盒子具备处理／输出能力。
较新公开 Rockchip Codec2 的 RK3588 专用动态 HDR 元数据能力仍关闭。现有 MPP 的
解析接口也不能代替 Codec2、图形缓冲、HWC 与 HDMI 的完整传递链。

因此：本版没有开启或宣称 HDR10+ HDMI 输出、Dolby Vision／LLDV／FEL 合成能力。
不通过强改能力列表、照搬其他芯片的杜比库、忽略增强层或强制转为 8 位来冒充支持。
当前约束下，尚未找到可验证、可安全集成的原生 Dolby Vision 启用途径。

依据：[Android HDR 平台要求](https://source.android.com/docs/core/display/hdr)、
[Rockchip Codec2 能力表](https://github.com/khadas/android_vendor_rockchip_hardware_interfaces_codec2/blob/khadas-edge2-android13/component/osal/C2RKChipCapDef.cpp)、
[Dolby Profile 说明](https://ott.dolby.com/OnDelKits/Dolby_Vision_Online_Delivery_Kit/v1/Documentation/Specs/Visio_Profiles/help_files/topics/c_notes_profile_public.html)。

## 软件解码

VP8／VP9 使用 libvpx 1.17.0 的解码后端；AV1 使用固定修订的新版 libgav1。
重编 API32 Codec2 包装层，保留 CFI，Surface 和明确请求 P010 的 10 位输出保持完整精度。
明确请求旧式 8 位复制输出的客户端保留原来的转换约定，避免将 10 位缓冲误标成 8 位。
Surface／P010 路径不启用降位转换；最高画质应使用 HW+ 或支持 P010 的客户端。
共享 libvpx 和 VP8／VP9 编码器保留原版，避免新版编码 ABI 影响旧消费者。
原厂禁用的 AV1 软件项随模块开关启用，硬解优先级保留。

原生 AV1 8 位、AV1 10 位、1080p AV1 10 位、720p VP9 10 位共
35,251,200 个样本与软件参考完全一致，10 位样本包含非零低两位。
真实媒体服务播放 AV1、VP8、VP9，并完成暂停／拖动／EOS。
这些系统升级不会替换应用自带的 mpv／FFmpeg。

最终显示日志还有一个边界：软件 AV1 的 PQ／HLG Surface 播放虽然分配了完整 10 位
P010，并触发 HDMI 10 位信令，原厂 HWC 却退到 AB24（8 位 RGBA）客户端合成。
这不能算软件 HDR 到显示端的完整 10 位验证通过。硬件 HEVC 测试则可见独立 YU10
视频平面。模块保留硬解优先，不强制把支持的硬解视频改为软件解码；最高画质仍应
优先 HW+。软件解码原始缓冲／明确 P010 复制的逐样本精度与最终显示合成精度分别记录，
不能用前者代替后者。HWC 合成和 HDR10 静态元数据属于仍待解决的显示链路问题。

实际重启、回退和最终版本测试以统一模块 `VALIDATION.json` 为准；
没有将短时自动测试写成所有视频、DRM、8K 或长期播放的保证。
