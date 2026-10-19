#!/system/bin/sh
# Independent, reboot-only rollback. Never changes boot or other module flags.
set -eu
MODROOT=${0%/*}
state=/data/adb/rk3588-refresh-session
umask 077
mkdir -p "$state"; chmod 0700 "$state"
boot=$(cat /proc/sys/kernel/random/boot_id)
withdraw() {
 sed 's/^display_session=.*/display_session=0/' "$MODROOT/config" > "$MODROOT/config.session-next"
 chmod 0600 "$MODROOT/config.session-next"
 mv "$MODROOT/config.session-next" "$MODROOT/config"
 rm -f "$state/pending"
 resetprop -n debug.rkdecode.refresh_session 0
}
case "$1" in
 begin)
  resetprop -n vendor.rkdecode.refresh_session pending
  resetprop -n debug.rkdecode.refresh_session 0
  if [ -f "$state/pending" ] && [ "$(cat "$state/pending")" != "$boot" ]; then
   cp "$state/pending" "$state/previous-unconfirmed"
   withdraw
  fi
  if grep -qx display_session=1 "$MODROOT/config"; then printf '%s\n' "$boot" > "$state/pending"; fi
  ;;
 verify)
  test "$(getprop vendor.rkdecode.refresh_session)" = active
  test "$(cat "$state/pending")" = "$boot"
  pid=$(pidof surfaceflinger);test -n "$pid"
  grep -q '/system/lib64/librkses.so' "/proc/$pid/maps"
  sha256sum -c "$MODROOT/components/refresh_session/platform.sha256" >/dev/null
  printf '%s\n' "$boot" > "$state/qualified"
  rm -f "$state/pending"
  ;;
 failed) withdraw ;;
 *) exit 2 ;;
esac
