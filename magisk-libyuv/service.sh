#!/system/bin/sh
MODDIR=${0%/*}
[ ! -e "$MODDIR/disable" ] || exit 0
cd "$MODDIR" || exit 1
expected=$(sha256sum "$MODDIR/system/lib64/libyuv.so"); expected=${expected%% *}
active=$(sha256sum /system/lib64/libyuv.so); active=${active%% *}
if [ "$expected" = "$active" ] &&
 sha256sum -c contracts/platform.sha256 > "$MODDIR/status.txt" 2>&1 &&
 timeout 15 "$MODDIR/bin/quality_probe64" /system/lib64/libyuv.so >> "$MODDIR/status.txt" 2>&1; then
 echo "PASS active library, dependencies and full precision self-test. Playback validation is separate." >> "$MODDIR/status.txt"
else
 touch "$MODDIR/disable"
 echo "FAIL; disabled. Rebooting once to restore original libyuv." >> "$MODDIR/status.txt"
 sync
 reboot
fi
