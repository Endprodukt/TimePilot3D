#!/bin/sh
# make_pc.sh - the PC version of the build that stands here, to test on a
# PC (with a digital joystick or the keys) and to hand to others: the PC
# build made anew, then a folder build/TimePilot3D-<version>-PC (program,
# SDL2.dll, game.ini, sounds, music, LIESMICH.txt - no save/, so it begins
# with the first start's setup) and the same as a ZIP, both in the
# project's own build folder (F:\Time Pilot 3D\build) also when this runs
# in a worktree. Every version gets one (the user's rule, 2026-10-21);
# tools/deploy.sh runs this after each deploy. Git Bash, MSYS2 (BUILD.md).
set -eu
root=$(cd "$(dirname "$0")/.." && pwd)
export PATH="/c/msys64/mingw64/bin:$PATH"
[ -f "$root/build/pc/CMakeCache.txt" ] || cmake -S "$root" -B "$root/build/pc" -G Ninja -DCMAKE_C_COMPILER=gcc -DCMAKE_CXX_COMPILER=g++ -DCMAKE_PREFIX_PATH=C:/msys64/mingw64 > /dev/null
cmake --build "$root/build/pc" > /dev/null
version=$(sed -n 's/^version *= *//p' "$root/package/game.ini" | tr -d '\r')
main=$(dirname "$(git -C "$root" rev-parse --path-format=absolute --git-common-dir)")
name="TimePilot3D-$version-PC"
out="$main/build/$name"
rm -rf "$out" "$out.zip"
mkdir -p "$out"
cp "$root/build/pc/timepilot3d.exe" "$root/build/pc/SDL2.dll" "$root/package/game.ini" "$out/"
cp "$root/package/LIESMICH-PC.txt" "$out/LIESMICH.txt"
cp -r "$root/package/sounds" "$root/package/music" "$out/"
rm -f "$out/sounds/README.txt"
powershell.exe -NoProfile -Command "Compress-Archive -Path '$(cygpath -w "$out")' -DestinationPath '$(cygpath -w "$out.zip")' -Force"
echo "make_pc: $out (and .zip)"
