#!/system/bin/sh
MODDIR=${0%/*}
if [ -e "$MODDIR/disable" ]; then
 sh "$MODDIR/mode-control.sh" on
else
 sh "$MODDIR/mode-control.sh" off
fi
