# RK HEVC Main10 MediaCodec copy-back candidate

`ffmpeg-rk-hevc-p010-copy.patch` applies to public FFmpeg revision `bf4d5037b4`
(the revision used by the pinned original player's FFmpeg ABI). It adds a
bounded P010/54 copy path and only requests it for RK HEVC known Main10 streams;
it preserves Surface decoding, crop/stride/offset checks and HDR metadata.
This is a local repair, not an existing upstream P010 fix or a whole player upgrade.

`tools/stage_lsposed_p010.py` checks the private input APK's four original
library hashes, strips the source-built `libavcodec.so`, then stages only that
new library, the public FFmpeg source archive, local patch, contract and dav1d
notice. The original APK/libraries, test media and keys must never be staged.
Build against Android NDK (API21), the matching FFmpeg headers/ABI and static
dav1d 1.5.3, with JNI, MediaCodec, shared libraries and LGPLv3 enabled. Preserve
the player's decoder set and exported avcodec API; do not substitute its other
FFmpeg or mpv libraries. FFmpeg's full configure/make procedure and patches must
be retained with any candidate binary; the user-authorized 0.3.7 release includes the accepted payload with matching source and licenses.

Supply the staged directory to Gradle with `-Pp010PayloadDir=<directory>`.
Without it the build removes any previous generated payload and the feature
fails closed. A build containing a payload requires its contract and dav1d
notice. Signing must use the existing local certificate, never a packaged key.

The existing LSPosed module's `hw_main10_copy` switch is off by default. It
preloads the generated client in Yamby's app namespace only after API32/RK3588,
Decode Stack and original ABI/hash checks. Disable it and restart the app/reboot
to restore the original loader; the original app APK and native libraries on
disk are untouched. It never changes HW into HW+ or rewrites player statistics.

The final test6 client requires Decode Stack `p010_copy_pool=1` and the exact
framework/adapter hashes in its contract, in addition to the original app ABI
checks. Turning off the system item or whole stack and rebooting automatically
restores the original native loader. No forced NDK selection or flush/rebuild
workaround remains in the final payload.

Installed HW-menu picture, pause/seek/resume and the paired two-level reboot
rollback passed for the pinned Main10 case. The final payload plus mounted new
framework also passed complete visible P010 byte comparisons for three 4K frames
and three HLG 720p frames. Earlier 90-frame trial results remain separately scoped.
This does not prove every green-screen cause or complete HDMI HDR. See
[the new acceptance record](../../docs/decode-p010-pool-080.zh-CN.md).
