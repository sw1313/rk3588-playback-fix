#!/system/bin/sh
MODDIR=${0%/*}
if sh "$MODDIR/control.sh" restore; then
    rm -f /data/adb/rk3588-frameguard/paused /data/adb/rk3588-frameguard/original.conf
    rmdir /data/adb/rk3588-frameguard 2>/dev/null
fi
