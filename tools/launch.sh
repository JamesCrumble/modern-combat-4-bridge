#!/bin/sh
# Installs the built APK (if "install" given), force-stops and starts the game.
P=${MC4_ROOT:-$(cd "$(dirname "$0")/.." && pwd)}
A=$P/tools/adb
if [ "${1:-}" = install ]; then
  $A install -r "$(wslpath -w $P/port/MC4-Bridge-debug.apk)" || exit 1
fi
$A shell am force-stop com.jamescrumble.mc4
$A shell am start -n com.jamescrumble.mc4/.MainActivity
