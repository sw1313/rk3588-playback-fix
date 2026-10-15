#!/system/bin/sh
MODROOT=${0%/*}
if [ -e "$MODROOT/disable" ]; then
 sh "$MODROOT/mode-control.sh" all on
else
 sh "$MODROOT/mode-control.sh" all off
fi
