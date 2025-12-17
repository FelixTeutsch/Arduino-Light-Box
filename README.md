# Arduino Light Box

A small Arduino sketch to drive three LEDs (plus an extra side LED) with an IR remote and a push button. It supports manual control (keys 1–3), several animated patterns, speed control, and a toggle for an extra LED.

## Features
- Manual modes for LEDs 1–3 (via remote keys 1/2/3)
- Several animated patterns selectable by remote or the push button
- Speed control with remote `+` / `-`
- Extra LED (replacement for old pattern 7) toggled by a remote key
- Physical button cycles through a subset of patterns

## Hardware
- Arduino (Uno/Nano/any AVR or compatible board)
- IR receiver module (e.g. TSOP382) connected to digital pin `3`
- 3 LEDs (or LED channels) on pins `9`, `10`, `11`
- Optional extra LED on pin `12`
- Push button between `A0` and GND (uses internal pull-up)

### Pin mapping
- `IR_RECEIVE_PIN` : `3`
- `LED1_PIN`       : `9`
- `LED2_PIN`       : `10`
- `LED3_PIN`       : `11`
- `EXTRA_LED_PIN`  : `12` (independent toggle)
- `BUTTON_PIN`     : `A0` (button to GND)

Wiring notes:
- LEDs should be connected with appropriate resistors to the pins and to GND.
- The push button should connect the `A0` pin to GND when pressed. The code uses `INPUT_PULLUP` so no external pull-up is required.

## Remote controls (defaults in sketch)
These values come from the NEC remote codes used in the sketch. If your remote differs, see the Debugging section.
- `POWER` (CMD_POWER): toggle whole sketch running state
- `1` (CMD_1): manual mode — cycles LED1 (off → blink → on → off)
- `2` (CMD_2): manual mode — LED1 on, LED2 cycles, LED3 off
- `3` (CMD_3): manual mode — LED1+LED2 on, LED3 cycles
- `4`, `5`, `6`, `8`: select animation patterns 4,5,6,8
- `9` (CMD_9): toggle extra LED (this replaces old pattern 7)
- `+` (CMD_PLUS): speed up (decrease interval)
- `-` (CMD_MINUS): slow down (increase interval)

## Button behavior
- Physical button on `A0` cycles through patterns: `4 → 5 → 6 → 8 → 9` (skips pattern 7 because that is the extra-LED toggle).
- Button has a simple debounce implemented (200 ms).

## Patterns overview
- Manual (0): direct control via keys 1..3
- Pattern 4: progress-style fill (LEDs light in sequence)
- Pattern 5: knight rider style (left-right)
- Pattern 6: chase (one after another)
- Pattern 8: binary counter on LEDs (0..7)
- Pattern 9: all LEDs blink together
- Extra LED (pin 12) is an independent toggle (used instead of pattern 7)

## Configuration (in `light-box-code.ino`)
- `blinkInterval` controls animation speed (default 300 ms)
- `BLINK_MIN` / `BLINK_MAX` set allowed speed range
- `BUTTON_DEBOUNCE` is 200 ms by default

## Library dependency
This sketch uses the [IRremote](https://github.com/Arduino-IRremote/Arduino-IRremote) library. Install it via the Arduino Library Manager or with the Arduino CLI before compiling.

## Build & Upload
You can open `light-box-code.ino` in the Arduino IDE and upload as usual. Or use `arduino-cli`:

```bash
# replace <fqbn> with your board (e.g. arduino:avr:uno)
arduino-cli compile --fqbn <fqbn> /path/to/Arduino-Light-Box
arduino-cli upload -p /dev/tty.usbmodemXXXX --fqbn <fqbn> /path/to/Arduino-Light-Box
```

## Debugging / Customizing remote codes
- If the remote codes on your remote differ, enable serial debug to read incoming commands. In the sketch there is a commented `Serial.print` line near where IR commands are handled — uncomment it to print codes to the Serial Monitor.
- You can also use example IR receiver sketches (like `IRrecvDump`) from the IRremote library to capture codes, then update the `CMD_*` constants in `light-box-code.ino`.

## Notes
- Turning the sketch “off” (Power) stops animations and turns LEDs off, but the extra LED toggle is intentionally left unchanged on power off.
- No persistent storage: states are not saved across power cycles.

## License
See the `LICENSE` file in this repository.

---

If you want, I can: add a small wiring diagram (SVG/ASCII), include a `platformio.ini`, or create an example `boards.txt` / arduino-cli command tailored for your board — tell me which board you use.
# Arduino-Light-Box
Code for the Arduino Light Box for the movie Missing Peace! &amp; the subject Prototyping
