#!/system/bin/sh
# Preserve every original graphics ABI check. Accept only complete, real
# Decode HDR pairs. QMS trials additionally require an exact current-boot
# owner/loader receipt; this compatibility check is not HDMI acceptance.
set -eu
MODDIR=${0%/*}
decode=/data/adb/modules/rk3588_decode_stack
sha() { v=$(sha256sum "$1") || return 1; echo "${v%% *}"; }
qms_capture_pair() {
 [ ! -e "$decode/disable" ] && [ ! -e "$decode/remove" ] || return 1
 grep -qx 'display_qms=1' "$decode/active-config" || return 1
 boot=$(cat /proc/sys/kernel/random/boot_id)
 [ "$(getprop vendor.rkhdr.qms_boot)" = "$boot" ] &&
 [ "$(getprop vendor.rkhdr.qms)" = active ] || return 1
 [ "$(cat /sys/module/rk_decode_qms/parameters/owner_online)" = 1 ] || return 1
 qms_profile=$(sed -n 's/^profile_id //p' "$decode/components/qms/profile")
 case "$qms_profile" in
  af8c99be2351addbbf36cfe8ec9672e5b5350577dba85f30f46e0e08c4d69105)
   qms_module=e66681590d923d41d0da3565d6bbbbeccd0a81dca2109d0b5755371f49cc0cb6
   qms_helper=4cba587a07dfef61bf6d3dc59e9867bec4cc9ff5b4d725dac786612beb54f00e ;;
  32252295ca6cf439885a200f8d0b681bd8a7d09ce42cd51dd5c6131cc92bcf0d)
   qms_module=a67dc768e2646a10006f99e5f5a6b6c95fefe5f9b65134853f64d93f37c61181
   qms_helper=35e6a5260798387c2a2b5f62f84c1605c01023f06d2b1872ca37f391c1a5b73b ;;
  06dd246e478f3a5d0076b4e284f812aed617f55281384c20f0b11f0abf859d4e)
   qms_module=af570721365c144b0e3b01c3c527f9fba295d37fadec9a204426fea80ab57571
   qms_helper=606177689ce43b55850bd5d31dc57de5c7d62d6df8be12bf5bd431e4a52d4d47 ;;
  6152e6e99935c7abe63acf4d06113fc7feb0d20911d1168bf93c005abd400d05)
   qms_module=80a7d22b66759ab3df0bb389dc9d15b5f050e063da866a4f83dc79d302c1f9e0
   qms_helper=434ccc1c8dfd80570989814483d41e991a80c6ae344b93cb50a79da10637111e ;;
  2de153df39ad38de1a1ba31455713f44fafe4728ca74d0e65534caa187391447)
   qms_module=d0dec51c8153f4370abeaf571e5b03a1d3438f513dda95c3bf8d63a40cf33c48
   qms_helper=269e10f99a7bcdeb35df73e07d4e0fef495d29aedc4c6d1713980277f8fdd6a3 ;;
  bdc1b81df3b459ca96b4f66106c65db6336f5a15f352e08a77939583c74e097e)
   qms_module=45ffdfb0493ce3d0a6d6246c18d9513ce78fd67ed8a61ebff2018a5caab2902d
   qms_helper=3d1acdbd16f71289ddbd8f5fb9156e9f77cd7d791214ad0232545e09fd7ad5fb ;;
  a800329a8c6acbeca5e1ba9e97c91ed77b3dcd566051cc35d054e9ef82a83b97)
   qms_module=54344c90c5bc292e6c305371de967d6ae5576913d24dcf18e234927c53e3d468
   qms_helper=09ce63946356f78e9acdfb52f8a9b26201d8a5edcbf863885f8e69fddef9e865 ;;
  fe9d5bd4f54d85c55ae602ab9999f8be1f7ea4917c9d3c4d4bd0b15c11ca1e1a)
   qms_module=292459a5108ea3d1a384c105d90999ae369992e3f817b09011de8599031c550f
   qms_helper=ebe64d31d03884ad23438185262b11d65511bf387032a4437171bf3c38d7e892 ;;
  7ba15bf86aeb16c7d3d950fb000dd8cf16ce99c588a6d5a1d347f9ca3f36431c)
   qms_module=7fb4ea7533a4ffdf4a4f370872964c91c5148d30a6d8c748517b7fb56ffbbe2c
   qms_helper=5a2d337100850838466516fe66a6799fed65e0b3463a22bf7740f0ddba6c3375 ;;
  a7d37e0d118a2800bdaca17cde2030af112f462a732d6554a9146a60efda70cb)
   qms_module=980f8f73e0219886ddddcf1be541a929050be27098070686a402b2f838780330
   qms_helper=0d4e841a4c34d0cff4c6210587ebaf84ca07aad6a7ac5d0f49ad18c07c9168a5 ;;
  *) return 1 ;;
 esac
 [ "$(cat /data/adb/rk3588-decode-qms/receipt)" = "$boot $qms_profile prepared" ] || return 1
 [ "$(sha "$decode/components/qms/rk_decode_qms.ko")" = "$qms_module" ] || return 1
 [ "$(sha "$decode/components/qms/boot-stage64")" = "$qms_helper" ] || return 1
 sha256sum -c "$MODDIR/contracts/${1:-decode-sf-target-capture-qms.sha256}"
}
sf_pair() {
 [ "$1" = fddde345b957a0898c44cf1c62d23e9736d98d246e46bb3f39244c0e7d6b0d1a ] || return 1
 [ ! -e "$decode/disable" ] && [ -f "$decode/active-boot-id" ] || return 1
 [ "$(cat "$decode/active-boot-id")" = "$(cat /proc/sys/kernel/random/boot_id)" ] || return 1
 grep -qx 'hwc_client_hdr10=1' "$decode/active-config" || return 1
 [ "$(getprop debug.rkhdr.client_hdr10)" = 1 ] || return 1
 [ "$(cat "$decode/components/hdr/sf-target10-preflight")" = a6f27121a627b42f53066756262312b3afe80800271afe14e57923081a72929f ] || return 1
 [ "$(sha "$decode/components/hdr/originals/bin/surfaceflinger")" = "$1" ] || return 1
 case "$(sha /system/lib64/libsfhdr.so)" in
  1386aefcdc263c61c0c04e7d952ddaf8cd665eb119b91d3c3c3abfc267ac787e)
   grep -qx 'display_session=1' "$decode/active-config" || return 1
   # Exact loader linkage is GPU-compatible even if the independent session
   # callback declined its ABI. Decode withdraws that item; never disable Mali
   # merely because this optional display policy failed to attach.
   if [ "$(sha /vendor/lib64/hw/hwcomposer.rk30board.so)" = b34b647f6664de292c5f10d3d75b1a0bb5ae69c33960e088530ecb8ad96102e9 ];then
    qms_capture_pair decode-sf-target-session-qms.sha256 || return 1
    pair=decode-sf-target-session-qms.sha256
   elif [ "$(sha /vendor/lib64/hw/hwcomposer.rk30board.so)" = 77030ef54b8c8146966c1926f81d34fe309221f5d54344c54b2c7acba4767e5c ];then
    pair=decode-sf-target-session.sha256
   else return 1;fi ;;
  894b3a1b73d6d9b070377114cc518ef8cae879b9204ebe9b0d740a968a6f4891) pair=decode-sf-target-mapper.sha256 ;;
  bc552b3cb34b95bbc355a15033b228cab922950986bcdb84501222b2beea80f3)
   if [ "$(sha /vendor/lib64/hw/hwcomposer.rk30board.so)" = b34b647f6664de292c5f10d3d75b1a0bb5ae69c33960e088530ecb8ad96102e9 ];then
    qms_capture_pair || return 1
    pair=decode-sf-target-capture-qms.sha256
   else pair=decode-sf-target-capture.sha256;fi ;;
  *) return 1 ;;
 esac
 sha256sum -c "$MODDIR/contracts/$pair" || return 1
 echo '/system/bin/surfaceflinger: OK (exact paired Decode Stack physical HDR adapter)'
}
while read -r expected path; do
 [ -n "$expected" ] && [ -n "$path" ] || exit 1
 if [ "$(sha "$path")" = "$expected" ]; then
  echo "$path: OK"
 elif [ "$path" = /system/bin/surfaceflinger ]; then
  sf_pair "$expected" || exit 1
 else
  echo "$path: FAILED";exit 1
 fi
done < "$MODDIR/contracts/unchanged-platform.sha256"
