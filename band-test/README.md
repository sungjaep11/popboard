# Directional wrist-band bridge

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
