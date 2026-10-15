#!/system/bin/sh
MODDIR=${0%/*}
STATE=/data/adb/rk3588-mali-native
KEY=updatable_driver_production_opt_in_apps
mkdir -p "$STATE" || exit 1
get() { settings get global "$1"; }
check_scope() {
 all=$(get updatable_driver_all_apps)
 allow=$(get updatable_driver_production_allowlist)
 case "$all" in null|0|'') ;; *) echo "Refusing an existing global graphics override"; return 1;; esac
 case "$allow" in null|'') ;; *) echo "Refusing an existing graphics allowlist"; return 1;; esac
}
update_list() {
 current=$(get "$KEY")
 [ "$current" = null ] && current=
 printf '%s' "$current" | tr ',' '\n' | while IFS= read -r p || [ -n "$p" ]; do
  [ -n "$p" ] || continue
  if ! grep -Fxq "$p" "$STATE/owned-apps" 2>/dev/null; then
   echo "$p"
  elif tr ',' '\n' < "$STATE/original-opt-in" | grep -Fxq "$p"; then
   echo "$p"
  fi
 done > "$STATE/list.tmp"
 if [ "$1" = on ]; then
  cat "$STATE/apps" >> "$STATE/list.tmp"
 fi
 list=$(awk 'NF && !seen[$0]++' "$STATE/list.tmp" | paste -sd, -)
 if [ -n "$list" ]; then settings put global "$KEY" "$list" || return 1
 elif [ "$(cat "$STATE/original-opt-in")" = null ]; then settings delete global "$KEY" >/dev/null || return 1
 else settings put global "$KEY" "" || return 1
 fi
 if [ "$1" = on ]; then cp "$STATE/apps" "$STATE/owned-apps"; else : > "$STATE/owned-apps"; fi
 rm -f "$STATE/list.tmp"
}
apply() {
 if [ "$(cat "$STATE/enabled")" = 1 ]; then
  check_scope || return 1
  existing=$(getprop ro.gfx.driver.0)
  case "$existing" in ''|com.sw1313.malidriver.g15|com.sw1313.malidriver.g18) ;; *) echo "Another driver selection is active";return 1;; esac
  selected=$(cat "$STATE/selected")
  case "$selected" in g15|g18) ;; *) echo "Unknown driver";return 1;; esac
  update_list on || return 1
  resetprop ro.gfx.driver.0 "com.sw1313.malidriver.$selected" || return 1
 else
  update_list off || return 1
  existing=$(getprop ro.gfx.driver.0)
  case "$existing" in ''|com.sw1313.malidriver.g15|com.sw1313.malidriver.g18)
   original=$(cat "$STATE/original-driver")
   if [ -n "$original" ]; then resetprop ro.gfx.driver.0 "$original"; elif [ -n "$existing" ]; then resetprop --delete ro.gfx.driver.0; fi;;
  esac
 fi
}
valid_app() {
 printf '%s\n' "$1" | grep -Eq '^([A-Za-z][A-Za-z0-9_]*\.)+[A-Za-z0-9_]+$' || return 1
 case "$1" in android|com.android.*|com.hush.yamby|com.google.android.tvlauncher|com.scrcpyandroid.rkplaybackfix) return 1;; esac
 [ -n "$(pm path "$1" 2>/dev/null)" ]
}
case "$1" in
 status) echo "Enabled: $(cat "$STATE/enabled")";echo "Selected: $(cat "$STATE/selected")";echo "Framework: $(getprop ro.gfx.driver.0)";echo "Opt-in: $(get "$KEY")";echo "Managed apps:";cat "$STATE/apps";;
 g15|g18) echo "$1" > "$STATE/selected";echo 1 > "$STATE/enabled";apply;;
 on) echo 1 > "$STATE/enabled";apply;;
 off) echo 0 > "$STATE/enabled";apply;;
 apply) apply;;
 app)
  valid_app "$3" || { echo "Invalid, protected, or uninstalled app";exit 1; }
  case "$2" in
   add) grep -Fxq "$3" "$STATE/apps" || echo "$3" >> "$STATE/apps";;
   remove) awk -v p="$3" '$0!=p' "$STATE/apps" > "$STATE/apps.tmp" && mv "$STATE/apps.tmp" "$STATE/apps";;
   *) exit 1;;
  esac
  apply;;
 *) echo "Usage: mode-control.sh status|g15|g18|on|off|app add PACKAGE|app remove PACKAGE";exit 1;;
esac
