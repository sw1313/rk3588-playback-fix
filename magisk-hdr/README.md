# HDR10 static metadata adapter

Unified-module component for the exact verified API32 firmware; not a generic
replacement display driver and not a standalone installer.

The original ARM64 HWC is copied from the device during installation after an
exact hash check. It is never included in the downloadable package. A small
adapter forwards the original HWC2 device functions and observes standard
SMPTE2086/CTA861.3 buffer metadata written by the Rockchip Codec2 bridge.
It adds these values to a duplicate of the original DRM atomic request, preserving
global HDR disable/area settings and all original pixel planes, formats,
fences and composition policies. The original request remains untouched and is used if the
additional metadata transaction is rejected. HDR10+ and Dolby capabilities are
not advertised or spoofed.

The adapter changes five original HWC import-table pointers in its own process,
temporarily restoring write access only to their data pages, then restoring the
original page permissions. It does not patch executable instructions or kernel
memory. Installer hash/ABI checks and a separate-process check gate this path.

Unified switches `hdr_static` and `hdr10plus` are independent and require reboot.
`hdr_static` needs the selected compatible Codec2 component; disabling it removes
the HWC overlay and restores the original HWC. `hdr10plus` controls the standard
Codec2 HDR10+ info bridge and does not imply HDMI HDR10+ support.

This firmware's mapper rejects clearing its variable-size HDR10+ buffer field.
The bridge therefore leaves that field untouched and carries dynamic data through
standard C2Buffer/BufferQueue info, with explicit empty frames. Static metadata
uses explicit zero structures when absent so reused buffers cannot retain it.

`bin/packet_probe64` reads the HDMI-QP static InfoFrame registers and validates the
header/checksum. The stock kernel debug `status` static fields are one byte early;
use the decoded complete packet for those fields. Register verification is not a
wire measurement. This component neither claims Dolby Vision nor lowers picture
bit depth, resolution or bitrate. Applications that render into their own buffers
must still supply correct output metadata; no application libraries are changed.

EOTF transitions survive TEST_ONLY validation until the same request is really
committed. The original request-free import clears abandoned state; a zero HDR
blob explicitly counts as an SDR/disabled transition. The wrapper preserves SDR/disable decisions. On a single active
output it also
repairs stale PQ/HLG EOTF using a homogeneous validated DEVICE HDR layer and
matching active planes in that same atomic request. The exact original HWC maps
PQ planes to 2 and HLG planes to 0; the connector still uses 2 and 3 respectively.
Conflicting, unknown, incomplete or client-only composition is bypassed. HLG
clears HDR10 mastering/light fields. This repair shares the `hdr_static` switch;
turning it off and rebooting restores the original HWC. See
[0.3.1 validation and limits](../docs/hdr-transition-031.zh-CN.md).


Version 0.1.2 also repairs native SDR client output while retaining the original
plane plan. A hash-pinned, color-managed SurfaceFlinger produces an eight-bit
AB24/UNKNOWN sRGB client target. If all HDR video layers are CLIENT and none is
DEVICE/unknown, the original SwitchHdrMode is called once with a scoped override
of its own force-disable property read. The property is never written globally.
The original HWC updates its own HDR state and HDMI colorspace consistently;
hardware HDR automatically resumes when its real planner selects DEVICE again.
Explicit HDR/ten-bit client targets, unknown plans and multiple outputs bypass
this rule. Existing eight-bit GPU composition is not upgraded to ten-bit by this
fix; no pixel conversion, source dataspace edit or extra validation pass is added.
See [0.3.2 implementation and validation](../docs/hdr-client-032.zh-CN.md).
