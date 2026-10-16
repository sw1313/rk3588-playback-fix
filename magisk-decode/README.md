# Unified RK3588 decode stack

## 0.6.6 English / Chinese display

The WebUI has an **English / 中文** button for the complete interface, including
dependencies, inactive choices, backend names, reboot/health state and errors.
The first visit follows the browser language (Chinese or English fallback).
The button saves a preference in this WebUI host; switching display language
never sends a root command or changes a component choice. Reopening retains the
choice unless host storage is cleared. The page remains offline.

Module metadata, installer, CLI and Action messages already support English.
No root control, decoder binary, dependency or rollback logic changed here.
For all four components, see [language coverage](../docs/localization-066.md).

## 0.6.5 backend selector and dependency tree

Codec2 versions now share one selector: legacy 0.4.0 backend, 1.14.3 SIMD or
1.14.16. Exactly one backend is loaded. The Codec2 compatibility switch remains
separate, and disabling it restores the stock Codec2 after reboot.

Dependent controls appear beneath their prerequisite. Codec2 also lists RGA as
an additional requirement. Unmet dependencies gray the dependent card and its
saved switch without clearing the choice; re-enabling the parent restores it.
Controls remain editable. Enabling a child automatically enables required
parents, including selecting 1.14.16 when AV1 native/AFBC requires it. Explicitly
selecting a backend enables the Codec2/MPP/RGA prerequisites. Disabling the whole
module is never reversed implicitly. All writes still require reboot.

The WebUI uses atomic dependency plans through mode-control.sh. Existing direct
CLI on/off controls remain available with their original behavior; Action,
boot assembly, health checks and whole-stack rollback remain unchanged.
See [operation and validation](../docs/decode-webui-065.zh-CN.md).

## 0.6.4 offline KSU WebUI

Open Decode Stack in KernelSU's compatible WebUI host, or use the standalone
KsuWebUI app with Magisk. The `webroot/index.html` panel is offline and calls
only this module's allowlisted `webui.sh` / existing `mode-control.sh` interface.
It displays all fourteen saved selections, direct dependencies, effective
next-boot choices, active boot choices, health and pending reboot/update state.
It does not manage the separate Mali, LSPosed or Frame Guard modules.

Selections remain saved while dependencies are disabled. Every change still
requires reboot; toggling a choice back cancels the pending change if the
effective configuration again matches the active boot. A boot-only receipt
prevents installer assembly or stale `active-config` from being presented as
active. Upgrading from older releases without a receipt shows unknown active
state and requests reboot. Configuration errors disable individual controls,
while the whole-module switch remains available for rollback.

Existing CLI, Magisk enable switch, Action, firmware preflights and automatic
recovery remain. No decoder ELF, quality setting, Yamby or LSPosed file changes.
The WebUI API is compatible with KernelSU; this is not validation of the entire
firmware-specific mounting stack on arbitrary KernelSU devices.
See [operation and validation](../docs/decode-webui-064.zh-CN.md).

## 0.6.3 AV1 output delivery

Adds independently switchable lossless ten-bit AV1 AFBC on verified RK3588
composer surfaces with 64-aligned width. The local real AV1 4K60 HDR source
rendered all 3597 frames of the final-binary 60-second test with zero player
drops; the earlier prototype separately rendered 18808 frames. These are
source-specific measurements, not universal smoothness guarantees. Other widths,
explicit P010 and texture/CPU consumers retain their full-precision paths.
Filter-dependent top padding comes from the completed frame.
See [comparison, exact pixels and limitations](../docs/decode-av1-063.zh-CN.md).
The prior `av1_grain` correction and all component/whole-stack rollback remain.


One Magisk module combines the verified Android 12 / API32 system decode
components. It is restricted to the tested RK3588 firmware hashes and original
5.10 kernel interfaces. Yamby files, LSPosed 0.3.3, the GPU driver, clocks and
Frame Guard are retained.

Version 0.6.1 selects full-ten-bit AV1 output by actual consumer usage. Display
composer surfaces use native NV15 planes; texture consumers retain validated
standard P010 hardware output, with the previous full-precision copy route
available through `av1_native off`. Local file tests include the installed
SmartTube ExoPlayer engine. See [measurements, quality checks and limits](../docs/decode-av1-061.zh-CN.md).

Version 0.6.0 adds the separately selectable Codec2 1.14.16 decoder core with
cacheable internal copy buffers, read-only DMA synchronization and the newer
work/output-delay logic. The retained 1.14.3 SIMD core is packaged independently
for rollback. See the [port, test scope and remaining limits](../docs/decode-sdk-060.zh-CN.md).

Version 0.5.2 accelerates packed ten-bit to P010 conversion with bounded NEON
unpacking, an aligned cacheable row and sequential bulk writes. The independent
`p010_fast` switch restores scalar full-ten-bit conversion after reboot. See the
[performance, format coverage and remaining limits](../docs/decode-simd-052.zh-CN.md).

Version 0.5.0 ports the pinned SDK14 Codec2 core to this API32 stack, retains
the newer MPP compatibility pair and adds a `codec2_sdk` switch to restore
the previous backend independently. See the [SDK scope, port and kernel-driver
boundary](../docs/decode-sdk-050.zh-CN.md). This is a userspace multimedia port;
the original system framework, HIDL implementation and kernel remain.

Version 0.5.1 adds a separately selectable SDK buffer-output route for explicitly
configured eight-bit AVC/HEVC profiles, avoiding the legacy asynchronous stop/drain
failure exposed by 4K decoding. Unknown/ten-bit and AV1/VP9 buffer profiles retain
their existing route. It retains
Surface/P010 selection and full pixel precision. See the
[original-stack comparison, repair and rollback scope](../docs/decode-buffer-051.zh-CN.md).

Version 0.4.0 added six selected upstream MPP decoder fixes and the libyuv
scaling integer-overflow fix while retaining the existing system ABI. See the
[upgrade scope, exact commits and tests](../docs/decode-upgrade-040.zh-CN.md).
It does not replace the kernel VPU/RGA drivers or add unsupported hardware
formats. The independent global Mali module is retained.

| Component | Included change |
| --- | --- |
| MPP + VPU, ARM32/ARM64 | MPP 1.1.0 plus selected upstream VP9 probability, reset/CSD replay, empty CSD, AV1 metadata capacity/gating and HEVC first-task PS/RPS fixes; retains verified JPEG/PREP/HDR compatibility |
| RGA, ARM32/ARM64 | Existing validated librga 1.10.6 for this firmware's gralloc4 ABI |
| Codec2 | Selectable SDK14 core port with full-precision Surface/P010 and HDR metadata; previous backend and six guarded platform patches retained; all 21 original registry entries are retained |
| libyuv, system ARM64 | Version 1974 plus upstream scaling overflow fix `0b9e5c922dee`; all 710 original exports retained; 59 removed APIs use isolated Android 12 source implementations |
| HDR metadata | Full HDR10 static mastering/light values on the verified direct Surface path; standard per-frame Codec2 HDR10+ metadata bridge, with independent switches; no native HDR10+/Dolby HDMI claim |
| Software Codec2, ARM64 APEX | libvpx 1.17.0 decoder-only backend and pinned libgav1; API32 wrappers retain CFI and use full-precision P010 for tested ten-bit output; original shared libvpx and VP8/VP9 encoders remain original |

MPP and VPU are one indivisible selection. Codec2 is effective only when both
MPP and RGA are enabled. The two HDR switches also require effective Codec2; their saved selections survive dependency suppression. Turning off either dependency temporarily suppresses
Codec2 without erasing its requested setting. No switch enables an 8-bit fallback
for 10-bit media, lowers resolution or bitrate, or forces image enhancement.
IEP2 is already built into MPP. VDPP has no usable backend/kernel client on this
device and is not included.

Version 0.2.0 also backports the upstream MPP dynamic-HDR reference-counting and
parser-pool removal fixes, plus safe duplicate/self-copy/alias handling. Software
P010 requires Codec2's platform compatibility patches; disabling Codec2 also
suppresses the software selection without erasing it. The firmware-disabled AV1
software entry is enabled only with this selection. Hardware decoder ranking is
retained.

## Installation and switches

Install `rk3588-decode-stack-0.6.6.zip` in Magisk and reboot. The installer checks
all components before disabling overlapping standalone MPP, RGA, Codec2 and
libyuv modules. It also works from the original component state: Codec2's
preflight uses the hash-verified staged dependencies. No vendor partition is
written, and no APK or boot image is replaced.

Magisk's module switch or **Action** toggles the whole stack; reboot to apply.
The Action button preserves component selections. Individual switches are
available from a root shell:

The WebUI exposes these same choices graphically. On the verified Magisk box,
open the standalone KsuWebUI app and select **RK3588 Decode Stack**; grant root
when requested. Magisk itself does not embed the WebUI. See the linked guide.

```sh
su -c 'sh /data/adb/modules/rk3588_decode_stack/mode-control.sh status'
su -c 'sh /data/adb/modules/rk3588_decode_stack/mode-control.sh libyuv off'
su -c 'sh /data/adb/modules/rk3588_decode_stack/mode-control.sh libyuv on'
```

Replace `libyuv` with `mpp`, `rga`, `codec2`, `codec2_sdk`, `codec2_sdk_next`, `codec2_buffer`, `software`, `hdr_static`, `hdr10plus`, `p010_fast`, `av1_native`, `av1_grain` or `av1_afbc` for the other selections, and reboot.
`av1_native` requires the effective `codec2_sdk_next` core and keeps its saved
selection when dependencies are disabled. Turning it off retains ten-bit output
through the previous copy path; it does not enable eight-bit conversion.
`codec2_sdk_next off` restores the separately retained 1.14.3 SIMD core. It requires
effective `codec2_sdk`; dependency suppression preserves the saved selection.
`p010_fast` requires Codec2 and retains its saved selection when that dependency
is disabled. Turning it off restores scalar unpacking with the same ten-bit values.
`codec2_buffer` requires the effective SDK core. Turning it off restores the
legacy buffer route, including the failure reproduced with that upgrade combination.
Turning `codec2_sdk` off restores the prior 0.4.0 core; turning `codec2` off
suppresses both the SDK core and its dependent platform/software/HDR upgrades.
Selections prepare the next boot; they never replace a running process's library.
Disabling or uninstalling the unified module and rebooting restores the firmware
originals. Standalone modules remain disabled, so they cannot silently remount
their older payloads during rollback.

Payloads are assembled before Magisk mounts modules. Invalid payloads prevent
mounting. A failed post-mount file, dependency, precision or HIDL check disables
the entire module and reboots once; the disable flag prevents a recovery loop.
This bounded health check cannot detect every later playback failure.

Software decoder files are bind-mounted onto the three existing activated APEX
library paths at late start, with exact original/dependency hashes and factory
preflights. No APEX image, signature, linker configuration or partition is
modified. The shared libvpx encoder ABI is never replaced. A failed temporary
mount rolls back its mounts before the unified recovery disables the module.

## Validation

For 0.5.1, the final build passes 100 asynchronous early-stop cycles per AVC/HEVC
codec without a HAL restart. Both ARM32/ARM64 clients decode the complete 4K
fixtures to EOS; first-frame visible samples match independent software references.
Fourteen playback captures match 0.5.0 exactly, and the complete P010 reference
remains byte-identical, including its ten-bit low-order information. HDR switching,
HDR10+ metadata, original encoders, the buffer switch and whole-stack rollback are
covered. Actual AnTuTu decode activities complete without the recorded HAL crash;
this is not a claim about a new full benchmark score. See `buffer_051` in
`VALIDATION.json`; previous manual confirmations are not counted for this version.

The complete AnTuTu editing pipeline is **not** declared fixed: premature
application EOS was reproduced on the original decode stack under controlled
source-callback scheduling pressure. One upgraded run finished with 449 of 450
source frames. The separate image-effects exit crash is also unresolved.
No EOS suppression, score adjustment or frame duplication is included. See
[editing evidence](../docs/antutu-editing-eos.zh-CN.md) and
[image-effects evidence](../docs/antutu-image-effects.zh-CN.md).

For 0.5.0, fourteen system playback cases pass and all fourteen captured RGB
frames match 0.4.0 exactly. Eleven native reference cases retain every tested
sample, including 4K ten-bit output. A five-size HEVC ten-bit adaptive stream
produces 33 exact-reference frames, and four original OMX aliases decode exact
reference pixels. Full P010, four software playback cases, static HDR, standard
per-frame HDR10+ metadata and direct/client HDR transitions are covered. Clean
source rebuilds reproduce the installed core byte-for-byte. Version-specific
rollback and manual playback status are recorded under `sdk_050`; earlier
manual confirmations are not counted for the new core. The user confirmed normal original-Yamby HW+ playback of usual ten-bit and second-video content, pause/seek/switch/reopen, picture and sound, without gray edges or whole-display blackouts on 2026-10-15.

For 0.4.0, both MPP/VPU ABIs retain every previous export and libyuv retains
all 710 original exports. Four native reset/reference cases, 288 scaling-boundary
cases, 227 libyuv precision/geometry/JPEG cases, 36 hardware JPEG cases, fourteen
hardware and four software playback cases, twelve HDR/SDR transitions, and
AVC/HEVC encoding checks passed. Hardware test frames and P010 output match the
0.3.2 baseline exactly. Disable/reboot restored 31 original decode files while
global Mali remained active; re-enabling verified nineteen decode overlay hashes.
The user confirmed normal Yamby HW+ ten-bit and second-video playback, pause,
seek, switching and reopening on 2026-10-15, without gray edges or whole-screen
blackouts. The exact scope and retained limits are recorded under `decoder_040`.

Device results and exact limits are recorded in `VALIDATION.json`. Tests cover
36 hardware JPEG input/size/quality combinations, both VPU ABIs with H.264 and
HEVC Main10 pixel references, 227 libyuv precision/geometry/JPEG cases, and an
old-header C++ MJpegDecoder ABI/canary test. Fourteen system playback cases cover
pause/resume, seek/flush, EOS, HDR metadata, AV1 8/10-bit, HEVC 10-bit and 4K.
AVC/HEVC encoding output pixels match the original reference. Full P010 output
retains all tested 10-bit sample values.

The user confirmed normal HEVC ten-bit playback in original Yamby HW+ on 0.3.0.
No HDR10 clip was available for their manual confirmation; synthetic automatic
output tests and manual application confirmation are recorded separately. Camera hardware and arbitrary media are not fully
validated. Original Yamby's HEVC Main10 HW copy-back path still lacks P010
support; HW+ remains the appropriate path. No Yamby or LSPosed change is used to
make this system upgrade work.

Older system MPP copies and proprietary `libiep`/PQ libraries remain original.
Version 0.3.0 preserves the original HWC binary on-device and adds a metadata
adapter around it. The verified direct Rockchip Codec2 Surface path now carries
HDR10 mastering primaries, white point, luminance and MaxCLL/MaxFALL into the DRM
blob and checksum-valid HDMI InfoFrame registers. This is register verification,
not an external wire measurement. Apps that render into their own buffers must
forward output metadata themselves; no app is changed.

The standard HEVC HDR10+ bridge returns exact per-frame T.35 payloads through
C2Buffer/configUpdate and explicit empty values between metadata frames. It does
not enable an unsupported HDR10+ HDMI transmitter path. See
[implementation and validation](../docs/hdr-metadata-030.zh-CN.md).

This firmware can start its composer before Magisk mounts. When `hdr_static` is
active, the boot service checks the loaded adapter and, when needed, reloads the
original composer, SurfaceFlinger and framework consumers once during boot.
Turning this selection off restores the original HWC and omits that reload.
No executable instruction, kernel memory or boot image is patched.

Software HDR Surface playback can also fall back to the stock HWC's eight-bit
client framebuffer; full decoder/P010 sample precision does not prove full
display-pipeline precision. Hardware decoding remains preferred.

## Build and licensing

Build the six component payloads using `tools/build_mpp.py`, `tools/build_rga.py`,
`tools/build_codec2.py`, `tools/build_libyuv.py` `tools/build_software.py` and `tools/build_hdr.py`. Then combine their clean
pre-install directories with `tools/build_decode_stack.py --mpp ... --rga ...
--codec2 ... --libyuv ... --software ... --hdr ... --work ...`. The combiner rejects a Codec2 input that
already contains generated firmware patches, or an HDR input containing a copied original HWC. Those six files and the codec XML
are generated only on the device from exact-byte/hash-verified originals.

For the optional SDK core, also build `tools/build_codec2_sdk.py` from the
pinned source and pass its clean package using `--codec2-sdk ...` to the
combiner. The original Codec2 package is still required as the fallback.

Component source revisions, licenses, source patches and build recipes are
included in their directories. The package contains newly built open-source
binaries and patch recipes; it contains no copied firmware libraries, personal
logs, device addresses, app data, signing keys or media from the device.

## 中文

这是合并的解码系统组件模块：MPP／VPU、RGA、Codec2、libyuv、VP8／VP9／AV1 系统软件解码器，以及 HDR 元数据适配，保留分项开关。
只适用于已验证指纹的 RK3588 Android 12 固件。关闭 MPP 或 RGA 时，依赖它们的
Codec2 会暂时停用；重新开启依赖后会恢复原先选择。10 位视频保持完整精度，
不会通过降为 8 位、降低分辨率或码率来兼容。Yamby、LSPosed 和显示黑屏修正不变。
软件解码开关名为 `software`；它依赖 Codec2 的平台兼容补丁。新版 MPP 修复动态 HDR
元数据残留和生命周期问题，但不代表盒子已经获得 HDR10+ 或杜比视界输出。

安装后重启生效。面具中的总开关或“操作”控制整包，分项开关使用上面的 root
命令，均需重启。停用／卸载整包并重启即可恢复原库。旧独立模块会停用，避免
同时覆盖同一个文件；不会写入真实系统分区。

0.3.0 增加 `hdr_static` 与 `hdr10plus` 两个独立开关。前者补全已验证直接 Surface
路径的 HDR10 静态元数据；后者把 HEVC HDR10+ 逐帧数据交给标准 Codec2 接口。
它们不代表原生 HDR10+ HDMI 或 Dolby Vision 已开启。HDR 静态适配启用时，
冷启动可能额外重载一次显示与框架服务，以处理原固件早于 Magisk 挂载启动 HWC 的顺序。
具体测试、应用输出边界见上面的实现文档。

0.5.0 增加同平台 Android14 SDK 的 Codec2 核心移植及 `codec2_sdk` 开关。
关闭此分项并重启可恢复 0.4.0 后端；整包关闭并重启恢复原固件组件。
保留较新的现用 MPP，没有回退到 SDK 随带的旧版 MPP，不替换内核 VPU。
具体适配范围与测试见[SDK 移植说明](../docs/decode-sdk-050.zh-CN.md)。

## 0.3.1 HDR transfer repair

The existing `hdr_static` switch also controls the guarded same-Surface PQ/HLG
repair. SDR/disable decisions, pixel formats and composition policy are retained.
Client-only/mixed/unknown paths are bypassed. Dolby experimental port/probe code
is removed; no Dolby output feature had been enabled in the daily module.
See [tests and limits](../docs/hdr-transition-031.zh-CN.md).

## 0.3.2 native client output

The `hdr_static` switch also controls the verified managed eight-bit native SDR
client-output correction. The original planner and its HDR policy run once;
only a scoped property read is overridden for client-only HDR sources already
rendered to SDR. No global force-disable property is written. Original Yamby,
LSPosed, decoder backends and boot remain unchanged. See
[implementation, tests and limits](../docs/hdr-client-032.zh-CN.md).

`av1_afbc off` and reboot restores the prior linear ten-bit composer route.
It requires effective `av1_native` and `codec2_sdk_next`; saved choices survive
suppressed dependencies. The switch does not change the film-grain or HDR flags.
