#!/system/bin/sh
set -eu
MODROOT=${0%/*}
. "$MODROOT/common.sh"
resolve
case "${MODROOT##*/}" in rk3588_decode_stack|package) ;; *) echo "Unexpected module directory"; exit 1 ;; esac
# Runs before Magic Mount. Never changes a library in a live process.
cd "$MODROOT"
rm -rf "$MODROOT/system.next"
mkdir -p system.next
: > active.next.sha256
for component in mpp rga codec2 libyuv software hdr; do
 if selected "$component"; then
  sed -n "/  components\/$component\/system\//p" files.sha256 > selected.sha256
  [ -s selected.sha256 ] || exit 1
  sha256sum -c selected.sha256 >/dev/null 2>&1 || exit 1
  cp -a "components/$component/system/." system.next/
  while read -r hash file; do
   target=${file#components/$component/system/}
   case "$target" in vendor/*) target=/$target ;; *) target=/system/$target ;; esac
   printf '%s  %s\n' "$hash" "$target" >> active.next.sha256
  done < selected.sha256
 fi
done
rm -rf "$MODROOT/system.previous"
[ ! -d system ] || mv system system.previous
mv system.next system
mv active.next.sha256 active.sha256
rm -rf "$MODROOT/system.previous"
printf 'mpp=%s\nrga=%s\ncodec2=%s\nlibyuv=%s\nsoftware=%s\nhdr_static=%s\nhdr10plus=%s\n' "$MPP" "$RGA" "$C2" "$YUV" "$SOFT" "$HDR_STATIC" "$HDR_PLUS" > active-config
rm -f skip_mount selected.sha256
