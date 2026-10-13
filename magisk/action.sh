#!/system/bin/sh
MODDIR=${0%/*}
STATE=/data/adb/rk3588-frameguard
if [ -f "$STATE/paused" ]; then
    sh "$MODDIR/control.sh" apply && rm -f "$STATE/paused"
else
    sh "$MODDIR/control.sh" restore && { mkdir -p "$STATE"; touch "$STATE/paused"; }
fi
