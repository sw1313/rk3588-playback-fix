# RK3588 Playback Fix · v0.8.6

Decode Stack 0.8.6 retains the matched video frame rate across pause, resume and same-video seeking in Auto. The system-wide Layer policy covers both legacy Binder and BLAST transaction requests. The specified Exo scenario passed physical HDMI feedback; the release keeps the tested runtime binary. Genuine resolution/HDR-format changes and every receiver are not certified blackout-free.

本版正式整理 Decode Stack 0.8.6、Mali Global 0.4.2 和 LSPosed 客户端 0.3.7，更新中文／英文 WebUI 说明，保留分项开关、依赖和重启回退。HDMI 短黑修复已在指定 Exo 场景实屏确认；不以投屏或日志代替验收。

[**Downloads / 下载**](DOWNLOADS.md) · [中文说明](README.zh-CN.md) · [Release](https://github.com/sw1313/rk3588-playback-fix/releases/tag/v0.8.6)

- Decode Stack: native queue/backpressure repair, selected zero-copy/ten-bit format paths, HDMI settings/audio/DDC compatibility and matched-rate retention. [Controls and scope](magisk-decode/README.md).
- Mali Global: matched g18 userspace, optional kernel/CSF, first-binding preparation and bounded DVFS/stability changes. Per-game FPS gains still require comparison. [Module guide](magisk-mali-global/README.md).
- LSPosed client: retains the independent MKV seek-index repair and guarded HW/Main10 client format repair. The system blackout fix is not an app hook.
- Frame Guard 0.1.1 and optional PanVK Test8.1 are carried unchanged. Keep Frame Guard off for Auto/content matching; PanVK remains experimental and app-scoped.

Exact firmware checks target RK3588/H96 Max V58, Android 12/API32 and the pinned 5.10.66 image. Updates preserve existing selections. Low-level QMS/first binding require private device/root-matched preparation and remain off on an unprepared installation. The public ZIPs do not distribute firmware, a boot image, proprietary Mali libraries or CSF firmware. Disable a feature and reboot for rollback; whole-module disable restores its original firmware path.

Known limits include full HDR color calibration, every application/receiver combination, different-rate media changes beyond the recorded scenario and long-term game performance. A formal release number does not turn these untested combinations into passes.

[Accepted session evidence](docs/decode-refresh-session-086.zh-CN.md) · [Remaining goals](docs/main-goals-priority.zh-CN.md) · [Source/build](SOURCE_CODE.md) · [Licenses](THIRD_PARTY_NOTICES.md)

Original contributions are MIT; upstream file licenses, GPL kernel sources and LGPL FFmpeg source accompany the applicable components. Private firmware link inputs and signing keys are excluded.
