# Codec2 buffer lifecycle probes

These independent probes use existing MediaCodec components and accept a local
video path. They do not patch the player or install hooks. Benchmark APKs,
benchmark video assets, signing keys and device logs are not distributed.

`linear_probe.c` is compiled with the Android NDK for API32, linked with
`mediandk`. Its arguments are `FILE CODEC MODE FRAMES [FIRST_FRAME_DUMP]`.
Modes: 0 sends EOS after FRAMES input packets; 1 stops early after FRAMES queued
packets; 2 stops while holding an output buffer; 3 flushes/seeks once after
FRAMES outputs; 4 retains output buffers for three seconds before stopping.
Check the exit status, output frame count, EOS status and Codec2 service process
identity. Frame dumps include allocator/layout padding and must be interpreted
using the reported output layout, not compared blindly as planar I420.

`AsyncActivity.java` can be built into a separate API32 test application using
an Android SDK. Package: `com.rk3588.lifecycle`; exported activity:
`.AsyncActivity`. Intent arguments: `clip` (readable local MP4), `codec`
(explicit component name), `rounds` (integer), `case` (report basename).
The app writes JSON under its own external-files directory. It queues actual
compressed video with asynchronous callbacks, waits for decoded output and
stops while input/output work remains. It records exceptions and completion
counts; the runner must separately check that the shared Codec2 HAL did not die.
Use a dedicated local signing key. The test app has no special permissions.

Use the same files and options for original-stack, pre-fix and repaired-stack
comparisons. A stopped/failed run is not a throughput measurement. Capture and
inspect tombstones before a test; older crash files must not be counted as new
failures. Keep raw device diagnostics private.
