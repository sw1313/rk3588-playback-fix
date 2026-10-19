#!/system/bin/sh
# Compile privately on the exact device. Keep ART's trusted-system-OAT policy.
set -eu
umask 077
C=${0%/*}
cd "$C"
[ "$(getprop ro.build.version.sdk)" = 32 ]
[ "$(getprop ro.board.platform)" = rk3588 ]
[ "$(sha256sum variants/services.jar | cut -d ' ' -f 1)" = "$(cat preflight)" ]
export DEX2OATBOOTCLASSPATH="$(sed -n 's/^export DEX2OATBOOTCLASSPATH //p' /data/system/environ/classpath)"
export BOOTCLASSPATH="$(sed -n 's/^export BOOTCLASSPATH //p' /data/system/environ/classpath)"
[ -n "$DEX2OATBOOTCLASSPATH" ] && [ -n "$BOOTCLASSPATH" ]
mkdir -p variants/oat/arm64
rm -f oat.sha256 oat-inputs.sha256
# Match the original system-server loader context and boot images. Running the
# installed compiler preserves the user's compiler adapter (e.g. LSPosed).
/apex/com.android.art/bin/dex2oat64 -j2 --runtime-arg -Xms64m --runtime-arg -Xmx512m \
 --runtime-arg "-Xbootclasspath:$DEX2OATBOOTCLASSPATH" \
 --boot-image=/apex/com.android.art/javalib/boot.art:/system/framework/boot-framework.art \
 --instruction-set=arm64 --instruction-set-variant=cortex-a53 --instruction-set-features=default \
 --compiler-filter=speed --compilation-reason=install \
 --class-loader-context='PCL[/system/framework/com.android.location.provider.jar]' \
 --dex-file=variants/services.jar --dex-location=/system/framework/services.jar \
 --oat-file=variants/oat/arm64/services.odex --oat-location=/system/framework/oat/arm64/services.odex \
 --app-image-file=variants/oat/arm64/services.art > variants/compile.log 2>&1
# The next precompiled system-server jar records services' dex checksums.
# Recompile its unchanged source against the new services, with canonical
# runtime paths stored in the context. No Ethernet Java code is patched.
/apex/com.android.art/bin/dex2oat64 -j2 --runtime-arg -Xms64m --runtime-arg -Xmx512m \
 --runtime-arg "-Xbootclasspath:$DEX2OATBOOTCLASSPATH" \
 --boot-image=/apex/com.android.art/javalib/boot.art:/system/framework/boot-framework.art \
 --instruction-set=arm64 --instruction-set-variant=cortex-a53 --instruction-set-features=default \
 --compiler-filter=speed --compilation-reason=install \
 --class-loader-context="PCL[/system/framework/com.android.location.provider.jar:$PWD/variants/services.jar]" \
 --stored-class-loader-context='PCL[/system/framework/com.android.location.provider.jar:/system/framework/services.jar]' \
 --dex-file=/system/framework/ethernet-service.jar --dex-location=/system/framework/ethernet-service.jar \
 --oat-file=variants/oat/arm64/ethernet-service.odex --oat-location=/system/framework/oat/arm64/ethernet-service.odex \
 >> variants/compile.log 2>&1
for suffix in odex vdex art; do
 [ -s "variants/oat/arm64/services.$suffix" ]
 sha256sum "variants/oat/arm64/services.$suffix" >> oat.sha256
done
for suffix in odex vdex; do
 [ -s "variants/oat/arm64/ethernet-service.$suffix" ]
 sha256sum "variants/oat/arm64/ethernet-service.$suffix" >> oat.sha256
done
# These are generated private contracts, never firmware shipped in the zip.
oldifs=$IFS; IFS=:
for input in $DEX2OATBOOTCLASSPATH; do sha256sum "$input"; done > oat-inputs.sha256
IFS=$oldifs
sha256sum /system/framework/com.android.location.provider.jar /system/framework/ethernet-service.jar /apex/com.android.art/lib64/libart.so >> oat-inputs.sha256
sha256sum /apex/com.android.art/javalib/arm64/boot*.art /apex/com.android.art/javalib/arm64/boot*.oat \
 /system/framework/arm64/boot*.art /system/framework/arm64/boot*.oat >> oat-inputs.sha256
chmod 0644 oat.sha256 oat-inputs.sha256 variants/oat/arm64/services.* variants/oat/arm64/ethernet-service.*
