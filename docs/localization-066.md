# English and Chinese / 中英文支持

| Component / 组件 | English support / 英文覆盖 | Language control / 切换入口 |
| --- | --- | --- |
| Decode Stack 0.6.6 | Entire offline WebUI, component help, dependencies, statuses, errors; existing English metadata, installer and CLI | **English / 中文** button at the top of KsuWebUI |
| Yamby/mpv LSPosed 0.3.4 | Settings, app name, LSPosed description and original English diagnostic messages | **English / 中文** button in the plugin settings |
| Native Mali Global 0.2.0 | Existing English module description, installer, Action and CLI | No separate graphical settings page; original package unchanged |
| Frame Guard 0.1.0 | Existing English module description, installer and Action/control output | No separate graphical settings page; original package unchanged |

## WebUI

On first use, Chinese browser locales select Chinese; other locales fall back
to English. The language button updates all displayed text and accessibility
labels without changing saved selections or sending a root command. It works
while showing errors or pending reboot status. The host stores the preference
locally; clearing its data removes that preference. If storage is unavailable,
the button still works for the current page. All translations are bundled offline.

首次按浏览器语言选择中文或英文；顶部按钮即时切换组件说明、依赖、后端选项、
当前／重启后状态、保存提示和报错，不修改开关、不执行 root 命令。宿主保存语言
偏好；清除宿主数据会清除偏好。存储不可用时仍可在当前页面切换。

## LSPosed settings / 插件设置

The settings page initially follows Android resource selection, with English as
the default and Chinese resources for Chinese locales. Its language button saves
an activity-only display choice in a separate private preference file. The
existing `fixes` preferences, LSPosed scope and hooks remain untouched. App name
and LSPosed description follow the system locale; the manual button controls the
settings page. Reopen the page to confirm the saved language.

插件设置初始跟随系统语言，支持中文并以英文作为默认资源。按钮把界面语言保存
到独立偏好文件，不更改原 `fixes` 选项、不改变作用域和钩子。应用名称与 LSPosed
模块描述跟随系统语言，手动按钮控制设置页。APK 使用原签名原包名升级，保留设置。

This is a presentation-only update. Existing hook/geometry compiled classes and
decoder ELF files are checked for byte identity. It does not add playback
features or claim a new format/precision test matrix. See
[validation](validation/decode-0.6.6.json). The Mali and Frame Guard packages
are retained byte-for-byte because their existing messages are already English.

本次仅更新显示语言。原钩子／裁剪几何编译类和解码 ELF 做逐字节核对；不新增播放
功能、不冒充新一轮完整格式测试。Mali 与 Frame Guard 因已有英文而保留原包字节。
