#!/bin/sh
# deploy.sh - copy the Pi build and game.ini to the cabinet, put the new
# program in place and start it there, so that what is on the screen is the
# new build (the user's wish, 2026-10-12). A game that is running is ended
# first. When the game is left, the cabinet's menu comes back (run.sh).
# Run from the project's folder, in Git Bash. See BUILD.md.
K="-i $HOME/.ssh/id_ed25519_vectorpie -o ConnectTimeout=8 -o BatchMode=yes"
H=pi@192.168.0.122
D=/persistent/games/timepilot3d
scp -q $K build/pi/timepilot3d $H:$D/timepilot3d.new || exit 1
scp -q $K package/game.ini $H:$D/game.ini || exit 1
if ls package/sounds/*.wav > /dev/null 2>&1; then ssh $K $H "mkdir -p $D/sounds" && scp -q $K package/sounds/*.wav $H:$D/sounds/ || exit 1; fi
if ls package/music/*.wav > /dev/null 2>&1; then ssh $K $H "mkdir -p $D/music" && scp -q $K package/music/*.wav $H:$D/music/ || exit 1; fi
# The menu finds the game by its folder alone (its game.ini, as it finds
# every SDK game: no line in gamelist.ini, since 2026-10-21), and takes the
# marquee and the high scores from it (marquee.png, save/highscores.txt).
# All of it is on /persistent, which a system update leaves alone (BUILD.md).
scp -q $K package/marquee.png $H:$D/marquee.png || exit 1
ssh $K $H "mkdir -p $D/save && touch $D/save/highscores.txt" || exit 1
ssh $K $H "cd $D && pkill -INT -x timepilot3d; for i in 1 2 3 4 5 6; do pgrep -x timepilot3d > /dev/null || break; sleep 0.5; done; pkill -KILL -x timepilot3d; chmod 755 timepilot3d.new run.sh && mv -f timepilot3d.new timepilot3d && md5sum timepilot3d && grep '^version' game.ini && (setsid nohup ./run.sh > /dev/null 2>&1 &) ; sleep 6; pgrep -x timepilot3d > /dev/null && echo game-running || echo GAME-NOT-RUNNING"
md5sum build/pi/timepilot3d
