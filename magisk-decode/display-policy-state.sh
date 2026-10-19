#!/system/bin/sh
# Independent next-boot fallback; never changes GPU, Zygisk or other module flags.
set -eu
umask 077
MODROOT=${0%/*}
STATE=/data/adb/rk3588-display-policy
BOOT=$(cat /proc/sys/kernel/random/boot_id)
mkdir -p "$STATE"; chmod 0700 "$STATE"
disable_policy() {
 sed 's/^display_policy=.*/display_policy=0/' "$MODROOT/config" > "$MODROOT/config.policy-failed"
 chmod 0600 "$MODROOT/config.policy-failed"
 mv "$MODROOT/config.policy-failed" "$MODROOT/config"
 printf '%s %s\n' "$BOOT" "$1" > "$STATE/failure"
 # Consume the failed trial after recording it. Keeping it in pending makes
 # every later explicit enable roll back again, even when the new boot works.
 # Preserve the receipt for diagnosis; a new attempt must qualify afresh.
 if [ -f "$STATE/pending" ]; then
  mv "$STATE/pending" "$STATE/failed-pending"
 fi
}
withdraw_conflicting_cache() {
 # ART GetBestInfo() prefers a usable /data OAT over the prebuilt system OAT.
 # A previous jar-only trial left verify-mode OAT containing extracted DEX;
 # zygote can preload it and fail SetOnlyUseTrustedOatFiles after fork.
 # Archive only this overlay's system-server cache entries, never app caches.
 archive=''
 for jar in services ethernet-service; do
  for suffix in dex vdex art; do
   cached="/data/dalvik-cache/arm64/system@framework@$jar.jar@classes.$suffix"
   [ -e "$cached" ] || [ -L "$cached" ] || continue
   [ -f "$cached" ] && [ ! -L "$cached" ] || return 1
   if [ -z "$archive" ]; then
    mkdir -p "$STATE/cache-backups"
    archive=$(mktemp -d "$STATE/cache-backups/attempt.XXXXXX") || return 1
   fi
   mv "$cached" "$archive/" || return 1
  done
 done
 if [ -n "$archive" ]; then
  printf '%s\n' "$archive" > "$STATE/cache-last"
 fi
}
case "$1" in
 restore-hdr)
  # No saved choice means the native default. Component off never replays it.
  if [ "$(sed -n 's/^display_policy=//p' "$MODROOT/active-config")" = 1 ]; then
   saved=$(getprop persist.vendor.rkdisplay.hdr_force_disable)
   case "$saved" in 0|1) resetprop -n vendor.hwc.hdr_force_disable "$saved" ;; esac
  fi
  ;;
 begin)
  old=$(cat "$STATE/pending" 2>/dev/null || true)
  qualified=$(cat "$STATE/qualified" 2>/dev/null || true)
  if [ -n "$old" ] && [ "$old" != "$qualified" ] && [ "${old%% *}" != "$BOOT" ]; then
   disable_policy previous-start-not-qualified
   exit 0
  fi
  if [ "$(sed -n 's/^display_policy=//p' "$MODROOT/config")" = 1 ]; then
   if [ ! -s "$MODROOT/components/display_policy/preflight" ]; then
    disable_policy preflight-missing
    exit 0
   fi
   c="$MODROOT/components/display_policy"
   if ! (cd "$c" && sha256sum -c oat.sha256 && sha256sum -c oat-inputs.sha256) >/dev/null 2>&1; then
    disable_policy trusted-oat-preflight-failed
    exit 0
   fi
   if ! withdraw_conflicting_cache; then
    disable_policy conflicting-cache-withdrawal-failed
    exit 0
   fi
   digest=$(cat "$MODROOT/components/display_policy/preflight")
   (umask 077; printf '%s %s\n' "$BOOT" "$digest" > "$STATE/pending.next")
   mv "$STATE/pending.next" "$STATE/pending"
  fi
  ;;
 qualified)
  [ "$(getprop sys.boot_completed)" = 1 ]
  [ -n "$(pidof system_server)" ]
  [ "$(sed -n 's/^display_policy=//p' "$MODROOT/active-config")" = 1 ]
  digest=$(cat "$MODROOT/components/display_policy/preflight")
  [ "$(sha256sum /system/framework/services.jar | cut -d ' ' -f 1)" = "$digest" ]
  for artifact in services.odex services.vdex services.art ethernet-service.odex ethernet-service.vdex; do
   expected=$(awk -v f="variants/oat/arm64/$artifact" '$2==f {print $1}' "$MODROOT/components/display_policy/oat.sha256")
   [ -n "$expected" ] && [ "$(sha256sum "/system/framework/oat/arm64/$artifact" | cut -d ' ' -f 1)" = "$expected" ]
  done
  pid=$(pidof system_server)
  grep -q '/system/framework/oat/arm64/services.odex' "/proc/$pid/maps"
  grep -q '/system/framework/oat/arm64/ethernet-service.odex' "/proc/$pid/maps"
  [ "$(cat "$STATE/pending")" = "$BOOT $digest" ]
  cp "$STATE/pending" "$STATE/qualified.next"
  mv "$STATE/qualified.next" "$STATE/qualified"
  ;;
 failed) disable_policy runtime-start-not-qualified ;;
 *) exit 2 ;;
esac
