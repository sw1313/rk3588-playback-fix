# 同平台 SDK 多媒体栈移植：decode-stack 0.5.0

本次采用 Khadas Edge2 Android14 SDK 的 Rockchip Codec2 核心，向已验证的
Android12／API32 厂商 ABI 移植。保留现有更新后的 MPP／VPU 兼容库、RGA、
libyuv、软件解码与 HDR 适配层，不替换整个 Android14 BSP。

## 固定来源与实际启用范围

| 项目 | 来源／处理 |
| --- | --- |
| SDK | [Khadas 官方 Android 源码说明](https://docs.khadas.com/products/sbc/edge2/development/android/download-android-source-code)，`khadas-edge2-android14` |
| Codec2 | `khadas/android_vendor_rockchip_hardware_interfaces_codec2`，`076fe2f6ec28546d329964ae9e175301717df6ab`，源码版本 `1.14.3_[2]` |
| 核心构建 | 完整 factory、base、video、OSAL、store；针对 API32 编译和链接，未替换系统 C++ 库 |
| 实际路由 | 现有服务选择普通 AVC／HEVC／VP9／AV1 Surface 与显式 P010 输出；原有安全解码、编码和其他旧路径保留 |
| MPP | 保留 0.4.0 的 MPP 1.1.0＋已验证回移修正，32／64 位成对启用 |
| SDK 自带 MPP | 修订 `98b76bec`，2024-04-24；双 ABI 和 AVC／HEVC 十位隔离测试通过，但较现用 MPP 更旧，不回退、不分发其闭源固件库 |
| HIDL／注册表 | 复用已验证 API32 服务适配层与原厂 HIDL 实现，保留 21 个原始条目；没有将 SDK14 服务整体覆盖进 API32 |

完整构建 SDK 编码器源码不表示启用了新编码路径。显示 HAL、allocator／mapper、
内核、boot、GPU、Yamby 原版组件和 LSPosed 均保持原有状态。

同一份 SDK 的其他多媒体／显示组件也已核查：

| SDK 组件 | 核查结果与处理 |
| --- | --- |
| librga `a8546c7d9944` | 源码头文件报告 `1.10.1_[0]`，较现用已验证的 `1.10.6_[3]` 旧，保留现用版本 |
| bifrost gralloc `dab8cd95d590` | 包含 allocator 4.0 与 AIDL 路线及私有格式／capabilities；保持原 allocator／mapper，避免改变已验证的十位导入 ABI |
| drmhwc2 `041fe88def0b` | 构建与 gralloc、libdrm、PQ／SVEP 等组件联动，不能以单个“解码驱动”整体覆盖；本次保留原 HWC 与现有 HDR 适配层 |
| hwc3 AIDL `53dc88d6790e` | 服务依赖 `android.hardware.graphics.composer3-V2-ndk`，本机使用原 HIDL composer；没有连同新系统框架移植这一服务 |
| kernel-6.1 | 与实机原内核、板级配置和私有 ABI 不同，不加入模块 |

这些组件尚未通过在原系统上的整套移植验证；未将它们标为通用不可移植，也未
因 SDK 较新就直接启用。SDK 的“整套更新”包含系统、HAL 和内核联动，本模块
实际交付的是通过测试的用户态解码栈组合。

## API32 兼容改动

SDK 原本仅在较新首发 API 等条件下开放 P010。本移植只在现有且已验证的
Codec2 质量开关启用时开放 API32 P010，不伪造系统版本。

旧 gralloc 的缓冲区尾部空间和 SDK14 的 MPP 导入预期不同。复制输出使用
MPP 内部解码缓冲区，再按真实平面布局复制有效像素；线性十位使用有界的
packed-10→P010 转换，保留低两位。Surface 输出保留支持的直接路径；需要时
使用既有完整精度兼容复制。格式、尺寸和色彩 usage 改变会重新分配缓冲区。
AV1 Surface 复制也重新应用原生十位输出请求，避免仅显式 P010 测试通过而
实际 Surface 被截成八位。

沿用 API32 的 AFBC 选择与直接导入色彩标记，避免 SDK14 默认尺寸阈值和
gralloc usage 改变旧驱动的色彩采样。参数描述复用现有服务的 reflector，确保
HDR 信息不仅写入缓冲区，也能被标准 MediaCodec 接口正确描述和读取。

现有 HDR10 静态参数、标准 Codec2 HDR10+ 数据与显式清空逻辑移植到新核心，
避免新 SDK 覆盖先前的修复。没有加入八位回退、分辨率缩减、色调映射或强制增强。

## 内核 VPU 能否独立升级

实机 `CONFIG_ROCKCHIP_MPP_SERVICE=y`，VPU 引擎均为内建驱动，并非可卸载的
MPP `.ko`。当前内核为 5.10.66；SDK14 清单配套的是 kernel-6.1。
Android12 参考树 `f074637476d5cac891053339827c2bf568737a98` 的 22 个 MPP 顶层
C／头文件，以及 SDK14 树 `4d99945c7efd5500b8eedaa6633dbe8a75c15dab` 的 23 个文件中，
未发现独立 firmware loader 调用。该检查范围不能证明所有外围驱动都不使用固件。

参考树并非这台电视盒子内核的精确厂商源代码。不能仅凭相同版本号或 vermagic
就认定私有结构与寄存器接口一致。加载另一个模块也不会替换已内建的 VPU。
在“不更换系统、不更换内核”的要求下，本次不做卸绑运行中的 VPU、函数热替换、
AV1 时钟修改或 boot 写入。可独立部署的升级限于经现有内核 ioctl 验证的用户态层；
不声称因此新增硬件格式、杜比视界或 HDMI HDR10+ 能力。

## 开关与回退

```sh
su -c 'sh /data/adb/modules/rk3588_decode_stack/mode-control.sh codec2_sdk off'
# 重启：恢复 0.4.0 已验证的 Codec2 后端，其余升级保留
su -c 'sh /data/adb/modules/rk3588_decode_stack/mode-control.sh codec2_sdk on'
# 重启：重新选择 SDK14 移植后端
su -c 'sh /data/adb/modules/rk3588_decode_stack/mode-control.sh all off'
# 重启：恢复整包覆盖前的原系统组件
```

关闭 MPP、RGA 或 Codec2 时，新后端也随依赖停用，用户保存的选择保留。
已运行的进程不会被动态改库。固件哈希限制和一次性失败回退仍然生效。

## 验证记录

逐项结果、当前版本的人工确认与未覆盖范围以
[`VALIDATION.json`](../magisk-decode/VALIDATION.json) 为准。隔离的像素精度结果
不代替 MediaCodec Surface、HDR 输出或实际播放器测试，不复用旧版本的人工确认。
