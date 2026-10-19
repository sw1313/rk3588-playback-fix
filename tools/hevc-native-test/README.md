# HEVC late-profile display/HDR acceptance

Isolated `com.rk3588.nativehdrtest` fixture for the Decode Stack native-display
option. It queries and requests linear output, removes the configuration profile
and retains source packets and full video dimensions. Each fresh playlist case
pauses, resumes, flushes/seeks and drains to EOS. It reports MediaCodec metadata;
an independent collector must verify DRM/HDMI output while that case is active.
PixelCopy is eight-bit and cannot establish ten-bit precision; use the separate
native HEVC precision probe and independent software references.

Optional `--ez explicit_p010 true` explicitly requests HAL format 54 before
configuration and records that choice. This exercises required full-precision
P010 conversion instead of the native NV15 bypass, including HDR/seek/EOS.
The default remains the original native-display diagnostic configuration.

Build with `tools/decode-performance/build.py --source tools/hevc-native-test
--sdk ... --work ... --keystore PRIVATE_PATH`. The private signing key, APK,
media and raw device evidence must stay outside public packages. Install only
for bounded acceptance, then uninstall the fixture. This app is not a module
deliverable and does not change the user's player.
