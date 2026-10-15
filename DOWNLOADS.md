# Current downloads / 当前下载

Each component has its own version. Release tags/titles are version-first, with component suffixes for equal versions.
各组件独立版本；系统解码与 GPU 模块仍为预发布，仅适用于其契约匹配的固件。

| Component / 组件 | Version / 版本 | Package / 直接下载 | Release notes / 发布说明 |
| --- | --- | --- | --- |
| Decode Stack / 解码合并模块 | **0.5.1, pre-release** | [rk3588-decode-stack-0.5.1.zip](https://github.com/sw1313/rk3588-playback-fix/releases/download/v0.5.1/rk3588-decode-stack-0.5.1.zip) | [Decode 0.5.1](https://github.com/sw1313/rk3588-playback-fix/releases/tag/v0.5.1) |
| Native Mali Global / 全局 GPU | **0.2.0, pre-release** | [rk3588-mali-global-0.2.0.zip](https://github.com/sw1313/rk3588-playback-fix/releases/download/v0.3.2-system/rk3588-mali-global-0.2.0.zip) | [GPU scope and rollback](magisk-mali-global/README.md) |
| Yamby/mpv LSPosed plugin / 插件 | **0.3.3** | [RK3588-Mpv-Fix-0.3.3.apk](https://github.com/sw1313/rk3588-playback-fix/releases/download/v0.3.3-yamby/RK3588-Mpv-Fix-0.3.3.apk) | [Plugin 0.3.3](https://github.com/sw1313/rk3588-playback-fix/releases/tag/v0.3.3-yamby) |
| Frame Guard / 刷新率 | **0.1.0** | [rk3588-frameguard-0.1.0.zip](https://github.com/sw1313/rk3588-playback-fix/releases/download/v0.3.3-yamby/rk3588-frameguard-0.1.0.zip) | [Frame Guard guide](README.zh-CN.md#frame-guard-刷新率模块-010) |

[SHA256 checksums / 校验值](https://github.com/sw1313/rk3588-playback-fix/releases/download/v0.5.1/SHA256SUMS-current.txt) ·
[Machine-readable index / 组件索引](release-manifest.json)

Only Decode Stack changed in this publication. The other three downloads are the
existing, unchanged published files. Do not install every historical release.
仅解码模块更新，其他三个链接指向保持原样的已发布文件。无需因为解码更新重装其他组件。

For system decoding, use the unified ZIP and keep old standalone MPP/RGA/Codec2/libyuv
modules disabled. The old system collection still contains Decode Stack **0.3.2**;
download its Mali asset directly if needed, not that old decoder asset.
解码合并模块保留分项开关、依赖处理和重启回退，详见[模块说明](magisk-decode/README.md)。

Known issues remain: [AnTuTu full editing EOS](docs/antutu-editing-eos.zh-CN.md) and
[image-effects exit crash](docs/antutu-image-effects.zh-CN.md). These are not fixed
by 0.5.1. Fresh manual player validation for 0.5.1 remains pending; the package is
a pre-release despite completed automated precision and rollback tests.
