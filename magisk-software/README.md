# API32 software decoder component

This internal component is installed through the unified decode stack, not as a
separate Magisk module. It rebuilds Android 12.1 r27 AV1/VP8/VP9 Codec2 wrappers
against libvpx 1.17.0 and libgav1 revision
`085032331f60c10055d32ab52932bd7bd77e6f53`. The existing APEX service, core Codec2
ABI, shared libvpx and encoders are retained. Cross-DSO CFI remains enabled.

Ten-bit AV1 and VP9 use P010 with all ten decoded bits for Surface and explicit
P010 clients. This avoids the old
wrapper's eight-bit conversion for non-PQ ten-bit content. Native probes check
every visible sample, including nonzero low two bits; real MediaCodec Surface
tests separately cover pause/resume, seek/flush and EOS. This is a software
fallback upgrade, not a new hardware decoder or an update to app-bundled FFmpeg.
Explicit legacy eight-bit copy requests retain the original conversion contract;
they are not handed an incompatible ten-bit buffer. No eight-bit conversion is
selected for Surface or P010 playback. Use HW+ or a P010-capable client for full
ten-bit quality, as with the existing hardware Codec2 compatibility component.
The stock HWC can fall back to an eight-bit client framebuffer for software HDR
Surface playback despite ten-bit P010 decoder buffers and a ten-bit HDMI driver
state. This component does not certify end-to-end software HDR precision or
promote it over the existing hardware path. See the dedicated HDR report.

The installer generates one exact-hash-guarded change to the device's own codec
XML, enabling its disabled `c2.android.av1.decoder` entry. It does not promote
software decoders over hardware. No firmware XML or proprietary libraries are
redistributed. Late-start binding replaces only three existing activated APEX
library paths. Disable the unified software component or the entire stack and
reboot to restore originals. Software P010 is effective only with Codec2's
selected platform compatibility patches.

`tools/build_software.py --help` describes private SDK link inputs. Its sources
and API32 wrappers are pinned and hash checked. Build the libvpx static input
from the pinned source with:

```text
--target=arm64-linux-gcc --disable-examples --disable-tools --disable-docs
--disable-unit-tests --disable-vp8-encoder --disable-vp9-encoder --enable-pic
--enable-vp9-highbitdepth --disable-shared --enable-static --disable-webm-io
```

Use NDK API32 CC/CXX/AR/RANLIB/STRIP tools; configure and build with a POSIX shell
and GNU Make. The script verifies decoder-only/high-bit-depth/PIC configuration.
The package contains the upstream Apache/BSD licenses and patent grant.
