#!/system/bin/sh
# Caller validates arguments and the existing config before entering here.
# The complete dependency plan is committed with one rename, never partial flags.
apply_selection() {
 awk -v mode="$1" -v target="$2" -v choice="$3" '
 function enable(key, list, n, i) {
  value[key] = 1
  n = split(dep[key], list, " ")
  for (i = 1; i <= n; i++) if (list[i] != "") enable(list[i])
 }
 BEGIN {
  dep["codec2"] = "mpp rga"
  dep["codec2_sdk"] = "codec2"
  dep["codec2_sdk_next"] = "codec2_sdk"
  dep["codec2_buffer"] = "codec2_sdk"
  dep["software"] = "codec2"
  dep["p010_fast"] = "codec2"
  dep["hdr_static"] = "codec2"
  dep["hdr10plus"] = "codec2"
  dep["av1_native"] = "codec2_sdk_next"
  dep["av1_grain"] = "mpp"
  dep["av1_afbc"] = "av1_native"
 }
 { line[NR] = $0; split($0, pair, "="); value[pair[1]] = pair[2] }
 END {
  if (mode == "backend") {
   value["codec2_sdk"] = target != "legacy" ? 1 : 0
   value["codec2_sdk_next"] = target == "latest" ? 1 : 0
   enable("codec2")
  } else if (choice == "on") enable(target)
  else value[target] = 0
  for (i = 1; i <= NR; i++) {
   split(line[i], pair, "=")
   if (pair[2] == "0" || pair[2] == "1") print pair[1] "=" value[pair[1]]
   else print line[i]
  }
 }' "$MODROOT/config" > "$MODROOT/config.next" || return 1
 mv "$MODROOT/config.next" "$MODROOT/config"
}
