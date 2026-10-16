# Codec2 1.14.16 decoder core for the verified API32 stack

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
