# Real-time Surface diagnostic

This separate diagnostic application does not modify any player or hook module.
Build `MainActivity.java` and `AndroidManifest.xml` with the Android SDK, package
the DEX and sign with a local diagnostic key. Do not publish that key or captured
device logs. The test package is `com.rk3588.perftest`.

Grant local video read permission and start with an accessible local clip:

```sh
am start -n com.rk3588.perftest/.MainActivity \
  --es clip /sdcard/Movies/test.mp4 --es codec c2.rk.av1.decoder \
  --es case av1-local --ei frames 360
```

Set `frames` to the independently probed frame count. The application feeds an
asynchronous MediaCodec, schedules output to a Surface by presentation time with
a 200ms initial margin, and writes a JSON result plus a PixelCopy capture to its
own external files directory. It reports input/output counts, EOS, output-deadline
lateness, render callbacks and stop completion. Timings depend on workload and
thermal state; repeat longer clips when assessing sustained playback.

Use a unique `case` per build/run, or remove that case's old JSON/PNG before
starting and verify their absence. Polling an existing report can otherwise
mistake a previous run for completion of the current one.

Some vendor render callbacks echo scheduled timestamps. Perfect callback gaps
alone do not establish smooth playback. Inspect `late_gt20ms`, maximum lateness,
complete frame/EOS counts and actual player drop counters. Local files exclude
network starvation but do not reproduce a streaming player's entire pipeline.

## Surface / texture display-path comparison

The default `output=surface` remains the direct SurfaceView test. Passing
`--es output texture` uses a TextureView consumer with the same asynchronous
MediaCodec feed, input and 200ms startup margin. The latter includes Android's
texture rendering and may change output precision, HDR handling and buffer
layout. It is a diagnostic workload, **not an equivalent renderer or quality
reference for mpv/Yamby**, and must not be enabled as a playback optimization.
Both modes decode the full source; neither intentionally drops late frames.

Build and install the independent diagnostic (JDK, Android SDK required):

```sh
python tools/decode-performance/build.py --sdk /inputs/android-sdk --work /private/new-build --keystore /private/diagnostic.keystore
adb -s DEVICE install /private/new-build/rk-display-path.apk
adb -s DEVICE shell pm grant com.rk3588.perftest android.permission.READ_EXTERNAL_STORAGE
```

Keep the key and APK local. The build directory must be new. The builder uses
the newest installed numeric SDK platform and build-tools; record the chosen
versions when comparing builds.

The collector requires an explicitly selected ADB serial and root access for
vendor counters. Obtain the expected frame count separately, for example with
ffprobe packet counting on a source with one displayed frame per packet.

```sh
python tools/decode-performance/collect_display_path.py --serial DEVICE --out /private/new-surface-run --mode surface --clip /sdcard/Movies/test.webm --frames 3597 --seconds 85
python tools/decode-performance/collect_display_path.py --serial DEVICE --out /private/new-texture-run --mode texture --clip /sdcard/Movies/test.webm --frames 3597 --seconds 85
```

Use `--adb /path/to/adb` if needed. On Windows use one current SDK ADB version
consistently; old clients can restart the shared server and interrupt tests.
Each capture directory is new and each diagnostic launch gets a UUID that must
match its fresh report. The collector records GPU/DMC policy and frequency,
temperature, RGA load, DRM plane format and crop, MPP sessions, SurfaceFlinger
state and a present-fence latency ring. It reads logs without clearing the
shared buffer. It changes no DVFS, display, driver or module switches. It starts
and stops only the selected independent diagnostic application.

`--mode observe --seconds 25` captures an already playing app without launching
or stopping it. `--mode yamby --clip DEVICE_PATH --seconds 25` opens a local file
in Yamby's existing mpv activity and leaves it running. The file must be
accessible to Yamby under Android's storage rules: a successful activity launch
does **not** establish successful playback. Inspect the lowercase `mpv` log
for file-open errors, actual video format and hardware decode selection.
`capture_complete` means only that snapshots completed. Do not substitute it
for an EOS/frame-count result.

Optional `--mode exo` uses the **already installed** historical
`com.rk3588.localtest` diagnostic and its installed SmartTube engine; that APK
and the engine are not supplied by this builder. It validates the newly unique
case label against the report. Engine/harness versions and hashes must be
recorded separately before comparing results.

Interpretation rules:

- Inspect the physical HDMI display separately from virtual mirroring displays.
  A scrcpy display using CLIENT does not imply HDMI fell back to CLIENT.
- Physical DEVICE composition alone does not prove the original YUV video is
  scanned out: an app may already have rendered it into an RGB buffer on the GPU.
- AB24 is eight-bit RGB; AB30 is ten-bit RGB. Neither an RGB plane's width nor
  HDMI ten-bit signaling proves that all source detail/precision survived.
- The latency files contain overlapping windows, not a full-run frame census.
  Zero/pending present fences and duplicate timestamps are excluded. Empty
  windows are unknown, never evidence of smooth playback. Do not sum the
  overlapping windows or confuse them with panel optical measurements.
- `complete` means full decode/EOS; high lateness can coexist with `complete`.
  Callback timestamps may echo the requested time even when decoding is late.
- Preserve active mirroring in the evidence. If it cannot be disconnected,
  treat results as measurements under that extra load and avoid a clean-device
  or universal performance claim. Dynamic frequency and temperature also vary.

Raw captures can include private app state, media names, buffer addresses and
device details. Store them in ignored `dist/` or outside the repository. Publish
only a reviewed, anonymized summary. Parser checks:

```sh
python -m unittest discover -s tools/tests -p test_display_path.py
```

For the new texture AFBC route, `texture_precision.cpp` creates a GPU-only
AImageReader consumer, decodes a bounded number of frames through the installed
Codec2 service, and synchronously decompresses the **actual** ten-bit AFBC
allocations with RGA. It supports eight- and ten-bit AFBC. On API32 the image
crop can omit decoder padding: use per-buffer `AMediaCodec_getBufferFormat`
crop matched to the acquired image by PTS, then write visible planar
little-endian uint16 Y/U/V samples. Compare these samples with a software
`yuv420p10le` reference, including nonzero low two bits. For eight-bit references,
compare each uint16 value with the corresponding uint8 value, without
reinterpreting pairs of reference bytes as uint16. This verifies decoded
sample precision; it does not independently validate a player's HDR tone map
or the HDMI display. Compile with NDK API32, the pinned MPP/RGA public headers
and compatible ARM64 library link inputs (`mediandk`, `android`, `dl`).
Invocation: `texture_precision CLIP FRAME_LIMIT RAW_OUTPUT [SOURCE_BITS=10]`.

Example build with the module's pinned public MPP/RGA headers and matching
source-built ARM64 libraries (all paths are local inputs):

```sh
clang++ --target=aarch64-linux-android32 -std=c++17 -O2 -static-libstdc++ \
  -I/inputs/mpp/inc -I/inputs/mpp/osal/inc \
  -I/inputs/librga/include -I/inputs/librga/im2d_api \
  tools/decode-performance/texture_precision.cpp -o /private/texture_precision \
  /inputs/lib64/libmpp.so /inputs/lib64/librga.so -lmediandk -landroid -ldl
```

Use the NDK compiler executable for the host platform. The device must already
load the matching MPP/RGA libraries. Keep media and raw output private.

To test an aligned initial configuration followed by a nonaligned stream header,
use `--configured-width 1920 --configured-height 1080` with the 854-wide texture
clip. Only the diagnostic configuration is overridden; the encoded media is
unchanged. Verify the reported actual output size, complete frame count and
explicit full-precision fallback message. Do not call it native AFBC coverage.

When Mali's optional kernel handover is selected, wait for **both**
`vendor.rkmaliglobal.ready=active` and `vendor.rkmalikernel.ready=active` before
starting playback. Userspace readiness alone precedes kernel qualification.
Playback during that interval can keep the GPU active while the supply bridge
requires runtime suspension, triggering the existing guarded reboot fallback.

For the bounded HWC scaling-policy check, `--es output canvas --ei canvas_frames
1200 --ei display_width 1920 --ei display_height 1080` draws a moving 641x360
linear RGBA SurfaceView. The logical window and physical output must remain
identical between rebooted switch states. Wait for startup overlays to disappear,
then collect an equal SurfaceFlinger TimeStats interval and DRM plane state while
the producer is still moving. `drawn_frames` counts successful buffer posts;
completion requires the requested count. PixelCopy only checks source equality
at eight-bit precision, not the final HDMI image or ten-bit output precision.
TimeStats millisecond buckets are quantized software timings, not optical
latency. Their nominal 16ms period must not be reported as 62.5 actual FPS.

For the HEVC late-profile native-display check, `--ez force_linear true --ez
omit_profile true` queries the real codec vendor-parameter list, requires a
single FBC-disable key, requests linear output and omits only the configuration
profile. The encoded stream is unchanged. The collector must independently
verify stream dimensions, bit depth and frame count; use a fresh case label.
An already declared ten-bit profile can use the preceding native route and is
not evidence of this option's benefit.

`native_hevc_precision.cpp` checks the actual composer ImageReader path. It
waits for image acquisition, queries mapper plane offsets/strides, pairs DMA-BUF
read synchronization and reads NV15 or P010 samples without writing pixels.
Build with `build_native_hevc_precision.py --ndk ... --headers ... --private-libs
... --work ...`. Private inputs are the API32 ARM64 ui/utils/cutils/libc++ link
libraries; none are distributed. Run `native_hevc_precision64 CLIP FRAME_LIMIT
RAW_OUTPUT EXPECTED_HAL_FORMAT [CONSUMER]`, where format is decimal 23 (NV15) or
54 (P010), and consumer is 0 (composer), 1 (texture) or 2 (explicit P010 request).
Compare all visible planar uint16 samples to an independent software decoder,
including a source with nonzero low two bits. HAL format alone does not prove
precision. This readback does not certify the physical HDMI wire or panel.
