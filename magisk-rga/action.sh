#!/system/bin/sh
MODDIR=${0%/*}
if [ -e "$MODDIR/disable" ]; then
    rm "$MODDIR/disable"
    echo "RGA upgrade enabled. Reboot to activate."
else
    touch "$MODDIR/disable"
    echo "RGA upgrade disabled. Reboot to restore stock libraries."
fi
