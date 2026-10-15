#!/system/bin/sh
MODDIR=${0%/*}
case "$1" in
 quality) echo quality > "$MODDIR/mode"; resetprop debug.rkcodec2.surface_upgrade 1 ;;
 compatibility) echo compatibility > "$MODDIR/mode"; resetprop debug.rkcodec2.surface_upgrade 0 ;;
 *) echo "Usage: mode-control.sh quality|compatibility"; exit 2 ;;
esac
echo "Mode saved. Reopen playback to create a decoder with the new policy."
