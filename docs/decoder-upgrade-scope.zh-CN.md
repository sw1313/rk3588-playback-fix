# 当前固件的解码升级范围

保留 Android 12/API 32 和 5.10.66 内核。Yamby APK、内置 mpv、FFmpeg 保持原版，LSPosed 0.3.3 不增加功能。系统用户态组件依据实际兼容性测试决定启用，优先完整位深与正确色彩。

| 组件 | 当前处理 | 验证边界 |
| --- | --- | --- |
| MPP 1.1.0 | 保留已安装升级模块 | 32/64 位加载及解码；不新增内核缺少的能力 |
| RGA 1.10.6 | 保留已安装升级模块 | 图像任务、Codec2 加载及重启回退；用户已确认 HEVC 10 位 HW+ |
| Codec2 | 新增独立 Magisk 模块 0.1.0 | 普通 AVC/HEVC/VP9/AV1 按 Surface/P010 输出选择新组件；旧式复制输出保留原组件 |
| Android P010 兼容层 | 与 Codec2 模块一同开关的六处精确补丁 | ARM32/64 描述符与无损复制兼容，安装时由设备原库生成 |
| libvpu.so | 候选版本未安装 | H.264/HEVC 10 位原生解码正确，但相机/JPEG 实际调用兼容性未完成；JPEG 探针原库/候选都失败，不能算通过 |
| IEP/IEP2 | 已包含于当前 MPP 构建 | 不再叠加旧独立库 |
| VDPP | 未启用 | 内核设备、ioctl 和实际路径未全部验证 |
| gralloc HAL、GPU、媒体 APEX、内核 | 保留原版 | 不直接移植其他 Android 版本或设备的驱动 |

MPP、RGA、Codec2 是独立模块。Codec2 严格匹配当前 MPP/RGA 和固件 ABI，未知版本拒绝安装。详见 [模块说明](../magisk-codec2/README.zh-CN.md) 与 [实际测试记录](../magisk-codec2/VALIDATION.json)。

## 最终结构

此前额外注册一个 HIDL HAL 实例的实验未通过启动注册，已停用并移除。新模块保持原服务实例、原厂组件库、现有 HIDL 库和 VINTF 声明，增加候选解码组件并在服务内选择。普通 Surface 使用候选组件；明确请求 P010 的复制客户端使用完整 10 位转换；旧式复制客户端保留原组件。策略在创建解码器时确定，不在当前视频中切换。

候选基于公开 Android 12 Rockchip Codec2 固定修订，处理 P010、AV1 颜色初始化、Surface AFBC、AV1 线性 10 位 Surface、色彩范围和 HDR 静态信息。没有全量移植 Android 13/14 组件。服务复用此固件私有 HIDL ABI，必须匹配原库哈希，不能称为所有 RK3588 的通用模块。

## 实测内容

- 独立组件覆盖两种 ABI、AVC、HEVC Main10、VP9 10 位、AV1 8/10 位，以及 854×480、720p、1080p、4K 的 HEVC 10 位。可见像素与软件参考逐样本一致；历史失败与修复后复测分别保留。
- Android MediaCodec P010 输出完成 12 帧和 EOS。4,147,200 个可见样本最大误差 0，其中 1,884,604 个样本最低两位非零。正式安装后的缓冲输出与该路径一致，没有降为 8 位。
- 同一解码器连续处理 640×360、854×480、720p、1080p、4K，共 33 帧、72,118,080 个可见样本，输出与软件参考完全相同。
- 14 组 Surface 播放测试包含暂停/恢复、flush/拖动、EOS 和画面对照，安装重启后全部复测通过。包含 AVC/HEVC 8 位、HEVC/VP9/AV1 10 位、AV1 8 位、AV1 全范围、HEVC BT.2020/PQ HDR、4K。
- 常规画面与原厂一致；AV1 全范围传递与软件参考对照改善。HDR 静态信息正确传递，SDR 不附带空的 HDR 数据。截图只作画面证据，位深用原始数据验证；HDR 元数据不等于所有显示器的 HDR 握手或 HDMI 输出位深已验证。
- H.264、AV1 8/10 位旧式复制输出与原厂输出校验一致。最终还补测 VP8、MPEG-2、H.263 及固件原有软件 MPEG-4，14 组均通过；H.264/HEVC 编码后解码得到的像素与原厂一致。完整保留原厂 21 个工厂入口，安装和启动逐项检查；漏项故障注入触发自动回退通过。
- 兼容/最高画质模式切换、完整停用重启回退、重新启用重启通过。服务、配置、六处补丁及保留的平台库共 19 个文件哈希匹配预期。
- Yamby 保存的 9 个原生库与应用现有库逐项 SHA-256 一致，LSPosed 仍为 0.3.3。用户于 2026-10-15 已确认：原版 Yamby 的 HW+ 播放常看 HEVC 10 位及另一段视频，画面、声音、边缘和暂停/拖动/退出重进均正常；使用其常用渲染设置，未指定具体 gpu/gpu-next。

## HW 复制输出的客户端限制

原固件的 HEVC Main10 复制输出会返回全零数据，而 HW+ 正常。新系统组件能把紧凑 10 位转换为 P010，但调用方仍要请求、接受这种格式。原版 Yamby 的 FFmpeg MediaCodec 复制分支不接受 P010；在不改应用、不丢失位深的约束下，不能承诺单靠系统模块修复它的 HEVC Main10 HW 模式。HW+ 保留完整精度路径，没有添加 10→8 位转换。

默认选择最高已验证兼容的路径；不支持的应用或文件可以切兼容模式，完整回退则停用模块并重启。长期播放、所有应用/文件、DRM、安全解码、相机、录像、8K、杜比视界、HDR10+ 未全部验证，不承诺零新问题。

## 公开源码依据

- [固定 Android 12 Codec2 基线](https://github.com/khadas/android_vendor_rockchip_hardware_interfaces_codec2/tree/1e9d80a10b09c9a6e0bf5cc1dbe4b6d3e66dbc2b)
- [Android 13 Rockchip 10 位转换](https://github.com/khadas/android_vendor_rockchip_hardware_interfaces_codec2/blob/khadas-edge2-android13/component/osal/C2RKMediaUtils.cpp)
- [FFmpeg MediaCodec 解码](https://github.com/FFmpeg/FFmpeg/blob/master/libavcodec/mediacodecdec_common.c)
