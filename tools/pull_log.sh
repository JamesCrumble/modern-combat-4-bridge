#!/bin/sh
# Copies the phone's game log to logs/phone.log
P=${MC4_ROOT:-$(cd "$(dirname "$0")/.." && pwd)}
A="$P/tools/adb"
D=/sdcard/BridgeGames/com.jamescrumble.mc4
[ -n "$($A shell ls $D/mc4.log 2>/dev/null)" ] || D=/sdcard/Android/data/com.jamescrumble.mc4/files
$A shell cat $D/mc4.log | tr -d '\r' > "$P/logs/phone.log"
wc -l "$P/logs/phone.log"
