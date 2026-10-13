#!/bin/bash
# Disassembles a range of the game's library. Usage: tools/dis.sh <start hex> <length hex or symbol size>
P=$(cd "$(dirname "$0")/.." && pwd)
L=$P/mc4data/lib/armeabi-v7a/libModernCombat4.so
arm-linux-gnueabihf-objdump -d -C --no-show-raw-insn --start-address=$((0x$1)) --stop-address=$((0x$1 + 0x${2:-100})) "$L" | tail -n +7
