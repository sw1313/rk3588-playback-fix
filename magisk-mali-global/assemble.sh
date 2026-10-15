#!/system/bin/sh
set -eu
MODDIR=${0%/*}
STATE=/data/adb/rk3588-mali-global
case "$MODDIR" in /data/adb/modules/rk3588_mali_global|/data/adb/modules_update/rk3588_mali_global) ;; *) exit 1;; esac
cd "$MODDIR"
mode=$(cat "$STATE/mode")
if [ "$mode" = on ]; then
 boot=$(cat /proc/sys/kernel/random/boot_id)
 if [ -s "$STATE/boot-pending" ] && [ "$(cat "$STATE/boot-pending")" != "$boot" ]; then
  echo off > "$STATE/mode";mode=off
  echo "Previous global startup was not verified; recovering original GPU." > "$STATE/recovery.txt"
 fi
fi
rm -rf "$MODDIR/system.next"
mkdir -p "$MODDIR/system.next"
: > "$MODDIR/active.next.sha256"
if [ "$mode" = on ]; then
 sha256sum -c payload.sha256 >/dev/null 2>&1
 while read -r hash file; do
  relative=${file#payload/}
  case "$relative" in lib/*|lib64/*) ;; *) exit 1;; esac
  target="$MODDIR/system.next/vendor/$relative"
  mkdir -p "${target%/*}"
  ln "$MODDIR/$file" "$target"
  printf '%s  /vendor/%s\n' "$hash" "$relative" >> "$MODDIR/active.next.sha256"
 done < payload.sha256
 echo "$boot" > "$STATE/boot-pending"
elif [ "$mode" != off ]; then
 exit 1
fi
rm -rf "$MODDIR/system.previous"
[ ! -d "$MODDIR/system" ] || mv "$MODDIR/system" "$MODDIR/system.previous"
mv "$MODDIR/system.next" "$MODDIR/system"
mv "$MODDIR/active.next.sha256" "$MODDIR/active.sha256"
rm -rf "$MODDIR/system.previous"
echo "$mode" > "$MODDIR/active-mode"
rm -f "$MODDIR/skip_mount"
