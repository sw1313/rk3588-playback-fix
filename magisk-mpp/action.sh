#!/system/bin/sh
MODDIR=${0%/*}
touch "$MODDIR/disable" || exit 1
echo "MPP upgrade disabled. Reboot to restore the firmware's original libraries."
echo "To try again, enable the module in Magisk and reboot."
