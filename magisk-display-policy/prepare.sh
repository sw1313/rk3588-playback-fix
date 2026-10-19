#!/system/bin/sh
set -eu
C=${0%/*}
cd "$C"
sha256sum -c checksums.sha256 >/dev/null 2>&1
mkdir -p originals variants
chmod 0700 originals variants
sh ./original-services.sh
rm -f variants/services.next.jar preflight
dalvikvm -cp patcher.jar com.scrcpyandroid.displaypolicy.PatchServices originals/services.jar variants/services.next.jar helper.dex
while read -r expected entry; do
 [ "$(unzip -p variants/services.next.jar "$entry" | sha256sum | cut -d ' ' -f 1)" = "$expected" ]
done < contracts/dex.sha256
dalvikvm -cp "variants/services.next.jar:probe.jar" com.scrcpyandroid.displaypolicy.AbiProbe
mv variants/services.next.jar variants/services.jar
sha256sum variants/services.jar | cut -d ' ' -f 1 > preflight
chmod 0644 variants/services.jar preflight
sh ./compile-artifacts.sh
