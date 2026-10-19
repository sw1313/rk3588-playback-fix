#!/system/bin/sh
# Exact-image native E-DDC component. Never changes other module selections.
set -eu
umask 077
MODROOT=${0%/*}
STATE=/data/adb/rk3588-decode-ddc
BOOT=$(cat /proc/sys/kernel/random/boot_id)
mkdir -p "$STATE"; chmod 0700 "$STATE"
disable_ddc() {
 sed 's/^display_ddc=.*/display_ddc=0/' "$MODROOT/config" > "$MODROOT/config.ddc-next"
 chmod 0600 "$MODROOT/config.ddc-next"
 mv "$MODROOT/config.ddc-next" "$MODROOT/config"
 printf '%s %s\n' "$BOOT" "$1" > "$STATE/failure"
 resetprop -n vendor.rkdecode.ddc failed
}
case "$1" in
 begin)
  [ "$(sed -n 's/^display_ddc=//p' "$MODROOT/config")" = 1 ] || exit 0
  old=$(cat "$STATE/pending" 2>/dev/null || true)
  qualified=$(cat "$STATE/qualified" 2>/dev/null || true)
  if [ -n "$old" ] && [ "$old" != "$BOOT" ] && [ "$old" != "$qualified" ]; then
   disable_ddc previous-start-not-qualified; exit 0
  fi
  c="$MODROOT/components/ddc"
  if ! (cd "$c" && sha256sum -c checksums.sha256) > "$STATE/load-status" 2>&1; then
   disable_ddc component-checksum; exit 0
  fi
  printf '%s\n' "$BOOT" > "$STATE/pending.next"
  mv "$STATE/pending.next" "$STATE/pending"
  if [ ! -d /sys/module/rk_decode_ddc ]; then
   if ! insmod "$c/rk_decode_ddc.ko" >> "$STATE/load-status" 2>&1; then
    disable_ddc exact-image-load-failed; exit 0
   fi
  fi
  if [ "$(cat /sys/module/rk_decode_ddc/parameters/online)" != 1 ]; then
   disable_ddc adapter-not-published; exit 0
  fi
  # Kernel initial probing precedes Magisk. Refresh the native connector
  # capability cache once after publication, with DRM's own locking. This
  # requests detection, never forces a connector or fabricates an EDID.
  if [ "$(cat /sys/class/drm/card0-HDMI-A-1/status)" = connected ]; then
   # Some vendor sysfs implementations return a short write after probing.
   # Record it; do not misclassify a published adapter as a load failure.
   if ! printf 'detect\n' > /sys/class/drm/card0-HDMI-A-1/status 2>> "$STATE/load-status"; then
    echo 'Native connector reprobe returned nonzero; capability/result acceptance remains separate.' >> "$STATE/load-status"
   fi
  fi
  resetprop -n vendor.rkdecode.ddc active
  ;;
 qualified)
  [ "$(sed -n 's/^display_ddc=//p' "$MODROOT/active-config")" = 1 ] || exit 0
  [ "$(getprop sys.boot_completed)" = 1 ]
  [ "$(cat /sys/module/rk_decode_ddc/parameters/online)" = 1 ]
  [ "$(cat "$STATE/pending")" = "$BOOT" ]
  printf '%s\n' "$BOOT" > "$STATE/qualified.next"
  mv "$STATE/qualified.next" "$STATE/qualified"
  # A healthy boot is not physical HDMI acceptance.
  ;;
 recover)
  [ "$(sed -n 's/^display_ddc=//p' "$MODROOT/active-config")" = 1 ] || exit 0
  [ "$(cat /sys/class/drm/card0-HDMI-A-1/status)" = connected ] || exit 0
  # An active cold QMS owner already owns initialization. Never modeset it.
  [ ! -d /sys/module/rk_decode_qms ] || exit 0
  [ "$(cat "$STATE/recovery-attempt" 2>/dev/null || true)" != "$BOOT" ] || exit 0
  [ "$(cat /sys/module/rk_decode_ddc/parameters/online)" = 1 ]
  c="$MODROOT/components/ddc"
  (cd "$c" && sha256sum -c checksums.sha256) > "$STATE/recovery-status" 2>&1
  printf '%s\n' "$BOOT" > "$STATE/recovery-attempt"
  # One factory full modeset: same logical timing and layer buffers. Correct
  # native EDID-derived format negotiation is allowed, never forced by us.
  insmod "$c/rk_decode_ddc_recover.ko" recovery_check=1 commit_once=1 >> "$STATE/recovery-status" 2>&1
  result=1
  if [ "$(cat /sys/module/rk_decode_ddc_recover/parameters/carrier_commit_result)" = 0 ] &&
     [ "$(cat /sys/module/rk_decode_ddc_recover/parameters/carrier_preserved)" = 1 ] &&
     [ "$(cat /sys/module/rk_decode_ddc_recover/parameters/recovery_verified)" = 1 ]; then
   printf '%s\n' "$BOOT" > "$STATE/recovered"
   result=0
  fi
  rmmod rk_decode_ddc_recover >> "$STATE/recovery-status" 2>&1
  exit "$result"
  ;;
 *) exit 2 ;;
esac
