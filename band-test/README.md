# Directional wrist-band connection

## Direct Teensy-to-PCA9685 I2C (current photographed wiring)

Teensy pin 16 connects to band SCL, pin 17 to SDA, with common GND.
`vcm-tune` now uses `Wire1` at 100 kHz to drive PCA9685 address `0x60`.
MKR1000 is no longer in the command path. Its existing SDA/SCL wiring may stay.

1. **First upload `band-passive/band-passive.ino` to MKR1000.** This leaves its
   I2C pins as inputs. Do not run `band-test` on the shared bus: two controllers
   could collide or overwrite each other's motor settings.
2. Upload `vcm-tune/vcm-tune.ino` to Teensy 4.0 (USB type Serial + HID).
3. Through Teensy USB serial at 115200, send `bandstatus`. Expected:
   `#band-direct bus=Wire1 scl=16 sda=17 address=0x60 ready=1 code=0`.
   This reports the last initialization/transaction state, not motor power.
4. Send `bandtest 0` through `bandtest 7` to test each raw motor at amplitude
   1000 for 50 ms, bypassing touch/force thresholds. `bandstop` stops both wrists.

The driver retries failed initialization every 500 ms and reports I2C write
errors. Normal key feedback retains `BAND_DIRECT_*` settings in `vcm-tune.ino`:
250 Hz, 50 ms pulse, amplitude 1000, buckling trigger, center deadband 0.47.
Pulse stop timing is serviced in the main loop, so a blocking sensor read may
extend a pulse. A successful I2C response does not verify motor supply voltage.
The web page's band controls use the Teensy USB connection. Updated firmware
is required (`bandstatus` reports `#band-control version=1`). All band settings
are sent with `bandset <name> <value>` after 100 ms of idle editing. Settings
are kept in browser localStorage and reapplied on reconnect, not in EEPROM.
The test button uses the configured amplitude/frequency/duration for one pulse;
Stop ends current output. Touch events are never bridged back through the PC.
Supported names: freq, amp, ms, mode, trigger, ampMode, centerAmp, edgeAmp,
curve, deadband, invert.

## Legacy PC bridge (requires disconnecting Teensy from the band I2C bus)

The instructions below are for the previous USB bridge setup. Do not run it
alongside the direct I2C firmware on the same connected bus.

1. Upload `vcm-tune/vcm-tune.ino` to the Teensy 4.0.
2. Upload `band-test/band-test.ino` to the Arduino MKR1000.
3. Connect both boards to the PC or the same USB hub.
4. Create the local Python environment once:
   `python3 -m venv band-test/.venv`
5. Install the bridge dependency once:
   `band-test/.venv/bin/python -m pip install pyserial`
6. Check ports: `band-test/.venv/bin/python band-test/band_bridge.py --list`
7. Run: `band-test/.venv/bin/python band-test/band_bridge.py`

If automatic detection is ambiguous:

```sh
band-test/.venv/bin/python band-test/band_bridge.py \
  --teensy /dev/cu.usbmodem_TEENSY \
  --band /dev/cu.usbmodem_MKR1000
```

Do not keep Arduino Serial Monitor, Plotter, or Web Serial open on either port
while the bridge is running.

Alternatively, open `test.html` in Chrome or Edge. Connect the Teensy with
`device connect`, then connect the MKR1000 with `band connect`. The page becomes
the bridge, so do not run `band_bridge.py` at the same time.

The `band direction invert` toggle in `test.html` swaps left/right and up/down
before forwarding each hit. The choice persists across page reloads.

The MKR1000 controls four motors per wrist. Default mapping:

- left wrist `L/U/R/D` = motors `1/0/3/2`
- right wrist `L/U/R/D` = motors `7/4/5/6`
- each key hit activates exactly one motor; the stronger of its horizontal and
  vertical offsets determines the direction (horizontal wins an exact tie)

Identify the physical channel positions with:

```sh
band-test/.venv/bin/python band-test/band_bridge.py --test-motors
```

Then reorder `MOTOR_FOR_HAND_DIRECTION` in `band-test.ino` if needed.
