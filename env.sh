#!/usr/bin/env bash
# Source this from the repository root: `source env.sh`. Touches only the current shell.
_P="$(cd "$(dirname "${BASH_SOURCE[0]:-$0}")" && pwd)"
# Third-party toolchains/dependencies: looked up inside this project first (jdk/, android-sdk/, ...), then in ../__shared-third-party__
export SHARED_3P="$(cd "$_P/../__shared-third-party__" 2>/dev/null && pwd || echo "$_P/../__shared-third-party__")"
_tp() { if [ -e "$_P/${2:-$1}" ]; then echo "$_P/${2:-$1}"; else echo "$SHARED_3P/$1"; fi; }
export MC4_PROBE="$_P"
export MC4_REPO="$_P/port"
export MC4_GAMEDIR="$_P/mc4data"
export JAVA_HOME="$(_tp jdk)"
export ANDROID_HOME="$(_tp android-sdk)"
export ANDROID_SDK_ROOT="$ANDROID_HOME"
export PKG_CONFIG_PATH="$(_tp sdl-arm)/lib/pkgconfig"
export PATH="$JAVA_HOME/bin:$ANDROID_HOME/build-tools/34.0.0:$ANDROID_HOME/cmdline-tools/latest/bin:$PATH"
unset -f _tp
unset _P
