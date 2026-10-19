#!/usr/bin/env python3
# fakedvg.py <out-file> <seconds> <command...>
# Runs a VectorPie game against a pretended vector board and records what it
# sends, so the pictures can be looked at without a monitor (tools/dvgview.py
# turns the record into PNGs). Runs on the cabinet or any Linux; the real
# board is not touched. Each record: 8 bytes time (double, seconds since the
# start), 4 bytes length, the bytes as sent. See BUILD.md.
import os, pty, select, struct, subprocess, sys, time, tty

out, secs, cmd = sys.argv[1], float(sys.argv[2]), sys.argv[3:]
master, slave = pty.openpty()
tty.setraw(slave)
env = dict(os.environ, VP_OUTPUT="dvg", VP_DVG_PORT=os.ttyname(slave), SDL_AUDIODRIVER="dummy")
p = subprocess.Popen(cmd, env=env)
start = time.time()
with open(out, "wb") as f:
    while time.time() - start < secs and p.poll() is None:
        if select.select([master], [], [], 0.2)[0]:
            data = os.read(master, 65536)
            f.write(struct.pack("<dI", time.time() - start, len(data)) + data)
if p.poll() is None:
    p.send_signal(2)
    try:
        p.wait(3)
    except subprocess.TimeoutExpired:
        p.kill()
