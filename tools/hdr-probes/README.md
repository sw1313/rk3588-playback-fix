# HDR interface diagnostics

These are development diagnostics, not a Magisk installer or HDR enablement tool.
They were run on the verified API32 firmware; they do not certify arbitrary firmware.

- `drm_hdr_probe.c` reads connector, CRTC and plane properties through DRM GET
  ioctls. Enabling atomic property visibility affects only its file descriptor.
  It does not take display master, perform atomic commits, or change display properties.
  It also reports each plane's actual fourcc format list through GETPLANE.
- `client_hdr_caps.c` enumerates EGL configurations and colorspace extension
  availability. It creates no context, surface or buffer. RGB10/FP16 enumeration
  alone is not proof of working HDR composition, allocation or scanout.
- `mapper_hdr_probe.cpp` lists mapper 4.0 metadata types, allocates one private
  64x64 RGBA buffer, queries standard HDR types and queries/sets a zero Rockchip
  private HDR offset on that scratch buffer. It never submits the buffer for
  display and releases it on exit. It does not modify another application's buffers.
- `verified-api32.json` contains sanitized observations and explicit boundaries,
  without device addresses, EDID identity, media, or account data.

Build using NDK 29, the prepared API32 VNDK headers and original ARM64 libraries
as private link inputs. Those libraries must never be included in distributions.

```text
python tools/hdr-probes/build.py --ndk <ndk> --headers <vndk32-headers> --private-libs <vendor64> --drm-headers <directory-containing-drm.h-and-drm_mode.h> --work <output>
```

The build script performs no device operations. Run the resulting programs with
root on the intended connected device to obtain local observations. This may log
buffer-allocation diagnostics; it does not install persistent settings or reboot.
Do not enable display capabilities based solely on a feature bit or scratch-buffer test.

See [the integration investigation](../../docs/hdr-integration-feasibility.zh-CN.md)
for the distinction between metadata transport, tone mapping and actual HDMI signaling.


`atomic_reuse_test.cpp` includes the actual HWC adapter commit hook and replaces
only its property reader and DRM interface with an isolated fixture. Build it
with `tools/build_hdr.py --atomic-test` and the same private link inputs; the
executable stays outside the module package. Its 16 cases cover identical and
changed PQ/HLG metadata, SDR, TEST_ONLY, multiple outputs, failed blob reads,
request duplication/property/commit failures and the default-off path. It never
opens the live DRM node. Hardware output and reboot rollback require separate
on-device checks; this fixture does not substitute for those checks.

`mode_cache_probe.c` exercises the hash-pinned factory GetDisplayAttribute and
SetActiveConfig methods on isolated synthetic objects, mocking mode setters and
DRM updates only within its own process. It reproduces the advertised/raw-list
mismatch and compares the source candidate in `magisk-hdr/src/mode_cache.h`
against the actual normal-path methods. Decode Stack 0.7.9 now integrates it
behind `hwc_mode_cache`; separate installation evidence is in the
[0.7.9 addendum](../../docs/decode-hwui-rgb10-079.zh-CN.md).
Build with NDK clang for aarch64-linux-android32, `-O2 -Wall -Wextra
-Werror -ldl`. Before running, independently verify the private input against
one of the exact hashes in
`docs/validation/playback-root-causes-20261018.json`; do not use other HWC files.
The executable's address checks alone are not a complete firmware hash guard.
Never distribute the private HWC input. This is not live Exo, HDMI or rollback
acceptance and does not authorize changing Frame Guard.
