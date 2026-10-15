#!/system/bin/sh
MODDIR=${0%/*}
if [ "$(cat /data/adb/rk3588-mali-native/enabled)" = 1 ]; then
 sh "$MODDIR/mode-control.sh" off
else
 sh "$MODDIR/mode-control.sh" on
fi
echo "Force-stop and reopen the opted-in app to apply the change."
sh "$MODDIR/mode-control.sh" status
