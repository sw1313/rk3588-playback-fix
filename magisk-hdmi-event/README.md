# Internal HDMI event component

For Decode Stack on the pinned API32 firmware only. This is a local compatibility
repair, not an upstream kernel or whole Composer upgrade. Installer inputs are
exact-hash checked and firmware copies are generated on the device, never bundled.

The native UEventObserver import in ARM64 `libandroid_runtime` is adapted only
inside `system_server`. Only the exact `fde80000.hdmi/extcon5` HDMI-off event is
deferred while independent DRM status is `connected`. A matching HDMI-on within
400ms coalesces that transient reset; all original event bytes remain unchanged.
Real disconnect, unreadable/unknown physical status, or timeout forwards the off
event. Unrelated events pass immediately; duplicate off cannot extend the bound.
This does not alter decoder work, statistics, fences, audio HAL or HDMI timing.

`hdmi_audio_guard` defaults off and requires reboot. A first framework restart
may be needed after Magic Mount; GPU first binding and Zygisk selection are not
changed. The module validates actual system_server mapping/readiness, otherwise
disables and reboots to originals. Item-off/whole-off plus reboot restores the
factory runtime and removes the helper overlay.

Experimental: isolated tests do not establish real unplug, audio continuity,
player behavior or final blackout repair. Those require separate device evidence.

Device follow-up: actual Exo startup/pause/resume and real unplug/replug are
recorded separately in [the Decode Stack addendum](../docs/decode-hdmi-event-082.zh-CN.md).
This is bounded event repair; actual HDMI timing blanks and precise player seek
remain separate acceptance targets. The tested installer is immutable; later
source metadata is not represented as having been embedded in that ZIP.
