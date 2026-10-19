# Decode Stack internal P010 pool component

This is an internal Decode Stack component, not a standalone installer. The
independent default-off `p010_copy_pool` selection targets the pinned API32
ARM64 framework and requires the SDK-next core and buffer-transfer option.
The generated private framework variant and source-built `libcp10.so` are
selected together only at boot. Disable the item and reboot for the preceding
framework path; disabling Decode Stack and rebooting restores firmware originals.

The selective AOSP backport fixes P010 byte-stride/depth double counting and
allocates 24bpp output buffers instead of the original 12bpp pool. It keeps all
ten-bit samples, dimensions, FPS, buffer ownership and fences. It does not add
HDR output capabilities or upgrade the whole Android framework. Provenance and
the omitted parts of the upstream change are recorded in `UPSTREAM.json`.

The installer generates the full framework variant on-device from the exact
already-verified Codec2 compatibility component. Neither original nor patched
full firmware libraries belong in the public component package. Public API32
headers and original Apache-2.0 notices are retained with the builder inputs.
Only ARM64 is currently in scope; the 32-bit framework keeps its previous path.

The isolated test invokes the actual AllocateEmpty method without opening a HAL.
Its checks and 90-frame precision comparison do not substitute for installed
playback, seek, compatibility, performance and rollback acceptance.
