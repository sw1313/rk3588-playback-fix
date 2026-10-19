#!/system/bin/sh
# Generate a PRIVATE selective backport from the exact retained factory HWC.
# Rockchip e3e3d168e524 removes two RK356x-only CheckGLESLayer restrictions.
# On this one ARM64 build both are reached only through the instruction below.
# Replacing that branch with NOP preserves all subsequent guards and the entire
# plane planner, atomic/fence handling and buffer geometry. No live code writes.
set -eu
cd "${0%/*}"
original=system/vendor/lib64/hw/hwcomposer.rk30board.rkhdr.so
expected=ec3e42d172af843a74bad42674d6e5932f0070873c7bb73cf681a285ada44478
patched=47702a172f0b95ecc88dbc60a9b42a09b80841e9b4269ce74d7af916af33901b
[ "$(sha256sum "$original" | cut -d ' ' -f 1)" = "$expected" ]
mkdir -p variants
target=variants/hwcomposer.rk30board.scaled.so
[ ! -L "$target" ] && [ ! -L "$target.next" ]
cp "$original" "$target.next"
# ELF PT_LOAD mapping and complete source/destination hashes are pinned.
# VA/file offset 0x85dc8: cbz w8,+0x64 (28 03 00 34) -> nop (1f 20 03 d5).
printf '\037\040\003\325' | dd of="$target.next" bs=1 seek=548296 conv=notrunc 2>/dev/null
[ "$(sha256sum "$target.next" | cut -d ' ' -f 1)" = "$patched" ]
mv "$target.next" "$target"
chmod 0644 "$target"
chmod 0755 bin/plane_probe64
timeout 10 /system/bin/env LD_LIBRARY_PATH=/vendor/lib64:/system/lib64:/apex/com.android.vndk.v32/lib64 bin/plane_probe64 "$original" "$target"
