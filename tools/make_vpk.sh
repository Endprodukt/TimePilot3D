#!/bin/sh
# make_vpk.sh - the game as a VectorPie package: build/timepilot3d-<version>.vpk
#
# Run from the project folder after the Pi build (BUILD.md). The package
# holds the Pi program and game.ini; the cabinet's menu installs it from a
# USB drive (Settings -> GAMES -> INSTALL FROM USB).
set -eu
root=$(cd "$(dirname "$0")/.." && pwd)
stage="$root/build/pkg/timepilot3d"
[ -f "$root/build/pi/timepilot3d" ] || { echo "make_vpk: no Pi build in build/pi" >&2; exit 1; }
rm -rf "$root/build/pkg"
mkdir -p "$stage"
cp "$root/build/pi/timepilot3d" "$root/package/game.ini" "$stage/"
[ -f "$root/package/marquee.png" ] && cp "$root/package/marquee.png" "$stage/"
if ls "$root"/package/sounds/*.wav > /dev/null 2>&1; then mkdir -p "$stage/sounds" && cp "$root"/package/sounds/*.wav "$stage/sounds/"; fi
if ls "$root"/package/music/*.wav > /dev/null 2>&1; then mkdir -p "$stage/music" && cp "$root"/package/music/*.wav "$stage/music/"; fi
sh "$root/vendor/VectorPie/sdk/tools/vp-pack/vp-pack.sh" "$stage" "$root/build"

# Built on Windows the program in the archive is not marked as one: the file
# system there has no such mark for vp-pack's chmod to set. Mark it here.
vpk=$(ls "$root"/build/timepilot3d-*.vpk | tail -n 1)
python - "$vpk" <<'PY'
import io, sys, tarfile
path = sys.argv[1]
out = io.BytesIO()
with tarfile.open(path, "r:gz") as old, tarfile.open(fileobj=out, mode="w:gz") as new:
    for member in old.getmembers():
        if member.name == "timepilot3d/timepilot3d":
            member.mode = 0o755
        new.addfile(member, old.extractfile(member) if member.isfile() else None)
open(path, "wb").write(out.getvalue())
PY

# Every version's package belongs in the project's own build folder (the
# user's rule, 2026-10-21) - also when this runs in a worktree of it
# (.claude/worktrees/...), whose build folder nobody looks in.
main=$(dirname "$(git -C "$root" rev-parse --path-format=absolute --git-common-dir)")
if [ -d "$main" ] && [ "$(cd "$main" && pwd)" != "$root" ]; then
    mkdir -p "$main/build" && cp "$vpk" "$main/build/" && echo "make_vpk: also in $main/build/$(basename "$vpk")"
fi
