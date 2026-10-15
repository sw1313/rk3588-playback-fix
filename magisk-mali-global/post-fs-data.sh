#!/system/bin/sh
MODDIR=${0%/*}
if sh "$MODDIR/assemble.sh"; then
 resetprop -n vendor.rkmaliglobal.ready pending
else
 touch "$MODDIR/skip_mount" "$MODDIR/disable"
 echo "FAIL before mounting; global GPU disabled." > "$MODDIR/status.txt"
 resetprop -n vendor.rkmaliglobal.ready failed
fi
