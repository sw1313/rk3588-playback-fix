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
[ "$(sha256sum "$INPUT/original.img" | cut -d ' ' -f 1)" = eabcbc53b3e56846036c8cb23fd77d246ad919aae427757e17364cb777186264 ] || exit 5
[ "$(wc -c < "$INPUT/original.img" | tr -d ' ')" = 50331648 ] || exit 5
mkdir -p "$STATE";chmod 0700 "$STATE"
mkdir "$STATE/boot";chmod 0700 "$STATE/boot"
for name in original.img candidate.img profile;do
 [ ! -L "$INPUT/$name" ] && [ -f "$INPUT/$name" ] || exit 5
 (umask 077;cp "$INPUT/$name" "$STATE/boot/$name")
 chown 0:0 "$STATE/boot/$name";chmod 0600 "$STATE/boot/$name"
done
(umask 077;printf '%s\n' "$1" > "$STATE/boot/image.sha256")
[ "$(sha256sum "$STATE/boot/profile" | cut -d ' ' -f 1)" = "$2" ] || exit 5
sync
"$MODDIR/bin/cold_boot64" verify
echo 'Private candidate staged and verified. No boot write or setting change performed.'
