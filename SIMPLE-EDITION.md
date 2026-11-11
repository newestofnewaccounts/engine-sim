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
| **HOLD TO START** | Hold it until the engine runs. It turns the ignition on for you. |
| **IGNITION** | Turns the engine off and on. |
| **GEAR – / GEAR +** | Change gear. **N** means neutral. |
| **THROTTLE** | Drag to rev the engine. Drag back to the left to idle. |
| **DYNO** | Measures power and torque. |
| **VOLUME** | Sound level, remembered for next time. |
| **ENGINES** | Pick one of the 20 built-in engines, or one you built. |
| **BUILD ENGINE** | Design your own engine: layout, cylinders, size, redline, cam and exhaust. |
| **VIEW** | Simple screen, detailed screen (all gauges) or engine only. |
| **HELP** | Shows the controls. Press **F1** any time. |

You can drag the engine picture to move it and scroll over it to zoom.

## Your engines

Engines you build are saved in `assets/engines/custom` and appear under
**ENGINES** as **[MY ENGINE]**. Click **EDIT** next to one to change it in the
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
- Remembers your last engine, volume and view
- Works when started from a shortcut
