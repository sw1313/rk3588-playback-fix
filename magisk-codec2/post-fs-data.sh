#!/system/bin/sh
MODDIR=${0%/*}
# Capture policy before Android clients create new decoder components.
case "$(cat "$MODDIR/mode" 2>/dev/null)" in
 compatibility) resetprop debug.rkcodec2.surface_upgrade 0 ;;
 *) resetprop debug.rkcodec2.surface_upgrade 1 ;;
esac
