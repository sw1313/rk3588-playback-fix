# RK3588 播放兼容修复

[**当前下载与版本表**](DOWNLOADS.md) · [GitHub 下载导航](https://github.com/sw1313/rk3588-playback-fix/blob/main/DOWNLOADS.md) · [English](README.md)

**[项目整套版本 v0.6.6 — 四个安装包齐全](https://github.com/sw1313/rk3588-playback-fix/releases/tag/v0.6.6)**

每个公开 Release 都是完整项目快照，附齐 Decode Stack、全局 Mali、Yamby/mpv 插件
和 Frame Guard。项目版本标识这次整套发布，包内继续保留各组件真实版本号。
GitHub **Latest** 指向最近更新的整套版本。系统组件仍为实验性质，只接受契约匹配的固件。

| 组件 | 当前版本 | 用途 |
| --- | --- | --- |
| [Decode Stack 解码合并模块](https://github.com/sw1313/rk3588-playback-fix/releases/tag/v0.6.6) | **0.6.6，实验组件** | 中英切换并记忆语言；版本单选、依赖联动与回退保持 |
| [原厂 Mali 全局模块](magisk-mali-global/README.md) | **0.2.0，实验组件** | 32／64 位系统标准图形路径使用原厂 g18p0 用户态驱动 |
| [Yamby／mpv LSPosed 插件](https://github.com/sw1313/rk3588-playback-fix/releases/tag/v0.6.6) | **0.3.4** | 默认 HW+ 设置与可选灰白边修正；这是插件版本，不是 LSPosed 框架版本 |
| [Frame Guard 刷新率模块](https://github.com/sw1313/rk3588-playback-fix/releases/tag/v0.6.6) | **0.1.0** | 可选的固定刷新率黑屏规避 |

## 四组件英文支持 · v0.6.6

Decode Stack **0.6.6** WebUI 与 LSPosed 插件 **0.3.4** 设置页新增 **English／中文**
按钮，均可记忆语言，切换不改变播放配置。Mali 与 Frame Guard 原有模块信息、安装、
Action／命令行提示已是英文，保留原包。[四组件覆盖与切换入口](docs/localization-066.md)。

Codec2 版本下拉单选、依赖层级、灰色保留选择和重启回退保持。解码二进制、钩子逻辑
和 Yamby 原版组件不动，LSPosed 仅改设置显示语言。

## Decode Stack 0.6.3

Decode Stack **0.6.3** 补齐有保护条件的无损 AV1 AFBC 显示输出，保留源位深、
HDR 和胶片颗粒，保留分项开关和整包回退。最终二进制在同一 60 秒本地 4K60 HDR
片源上渲染全部 3597 帧、播放器零丢帧，收到全部 3597 个呈现回调、没有超过 25ms
的间隔。另有同路径原型的 314 秒长片测试，未混记为最终二进制的整片测试。
详见[播放对照、精度与适用边界](docs/decode-av1-063.zh-CN.md)。

## 安装与历史版本

安装包文件名、直接下载链接和校验值统一见 [DOWNLOADS.md](DOWNLOADS.md)。
本次更新 Decode Stack 与 LSPosed 插件的语言显示；Mali 与 Frame Guard 使用已发布的原包，
不跟随解码模块更改版本号。旧系统合集包含的解码模块是 **0.3.2**，已不是当前版本。
旧版保留用于回退和核对历史，不再作为当前安装入口。

升级系统解码时，在 Magisk 安装统一 Decode Stack ZIP 并重启，保持旧独立
MPP／RGA／Codec2／libyuv 模块停用，避免重复覆盖。安装器先检查依赖，
再停用重叠模块。[分项开关、依赖与整包回退](magisk-decode/README.md)继续保留，
选择在重启后生效。

解码和全局 Mali 模块只接受契约匹配的固件哈希。实测设备为 H96Max V58、
RK3588、Android 12／API32、原 5.10.66 内核。不刷 boot，不替换内核，不改 Yamby 文件，
LSPosed 仅增加设置语言切换，不增加播放钩子。全局 Mali 首次安装联网下载约 69 MiB 的固定上游库并校验 SHA256；
公开 ZIP 不包含闭源 Mali 库。原按应用 Mali 实验已被全局模块替代，使用全局模块时保持旧实验停用。

## 0.5.1 更新与已知限制

通过受保护的 SDK14／API32 后端选择，处理已复现的 AVC／HEVC 八位缓冲区输出
停止／排空崩溃；新增 `codec2_buffer` 分项开关，保留 SDK 后端开关和整包回退。
Surface、P010、完整十位样本、分辨率和码率保持原样。未知／十位缓冲区 profile
及 AV1／VP9 继续使用原有选择，不代表所有 HW 复制路径都已修复。
详见 [0.5.1 原厂对照、修改范围与测试](docs/decode-buffer-051.zh-CN.md)。

安兔兔 12.0.2 的完整剪辑流程仍有提前 EOS／完成条件问题；原厂解码栈在受控源回调
调度压力下也能复现。测试界面结束不代表保留全部 450 帧，新版曾观察到 449 帧结果。
本次没有加入全局忽略 EOS、补帧或修改分数的规避。见[完整剪辑对照](docs/antutu-editing-eos.zh-CN.md)。

图像特效子进程另有尚未解决的退出异常：输出结果并打印退出后先发生 native SIGSEGV，
崩溃统计线程随后又访问已销毁的锁。第一次异常的故障回调尚未完整定位，未做原厂 Mali 对照，
不宣称已经修复或排除驱动退出清理影响。见[退出路径证据](docs/antutu-image-effects.zh-CN.md)。

0.6.3 本地播放、精度、生命周期和重启回退测试已通过。本次没有新增 0.6.3 的手动 Yamby
验收；原版组件和 LSPosed 保持不动，历史版本确认不重复计入。有限样本不等于通用兼容性。
HDR10+ 指标准 Codec2 元数据桥，不代表原生 HDMI HDR10+ 输出；不包含杜比视界／LLDV／FEL 或内核驱动升级。

## 组件说明

- [解码组件、开关和回退](magisk-decode/README.md)
- [SDK 移植与内核边界](docs/decode-sdk-050.zh-CN.md)
- [MPP／libyuv 升级范围](docs/decode-upgrade-040.zh-CN.md)
- [全局 Mali 安装与验证](magisk-mali-global/README.md)

旧独立模块源码和测试记录保留在 `magisk-mpp`、`magisk-rga`、`magisk-codec2`，
用于历史核查；不与当前解码合并模块叠加安装。

## LSPosed 模块 0.3.4

下载安装 `RK3588-Mpv-Fix-0.3.4.apk`，在 LSPosed 启用模块并勾选 Yamby，打开一次模块设置，再强行停止并重开 Yamby。进入 **设置 → MPV 播放器 → 默认解码方式 → HW+**。

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

## Frame Guard 刷新率模块 0.1.0

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

LSPosed 插件和 Frame Guard 没有网络权限和遥测；独立的全局 Mali 安装器会下载固定上游驱动。`RKPlaybackFix` 日志只记录解码／渲染选项。仓库和附件不含设备日志、截图、网络地址、本机用户名路径或签名私钥。

第三方组件来源及许可说明见 [英文 README](README.md)。这里不分发 Yamby 或 mpv 的二进制文件。
