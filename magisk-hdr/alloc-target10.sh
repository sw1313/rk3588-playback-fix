#!/system/bin/sh
set -eu
MODROOT=${0%/*}
sha() { v=$(sha256sum "$1");echo "${v%% *}"; }
original=/vendor/lib64/hw/android.hardware.graphics.allocator@4.0-impl-bifrost.so
factory=78bd95f083a08bdf36ed266d9a9ae9923d4d3185e9d33dcaa1ddcbb7baa9fa3b
candidate=49a69a0601883b1bccf7d979e891a049c1a30a9a66596cb47a57fc903ac7bc58
current=$(sha "$original")
if [ "$current" = "$candidate" ];then
 original=/data/adb/modules/rk3588_decode_stack/components/hdr/originals/vendor/lib64/hw/android.hardware.graphics.allocator@4.0-impl-bifrost.so
else
 [ "$current" = "$factory" ] || exit 1
fi
[ "$(sha "$original")" = "$factory" ] || exit 1
mkdir -p "$MODROOT/originals/vendor/lib64/hw" "$MODROOT/variants/vendor/lib64/hw"
cp "$original" "$MODROOT/originals/vendor/lib64/hw/android.hardware.graphics.allocator@4.0-impl-bifrost.so"
output="$MODROOT/variants/vendor/lib64/hw/android.hardware.graphics.allocator@4.0-impl-bifrost.so"
"$MODROOT/bin/byte_patch64" "$original" "$output" "$MODROOT/contracts/alloc-target10-arm64.tsv"
[ "$(sha "$output")" = "$candidate" ] || exit 1
for linear in 0 1;do
 timeout 15 env LD_LIBRARY_PATH="$MODROOT/target:/vendor/lib64:/system/lib64" "$MODROOT/bin/alloc-target-test64" "$output" "$linear"
done
printf '%s\n' "$candidate" > "$MODROOT/alloc-target10-preflight"
