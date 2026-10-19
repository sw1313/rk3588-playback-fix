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
if [ "$HWC_SCALE" = 1 ];then
 variant=components/hdr/variants/hwcomposer.rk30board.scaled.so
 expected=47702a172f0b95ecc88dbc60a9b42a09b80841e9b4269ce74d7af916af33901b
 [ "$(sha256sum "$variant" | cut -d ' ' -f 1)" = "$expected" ] || exit 1
 cp "$variant" system.next/vendor/lib64/hw/hwcomposer.rk30board.rkhdr.so
 sed '\|  /vendor/lib64/hw/hwcomposer.rk30board.rkhdr.so$|d' active.next.sha256 > active.filtered.sha256
 mv active.filtered.sha256 active.next.sha256
 printf '%s  /vendor/lib64/hw/hwcomposer.rk30board.rkhdr.so\n' "$expected" >> active.next.sha256
fi
if [ "$HDMI_GUARD" = 1 ]; then
 c=components/hdmi_event
 expected=727b7fec294cb7c5f3691c57ddd164cda4414e68f6c000ed6e0766b627fa7e77
 [ "$(cat "$c/preflight")" = "$expected" ] || exit 1
 [ "$(sha256sum "$c/variants/libandroid_runtime.so" | cut -d ' ' -f 1)" = "$expected" ] || exit 1
 sed -n '\|  components/hdmi_event/target/libhevt.so$|p' files.sha256 > selected.sha256
 [ "$(wc -l < selected.sha256)" = 1 ] || exit 1
 sha256sum -c selected.sha256 >/dev/null 2>&1 || exit 1
 mkdir -p system.next/lib64
 cp "$c/variants/libandroid_runtime.so" system.next/lib64/libandroid_runtime.so
 cp "$c/target/libhevt.so" system.next/lib64/libhevt.so
 printf '%s  /system/lib64/libandroid_runtime.so\n' "$expected" >> active.next.sha256
 read -r hash file < selected.sha256
 printf '%s  /system/lib64/libhevt.so\n' "$hash" >> active.next.sha256
fi
if [ "$P010_POOL" = 1 ]; then
 c=components/codec2_pool
 variant=$c/variants/lib64/libsfplugin_ccodec.so
 expected=f9e01d5ff014ab42a40ddaafee6f3df8122502dd1f0641de58ae75b1b96610dd
 [ "$(sha256sum "$variant" | cut -d ' ' -f 1)" = "$expected" ] || exit 1
 sed -n '\|  components/codec2_pool/system/lib64/libcp10.so$|p' files.sha256 > selected.sha256
 [ "$(wc -l < selected.sha256)" = 1 ] || exit 1
 sha256sum -c selected.sha256 >/dev/null 2>&1 || exit 1
 mkdir -p system.next/lib64
 cp "$variant" system.next/lib64/libsfplugin_ccodec.so
 cp "$c/system/lib64/libcp10.so" system.next/lib64/libcp10.so
 sed '\|  /system/lib64/libsfplugin_ccodec.so$|d' active.next.sha256 > active.filtered.sha256
 mv active.filtered.sha256 active.next.sha256
 printf '%s  /system/lib64/libsfplugin_ccodec.so\n' "$expected" >> active.next.sha256
 read -r hash file < selected.sha256
 printf '%s  /system/lib64/libcp10.so\n' "$hash" >> active.next.sha256
fi
if [ "$HWC_CLIENT_HDR" = 1 ]; then
 c=components/hdr
 expected=a6f27121a627b42f53066756262312b3afe80800271afe14e57923081a72929f
 [ "$(cat "$c/sf-target10-preflight")" = "$expected" ] || exit 1
 [ "$(sha256sum "$c/variants/bin/surfaceflinger" | cut -d ' ' -f 1)" = "$expected" ] || exit 1
 sed -n '\|  components/hdr/target/libsfhdr.so$|p' files.sha256 > selected.sha256
 [ "$(wc -l < selected.sha256)" = 1 ] || exit 1
 sha256sum -c selected.sha256 >/dev/null 2>&1 || exit 1
 mkdir -p system.next/bin system.next/lib64
 cp "$c/variants/bin/surfaceflinger" system.next/bin/surfaceflinger
 chmod 0755 system.next/bin/surfaceflinger
 cp "$c/target/libsfhdr.so" system.next/lib64/libsfhdr.so
 printf '%s  /system/bin/surfaceflinger\n' "$expected" >> active.next.sha256
 read -r hash file < selected.sha256
 printf '%s  /system/lib64/libsfhdr.so\n' "$hash" >> active.next.sha256
 expected=49a69a0601883b1bccf7d979e891a049c1a30a9a66596cb47a57fc903ac7bc58
 [ "$(cat "$c/alloc-target10-preflight")" = "$expected" ] || exit 1
 variant=vendor/lib64/hw/android.hardware.graphics.allocator@4.0-impl-bifrost.so
 [ "$(sha256sum "$c/variants/$variant" | cut -d ' ' -f 1)" = "$expected" ] || exit 1
 sed -n '\|  components/hdr/target/libga10.so$|p' files.sha256 > selected.sha256
 [ "$(wc -l < selected.sha256)" = 1 ] || exit 1
 sha256sum -c selected.sha256 >/dev/null 2>&1 || exit 1
 mkdir -p system.next/vendor/lib64/hw
 cp "$c/variants/$variant" "system.next/$variant"
 cp "$c/target/libga10.so" system.next/vendor/lib64/libga10.so
 printf '%s  /%s\n' "$expected" "$variant" >> active.next.sha256
 read -r hash file < selected.sha256
 printf '%s  /vendor/lib64/libga10.so\n' "$hash" >> active.next.sha256
fi
if [ "$HWUI_RGB10" = 1 ]; then
 variant=components/hdr/variants/lib64/libhwui.so
 [ "$(sha256sum "$variant" | cut -d ' ' -f 1)" = "870f4e31327e7c13269c81cf1c71835c32e2c5244d13fdd5c1360d1585ed6d6c" ] || exit 1
 mkdir -p system.next/lib64
 cp "$variant" system.next/lib64/libhwui.so
 printf '%s  /system/lib64/libhwui.so\n' '870f4e31327e7c13269c81cf1c71835c32e2c5244d13fdd5c1360d1585ed6d6c' >> active.next.sha256
 variant=components/hdr/variants/lib/libhwui.so
 [ "$(sha256sum "$variant" | cut -d ' ' -f 1)" = "32f0d5d578eac30884d783bc83aea565175a89e1c56acaedf1c1ab1865d0f644" ] || exit 1
 mkdir -p system.next/lib
 cp "$variant" system.next/lib/libhwui.so
 printf '%s  /system/lib/libhwui.so\n' '32f0d5d578eac30884d783bc83aea565175a89e1c56acaedf1c1ab1865d0f644' >> active.next.sha256
fi
if [ "$SDK" = 1 ]; then
 sdk_component=codec2_sdk
 [ "$SDK_NEXT" != 1 ] || sdk_component=codec2_sdk_next
 sed -n "/  components\/$sdk_component\/system\//p" files.sha256 > selected.sha256
 [ "$(wc -l < selected.sha256)" = 1 ] || exit 1
 sha256sum -c selected.sha256 >/dev/null 2>&1 || exit 1
 cp -a "components/$sdk_component/system/." system.next/
 sed '\|  /vendor/lib/hw/libcodec2_rx_component.so$|d' active.next.sha256 > active.filtered.sha256
 mv active.filtered.sha256 active.next.sha256
 while read -r hash file; do
  [ "$file" = "components/$sdk_component/system/vendor/lib/hw/libcodec2_rx_component.so" ] || exit 1
  printf '%s  /vendor/lib/hw/libcodec2_rx_component.so\n' "$hash" >> active.next.sha256
 done < selected.sha256
fi
if [ "$DISPLAY_SESSION" = 1 ]; then
 c=components/refresh_session
 (cd "$c" && sha256sum -c checksums.sha256 >/dev/null 2>&1) || exit 1
 sha256sum -c "$c/platform.sha256" >/dev/null 2>&1 || exit 1
 for name in librkses.so libsfhdr.so; do
  file=$c/system/lib64/$name
  expected=$(awk -v f="components/refresh_session/system/lib64/$name" '$2==f {print $1}' files.sha256)
  [ "${#expected}" = 64 ] && [ "$(sha256sum "$file" | cut -d ' ' -f 1)" = "$expected" ] || exit 1
  cp "$file" "system.next/lib64/$name"
  sed "\\|  /system/lib64/$name\$|d" active.next.sha256 > active.filtered.sha256
  mv active.filtered.sha256 active.next.sha256
  printf '%s  /system/lib64/%s\n' "$expected" "$name" >> active.next.sha256
 done
fi
if [ "$DISPLAY_QMS" = 1 ]; then
 c=components/qms
 (cd "$c" && sha256sum -c checksums.sha256 >/dev/null 2>&1) || exit 1
 expected=$(sed -n 's/^hwc_sha256 //p' "$c/profile")
 [ "$(sha256sum "$c/hwcomposer.rk30board.so" | cut -d ' ' -f 1)" = "$expected" ] || exit 1
 cp "$c/hwcomposer.rk30board.so" system.next/vendor/lib64/hw/hwcomposer.rk30board.so
 sed '\|  /vendor/lib64/hw/hwcomposer.rk30board.so$|d' active.next.sha256 > active.filtered.sha256
 mv active.filtered.sha256 active.next.sha256
 printf '%s  /vendor/lib64/hw/hwcomposer.rk30board.so\n' "$expected" >> active.next.sha256
fi
if [ "$DISPLAY_POLICY" = 1 ]; then
 c=components/display_policy
 (cd "$c" && sha256sum -c checksums.sha256 >/dev/null 2>&1) || exit 1
 (cd "$c" && sha256sum -c oat.sha256 && sha256sum -c oat-inputs.sha256) >/dev/null 2>&1 || exit 1
 expected=$(cat "$c/preflight")
 [ "${#expected}" = 64 ] || exit 1
 [ "$(sha256sum "$c/variants/services.jar" | cut -d ' ' -f 1)" = "$expected" ] || exit 1
 while read -r digest entry; do
  [ "$(unzip -p "$c/variants/services.jar" "$entry" | sha256sum | cut -d ' ' -f 1)" = "$digest" ] || exit 1
 done < "$c/contracts/dex.sha256"
 mkdir -p system.next/framework
 cp "$c/variants/services.jar" system.next/framework/services.jar
 printf '%s  /system/framework/services.jar\n' "$expected" >> active.next.sha256
 mkdir -p system.next/framework/oat/arm64
 for artifact in services.odex services.vdex services.art ethernet-service.odex ethernet-service.vdex; do
  file=variants/oat/arm64/$artifact
  [ "$(awk -v f="$file" '$2==f {n++} END {print n+0}' "$c/oat.sha256")" = 1 ] || exit 1
  expected=$(awk -v f="$file" '$2==f {print $1}' "$c/oat.sha256")
  cp "$c/$file" "system.next/framework/oat/arm64/$artifact"
  printf '%s  /system/framework/oat/arm64/%s\n' "$expected" "$artifact" >> active.next.sha256
 done
fi
rm -rf "$MODROOT/system.previous"
[ ! -d system ] || mv system system.previous
mv system.next system
mv active.next.sha256 active.sha256
rm -rf "$MODROOT/system.previous"
printf 'mpp=%s\nrga=%s\ncodec2=%s\nlibyuv=%s\nsoftware=%s\nhdr_static=%s\nhdr10plus=%s\ncodec2_sdk=%s\ncodec2_buffer=%s\np010_fast=%s\ncodec2_sdk_next=%s\nav1_native=%s\nav1_grain=%s\nav1_afbc=%s\nav1_texture_afbc=%s\ncodec2_lifecycle=%s\nhwc_metadata_reuse=%s\nhwc_scaled_overlay=%s\ngralloc_hdr_lease=%s\ncodec2_delay_notify=%s\nhevc_native_surface=%s\nhevc_eos_pending=%s\np010_pairs=%s\nhwc_mode_cache=%s\nhwc_client_hdr10=%s\nhwui_rgb10=%s\np010_copy_pool=%s\nhdmi_audio_guard=%s\ncodec2_surface=%s\ncodec2_native_backpressure=%s\n' "$MPP" "$RGA" "$C2" "$YUV" "$SOFT" "$HDR_STATIC" "$HDR_PLUS" "$SDK" "$BUFFER" "$P010_FAST" "$SDK_NEXT" "$AV1_NATIVE" "$AV1_GRAIN" "$AV1_AFBC" "$AV1_TEXTURE_AFBC" "$LIFECYCLE" "$HWC_REUSE" "$HWC_SCALE" "$GRALLOC_HDR_LEASE" "$DELAY_NOTIFY" "$HEVC_NATIVE" "$HEVC_EOS" "$P010_PAIRS" "$HWC_MODE_CACHE" "$HWC_CLIENT_HDR" "$HWUI_RGB10" "$P010_POOL" "$HDMI_GUARD" "$SURFACE" "$NATIVE_BP" > active-config
printf 'display_qms=%s\n' "$DISPLAY_QMS" >> active-config
printf 'display_policy=%s\n' "$DISPLAY_POLICY" >> active-config
printf 'display_ddc=%s\n' "$DISPLAY_DDC" >> active-config
printf 'display_session=%s\n' "$DISPLAY_SESSION" >> active-config
rm -f skip_mount selected.sha256
