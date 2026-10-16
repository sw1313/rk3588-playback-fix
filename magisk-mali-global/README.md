# RK3588 原厂 Mali 全局升级 0.2.1

## v0.6.7 project licensing snapshot / 开源说明

Original code: MIT. This installer includes LICENSE, NOTICE,
OPEN_SOURCE.md, THIRD_PARTY_NOTICES.md and bilingual SOURCE_CODE guides.
Upstream licenses remain. This package update changes documentation and version
metadata only; executable payload, controls and rollback are unchanged.
本次只补许可、源码与构建说明，不改驱动或播放行为。详见仓库根目录开源说明。

面向已校验的 Android 12/API32、RK3588、5.10.66 固件。升级系统标准路径的
**32／64 位原厂 Mali 用户态库**，不使用应用名单，也不使用 Android 的按应用
驱动包。桌面、SurfaceFlinger、播放器与普通应用均通过全局系统路径使用 g18p0。
应用自行加载私有驱动或纯软件渲染的路径不经过这些系统入口。

[下载安装包与 SHA256](https://github.com/sw1313/rk3588-playback-fix/releases/tag/v0.3.2-system)。

| 部件 | 处理 |
| --- | --- |
| `/vendor/lib{,64}/egl/libGLES_mali.so` | 原厂 g18p0；Magic Mount 覆盖，不写真实分区 |
| 三份缺失的 AIDL 接口库，两种架构 | 从 AOSP 冻结源码编译，保留原系统 Binder／C++ ABI |
| 原有 Vulkan 入口、allocator／mapper 服务 | 保留，通过兼容性与加载验证 |
| 内核、boot、Mali CSF 固件、GPU 时钟 | 保留 |
| Yamby、LSPosed、统一解码模块 | 不修改其组件或功能；全局 GPU 下的播放另行回归 |

驱动报告 EGL 1.5、OpenGL ES 3.2、Vulkan 1.3.231。Vulkan 版本报告不等于 Android 12
加载器完整支持所有 Vulkan 1.3 命令／扩展，也不是完整 CTS 或性能提升证明。

## 安装、开关与回退

在面具安装 ZIP 后重启。安装前检查原始固件哈希、全部载荷，并对双架构执行独立
GLES 着色器绘制、像素回读和 Vulkan GPU 缓冲任务。原按应用模块会恢复其设置并停用。

面具 **“操作”** 在“全局 g18p0／原厂”之间切换，**重启后生效**。全局库不能在
正在使用它的系统进程中直接热换。也可从 root shell 执行：

```sh
sh /data/adb/modules/rk3588_mali_global/mode-control.sh status
sh /data/adb/modules/rk3588_mali_global/mode-control.sh off
sh /data/adb/modules/rk3588_mali_global/mode-control.sh on
```

禁用／卸载整个模块并重启也恢复原始系统文件。开机挂载前校验载荷；启动失败会
禁用全局覆盖并自动重启一次回到原厂。若一次启动未完成验证，下一次启动自动选择
原厂，避免反复加载未验证的驱动。原始库与下载缓存仅保存在设备本地的 root 私有目录。

## 公开包为什么需要下载

公开 ZIP 不重新分发许可未核实的闭源 Mali 固件库。安装时通过 HTTPS 从 Khadas
公开仓库的固定提交下载 ARM32／ARM64 两个库，验证预期大小与 SHA256，再运行自检。
下载器使用固定通用 User-Agent，不发送设备日志、账号、序列号或安装应用列表。
**首次安装需要网络**，下载合计约 69 MiB；已校验缓存可离线复用。任一下载或检查失败，
安装中止，系统库保持原状。安装器的真实电视盒子下载路径已验证。

三份随包的接口库均从 AOSP `android-13.0.0_r1` 冻结 AIDL 源码构建：

- `android.hardware.common` V2。
- `android.hardware.graphics.common` V3。
- `android.hardware.graphics.allocator` V1。

它们是接口定义与封送实现，不替换 allocator 服务、mapper、系统 Binder 或 C++ 运行库。
源码、冻结哈希、NDK C++ 支持头与 Apache-2.0 许可随包，未使用虚假的成功返回或空服务。

## 测试范围

最终测试记录以 `VALIDATION.json` 为准。已覆盖的自动检查包括双架构独立加载、
GLES 绘制／回读、原有 Vulkan 入口及 GPU 缓冲回读、系统合成器的 g18p0 版本，
以及与原系统接口和解码组件的哈希一致性。真实窗口、视频、HDR、十位精度与
用户游戏／播放器确认分别记录，不用原按应用版的游戏确认替代全局版确认。

全局运行下，14 项硬件解码的测试帧与启用前逐像素一致，4 项系统软件解码通过；
12 项 HDR10／HLG／SDR 设备与客户端合成切换的 HDMI／DRM 状态符合预期。
HEVC P010 输出与统一解码模块 0.3.2 基线逐字节一致，保留非零低两位精度。
客户端原有八位 SDR 合成路径仍为八位，本模块不声称把它升级为完整十位 HDR 合成。

未声称完成所有应用、所有 Vulkan 扩展、完整图形 CTS、长时间温度或功耗验证。
测试固件原有 SELinux Permissive 状态不变，不为加载新版库降低安全设置。

## 构建

`tools/build_mali_global.py` 接收 NDK、SDK、API32 平台头、原系统 Binder／C++ 链接输入
和新的工作目录，构建两种架构的全部开源接口库、绘制探针及 TLS 下载器。
默认 AIDL 编译器为 SDK Build Tools 34.0.0，DEX 工具为 35.0.0。固件库仅是链接输入，
不会复制到公开输出；闭源驱动的来源、大小和哈希固定在 `contracts/driver-sources.txt`。
编译器路径经过映射，避免断言字符串或调试信息泄露构建机器目录。

直接来源：

- [Khadas 原厂 Mali 固定提交](https://github.com/khadas/android_vendor_rockchip_common/tree/6cf5e034b39c19dfd9e1a15ae5227f61c127d128/gpu/MaliG610)
- [AOSP common V2](https://android.googlesource.com/platform/hardware/interfaces/+/android-13.0.0_r1/common/aidl/aidl_api/android.hardware.common/2/)
- [AOSP graphics.common V3](https://android.googlesource.com/platform/hardware/interfaces/+/android-13.0.0_r1/graphics/common/aidl/aidl_api/android.hardware.graphics.common/3/)
- [AOSP allocator V1](https://android.googlesource.com/platform/hardware/interfaces/+/android-13.0.0_r1/graphics/allocator/aidl/aidl_api/android.hardware.graphics.allocator/1/)

2026-10-15 用户已确认全局版桌面、《传奇四叶草》主页／战斗／退出重进，以及原版
Yamby HW+ 常用十位视频暂停／拖动／重进均正常。确认后再次核对全局驱动、
系统合成器及 GPU／解码覆盖哈希。补充验收记录见发布页附件与本目录
`VALIDATION.json`；原安装包与运行库不变，无需重新安装或重启。
