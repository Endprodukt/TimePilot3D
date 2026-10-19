#!/bin/bash
# record.sh <seconds> <out.rec> [NAME=value ...]  -  build the game for Linux
# (in WSL or on any Linux PC) and record what it would send to the vector
# board, with tools/fakedvg.py; tools/dvgview.py makes pictures of the
# record. No monitor and no board needed. See BUILD.md.
set -e
here="$(cd "$(dirname "$0")/.." && pwd)"
build="$HOME/tp3d-build"
secs=$1; out=$2; shift 2
cmake -S "$here" -B "$build" -G Ninja -DCMAKE_BUILD_TYPE=Release > /dev/null
cmake --build "$build" 2>&1 | grep -E "warning|error|FAILED" || true
cd "$build"
env SDL_VIDEODRIVER=dummy "$@" python3 "$here/tools/fakedvg.py" "$out" "$secs" ./timepilot3d
