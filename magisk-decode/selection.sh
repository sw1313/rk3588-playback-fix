#!/system/bin/sh
# Caller validates arguments and the existing config before entering here.
# The complete dependency plan is committed with one rename, never partial flags.
apply_selection() {
 # Root bridges may inherit umask 000. Cold binding requires a root-only,
 # non-group-writable selection; preserve that contract on every rename.
 umask 077
 local members=''
 if [ "$1" = bundle ]; then
  . "$MODROOT/bundles.sh"
  members=$(bundle_members "$2") || return 2
 fi
 awk -v mode="$1" -v target="$2" -v choice="$3" -v members="$members" '
 function enable(key, list, n, i) {
  value[key] = 1
  n = split(dep[key], list, " ")
  for (i = 1; i <= n; i++) if (list[i] != "") enable(list[i])
 }
 BEGIN {
  dep["codec2"] = "mpp rga"
  dep["codec2_surface"] = "codec2"
  dep["codec2_native_backpressure"] = "codec2_sdk_next codec2_surface codec2_lifecycle"
  dep["codec2_sdk"] = "codec2"
  dep["codec2_sdk_next"] = "codec2_sdk"
  dep["codec2_buffer"] = "codec2_sdk"
  dep["software"] = "codec2"
  dep["p010_fast"] = "codec2"
  dep["hdr_static"] = "codec2"
  dep["hwc_metadata_reuse"] = "hdr_static"
  dep["hwc_scaled_overlay"] = "hdr_static"
  dep["hwc_mode_cache"] = "hdr_static"
  dep["display_qms"] = "hdr_static"
  dep["display_session"] = "hwc_client_hdr10"
  dep["hwui_rgb10"] = "hdr_static"
  dep["hwc_client_hdr10"] = "hdr_static hwui_rgb10"
  dep["gralloc_hdr_lease"] = "hdr_static codec2_sdk_next"
  dep["codec2_delay_notify"] = "codec2_sdk_next"
  dep["p010_pairs"] = "codec2_sdk_next p010_fast"
  dep["p010_copy_pool"] = "codec2_sdk_next codec2_buffer"
  dep["hevc_eos_pending"] = "codec2_sdk_next"
  dep["hevc_native_surface"] = "codec2_sdk_next hdr_static codec2_surface"
  dep["hdr10plus"] = "codec2"
  dep["av1_native"] = "codec2_sdk_next codec2_surface"
  dep["av1_grain"] = "mpp"
  dep["av1_afbc"] = "av1_native"
  dep["av1_texture_afbc"] = "av1_afbc"
  dep["codec2_lifecycle"] = "codec2_sdk_next"
 }
 { line[NR] = $0; split($0, pair, "="); value[pair[1]] = pair[2] }
 END {
  if (mode == "backend") {
   value["codec2_sdk"] = target != "legacy" ? 1 : 0
   value["codec2_sdk_next"] = target == "latest" ? 1 : 0
   enable("codec2")
  } else if (mode == "bundle") {
   count = split(members, selected, " ")
   for (j = 1; j <= count; j++) {
    if (choice == "on") enable(selected[j])
    else value[selected[j]] = 0
   }
  } else if (choice == "on") enable(target)
  else value[target] = 0
  for (i = 1; i <= NR; i++) {
   split(line[i], pair, "=")
   if (pair[2] == "0" || pair[2] == "1") print pair[1] "=" value[pair[1]]
   else print line[i]
  }
 }' "$MODROOT/config" > "$MODROOT/config.next" || return 1
 chmod 0600 "$MODROOT/config.next" || return 1
 mv "$MODROOT/config.next" "$MODROOT/config"
}
