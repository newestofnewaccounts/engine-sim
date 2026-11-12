# Engine Simulator – Simple Edition

A friendlier version of [Engine Simulator](https://github.com/ange-yaghi/engine-sim) by AngeTheGreat (MIT licence).

## Start it

1. Unzip the download anywhere (for example your Desktop).
2. Open the folder, then the `bin` folder, and double-click **engine-sim-app.exe**.
3. If Windows shows "Windows protected your PC", click **More info → Run anyway**.
   (The app isn't signed by a company, so Windows warns about it.)

## Play

Everything is on the button bar at the bottom of the screen:

| Button | What it does |
|---|---|
| **Hold to start** | Hold it until the engine runs. It turns the ignition on for you. |
| **Ignition** | Turns the engine off and on. |
| **Gear - / Gear +** | Change gear. **N** means neutral. |
| **Throttle** | Drag to rev the engine. Drag back to the left to idle. |
| **Dyno** | Measures power and torque. |
| **Volume** | Sound level, remembered for next time. |
| **Engines** | Pick one of the 20 built-in engines, or one you built. |
| **Build engine** | Design your own engine: layout, cylinders, size, redline, cam and exhaust. |
| **View** | Simple screen, detailed screen (all gauges) or engine only. |
| **Sound studio** | Holds the engine in one steady sound and records it to WAV (see below). |
| **Light / Dark** | Switches between the dark and light theme (remembered). |
| **Help** | Shows the controls. Press **F1** any time. |

## Sound Studio (recording sounds for games)

Click **Sound studio**. A panel opens on the right. Pick a mode:

| Mode | What you hear |
|---|---|
| **Drive** | Normal driving; nothing is held. |
| **Idle** | Throttle closed, no load. Starts the engine for you if needed. |
| **Cruise** | Held at the **Target** rpm with part throttle (set with **Load**). |
| **Wide open** | Held at the target rpm with full throttle (full load). |
| **Overrun** | Spun at the target rpm with the throttle closed (lift-off / engine braking). |
| **Redline** | Held at the redline with full throttle. |
| **Rev limiter** | Full throttle with no load, bouncing off the rev limiter. |

The dynamometer holds the exact rpm, so the sound stays steady. Wait for
**Steady**, then press **Record**, and **Stop** when you have enough. Files are
saved as 16-bit 44.1 kHz mono WAV in the `recordings` folder next to `bin`,
named like `GM LS - Overrun - 3000rpm.wav`. **Open folder** shows them.
The recording is exactly what you hear, so the **Volume** slider affects it.

You can drag the engine picture to move it and scroll over it to zoom.

## Your engines

Engines you build are saved in `assets/engines/custom` and appear under
**Engines** as **My engine**. Click **Edit** next to one to change it in the
builder. They are normal engine script files, so you can also edit them in a
text editor and press **Enter** in the app to reload.

## Keyboard (still works)

S starter · A ignition · Q/W/E/R throttle steps · Space + scroll fine throttle ·
arrows change gear · Shift clutch · D dyno · H + G-scroll RPM hold · Z + scroll volume ·
Tab view · F fullscreen · F1 help · Enter reload · 1–5 slow motion · F2/F3/F4 rotate view · Esc close / quit

## What changed from the original

- On-screen buttons and sliders for every common control
- Help screen on first launch
- Engine picker with real engine names
- In-app engine builder
- Simple view by default (detailed view one click away)
- A mistake in an engine script now shows an error instead of crashing
- Fixed a memory bug that could crash engines with more than 8 cylinders (V10, V12)
- Remembers your last engine, volume, view and theme
- Clean card-based look with a dark and a light theme and the Inter font
- Dashboard with a big rpm gauge, gear, speed, throttle, power and torque
- Sound studio: hold idle / cruise / wide open / overrun / redline / rev limiter and record WAV
- Works when started from a shortcut

## Credits

Based on Engine Simulator by AngeTheGreat (MIT licence). The interface font is
[Inter](https://rsms.me/inter/) by Rasmus Andersson (SIL Open Font License,
see `basic/fonts/Inter/OFL.txt`).
