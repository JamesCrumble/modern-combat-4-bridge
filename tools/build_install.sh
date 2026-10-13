#!/bin/bash
# Builds the port and the APK, installs it on the phone (keeps app data) and restarts the game there
# (straight into the game, past the settings screen).
# Usage: build_install.sh [noinstall|nolaunch]
P=${MC4_ROOT:-$(cd "$(dirname "$0")/.." && pwd)}
set -euo pipefail
source "$P/env.sh"
"$P/build.sh" > "$P/logs/build.log" 2>&1 || { tail -40 "$P/logs/build.log"; exit 1; }
grep -E 'warning|error' "$P/logs/build.log" | grep -vE '^In file' | tail -15 || true
cd "$P/port"
# Our own qemu (qemu/out/, tools/qemu_build.sh) when it exists; QEMU_BIN= (empty) forces Debian's.
Q="$P/qemu/out/qemu-arm-${MC4_QEMU_VARIANT:-F}.stripped"
if [ -z "${QEMU_BIN+x}" ] && [ -x "$Q" ]; then export QEMU_BIN="$Q"; fi
echo "qemu: ${QEMU_BIN:-debian 7.2}"
bash android/build_apk.sh > "$P/logs/apk.log" 2>&1 || { tail -40 "$P/logs/apk.log"; exit 1; }
tail -2 "$P/logs/apk.log"
[ "${1:-}" = noinstall ] && exit 0
"$P/tools/adb" install -r "$(wslpath -w "$P/port/MC4-Bridge-debug.apk")"
[ "${1:-}" = nolaunch ] && exit 0
sh "$P/tools/launch.sh"
