#!/system/bin/sh
# Upgrade a mounted policy using its verified private original, never a patch
# of the currently mounted patched services.jar. No live file is replaced.
set -eu
cd "${0%/*}"
original=922c443c3a2b609b7f7f3755bbfbeb90f3c0b0c141829f3b6151371c2aa607b6
sha() { v=$(sha256sum "$1") || return 1; printf '%s\n' "${v%% *}"; }
if [ -f originals/services.jar ];then
 [ "$(sha originals/services.jar)" = "$original" ];exit
fi
source=/system/framework/services.jar
if [ "$(sha "$source")" != "$original" ];then
 previous=/data/adb/modules/rk3588_decode_stack
 old=$previous/components/display_policy
 test ! -e "$previous/disable"
 test ! -e "$previous/remove"
 test "$(cat "$previous/active-boot-id")" = "$(cat /proc/sys/kernel/random/boot_id)"
 grep -qx display_policy=1 "$previous/active-config"
 expected=$(cat "$old/preflight")
 test "${#expected}" = 64
 test "$(sha "$source")" = "$expected"
 test "$(sha "$old/variants/services.jar")" = "$expected"
 source=$old/originals/services.jar
fi
test "$(sha "$source")" = "$original"
cp "$source" originals/services.jar
chmod 0600 originals/services.jar
test "$(sha originals/services.jar)" = "$original"
