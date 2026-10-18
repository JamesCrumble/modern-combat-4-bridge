#!/bin/sh
# Copies the player's game files to the phone: the game's APK (the Ultra one under gamebin/) and the
# two .obb files from mc4data/obb. The folders are made first and opened to the app (2777): what
# adb creates belongs to the shell user and is closed to the app otherwise.
# About 2.1 GB. Usage: tools/push_data.sh
P=${MC4_ROOT:-$(cd "$(dirname "$0")/.." && pwd)}
A=$P/tools/adb
D=/sdcard/Android/data/com.jamescrumble.mc4/files
G=com.gameloft.android.ANMP.GloftM4HM
APK=$(find "$P"/gamebin -name '*Ultra*.apk' 2>/dev/null | head -1)
[ -n "$APK" ] || { echo "no Ultra game APK under gamebin/"; exit 1; }
[ -f "$P/mc4data/obb/main.1100.$G.obb" ] || { echo "no .obb files in mc4data/obb"; exit 1; }
open_dirs() { $A shell "chmod 2777 ${D%/files} $D $D/$G"; }
$A shell mkdir -p "$D/$G" || exit 1
open_dirs
$A push "$(wslpath -w "$APK")" "$D/mc4-1.2.3e.apk" || exit 1
for f in "$P"/mc4data/obb/*.obb; do
  $A push "$(wslpath -w "$(readlink -f "$f")")" "$D/$G/" || exit 1
done
# Again after the copy: the app's own folder, made here before the app is installed, is the shell user's too.
open_dirs
$A shell "ls -la $D $D/$G; du -sh $D"
