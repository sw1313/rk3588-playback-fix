# RK3588 Playback Fix · v0.8.6

Reversible upgrades for **RK3588 playback and game graphics**: a system decode/display stack, global Mali driver module, LSPosed player client, and optional refresh-rate workaround and in-app Vulkan driver. Install the components you need; enabling everything is not required.

**The main 0.8.6 change keeps the matched video frame-rate request across pause, resume and same-video seeking in Auto**, avoiding unnecessary returns to the desktop refresh rate. The specified Exo scenario passed user feedback on the physical HDMI display. Native queue errors and MKV seeking back to the beginning have separate repairs and acceptance records.

[Downloads and SHA256](DOWNLOADS.md) · [Release](https://github.com/sw1313/rk3588-playback-fix/releases/tag/v0.8.6) · [详细中文说明](README.zh-CN.md) · [Release notes](docs/release-0.8.6.zh-CN.md)

## Supported hardware and prerequisites

System modules target the pinned **H96 Max V58 / RK3588, Android 12 / API32, Linux 5.10.66 firmware**. A matching SoC or kernel version alone is insufficient: installers check firmware hashes, ABI and component combinations. Unknown builds are refused; do not bypass these guards.

- Decode Stack and Mali Global require a working root/module environment. Their offline panels require KsuWebUI or a manager with the compatible root bridge.
- The client requires working Zygisk/LSPosed. The Yamby settings adapter targets **2.1.0.11 (20010012)**; newer app builds may need adaptation.
- QMS requires a QMS-capable receiver, Auto and matched kernel/startup components. VRR support alone does not establish QMS support. Matrices, receivers and adapters can change link capabilities.
- First binding before graphics and low-level display features need private preparation matched to the device, firmware and existing root. **Public ZIPs contain no boot image and do not provide universal boot adaptation.** Boot changes are separate from ordinary userspace selections.

## Components

| Component | Included version | Purpose and installation |
| --- | --- | --- |
| [Decode Stack](magisk-decode/README.md) | **0.8.6** | Magisk system module: decoding, buffer transfer, ten-bit/HDR, display compatibility, matched-rate retention and optional QMS |
| [Mali Global](magisk-mali-global/README.md) | **0.4.2** | Magisk GPU module: global ARM32/ARM64 g18p0 userspace, optional paired kernel/CSF, DVFS and synchronization stability |
| Yamby/mpv LSPosed client | **0.3.7** | APK: HW+ settings, optional edge compensation, guarded Main10 HW copy-back and MKV seek-index repair |
| Frame Guard | **0.1.1, unchanged** | Magisk fixed-60Hz workaround; restore its saved restrictions and keep it off for Auto content matching |
| [PanVK in-app driver](panvk/README.md) | **RC3 Test8.1, optional experimental** | Import in the supported application's driver manager; not a Magisk module or global Mali replacement |

The project version identifies a complete release snapshot; components keep their own version numbers. The release includes five components, source ZIP, manifest and SHA256 sums. Older releases remain available for history and rollback. Standalone MPP/RGA/Codec2/libyuv directories are historical sources and must not overlap the unified Decode Stack.

## Install or update

1. Choose the required assets in [DOWNLOADS.md](DOWNLOADS.md) and check filenames/SHA256.
2. Install Decode Stack and/or Mali Global ZIPs in the module manager. Decode validates dependencies before handling overlapping old modules. Do not re-enable those standalone decoder modules or combine global Mali with the retired app-scoped Mali experiment.
3. Open the module WebUI and inspect selections/dependencies. Updates retain existing choices; they do not select every upgrade. Low-level features remain gated when private startup preparation is missing.
4. Save and reboot. Check the active version, boot health and pending-reboot state. Loaded graphics libraries are not switched live by these controls.
5. For client features, install the APK, enable it in LSPosed, select the supported app scope, choose the needed repairs in client settings, then force-stop and reopen the player. Client version numbers are independent of the LSPosed framework version.

Mali downloads separately licensed userspace libraries and required CSF from fixed upstream HTTPS sources with size/SHA256 checks. A verified local cache supports reinstalling. Neither proprietary payload is bundled. See the [private startup preparation and recovery guide](magisk-mali-global/boot/README.md) before enabling first binding.

## Decode Stack controls

| WebUI group | What it does | Conditions and limits |
| --- | --- | --- |
| Playback core | MPP, RGA, Codec2 compatibility, libyuv and software decoders | Only paths actually called by an app are affected; no forced decoder for every video |
| Native playback stability | Native Surface output, pause backpressure, ownership/exit guards, output-delay notification and HEVC EOS task return | Dependencies are selected together; native output is retained instead of using copy fallback as a queue repair |
| AV1 playback | Native buffers, source film grain, lossless display/texture AFBC | Saves bandwidth/copies on compatible paths, with fallback for unsupported consumers |
| Ten-bit and HDR | P010 layouts/SIMD/paired unpacking, import reuse, HEVC direct display, HWUI formats, GPU targets and metadata | Preserves precision; clients missing P010 mapping still need the client repair; full color calibration remains incomplete |
| Display compatibility/composition | Mode lookup, localized resolution/HDR settings, matched-rate retention, scaled overlays, metadata reuse and false audio-unplug filtering | Separate repairs; actual layer eligibility determines DEVICE composition |
| HDMI QMS | Real refresh changes at the same resolution | Independent control, excluded from group-wide enable; requires receiver and private startup support |

Expand individual controls for per-feature rollback and the Codec2 backend selector: legacy, 1.14.3 SIMD or 1.14.16. Features requiring the newer core include that dependency. A newer backend is not proof of a speedup in every workload.

Groups show selected/current/next-boot counts. Mixed means a retained custom selection; clicking selects the entire group. Disabling a group changes its members, preserving other selections even if missing dependencies make them inactive. “Current” comes from this boot; “after reboot” is the saved effective configuration. Gray controls retain their selection. Boot health establishes loading/dependency checks, not universal playback acceptance. Both panels support remembered English/Chinese language choices.

## Auto, retention, QMS and fixed refresh

| Setting or feature | Behavior |
| --- | --- |
| System Auto | Allows native content-based mode selection. A desktop currently outputting 4K60 does not mean the saved Auto selection was lost |
| Keep matched rate during playback | Retains the same video layer's valid source vote when pause/resume/same-video seek clears it, preventing unnecessary desktop-rate reselection |
| QMS | Maintains the HDMI link for genuine same-resolution rate changes; does not replace retention of a playback request |
| Fixed resolution/rate | Intended to keep the selected mode; only Auto adapts. Localized identification is repaired; sustained locking of every fixed combination remains separately scoped |
| Frame Guard | Saves system limits and requests fixed 60Hz, preventing Auto content matching |

The 0.8.6 policy runs at the common SurfaceFlinger layer setter, covering legacy Binder and BLAST requests. It retains only eligible zero/default requests in Auto. **New positive frame-rate requests pass through and update normally.** Native policy excludes hidden/removed layers; there is no global cross-video vote cache. Disable `display_session` and reboot to restore original handling.

This firmware's implementation uses the system loader supplied with `hwc_client_hdr10`, hence that dependency; retention is not restricted to HDR video. QMS has independent controls and compatibility checks.

**Frame interpolation/generation:** if a player changes its request from 24/30fps to 60fps or another positive value, the policy allows the update. Native selection still considers display support and can choose a source-rate multiple. If interpolation produces extra frames but the player continues reporting the original rate, Auto may still select from that vote and limit the benefit. This combination has not been specifically validated.

The accepted Exo scenario was tested with Frame Guard off on the physical HDMI display. It does not certify every player/receiver blackout-free. Resolution changes, SDR/HDR signal-format changes or intermediate-device retraining may still blank the screen. See [implementation and bounded acceptance](docs/decode-refresh-session-086.zh-CN.md).

Direct HDMI connectivity has a separate EDID/E-DDC repair. A stock segmented-read bug loses real receiver capabilities in extension blocks 2/3. The exact-kernel component corrects those reads and performs native display initialization at startup, rather than fabricating EDID capabilities or forcing an HDR flag. Picture and sound recovery were recorded for the specified direct connection; this is distinct from blackout repair. [Direct HDMI repair and rollback](docs/decode-ddc-direct-hdmi-086.zh-CN.md).

## Ten-bit HDR and HW green-screen repair

Ten-bit output requires compatible decoder buffers, client formats, GPU targets, allocator, HWC and HDMI. An HDR badge or a ten-bit output setting alone does not prove the full chain.

Recorded checks cover specified explicit P010/PQ/HLG paths reaching real ten-bit AB30 AFBC scanout, mixed SDR UI and rollback. Full color calibration remains incomplete: reference gray differences reached 3/3/4 code values, so this is not bit-exact RGB certification. [Physical HDR evidence](docs/decode-physical-hdr-081.zh-CN.md).

| Player option | mpv setting | Meaning |
| --- | --- | --- |
| HW | `mediacodec-copy` | Hardware decoding followed by CPU-memory copy-back; needs correct ten-bit mapping/layout |
| HW+ | `mediacodec` | Hardware Surface path, usually avoiding copy-back |
| SW | `no` | Software decoding |

HW and HW+ both use hardware decoding; HW+ is not a picture-quality level. The specified Main10 HW issue involved both system buffer layout and missing client P010 mapping. Decode supplies the system repair; client 0.3.7 conditionally loads its paired repair only with the supported ARM64 original-library hash. Specified image/pause/seek checks are recorded, not every possible green-screen cause.

The Codec2 HDR10+ bridge carries standard per-frame metadata; it does not establish native HDMI HDR10+ output. Dolby Vision/LLDV/FEL upgrades are not provided.

## LSPosed client options

| Option | Scope |
| --- | --- |
| Add Yamby default HW+ | Uses the app's own settings/save callback. Manual HW/HW+/SW choices remain; `vo` and `gpu`/`gpu-next` stay app-controlled |
| Prefer HW+ in other mpv apps | RK3588 only, supported common interfaces, preserves explicit SW requests; pure-native/obfuscated wrappers may need adaptation |
| Yamby HW+ gray-edge compensation | Default off. Uses reported crop/texture dimensions, skips unmatched data, does not zoom the picture |
| Main10 HW ten-bit copy-back repair | Default off. Needs system ten-bit buffer repair and matched ARM64 original library. Off plus player restart restores original loading |
| Exo multi-SeekHead repair | Default off. For the matched Yamby APK, reads real MKV Cues to repair the specified seek-to-start issue; unrelated to system refresh policy |

For Yamby, choose Settings → MPV player → Default decoding → HW+. Settings completion is not chip-restricted and the APK requires Android 8.0+, but decoding capability is device-dependent; other repairs have their own RK3588/app/library guards. Use verified application scopes, not Android system processes. Restart the player after changes; adding an unsupported app to scope does not establish compatibility.

## Mali Global and gaming

Mali Global provides global ARM32/ARM64 **g18p0 userspace**, optional paired KMD/CSF, pre-graphics first binding, DVFS and CSF synchronization stability. g18 remains the current line; g25 has not been qualified.

- Userspace, kernel/CSF, DVFS and synchronization controls preserve dependencies and reboot rollback; saving never hot-switches the driver.
- First binding attaches the matched driver before graphics services start, avoiding a running-graphics handover. It requires private boot preparation, preserves existing root/external Zygisk, and has both-ABI Zygisk Next/LSPosed startup and rollback records.
- DVFS reduces load polling from 100ms to 20ms while retaining OPPs, voltage/frequency bounds and thermal protection. Bounded GLES burst results are not all-game FPS gains.
- The CSF fence backport repairs metadata-reference ordering for stability. The rejected CSF scheduling-tick 100→20ms experiment is a different mechanism and is not delivered as this optimization.
- Actual game priorities remain Legend Clover and Eden. Sustained frame time, temperature, power and factory-g13 comparisons need game evidence. Optional PanVK remains app-scoped and experimental for long-term stability.

[Module guide](magisk-mali-global/README.md) · [DVFS evidence](docs/mali-dvfs-031.zh-CN.md) · [CSF repair](docs/mali-csf-fence-042.zh-CN.md) · [Main goals and remaining work](docs/main-goals-priority.zh-CN.md). This release is not a wholesale Composer/HWC or built-in VOP2/HDMI kernel replacement.

## Rollback and recovery

| Action | Result |
| --- | --- |
| Disable a Decode feature and reboot | Restores that feature's prior path; dependent selections may become inactive |
| Disable/remove Decode Stack and reboot | Restores original system decode/display paths; independent Mali, LSPosed and Frame Guard remain separately managed |
| Select stock Mali kernel and reboot | Restores stock KMD/CSF; userspace selection remains independent. Follow the startup recovery procedure when first binding is enabled |
| Disable Mali Global and reboot | Uses the prepared independent startup recovery path to restore stock drivers; backups reside outside the module |
| Disable a client repair and restart the app | Restores corresponding original app handling; LSPosed scope can also be disabled |
| Select System GPU Driver in the app | Restores its system-driver choice after closing/reopening the game; no global module change |

**Frame Guard is an exception: disabling its module alone does not undo saved system refresh restrictions.** First restore/pause through its Action, or uninstall to restore values still owned by the module. Its original backup is `/data/adb/rk3588-frameguard/original.conf`; later manual changes are protected. The accepted release configuration keeps it disabled and paused.

On failed boot checks, inspect state/dependencies/rollback records instead of repeatedly forcing unknown tuples. A hard kernel hang or interrupted boot write can still require power cycling or physical Loader recovery; network ADB recovery cannot be guaranteed. See the startup guide for the exact limits.

## Overhead and acceptance limits

Firmware hashes and ABI qualification run mainly during installation/startup, not for every video frame. Rate retention is request-driven, with no new per-frame worker; temporary request tracing is off normally. QMS waits for real vblank/fences during switching, holds stable hardware output instead of resubmitting an identical full transaction, and retains up to 100ms read-only checks. Commands wake it immediately; this is not a mandatory 100ms switch delay. Necessary fractional-timing frame work remains.

This is not a zero-cost claim or a game benchmark. Playback CPU time, wakeups and contention need separate measurement. [Controls and QMS cost, including historical stages](docs/decode-controls-and-qms-cost.zh-CN.md).

| Recorded result | Not implied |
| --- | --- |
| Specified native Exo queue recovery, MKV seek repair and 0.8.6 matched-rate physical-display feedback | Every source, app and receiver is error/blackout-free |
| Specified P010 ten-bit scanout, precision and rollback checks | Complete HDR calibration, dynamic HDR or Dolby support |
| g18 pairing, startup, both-ABI injection, bounded CSF/DVFS checks | Universal game gains, long-term stability or g25 compatibility |

Historical AnTuTu full-editing EOS/completion and image-effects exit issues remain independently documented: [editing](docs/antutu-editing-eos.zh-CN.md), [exit](docs/antutu-image-effects.zh-CN.md). No score modification, frame omission or skipped synchronization is used to conceal them. Older reports describe their own candidate stage; current release acceptance is in [the 0.8.6 validation record](docs/validation/release-0.8.6.json) and subsequent supplements.

Detailed reports retain specific upstream sources, selective backports and local changes; version numbers alone do not establish benefits:

- [Lossless AV1 AFBC](docs/decode-av1-063.zh-CN.md), [texture AFBC](docs/decode-texture-afbc-070.zh-CN.md), [Gralloc transfer](docs/decode-gralloc-transfer-073.zh-CN.md).
- [Scaled hardware overlays](docs/decode-hwc-overlay-075.zh-CN.md), [HEVC ten-bit direct display](docs/decode-hevc-native-076.zh-CN.md), [paired P010 unpacking](docs/decode-p010-pairs-078.zh-CN.md).
- [Native backpressure](docs/decode-native-backpressure-084.zh-CN.md), [MKV index repair](docs/decode-seekhead-repair-candidate.zh-CN.md), [system HDMI investigation](docs/system-hdmi-blackout-086.zh-CN.md).

## Source, licensing and privacy

Original contributions are **MIT**. Kernel code, FFmpeg and other upstream files retain their applicable GPL/LGPL and other licenses. Source, patches, fixed upstream provenance, build tools and third-party notices are supplied; proprietary drivers are not represented as open source. System rebuilds require legally obtained exact private firmware/link inputs. The source ZIP is not a complete Android BSP.

[Source/build guide](SOURCE_CODE.md) · [Open-source scope](OPEN_SOURCE.md) · [Third-party notices](THIRD_PARTY_NOTICES.md) · [Release manifest](release-manifest.json)

Public source/assets exclude private boot/firmware/system libraries, device endpoints, raw logs, media, accounts and signing keys. The client has no telemetry; Mali's installer uses the network for pinned upstream payloads. The release APK retains the existing signing certificate for upgrades; the private key is not published. A self-build signed with another certificate cannot directly replace the release APK.
