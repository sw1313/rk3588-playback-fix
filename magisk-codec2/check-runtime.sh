#!/system/bin/sh
MODDIR=${0%/*}
for profile in runtime.sha256 runtime-mpp-compat.sha256 runtime-mpp-hdr.sha256 runtime-mpp-decoder040.sha256; do
 if [ "$1" = --staged ] && [ -n "$2" ]; then
  valid=1
  while read -r expected target; do
   case "$target" in
    /vendor/lib/libmpp.so|/vendor/lib64/libmpp.so) file="$2/mpp/system$target" ;;
    /vendor/lib/librga.so|/vendor/lib64/librga.so) file="$2/rga/system$target" ;;
    *) valid=0; break ;;
   esac
   observed=$(sha256sum "$file") || { valid=0; break; }
   [ "${observed%% *}" = "$expected" ] || { valid=0; break; }
  done < "$MODDIR/contracts/$profile"
  [ "$valid" = 1 ] || continue
 elif ! sha256sum -c "$MODDIR/contracts/$profile" >/dev/null 2>&1; then
  continue
 fi
 if [ -s "$MODDIR/contracts/$profile" ]; then
  echo "PASS verified MPP/RGA profile: $profile"
  exit 0
 fi
done
echo "FAIL no verified MPP/RGA profile matches the active libraries."
exit 1
