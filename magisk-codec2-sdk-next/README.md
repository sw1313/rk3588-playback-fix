# 0.7.8-test1 NV15-to-P010 pair unpacking (experimental)

Independent default-off `p010_pairs` reduces NEON unpack instructions for required
linear ten-bit conversion. Requires latest core and P010 acceleration; reboot to
apply/rollback. Precision, output dimensions, frame count, mapper/fences and row
staging remain unchanged. This is a local optimization, not a newer vendor SDK.
Bounded installed CPU-work reduction, exact ten-bit samples, native HDR and
two-level reboot rollback passed. Existing explicit-P010 HDMI-HDR limitation
reproduces identically ON/OFF; see [scope and results](../docs/decode-p010-pairs-078.zh-CN.md).

# 0.7.7-test1 HEVC EOS input ownership (experimental)

Default-off `hevc_eos_pending` returns consumed non-picture HEVC inputs after actual
decoder EOS. It does not create image frames or change precision, fences, slots or
reference frames. Requires the latest core; reboot to apply and roll back. Bounded device input ownership, ten-bit/HDR and two-level reboot rollback passed;
see [0.7.7 validation](../docs/decode-hevc-eos-077.zh-CN.md). Historical UHD/HLG
timeouts are not claimed fixed.

# 0.7.6-test1 HEVC native display (experimental)

Default-off `hevc_native_surface` keeps existing HEVC NV15 composer output
when info-change first discovers ten-bit. This is a local API32 compatibility
guard correction, not a full new SDK. Explicit P010, CPU/texture, encoder, tunnel
and output-crop retain their prior paths. FBC requests and fences are unchanged.
Requires the selected next core and HDR adapter; disable and reboot to restore
the preceding info-change conversion. Bounded new device acceptance and two-level reboot rollback passed; see the
parent 0.7.6 report. GPU-only original-player UHD pacing and universal display
latency remain outside this option.

# Codec2 output-delay notification extension (Decode Stack 0.7.4-test1)

`patches/codec2-delay-notify.patch` selectively backports independent
configuration delivery and the paired reserved-index/INCOMPLETE correction.
The existing slot calculation and low-memory policy are retained. The separate
`codec2_delay_notify` switch defaults off and requires reboot. Source pins,
excluded changes and API32 adaptations are in UPSTREAM.json. See
`docs/decode-output-delay-074.zh-CN.md` in the source snapshot for actual bounds.

---

# Gralloc HDR transfer extension for Decode Stack 0.7.3-test1

The local `patches/gralloc-hdr-lease.patch` transfers an already validated
mapper import into the current OutBuffer. The HDR bridge borrows it for
metadata writes; flush/release still clean up buffers. No global FD cache,
pixel conversion, AFBC layout, tunnel behavior or fence policy is changed.
`gralloc_hdr_lease` defaults off and requires static HDR plus this latest core.
Upstream screening, API32 ownership reference and local scope are recorded in
UPSTREAM.json. Final bounded device evidence is in
`docs/decode-gralloc-transfer-073.zh-CN.md` of the source snapshot.

---

# Codec2 1.14.16 decoder core for the verified API32 stack

The local 0.7.1-test1 candidate adds `codec2-lifecycle-backports.patch` and the
canonical `include/C2RKPoolRetry.h`. `codec2_lifecycle` defaults off and requires
reboot. UPSTREAM.json records each selected Rockchip-authored commit and the
partial-backport scope. The source version remains 1.14.16_[0]. New-route empty
work, held-output stop, flush/seek and bounded eight/ten-bit precision checks
passed; see the unified module VALIDATION.json and the lifecycle report. Busy
pool cancellation is a local companion, not an upstream patch attribution.

The historical 0.7.0-test2 candidate additionally applies
`patches/av1-texture-afbc.patch`. Its separately controlled GPU texture route
defaults off and excludes CPU/encoder, crop/tunnel, explicit P010 and
FBC-disable requests. Initial widths must be divisible by 64. Nonaligned or
linear-layout info-changes fall back to the existing full-precision copy path
before MPP resumes. Disabling `av1_texture_afbc` and rebooting restores the
prior texture policy. New-route device tests matched 151,787,520 samples across
21 eight/ten-bit frames and measured 30.222% less memory per tested UHD ten-bit
output allocation. Adaptive fallback and reboot rollback passed. Original-player
UHD image acquisition timeouts and present-window gaps remain unresolved, so
this stays experimental/default-off. See `VALIDATION.json`; earlier composer
tests are historical evidence only.

Decode Stack 0.6.3 adds `patches/av1-afbc-surface.patch` after the two existing
patches. The independently selectable `av1_afbc` route uses lossless ten-bit
AFBC for RK3588 composer surfaces when width is divisible by 64. Other widths,
explicit P010, texture/CPU consumers and client FBC-disable retain their prior
paths. The crop uses the completed AV1 frame's filter-dependent top padding.
See the unified 0.6.3 report for real-source playback and pixel comparisons.

This optional decode-stack component ports the decoder, component base, OSAL and
store from the pinned Khadas Rockchip Android14 source revision in UPSTREAM.json.
It retains the existing Android12/API32 firmware, HIDL service and original kernel.

Decode Stack 0.6.1 additionally applies `patches/av1-native-surface.patch` after
the API32 port patch. RK3588 AV1 output uses actual consumer usage to choose a
full-ten-bit NV15 display plane or validated standard P010 texture output.
AV1 Main profile alone never authorizes ten-to-eight-bit conversion. The
independent `av1_native` switch restores the previous full-precision copy path.
The private P010 format flag requires the paired MPP compatibility patch; it is
not a generic upstream MPP interface. See the unified versioned test report.

The API32 adapter retains full ten-bit P010, bounded NEON conversion, mapped plane
layouts, original AFBC/color policy and both existing HDR metadata bridges.
Copy-mode decoder buffers use MPP's cacheable internal allocation rather than
assuming the newer gralloc private decoder-tail layout. Source CPU reads pair
with read-only DMA synchronization; output synchronization remains owned by the
existing mapper. Check the unified module's versioned validation before release.

The newer standard work flow and output-delay logic are retained. A compatibility
repair makes the output handler's running flag atomic and stops it behind a
message barrier without reopening callbacks before synchronous EOS drain. This
keeps output frames and copy blocks under one consumer during that handoff.
Optional new Binder dump registration is omitted: the retained API32 service owns
registration. This candidate factory is decoder only; the existing router keeps
firmware encoder and secure paths. Neural encoder code/models are not bundled.

Build with tools/build_codec2_sdk.py --sdk-next, NDK29 and the exact pinned source
and API32 link inputs. Private firmware libraries are link inputs only and are
never included in the payload. Assemble with tools/build_decode_stack.py using
--codec2-sdk-next together with the previous --codec2-sdk package.

Set codec2_sdk_next off and reboot to restore the separately retained SIMD core.
Set codec2_sdk off to restore the legacy backend. Whole-module rollback restores
the firmware. Dependency suppression preserves requested choices. No switch
hot-replaces a loaded library. No quality reduction or clock increase is applied.

The version number alone does not prove universal real-time playback, new hardware
formats, HDMI HDR10+ or Dolby Vision support. See the recorded test limitations.
