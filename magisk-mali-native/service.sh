#!/system/bin/sh
MODDIR=${0%/*}
while [ "$(getprop sys.boot_completed)" != 1 ]; do sleep 1; done
if sh "$MODDIR/mode-control.sh" apply > "$MODDIR/status.txt" 2>&1; then
 echo "PASS scoped native Mali driver selection" >> "$MODDIR/status.txt"
else
 sh "$MODDIR/mode-control.sh" off >> "$MODDIR/status.txt" 2>&1
 echo "FAIL selection disabled; inspect status" >> "$MODDIR/status.txt"
fi
