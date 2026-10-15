#!/system/bin/sh
MODDIR=${0%/*}
CODEC2=/data/adb/modules/rk3588_codec2_upgrade
case "$1" in
 off)
  if [ -d "$CODEC2" ] && [ ! -e "$CODEC2/disable" ] && [ ! -e "$CODEC2/remove" ]; then
   touch "$MODDIR/resume-codec2" || exit 1
   touch "$CODEC2/disable" || exit 1
  fi
  touch "$MODDIR/disable" || exit 1
  echo "MPP/VPU disabled. Dependent Codec2 is also disabled if active. Reboot to restore originals."
  ;;
 on)
  rm -f "$MODDIR/disable" || exit 1
  if [ -e "$MODDIR/resume-codec2" ] && [ -d "$CODEC2" ] && [ ! -e "$CODEC2/remove" ]; then
   rm -f "$CODEC2/disable" "$MODDIR/resume-codec2" || exit 1
  fi
  echo "MPP/VPU enabled; dependent Codec2 selection restored. Reboot to activate."
  ;;
 status)
  if [ -e "$MODDIR/disable" ]; then echo "MPP/VPU: disabled for next boot"; else echo "MPP/VPU: enabled for next boot"; fi
  cat "$MODDIR/status.txt" 2>/dev/null
  ;;
 *) echo "Usage: sh mode-control.sh on|off|status"; exit 2 ;;
esac
