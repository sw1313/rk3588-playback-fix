#!/system/bin/sh
MODROOT=${0%/*}
CURRENTROOT=$MODROOT
NEXTROOT=/data/adb/modules_update/${MODROOT##*/}
if [ "$MODROOT" = "/data/adb/modules/${MODROOT##*/}" ] && [ -f "$NEXTROOT/config" ]; then
 MODROOT=$NEXTROOT
fi
. "$MODROOT/common.sh"
sync_selection() {
 [ "$MODROOT" != "$CURRENTROOT" ] || return 0
 cp "$MODROOT/config" "$CURRENTROOT/config" || return 1
 if [ -e "$MODROOT/disable" ]; then touch "$CURRENTROOT/disable"; else rm -f "$CURRENTROOT/disable"; fi
}
case "$1" in
 status)
  if [ -e "$MODROOT/disable" ]; then echo "Whole module: disabled for next boot"; else echo "Whole module: enabled for next boot"; fi
  echo "Requested next boot:"; cat "$MODROOT/config"
  resolve || exit 1
  echo "Effective next boot: mpp=$MPP rga=$RGA codec2=$C2 libyuv=$YUV software=$SOFT hdr_static=$HDR_STATIC hdr10plus=$HDR_PLUS codec2_sdk=$SDK codec2_sdk_next=$SDK_NEXT codec2_buffer=$BUFFER p010_fast=$P010_FAST av1_native=$AV1_NATIVE av1_grain=$AV1_GRAIN av1_afbc=$AV1_AFBC"
  echo "Active boot:"; cat "$CURRENTROOT/active-config" 2>/dev/null
  cat "$CURRENTROOT/status.txt" 2>/dev/null
  ;;
 all)
  case "$2" in
   off) touch "$MODROOT/disable"; echo "Entire decode stack disabled. Reboot restores firmware originals." ;;
   on) rm -f "$MODROOT/disable"; echo "Decode stack enabled with saved component selections. Reboot to activate." ;;
   *) exit 2 ;;
  esac
  sync_selection || exit 1
  ;;
 mpp|rga|codec2|libyuv|software|hdr_static|hdr10plus|codec2_sdk|codec2_sdk_next|codec2_buffer|p010_fast|av1_native|av1_grain|av1_afbc)
  case "$2" in on) value=1 ;; off) value=0 ;; *) exit 2 ;; esac
  resolve || exit 1
  sed "s/^$1=.*/$1=$value/" "$MODROOT/config" > "$MODROOT/config.next" || exit 1
  mv "$MODROOT/config.next" "$MODROOT/config" || exit 1
  sync_selection || exit 1
  echo "$1=$value saved. Reboot to apply. Codec2 requires MPP/RGA; software P010 requires Codec2. Selections are preserved when dependencies are off."
  ;;
 *) echo "Usage: sh mode-control.sh status | all on|off | mpp|rga|codec2|libyuv|software|hdr_static|hdr10plus|codec2_sdk|codec2_sdk_next|codec2_buffer|p010_fast|av1_native|av1_grain|av1_afbc on|off"; exit 2 ;;
esac
