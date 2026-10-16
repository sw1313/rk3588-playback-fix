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
