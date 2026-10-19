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

## Upstream lifecycle backport checks

`empty_input_probe.cpp` uses a GPU-only PRIVATE ImageReader Surface. Arguments:
`CLIP REAL_FRAME_LIMIT EMPTY_WORK_COUNT [FLUSH_AFTER_OUTPUTS]`. It submits empty
non-EOS work, then unmodified compressed packets, and requires the exact real
frame count plus EOS. Optional flush seeks to the start, resets the counters,
and requires a complete fresh sequence. Check that the newly patched core is
the actual selected decoder; a launch or timeout is not successful playback.

`surface_stall_probe.cpp CLIP ROUNDS` retains all dequeued MediaCodec output
indices and requests stop while decoded buffers are held. Stop intentionally
releases those client-held outputs. An ImageReader that is merely not acquired
can discard images asynchronously and does not prove allocation backpressure.
Require successful rounds and the component's actual cancellation log, without
counting unrelated older log lines or substituting this for a frame census.

Build these independent probes with NDK API32, C++17, `-static-libstdc++`,
`-lmediandk -landroid`. Run under an external process timeout. They do not modify
module switches, system properties or the player.

`pool_retry_probe.cpp` uses the exact canonical C2RKPoolRetry.h with a controlled
C2_BLOCKING backend and a cancellation request. Build for ARM32 and ARM64 with
that include directory plus the public API32 Codec2 core headers, C++17 and
`-static-libstdc++`. Its simulated delays are not playback performance results.

## Independent output-delay notification

`output_delay_probe.cpp CORE_LIBRARY COMPONENT EXPECT_ENABLED` starts a private
component with a listener, without queuing compressed input. It queries the
actual output delay and checks the config-only notification, reserved work ID,
FLAG_INCOMPLETE, zero decoded buffers and zero input completions. With the
feature disabled, the interface value remains set but no independent callback
is expected. Run with the actual reboot-selected module property; do not flip a
live device property to simulate reboot rollback.

Build against the core builder's generated API32 project and its private link
inputs (none are copied into the diagnostic):

```sh
python tools/decode-lifecycle/build_output_delay_probe.py --core-project /private/core/project/CMakeLists.txt --ndk /inputs/ndk --work /private/new-probe-build
```

`output_delay_stream_probe.cpp CLIP FRAME_LIMIT EMPTY_INPUTS FLUSH_AFTER CONFIG_WIDTH CONFIG_HEIGHT`
checks configured dimensions versus the actual compressed stream, completes an
exact frame count and EOS, and optionally flushes/seeks. It requires unique
timestamps within each phase and the real decoded dimensions; it does not
resize the source. Build with the same NDK flags as the other MediaCodec probes.

## HEVC EOS input ownership

`eos_pending_probe.cpp CORE_LIBRARY CLIP FRAME_LIMIT AUD_INPUTS [REENTER]`
loads the actual selected API32 core and queues CSD, legal non-picture HEVC AUD
packets, unchanged compressed picture packets and EOS. It snapshots input
completion counts before stop can conceal an EOS leak. `pts_exact=1`, exact
picture count, zero errors and zero duplicate completions are mandatory.
The caller must separately require `missing_before_stop=0` when enabled, or the
reproduced AUD count when testing rollback; a zero process exit alone does not
validate that ownership result. This is an ownership census, not a FPS test.

Use `REENTER=1` only for the enabled candidate: on each AUD completion the
listener calls `queue_nb` with an empty list and checks the original ordinal.
Require `reentered=AUD_INPUTS`. A legacy callback during stop may hold internal
locks, so the disabled control uses `REENTER=0`. All runs need an external
process timeout. Read the real reboot-selected property; do not simulate
rollback by changing a live property.

```sh
python tools/decode-lifecycle/build_eos_pending_probe.py --core-project /private/core/project/CMakeLists.txt --ndk /inputs/ndk --work /private/new-eos-probe
```


## Native Surface backpressure candidate

`native_backpressure_probe.cpp CLIP FRAME_COUNT HOLD_SECONDS [CONFIG_WIDTH CONFIG_HEIGHT]`
uses asynchronous callbacks, a composer/private Surface, real held output indices
for 5–15 seconds, and an exact decoded-frame/PTS/EOS census after resume. It does
not send player inputs or capture the screen. Optional configured dimensions
exercise growing-only output-delay retention across real stream info-change;
the compressed source is not scaled or rewritten. Require logs proving the
quality core, eligible native composer and actual backpressure branch. Both
on/off passing is not proof of fixing the user's Exo error. An underlength clip
is a failed census, not a codec failure. Use an external process timeout.
