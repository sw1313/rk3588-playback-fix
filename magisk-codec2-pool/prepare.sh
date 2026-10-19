#!/system/bin/sh
set -eu
MODROOT=${0%/*}
input=$1/system/lib64/libsfplugin_ccodec.so
sha() { v=$(sha256sum "$1"); echo "${v%% *}"; }
[ "$(sha "$input")" = 88b7f465273698775eea3c505a3b8ea2c5b8f663d342df02cb9454d9c8b0738a ] || exit 1
mkdir -p "$MODROOT/variants/lib64"
variant=$MODROOT/variants/lib64/libsfplugin_ccodec.so
"$MODROOT/bin/byte_patch64" "$input" "$variant" "$MODROOT/contracts/arm64.tsv"
[ "$(sha "$variant")" = f9e01d5ff014ab42a40ddaafee6f3df8122502dd1f0641de58ae75b1b96610dd ] || exit 1
timeout 15 env LD_LIBRARY_PATH="$MODROOT/system/lib64:/system/lib64:/vendor/lib64" "$MODROOT/bin/pool-test64" "$variant" 1
