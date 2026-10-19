#!/bin/sh
# bench.sh <seconds> [NAME=value ...] - run the game on the board for a while with
# VP_STATS, the menu stopped meanwhile as run.sh does it; the frame costs go to bench.log.
secs=$1; shift
sudo -n systemctl stop getty@tty1
while pgrep -x vector_pie_menu > /dev/null; do sleep 1; done
sleep 1
cd "$(dirname "$0")" || exit 1
env VP_OUTPUT=dvg VP_STATS=1 "$@" timeout -s INT "$secs" ./timepilot3d > bench.log 2>&1
sudo -n systemctl start getty@tty1
