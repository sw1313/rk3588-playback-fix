# HDR interface diagnostics

These are development diagnostics, not a Magisk installer or HDR enablement tool.
They were run on the verified API32 firmware; they do not certify arbitrary firmware.

- `drm_hdr_probe.c` reads connector, CRTC and plane properties through DRM GET
  ioctls. Enabling atomic property visibility affects only its file descriptor.
  It does not take display master, perform atomic commits, or change display properties.
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
