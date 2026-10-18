#!/bin/sh
# grep -E "$1" over the phone's game log (last $2 lines, default 40).
P=${MC4_ROOT:-$(cd "$(dirname "$0")/.." && pwd)}
A=$P/tools/adb
D=/sdcard/BridgeGames/com.jamescrumble.mc4
[ -n "$($A shell ls $D/mc4.log 2>/dev/null)" ] || D=/sdcard/Android/data/com.jamescrumble.mc4/files
F=$D/mc4.log
$A shell "grep -aE '$1' $F | tail -${2:-40}"
