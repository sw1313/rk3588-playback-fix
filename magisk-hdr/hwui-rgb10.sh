#!/system/bin/sh
set -eu
MODROOT=${0%/*}
sha() { v=$(sha256sum "$1"); echo "${v%% *}"; }
mkdir -p "$MODROOT/originals/lib" "$MODROOT/originals/lib64" "$MODROOT/variants/lib" "$MODROOT/variants/lib64"
original=/system/lib64/libhwui.so
if [ "$(sha "$original")" != 8e812c303735807beb570a786065ddd9b7ce448c68d4d2a2c86236d8705b4ea7 ]; then
 original=/data/adb/modules/rk3588_decode_stack/components/hdr/originals/lib64/libhwui.so
fi
[ "$(sha "$original")" = 8e812c303735807beb570a786065ddd9b7ce448c68d4d2a2c86236d8705b4ea7 ] || exit 1
cp "$original" "$MODROOT/originals/lib64/libhwui.so"
"$MODROOT/bin/byte_patch64" "$original" "$MODROOT/variants/lib64/libhwui.so" "$MODROOT/contracts/hwui-rgb10-arm64.tsv"
[ "$(sha "$MODROOT/variants/lib64/libhwui.so")" = 870f4e31327e7c13269c81cf1c71835c32e2c5244d13fdd5c1360d1585ed6d6c ] || exit 1
timeout 15 env LD_LIBRARY_PATH=/system/lib64:/system/lib64/bootstrap:/apex/com.android.art/lib64:/apex/com.android.runtime/lib64 "$MODROOT/bin/hwui_probe64" "$MODROOT/variants/lib64/libhwui.so" 1
original=/system/lib/libhwui.so
if [ "$(sha "$original")" != ce91abbde54db81e9e70a13188a7cd32bcedee4eda29534d7b1f56c4f601afa2 ]; then
 original=/data/adb/modules/rk3588_decode_stack/components/hdr/originals/lib/libhwui.so
fi
[ "$(sha "$original")" = ce91abbde54db81e9e70a13188a7cd32bcedee4eda29534d7b1f56c4f601afa2 ] || exit 1
cp "$original" "$MODROOT/originals/lib/libhwui.so"
"$MODROOT/bin/byte_patch64" "$original" "$MODROOT/variants/lib/libhwui.so" "$MODROOT/contracts/hwui-rgb10-arm.tsv"
[ "$(sha "$MODROOT/variants/lib/libhwui.so")" = 32f0d5d578eac30884d783bc83aea565175a89e1c56acaedf1c1ab1865d0f644 ] || exit 1
timeout 15 env LD_LIBRARY_PATH=/system/lib:/system/lib/bootstrap:/apex/com.android.art/lib:/apex/com.android.runtime/lib "$MODROOT/bin/hwui_probe32" "$MODROOT/variants/lib/libhwui.so" 1
