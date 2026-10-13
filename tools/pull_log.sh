#!/bin/sh
# Copies the phone's game log to logs/phone.log
P=${MC4_ROOT:-$(cd "$(dirname "$0")/.." && pwd)}
"$P/tools/adb" shell cat /sdcard/Android/data/com.jamescrumble.mc4/files/mc4.log | tr -d '\r' > "$P/logs/phone.log"
wc -l "$P/logs/phone.log"
