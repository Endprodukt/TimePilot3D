# Time Pilot 3D

By Oliver Carell (Endprodukt)

A 3D remake of Konami's arcade classic *Time Pilot* (1982), drawn entirely in vector lines for a real vector monitor.

You fly a fighter through five eras - 1910, 1940, 1970, 1982 and 2001 - each with its own enemy aircraft, its own city below and a boss that is fought in several stages, from a Zeppelin to an alien mothership that turns into a giant robot. Between the eras you race through a time tunnel. After 2001 the next round begins back in 1910, longer and harder than the last.

## What it runs on

The game is made for the **VectorPie** arcade cabinet: a Raspberry Pi driving a vector monitor through a USB-DVG board, played with a flight yoke and four buttons. It is written in C on the VectorPie SDK and installs from a `.vpk` package (the cabinet's menu: Settings, GAMES, INSTALL FROM USB).

## Play it on a PC

There is also a **PC version for Windows**, so anyone can try it without a cabinet. Download the ZIP from the [releases](https://github.com/Endprodukt/TimePilot3D/releases), unpack it and start `timepilot3d.exe`. It plays with the keyboard, a joystick or a gamepad; `README.txt` in the ZIP lists the keys.

## The project

Besides the game itself the repository holds the tools used to build it: packaging for the cabinet and the PC, recording and viewing the vector picture without a monitor, and making the sound effects from CC0 recordings from [freesound.org](https://freesound.org). The project's own notes (`BUILD.md`, `STATUS.md`, `DESIGN.md`) are in German.
