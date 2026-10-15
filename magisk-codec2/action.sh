#!/system/bin/sh
MODDIR=${0%/*}
if [ -e "$MODDIR/disable" ]; then
 rm "$MODDIR/disable"
 echo "Codec2 upgrade enabled. Reboot to activate."
else
 touch "$MODDIR/disable"
 echo "Codec2 upgrade disabled. Reboot to restore all original system files."
fi
