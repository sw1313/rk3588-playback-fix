#!/system/bin/sh
# Observe an installed external Zygisk without editing its switches or settings.
set -eu
MODDIR=${0%/*}
STATE=/data/adb/rk3588-mali-global
ZN=/data/adb/modules/zygisksu
LSP=/data/adb/modules/zygisk_lsposed
boot=$(cat /proc/sys/kernel/random/boot_id)
required=false
[ "$(cat "$STATE/zygisk-required-boot" 2>/dev/null)" != "$boot" ] || required=true
if [ "$required" != true ];then
 [ -d "$ZN" ] && [ ! -e "$ZN/disable" ] && [ ! -e "$ZN/remove" ] || exit 0
fi
[ -d "$ZN" ]
report="$STATE/kernel-run/zygisk-status"
timeout 8 "$ZN/bin/zygiskd" status > "$report"
grep -qx 'inject_state:1' "$report"
grep -qx 'modules_with_issue:0' "$report"
for entry in '0 zygote' '1 zygote_secondary';do
 set -- $entry
 pid=$(getprop "init.svc_debug_pid.$2")
 case "$pid" in ''|*[!0-9]*) exit 1;;esac
 grep -qx "zygote_state_$1:1,$1,$pid,$2,3,0" "$report"
done
if [ "$(cat "$STATE/lsposed-required-boot" 2>/dev/null)" = "$boot" ] || { [ -d "$LSP" ] && [ ! -e "$LSP/disable" ] && [ ! -e "$LSP/remove" ]; };then
 grep -q '^modules64:.*zygisk_lsposed' "$report"
 grep -q '^modules32:.*zygisk_lsposed' "$report"
 [ -n "$(pidof lspd)" ]
fi
echo 'PASS external Zygisk injection on both ABIs; configured LSPosed present.'
