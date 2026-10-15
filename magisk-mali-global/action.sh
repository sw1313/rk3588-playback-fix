#!/system/bin/sh
MODDIR=${0%/*}
if [ "$(cat /data/adb/rk3588-mali-global/mode 2>/dev/null)" = on ]; then
 sh "$MODDIR/mode-control.sh" off
else
 sh "$MODDIR/mode-control.sh" on
fi
sh "$MODDIR/mode-control.sh" status
