# MPP/VPU compatibility component

> Historical standalone component; its Release is archived. Current installations use the [complete project release](https://github.com/sw1313/rk3588-playback-fix/releases/tag/v0.5.1) and unified Decode Stack. Do not stack this standalone module on it.
> 历史独立组件，其 Release 已归档；当前请使用整套项目中的解码合并模块。

Recommended installation is the [unified decode stack](../magisk-decode/README.md).
This component pairs ARM32/ARM64 MPP **1.1.0** and its matching VPU wrapper,
based on upstream revision `c08762ebfadeb4e986d2fed993bc7a54862d3ebe`.
The kernel and original vendor Codec2/hardware-JPEG consumers stay unchanged.

Decode Stack 0.6.1 adds the scoped `013-av1-native-p010.patch`. It allows the
paired Codec2 core to request full-ten-bit P010 hardware postprocessing for
texture consumers, with explicit stride/size checks. Its private format flag
is not a generic upstream MPP API. Display-composer AV1 surfaces retain the
existing native ten-bit NV15 layout. The patch does not change kernel clocks,
replace a kernel driver or authorize ten-to-eight-bit fallback.

The RK3588 compatibility patches recognize enhanced VEPU2 JPEG kernel client
19, translate the tested Android 12 legacy PREP configuration ABI into modern
configuration keys, and use the resolved byte stride for JPEG multi-core
partition offsets. Only fields selected by the caller's change bits are read;
unsupported enhancement bits are rejected. There is no image-format conversion,
resolution reduction or 10-to-8-bit fallback. The VPU wrapper is paired because
the original wrapper still generates malformed JPEG with the newer MPP pair.

Both ABIs passed original libhwjpeg calls and valid software JPEG decoding for
36 format/size/quality cases: NV12/I420/YUYV; 640x368, 854x480, 1920x1080;
quality 90/99. Active VPU H.264/HEVC Main10 outputs match all visible reference
samples for 12-frame streams, with EOS. IEP2's static-gradient two-output
operation is byte-exact in both ABIs at three resolutions. Moving interlaced
content and physical cameras are not validated. See the unified validation
record for system playback, HDR, encoding and rollback results.

The module requires API32 RK3588 and exact firmware consumer hashes. MPP probes
retain all 86 ARM64 / 96 ARM32 imported symbols; the matching VPU libraries
retain the 25 symbols imported by the audited JPEG/camera/VPU API consumers.
Export matching alone does not establish complete runtime compatibility.
The separate older `/system/lib*/libmpp.so` copies are not replaced.

`tools/build_mpp.py` checks the pinned clean source, builds the compatibility
patches in an isolated copy, declares the verified upstream revision plus the
patch level, and compiles both libraries and preflight helpers. Original firmware
libraries are never shipped. Upstream uses Apache-2.0 or MIT; both license texts
are included. Existing standalone GitHub releases are older builds; use the
unified bundle and its switches for this version.

## Decoder backports in 0.4.0

The verified MPP 1.1.0 base now includes six selected upstream decoder fixes:
VP9 probability update, reset/CSD replay with the empty-cache companion fix,
AV1 metadata buffer capacity and legacy-HDR gating, and the first HEVC hardware
task's PS/RPS initialization. Existing HDR/JPEG compatibility patches remain.
This is not an upgrade of the kernel VPU driver or a wholesale develop snapshot.
See [decode-stack 0.4.0](../docs/decode-upgrade-040.zh-CN.md) and `UPSTREAM.json`.
Use the unified decode package for installation; keep overlapping standalone
modules disabled. `tests/reset_probe.c` links against the candidate `libmpp`
and exercises a consumed parameter-header cache through two reset cycles.

## Local 0.6.2 AV1 grain correction

Restores signaled AV1 film grain on the secondary output, with an independent
`av1_grain` switch in the unified stack. This is a picture fidelity correction;
real AV1 4K60 stutter remains unresolved. No DDR performance lock is retained.
See [validation details](../docs/decode-av1-062.zh-CN.md).
