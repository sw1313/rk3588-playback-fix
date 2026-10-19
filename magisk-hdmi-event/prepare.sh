#!/system/bin/sh
set -eu
MODROOT=${0%/*}
sha(){ v=$(sha256sum "$1");echo "${v%% *}"; }
original=/system/lib64/libandroid_runtime.so
factory=@ORIGINAL@
candidate=@PATCHED@
current=$(sha "$original")
if [ "$current" = "$candidate" ];then
 original=/data/adb/modules/rk3588_decode_stack/components/hdmi_event/originals/libandroid_runtime.so
else
 [ "$current" = "$factory" ] || exit 1
fi
[ "$(sha "$original")" = "$factory" ] || exit 1
sha256sum -c "$MODROOT/contracts/platform.sha256" >/dev/null
mkdir -p "$MODROOT/originals" "$MODROOT/variants"
cp "$original" "$MODROOT/originals/libandroid_runtime.so"
"$MODROOT/bin/byte_patch64" "$original" "$MODROOT/variants/libandroid_runtime.so" "$MODROOT/contracts/runtime-arm64.tsv"
[ "$(sha "$MODROOT/variants/libandroid_runtime.so")" = "$candidate" ] || exit 1
timeout 20 env LD_LIBRARY_PATH="$MODROOT/target:/system/lib64" "$MODROOT/bin/event-test64" "$MODROOT/variants/libandroid_runtime.so" "$MODROOT/target/libhevt.so"
printf '%s\n' "$candidate" > "$MODROOT/preflight"
