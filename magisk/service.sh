#!/system/bin/sh
MODDIR=${0%/*}
# Late-start service, bounded wait: never block early boot.
count=0
while [ "$(getprop sys.boot_completed)" != 1 ] && [ "$count" -lt 120 ]; do
    sleep 2
    count=$((count + 1))
done
[ "$(getprop sys.boot_completed)" = 1 ] || exit 0
[ -f "$MODDIR/disable" ] || [ -f "$MODDIR/remove" ] ||
    [ -f /data/adb/rk3588-frameguard/paused ] ||
    sh "$MODDIR/control.sh" apply
