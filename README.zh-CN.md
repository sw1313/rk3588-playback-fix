# RK3588 播放兼容修复

[English](README.md) · [下载安装包](https://github.com/sw1313/rk3588-playback-fix/releases/latest)

LSPosed 模块为所有芯片开放 Yamby 默认 HW+ 设置补全，其他 mpv 应用的强制 HW+ 仅限 RK3588。另有独立 Magisk 模块处理刷新率切换导致的整屏短暂黑屏。实体设备测试为 H96Max V58、RK3588、Android 12（API 32）。

## LSPosed 模块 0.3.1

下载安装 `RK3588-Mpv-Fix-0.3.1.apk`，在 LSPosed 启用模块并勾选 Yamby，打开一次模块设置，再强行停止并重开 Yamby。进入 **设置 → MPV 播放器 → 默认解码方式 → HW+**。

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
./gradlew -p lsposed :app:assembleDebug :app:lintDebug
python3 tools/package_magisk.py
```

Windows 使用 `gradlew.bat` 和 `python`。APK 输出在 `lsposed/app/build/outputs/apk/debug/`，ZIP 输出在 `dist/`。官方 Xposed API 82 仅用于编译，不打包进 APK。发布 APK 使用调试签名，签名私钥不上传。

APK 构建和 Lint 已通过；默认 HW+ 经应用重启后仍保留，之前绿屏的视频画面正常。Magisk 安装和 Android shell 语法检查通过；隔离设置存储测试覆盖备份、重复应用、手动修改保护、恢复未设置值、暂停／恢复、卸载恢复。完整设备重启后的自动应用待验证，其他播放器和固件未实测。

模块没有网络权限和遥测。`RKPlaybackFix` 日志只记录解码／渲染选项。仓库和附件不含设备日志、截图、网络地址、本机用户名路径或签名私钥。

第三方组件来源及许可说明见 [英文 README](README.md)。这里不分发 Yamby 或 mpv 的二进制文件。
