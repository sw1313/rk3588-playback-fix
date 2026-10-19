# Decode Stack 0.7.9 的显示适配增量（实验候选）

已集成 HWC 已上报模式表查找与两 ABI HWUI RGB10A2 映射，分别由
`hwc_mode_cache`、`hwui_rgb10` 控制，依赖 `hdr_static`，默认关闭、重启回退。
完整十位 HDR CLIENT 分项因 SF 真实目标仍为八位而锁定不生效。
范围、上游来源及实机结果见[0.7.9 补充记录](../docs/decode-hwui-rgb10-079.zh-CN.md)。
没有以整套 HWUI/HWC 版本升级或 HDMI 标签替代完整验收。

## 0.1.4：RK3588 缩放图层平面资格修正

Decode Stack 的默认关闭 `hwc_scaled_overlay` 选择性回移 Rockchip
`e3e3d168e524da25620ef55da734e48c8e56afe6`。安装时从精确哈希匹配的原 HWC
生成私有变体，仅将进入两条旧 RK356x 缩放限制的分支改为 NOP；原硬件平面
匹配、原子事务、fence 和像素缓冲保持不变。没有运行时修改代码页。
保留原伴随库，关闭并重启恢复原字节；整包停用恢复原固件。
这是选择性修正，未将整套 HWC 升级到最新版本，也未修改 VOP2 内核。
预检在独立进程调用真实旧／新方法，覆盖 768 组并保留其他资格检查。
实机合成路径与收益的验收见父模块 0.7.5 报告。

上游缩放策略补丁保留 Apache-2.0 许可、原作者及 Rockchip/AOSP 声明，见
`../LICENSES/hwc/`。本目录 MIT 许可仅覆盖本项目原创代码，原固件库不随包分发，
也不因本适配而重新授权。已测试安装 ZIP 的后补法律声明与实机结果单独附带，
不覆盖或重新打包已验收的安装器。

# HDR10 static metadata adapter


Component 0.1.3 adds the parent Decode Stack `hwc_metadata_reuse` option
(default off, requires `hdr_static`, reboot required). After querying the live
single-output connector and full metadata on every commit, identical HDR blobs
can be borrowed for the duplicate original transaction. Borrowed IDs are never
owned/destroyed or retained across commits. Changed metadata and option-off use
the previous create/commit/destroy path. This local optimization does not replace
the original HWC plane planner, VOP2 kernel, pixel buffers or fence handling.
See the parent 0.7.2 report for new-version evidence and limitations.

Unified-module component for the exact verified API32 firmware; not a generic
replacement display driver and not a standalone installer.

The original ARM64 HWC is copied from the device during installation after an
exact hash check. It is never included in the downloadable package. A small
adapter forwards the original HWC2 device functions and observes standard
SMPTE2086/CTA861.3 buffer metadata written by the Rockchip Codec2 bridge.
It adds these values to a duplicate of the original DRM atomic request, preserving
global HDR disable/area settings and all original pixel planes, formats,
fences and composition policies. The original request remains untouched and is used if the
additional metadata transaction is rejected. HDR10+ and Dolby capabilities are
not advertised or spoofed.

The adapter changes five original HWC import-table pointers in its own process,
temporarily restoring write access only to their data pages, then restoring the
original page permissions. It does not patch executable instructions or kernel
memory. Installer hash/ABI checks and a separate-process check gate this path.

Unified switches `hdr_static` and `hdr10plus` are independent and require reboot.
`hdr_static` needs the selected compatible Codec2 component; disabling it removes
the HWC overlay and restores the original HWC. `hdr10plus` controls the standard
Codec2 HDR10+ info bridge and does not imply HDMI HDR10+ support.

This firmware's mapper rejects clearing its variable-size HDR10+ buffer field.
The bridge therefore leaves that field untouched and carries dynamic data through
standard C2Buffer/BufferQueue info, with explicit empty frames. Static metadata
uses explicit zero structures when absent so reused buffers cannot retain it.

`bin/packet_probe64` reads the HDMI-QP static InfoFrame registers and validates the
header/checksum. The stock kernel debug `status` static fields are one byte early;
use the decoded complete packet for those fields. Register verification is not a
wire measurement. This component neither claims Dolby Vision nor lowers picture
bit depth, resolution or bitrate. Applications that render into their own buffers
must still supply correct output metadata; no application libraries are changed.

EOTF transitions survive TEST_ONLY validation until the same request is really
committed. The original request-free import clears abandoned state; a zero HDR
blob explicitly counts as an SDR/disabled transition. The wrapper preserves SDR/disable decisions. On a single active
output it also
repairs stale PQ/HLG EOTF using a homogeneous validated DEVICE HDR layer and
matching active planes in that same atomic request. The exact original HWC maps
PQ planes to 2 and HLG planes to 0; the connector still uses 2 and 3 respectively.
Conflicting, unknown, incomplete or client-only composition is bypassed. HLG
clears HDR10 mastering/light fields. This repair shares the `hdr_static` switch;
turning it off and rebooting restores the original HWC. See
[0.3.1 validation and limits](../docs/hdr-transition-031.zh-CN.md).


Version 0.1.2 also repairs native SDR client output while retaining the original
plane plan. A hash-pinned, color-managed SurfaceFlinger produces an eight-bit
AB24/UNKNOWN sRGB client target. If all HDR video layers are CLIENT and none is
DEVICE/unknown, the original SwitchHdrMode is called once with a scoped override
of its own force-disable property read. The property is never written globally.
The original HWC updates its own HDR state and HDMI colorspace consistently;
hardware HDR automatically resumes when its real planner selects DEVICE again.
Explicit HDR/ten-bit client targets, unknown plans and multiple outputs bypass
this rule. Existing eight-bit GPU composition is not upgraded to ten-bit by this
fix; no pixel conversion, source dataspace edit or extra validation pass is added.
See [0.3.2 implementation and validation](../docs/hdr-client-032.zh-CN.md).


## Paired physical HDR test7 — post-install evidence

The unchanged tested installers now have bounded device evidence in [the current validation record](../docs/validation/decode-physical-hdr-081.json) and [scope/limits](../docs/decode-physical-hdr-081.zh-CN.md). Primary PQ/HLG explicit P01054 reaches real 4K AB30 AFBC scanout; actual queued ten-bit target, mixed SDR UI/virtual output and item/whole reboot rollback passed. RGB gray-reference differences remain up to 3/3/4 code values, so this is not bit-exact RGB or full colorimetric acceptance. Mali test9 changes only the exact pairing policy, with GPU/KMD/CSF binaries unchanged. Frame Guard remains the default workaround; the newly reproduced original Exo queue timeout/black screen is still under investigation. Historical sections above and embedded installer validation are build-time snapshots, not broader release acceptance.
