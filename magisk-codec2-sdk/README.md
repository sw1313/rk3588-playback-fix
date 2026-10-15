# SDK14 Codec2 core on the verified API32 firmware

This optional decode-stack component builds the complete pinned Rockchip Codec2
factory, base, video decoder/encoder, OSAL and store sources from the Khadas
Edge2 Android14 SDK against the existing Android12 vendor ABI. It is not a
replacement Android14 BSP. The existing service adapter selects the new core
for ordinary AVC/HEVC/VP9/AV1 Surface decoding and explicit P010 output; original
secure, encoder and legacy paths and all 21 original registry entries remain.

The API32 port retains full ten-bit P010, bounded packed-to-P010 conversion,
format-aware buffers, color/range hints, HDR10 static information and standard
per-frame HDR10+ metadata. It does not enable Dolby, claim HDR10+ HDMI output,
change pixels through tone mapping, force enhancement or lower stream quality.
The newer source-built MPP 1.1.0 compatibility pair is retained: the SDK's
2024 MPP pair was isolated-tested but is older, and is not redistributed.

Build with `tools/build_codec2_sdk.py --help`, the exact public revision from
`UPSTREAM.json`, NDK29 and the prepared private API32 SDK/link inputs. The
source patch includes all added compatibility headers. Private system files are
link inputs only; the payload contains only the source-built plugin. Use the
result as `tools/build_decode_stack.py --codec2-sdk ...`; this component is not
a standalone Magisk installer.

The `codec2_sdk` selection depends on effective Codec2/MPP/RGA. Disable it and
reboot to restore the previously tested Codec2 backend while retaining the
other decode-stack upgrades. Disable the entire stack and reboot to restore
the original firmware components. Library selection never changes inside a
running playback process. See the versioned validation and SDK audit in
`../magisk-decode/VALIDATION.json` and `../docs/decode-sdk-050.zh-CN.md`.
