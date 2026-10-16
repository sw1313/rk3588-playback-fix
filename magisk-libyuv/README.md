# System libyuv compatibility build

This component is included in [the unified decode stack](../magisk-decode/README.md).
It builds libyuv 1974 at revision `0b9e5c922deec7c9921ae385f6242a9a27754d6e`
for the verified system ARM64 API32 ABI. All 710 original exports remain;
59 removed APIs use isolated Android 12 public-source objects. Existing JPEG
support is retained through the hash-pinned original libjpeg dependency.
Firmware files are private build inputs and never enter the package.

227 device cases passed, including exact full-precision I010/P010 round trips,
rotation against a coordinate reference and JPEG decoding against the original.
An API32-header MJpegDecoder caller passed object-size/canary checks, valid JPEG
pixel comparison, malformed-input handling and the old integer boolean ABI.
The device has no system ARM32 libyuv copy, so no ARM32 replacement is added.

Use the unified module's `libyuv` switch and reboot. This affects system callers
of this library; it does not update Yamby's bundled FFmpeg or mpv code. It does
not force 10-bit data into 8-bit conversion paths. See `tools/build_libyuv.py`
for the source pins and private-input requirements. Upstream and Android 12
BSD licenses and PATENTS are included under `licenses/`.
