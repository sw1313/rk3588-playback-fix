#!/system/bin/sh
set -eu
MODROOT=${0%/*}
sha() { v=$(sha256sum "$1"); echo "${v%% *}"; }
factory=fddde345b957a0898c44cf1c62d23e9736d98d246e46bb3f39244c0e7d6b0d1a
candidate=a6f27121a627b42f53066756262312b3afe80800271afe14e57923081a72929f
original=/system/bin/surfaceflinger
current=$(sha "$original")
if [ "$current" = "$candidate" ]; then
 original=/data/adb/modules/rk3588_decode_stack/components/hdr/originals/bin/surfaceflinger
else
 [ "$current" = "$factory" ] || exit 1
fi
[ "$(sha "$original")" = "$factory" ] || exit 1
mkdir -p "$MODROOT/originals/bin" "$MODROOT/variants/bin"
cp "$original" "$MODROOT/originals/bin/surfaceflinger"
"$MODROOT/bin/byte_patch64" "$original" "$MODROOT/variants/bin/surfaceflinger" "$MODROOT/contracts/sf-target10-arm64.tsv"
[ "$(sha "$MODROOT/variants/bin/surfaceflinger")" = "$candidate" ] || exit 1
timeout 15 env LD_LIBRARY_PATH=/system/lib64 "$MODROOT/bin/sf-target-test64" "$MODROOT/target/libsfhdr.so" "$MODROOT/originals/bin/surfaceflinger"
chmod 0755 "$MODROOT/variants/bin/surfaceflinger" "$MODROOT/originals/bin/surfaceflinger"
printf '%s\n' "$candidate" > "$MODROOT/sf-target10-preflight"
