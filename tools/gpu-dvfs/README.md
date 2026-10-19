# GPU response diagnostic

`burst.c` runs identical offscreen 512×512 GLES3 shader draws in bursts, with
800ms idle time before each burst. It synchronizes every draw with `glFinish`,
checks GL errors and verifies the same 8×8 RGBA readback hash on every frame.
It records monotonic completion timing and the frequency observed before/after
each draw. It never changes clocks, voltage, thermal policy or devfreq settings.

Build with the Android NDK compiler for the host platform:

```sh
clang --target=aarch64-linux-android32 -O2 -Wall -Wextra -Werror \
  tools/gpu-dvfs/burst.c -lEGL -lGLESv3 -o /private/burst64
python tools/gpu-dvfs/collect.py --adb /inputs/adb --serial DEVICE \
  --binary /private/burst64 --out /private/new-capture
```

The collector requires root and waits for the module's userspace **and** paired
kernel to be qualified. Select GPU dynamic response through the Mali Global
WebUI and reboot before collecting an on/off case. The collector only reads
the active policy; it never changes it. Use new output directories. Media and
playback apps are not used by this workload.

Compare identical frame/burst counts, shader version and readback hash. Preserve
temperature, starting frequencies and other activity in private evidence. The
draw+finish sum includes driver/CPU scheduling, and is not a GPU hardware-timer
measurement. Burst wall time also includes sysfs reads and pixel checks. These
are synthetic response measurements, not game FPS, screen latency, HDR precision
or energy results. Shorter polling increases potential idle polling frequency;
power and long-duration thermal effects require separate measurement.
