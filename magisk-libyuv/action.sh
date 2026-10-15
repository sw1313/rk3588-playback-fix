#!/system/bin/sh
MODDIR=${0%/*}
if [ -e "$MODDIR/disable" ]; then
 rm "$MODDIR/disable"
 echo "libyuv enabled; reboot to activate."
else
 touch "$MODDIR/disable"
 echo "libyuv disabled; reboot to restore the original library."
fi
