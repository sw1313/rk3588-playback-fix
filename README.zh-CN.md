# RK3588 播放兼容修复

当前解码模块为 [decode-stack 0.4.0](https://github.com/sw1313/rk3588-playback-fix/releases/tag/decode-v0.4.0)，
回移六项上游 MPP 解码修正并更新 libyuv 缩放实现，替代此前组件合集里的解码模块 0.3.2。
全局 Mali 和 Yamby／LSPosed 原版组件保留；具体升级范围与测试见[专项说明](docs/decode-upgrade-040.zh-CN.md)。

[English](README.md) · [当前系统组件安装包](https://github.com/sw1313/rk3588-playback-fix/releases/tag/system-v2026.10.15) · [LSPosed 发布页](https://github.com/sw1313/rk3588-playback-fix/releases/tag/v0.3.3)

此前系统组件合集提供全局 Mali 0.2.0、统一解码模块 0.3.2，以及保持原版的
LSPosed 0.3.3 和 Frame Guard 0.1.0。GPU／解码模块只接受各自契约中匹配的固件哈希，
不通用于所有 RK3588 ROM；统一解码模块替代旧独立解码模块，避免同时覆盖。
全局 Mali 首次安装需联网下载约 69 MiB 的固定上游库，校验后的设备本地缓存可复用。
公开包不包含闭源 Mali 库、设备原始日志、连接地址或签名密钥。系统模块以预发布形式提供，
安装前请阅读对应组件的适用范围、测试记录和回退方法。

LSPosed 模块为所有芯片开放 Yamby 默认 HW+ 设置补全，其他 mpv 应用的强制 HW+ 仅限 RK3588。另有独立 Magisk 模块处理刷新率切换导致的整屏短暂黑屏。实体设备测试为 H96Max V58、RK3588、Android 12（API 32）。

另有独立的 [MPP 1.1.0 用户态升级实验版](https://github.com/sw1313/rk3588-playback-fix/releases/tag/mpp-v0.1.1)，提供 32／64 位解码库，只接受当前已验证的 Android 12 厂商组件。0.1.1 修正了启动自检的路径误报，已在实体设备验证安装、重启和实际加载。与 Frame Guard、LSPosed 模块分开使用；安装范围、测试结果和回退方法见 [模块说明](magisk-mpp/README.md)。后续小样本 MediaCodec 测试中 H.264、AV1 8／10 位正常；原有 HEVC Main10 复制回内存路径问题仍未解决，这不代表完整播放器验证。

独立的 [RGA 1.10.6 用户态升级模块](https://github.com/sw1313/rk3588-playback-fix/releases/tag/rga-v0.1.0) 已按当前 Android 12 的系统 C++ ABI 和 gralloc4 接口编译，提供 32／64 位库。实体设备已通过真实 RGA 图像任务、Codec2 加载，以及停用后重启恢复原库的回退测试。模块包含严格的固件检查，不替换现有 TV 定制 Codec2，也不升级内核；具体适用范围和测试限制见模块说明。

另有独立的 [Codec2 系统兼容模块 0.1.0](https://github.com/sw1313/rk3588-playback-fix/releases/tag/codec2-v0.1.0)：只针对匹配哈希的 API32 固件，保留原厂组件库，改进 Surface、完整 10 位 P010、AV1 色彩范围与 HDR 静态信息。14 组系统播放测试、模式切换和重启回退已通过；不修改 Yamby 或增加 LSPosed 功能。原版 Yamby 的 HEVC Main10 HW 复制路径仍受其 P010 支持限制；最高精度播放继续使用 HW+。测试范围见 [系统升级说明](docs/decoder-upgrade-scope.zh-CN.md)。

## 合并的系统解码模块

[统一解码模块](magisk-decode/README.md)将 MPP／VPU 兼容升级、RGA 1.10.6、
Codec2 Surface／P010 修正、系统 libyuv 1974 和 libvpx／libgav1 软件解码后端合并为一个面具模块，保留分项开关、
依赖处理和整包重启回退。使用合并版时保持旧独立模块停用，避免重复覆盖。
Yamby 原版组件和 LSPosed 0.3.3 不变。完整测试结果及尚未纳入的组件见模块说明。
0.2.0 增加 MPP 动态 HDR 元数据生命周期修正和保持完整 10 位的系统软件解码升级。
HDR10／HLG 的实际输出与 HDR10+／杜比视界的能力边界见[专项测试说明](docs/hdr-software-upgrade.zh-CN.md)。

## 原厂 Mali 全局升级

[全局 Mali 模块 0.2.0](magisk-mali-global/README.md)覆盖系统标准图形路径的
32／64 位原厂 Mali g18p0，包括桌面和 SurfaceFlinger，不设置应用名单。
保留内核、boot、CSF 固件和原 allocator／mapper，提供全局／原厂开关及重启回退。
公开安装包从固定上游下载并校验闭源驱动，随包接口库均由 AOSP 冻结源码构建。
安装、精确固件范围与最终测试记录见组件说明。

原按应用实验现已被全局模块替代：

[独立 Mali 模块源码与测试记录](magisk-mali-native/README.md)支持已校验的 API32／5.10.66
固件，提供原厂 g15p0／g18p0 与原版切换。采用 Android 按应用驱动命名空间，
保留系统 GPU 库、CSF 固件、内核、Yamby、LSPosed 和解码模块；仅 ARM64 明确加入的应用生效。
真实窗口绘制、暂停恢复、Vulkan GPU 缓冲任务及运行时回退已验证，具体游戏验收见
该组件的 `VALIDATION.json`。固件二进制为用户提供的本地输入，此处不发布通用驱动安装包。

## LSPosed 模块 0.3.3

下载安装 `RK3588-Mpv-Fix-0.3.3.apk`，在 LSPosed 启用模块并勾选 Yamby，打开一次模块设置，再强行停止并重开 Yamby。进入 **设置 → MPV 播放器 → 默认解码方式 → HW+**。

已适配 Yamby **2.1.0.11（20010012）**，测试框架为 LSPosed **2.2.1**。Yamby 更新后可能需要重新适配。

Yamby 设置补全不限芯片；APK 要求 Android 8.0 及以上，并安装可用的 LSPosed。HW+ 能否播放具体视频取决于设备的 MediaCodec 实现和格式支持。代码已开放跨芯片使用，实体设备播放仅在 RK3588 上实测。Yamby 不会被强制改写解码方式，即使未来版本的设置适配失败也一样。

| 选项 | mpv 设置 | 含义 |
| --- | --- | --- |
| HW | `mediacodec-copy` | 硬件解码后，将帧复制回系统内存再渲染 |
| HW+ | `mediacodec` | 使用 MediaCodec Surface 路径，避免复制回系统内存 |
| SW | `no` | 软件解码 |

HW 和 HW+ 都使用芯片的硬件解码器。HW+ 通常减少复制开销，不代表画质本身更高。测试设备上，部分 HEVC Main10 视频在 HW 下绿屏，HW+ 下正常；具体厂商驱动缺陷尚未确定。

模块把 HW+ 加进 Yamby 自己的默认设置，使用原设置回调保存，并阻止播放器初始化时自动降回 HW。播放菜单仍可手动选 HW、HW+、SW。**模块不修改 `vo`，`gpu`／`gpu-next` 由播放器控制。**

其他 mpv 应用**仅在 RK3588 上**可加入 LSPosed 作用域并开启“其他 mpv 播放器：硬解优先使用 HW+”。模块匹配常见 MPVLib 字符串接口及支持的动态加载封装，把 copy／auto 硬解请求改为 `mediacodec`，保留明确的 SW 请求。仅 Yamby 实测通过，纯 native 或完全混淆接口需另行适配。其他芯片不安装这些通用 mpv 钩子，模块不用于 Android 系统进程。

## Yamby 顶部灰白边可选修正

0.3.3 的模块设置提供 **Yamby HW+ 灰白边自动裁剪修正**，默认关闭，仅限 RK3588。出现视频边缘填充行时开启，然后强行停止并重开 Yamby。

测试设备的解码器报告 `crop-top=4`：1920×1080 的有效画面位于 1920×1104 的硬件缓冲内。新版不再写死 1080p、4 像素或 H.264／HEVC 编码名单，而是通过 Yamby 的 mpv 日志回调读取当前 MediaCodec 裁剪信息与 AImageReader 纹理尺寸，并核对当前视频尺寸和非复制硬解状态。缺失、零偏移、无效或不匹配的信息不会启用修正。换片、播放器重建时清除旧片数据。

RGB 着色器使用实际报告的顶部／左侧偏移，同时兼容填充纹理和有效画面尺寸的中间纹理；执行条件支持 `gpu` 和 `gpu-next`，保留原有着色器和播放器的渲染器选择，着色器列表重置后自动恢复当前修正。此适配依赖 Yamby 提供相关详细解码／渲染消息；不同封装或未来消息格式变化可能需要更新适配。

这是渲染补偿，不是厂商驱动补丁；不放大画面、不改变宽高比，也无法恢复前面处理已经丢弃的边缘像素。此前已在 `gpu` 确认 1080p H.264／HEVC 修正有效，本次用户已确认自动版在 `gpu-next` 下消除灰线。720p／4K 与其他编码通过了元数据和着色器测试，不等于已经实测播放。仅针对已观察到的 RK3588 AImageReader 填充问题，不能处理所有类型的白边。

## Magisk 模块 0.1.0

用 Magisk 安装 `rk3588-frameguard-0.1.0.zip`，服务在开机完成后运行。仅允许 RK3588、API 32。

测试固件的 Rockchip HWC 反复拒绝 30／60Hz 切换请求，产生 `BadConfig` 和整屏黑屏。关闭自动内容帧率切换、固定请求为 60Hz 后，复现问题消失。模块设置：

```text
secure match_content_frame_rate = 0
system min_refresh_rate = 60
system peak_refresh_rate = 60
```

首次应用前备份原值到 `/data/adb/rk3588-frameguard/original.conf`。Magisk **“操作”** 按钮切换恢复／应用。卸载恢复仍由模块持有的值，保留之后的手动修改。**仅禁用模块不会撤销系统里已保存的值**，恢复请先点“操作”或卸载。固定 60Hz 会关闭按影片帧率自动切换。

## 构建、验证与隐私

0.3.1 使用实际入口代码和模拟 Android／Xposed 接口通过八组隔离作用范围检查，涵盖非 RK3588 的 Yamby 路由、通用钩子仅限 RK3588、未知适配、SW 和渲染器保留等。这不代表已在其他芯片上实测解码。新版已在 RK3588 测试设备上成功加载，用户确认正常 hook。

使用 JDK 17、Android SDK 35，配置 `ANDROID_HOME` 或未跟踪的 `lsposed/local.properties`：

```sh
./gradlew -p lsposed :app:assembleDebug :app:lintDebug :app:testDebugUnitTest
python3 tools/package_magisk.py
```

Windows 使用 `gradlew.bat` 和 `python`。APK 输出在 `lsposed/app/build/outputs/apk/debug/`，ZIP 输出在 `dist/`。官方 Xposed API 82 仅用于编译，不打包进 APK。发布 APK 使用调试签名，签名私钥不上传。

APK 构建和 Lint 已通过；默认 HW+ 经应用重启后仍保留，之前绿屏的视频画面正常。Magisk 安装和 Android shell 语法检查通过；隔离设置存储测试覆盖备份、重复应用、手动修改保护、恢复未设置值、暂停／恢复、卸载恢复。完整设备重启后的自动应用待验证，其他播放器和固件未实测。

模块没有网络权限和遥测。`RKPlaybackFix` 日志只记录解码／渲染选项。仓库和附件不含设备日志、截图、网络地址、本机用户名路径或签名私钥。

第三方组件来源及许可说明见 [英文 README](README.md)。这里不分发 Yamby 或 mpv 的二进制文件。

## HDR metadata integration 0.3.0

The unified decode stack now includes independently switchable HDR10 static
metadata propagation and a standard Codec2 HEVC HDR10+ metadata bridge. The latter
is not native HDMI HDR10+ output. Original Yamby and LSPosed remain unchanged.
See [implementation and measurements](docs/hdr-metadata-030.zh-CN.md)
and [unified controls/rollback](magisk-decode/README.md).

## HDR 切换修正 0.3.1

已移除独立 Dolby 移植实验、内核探针工具和相应开发报告。统一模块在已验证的
硬解直出路径修复同一 Surface 中 HDR10／HLG 输出模式残留，沿用 `hdr_static`
开关和重启回退。其他解码组件、Yamby 原版组件与 LSPosed 保持原状。
实现、测试与尚未修复的客户端合成限制见[0.3.1 记录](docs/hdr-transition-031.zh-CN.md)。

## 客户端合成修正 0.3.2

原生 SDR 客户端合成路径现在会清理源视频残留的 HDR 输出状态；可直接硬解的视频
继续保留十位 HDR。修正不新增降位转换，Yamby 和 LSPosed 保持原状。
见[实现与限制](docs/hdr-client-032.zh-CN.md)及[可升级组件核查](docs/decode-upgrade-audit-032.zh-CN.md)。
