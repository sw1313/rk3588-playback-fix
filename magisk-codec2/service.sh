#!/system/bin/sh
MODDIR=${0%/*}
[ ! -e "$MODDIR/disable" ] || exit 0
recover() {
 touch "$MODDIR/disable"
 resetprop debug.rkcodec2.surface_upgrade 0
 echo "FAIL: $1; module disabled, rebooting once to restore original files." >> "$MODDIR/status.txt"
 # The disable flag prevents this script from running again on the recovery boot.
 sync
 reboot
 exit 1
}
: > "$MODDIR/status.txt"
sha256sum -c "$MODDIR/contracts/runtime.sha256" >> "$MODDIR/status.txt" 2>&1 || recover "MPP/RGA dependency changed"
sha256sum -c "$MODDIR/contracts/platform.sha256" >> "$MODDIR/status.txt" 2>&1 || recover "Codec2/HIDL ABI changed"
# Early HAL processes may predate Magisk's mounts. Refresh only this codec
# service and its media list, never hwservicemanager or the graphics stack.
stop media
stop android-hardware-media-c2-hal-1-1
start android-hardware-media-c2-hal-1-1
start media
attempt=0
while [ "$(getprop sys.boot_completed)" != 1 ] && [ "$attempt" -lt 45 ]; do sleep 2; attempt=$((attempt+1)); done
if [ "$(getprop init.svc.android-hardware-media-c2-hal-1-1)" = running ] &&
 timeout 20 /system/bin/env LD_LIBRARY_PATH=/vendor/lib:/system/lib:/apex/com.android.vndk.v32/lib /vendor/bin/hw/android.hardware.media.c2@1.1-service --check-only > "$MODDIR/status.txt" 2>&1; then
 while read -r name; do
  grep -F "COMPONENT $name rank=" "$MODDIR/status.txt" >/dev/null || recover "missing stock component: $name"
 done < "$MODDIR/contracts/stock-components.txt"
 echo "PASS service, HIDL ABI and original/candidate interfaces load. Playback test results are separate." >> "$MODDIR/status.txt"
else
 recover "service or HIDL preflight failed"
fi
