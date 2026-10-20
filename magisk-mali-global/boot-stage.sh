#!/system/bin/sh
# Stage the private exact-root bundle; never writes boot or alters selection.
set -eu
MODDIR=${0%/*}
STATE=/data/adb/rk3588-mali-global
INPUT=/data/local/tmp/rk3588-mali-cold-prepared
[ "$#" = 2 ] && [ "$(id -u)" = 0 ] || exit 2
for digest in "$1" "$2";do
 [ "${#digest}" = 64 ] || exit 2
 case "$digest" in *[!a-f0-9]*) exit 2;;esac
done
[ ! -L "$STATE" ] && [ ! -L "$INPUT" ] && [ -d "$INPUT" ] || exit 3
[ ! -e "$STATE/boot" ] || { echo 'Existing boot store retained; replacement requires explicit review.';exit 4; }
. "$MODDIR/root-startup.sh"
root_startup_supported || { echo "First-binding backend unavailable: $(root_startup_reason). Ordinary module features remain separately available.";exit 5; }
sh "$MODDIR/platform-check.sh" >/dev/null 2>&1 || { echo 'Firmware graphics dependencies differ';exit 5; }
[ ! -L "$INPUT/profile" ] && [ -f "$INPUT/profile" ] || exit 5
[ "$(sha256sum "$INPUT/profile" | cut -d ' ' -f 1)" = "$2" ] || exit 5
original=$(sed -n 's/^original_sha256 //p' "$INPUT/profile")
[ "${#original}" = 64 ] || exit 5
case "$original" in *[!a-f0-9]*) exit 5;; esac
[ "$(sha256sum "$INPUT/original.img" | cut -d ' ' -f 1)" = "$original" ] || exit 5
[ "$(wc -c < "$INPUT/original.img" | tr -d ' ')" = 50331648 ] || exit 5
current=$(sha256sum /dev/block/mmcblk0p7 | cut -d ' ' -f 1)
[ "$current" = "$original" ] || [ "$current" = "$1" ] || { echo 'Prepared backup does not belong to the current boot';exit 5; }
mkdir -p "$STATE";chmod 0700 "$STATE"
mkdir "$STATE/boot";chmod 0700 "$STATE/boot"
# Remove only our known staged files if verification fails. Never touch an
# existing store or boot partition; activation still requires a separate action.
cleanup_stage() {
 rm -f "$STATE/boot/original.img" "$STATE/boot/candidate.img" "$STATE/boot/profile" "$STATE/boot/image.sha256"
 rmdir "$STATE/boot" 2>/dev/null || true
}
trap cleanup_stage EXIT
for name in original.img candidate.img profile;do
 [ ! -L "$INPUT/$name" ] && [ -f "$INPUT/$name" ] || exit 5
 (umask 077;cp "$INPUT/$name" "$STATE/boot/$name")
 chown 0:0 "$STATE/boot/$name";chmod 0600 "$STATE/boot/$name"
done
(umask 077;printf '%s\n' "$1" > "$STATE/boot/image.sha256")
[ "$(sha256sum "$STATE/boot/profile" | cut -d ' ' -f 1)" = "$2" ] || exit 5
sync
"$MODDIR/bin/cold_boot64" verify
trap - EXIT
echo 'Private candidate staged and verified. No boot write or setting change performed.'
