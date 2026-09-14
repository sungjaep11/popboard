#!/usr/bin/env python3
"""Forward directional key-hit events from a Teensy to an MKR1000 over USB."""

from __future__ import annotations

import argparse
import re
import sys
import time

try:
    import serial
    from serial.tools import list_ports
except ImportError:
    print("pyserial is required: python3 -m pip install pyserial", file=sys.stderr)
    raise SystemExit(2)


BAND_EVENT = re.compile(rb"^#band hand=([LR]) mask=([1-9]|1[0-5])$")


def available_ports():
    return sorted(list_ports.comports(), key=lambda port: port.device)


def describe_ports(ports) -> str:
    if not ports:
        return "  (no serial ports found)"
    return "\n".join(
        f"  {p.device}: {p.description or 'unknown'} "
        f"[VID:PID={p.vid or 0:04X}:{p.pid or 0:04X}]"
        for p in ports
    )


def find_teensy(ports):
    matches = [
        p for p in ports
        if p.vid == 0x16C0 or "teensy" in (p.description or "").lower()
    ]
    return matches[0].device if len(matches) == 1 else None


def find_band(ports):
    matches = [
        p for p in ports
        if "mkr1000" in (p.description or "").lower()
        or "mkr 1000" in (p.description or "").lower()
        or (p.vid == 0x2341 and p.vid != 0x16C0)
    ]
    return matches[0].device if len(matches) == 1 else None


def resolve_ports(args, need_teensy=True):
    ports = available_ports()
    if args.list:
        print(describe_ports(ports))
        raise SystemExit(0)

    teensy = args.teensy or (find_teensy(ports) if need_teensy else None)
    band = args.band or find_band(ports)
    if not band or (need_teensy and (not teensy or teensy == band)):
        print("Could not uniquely identify both boards.", file=sys.stderr)
        print(describe_ports(ports), file=sys.stderr)
        print(
            "Specify them explicitly with --teensy PORT --band PORT.",
            file=sys.stderr,
        )
        raise SystemExit(2)
    return teensy, band


def drain_lines(port, prefix: str):
    while port.in_waiting:
        line = port.readline().decode("utf-8", errors="replace").strip()
        if line:
            print(f"{prefix} {line}")


def bridge(teensy_path: str, band_path: str, baud: int, startup_delay: float):
    print(f"Teensy: {teensy_path}")
    print(f"Band:   {band_path}")
    print("Opening both boards...")

    with serial.Serial(teensy_path, baud, timeout=0.05) as teensy, \
         serial.Serial(band_path, baud, timeout=0.05) as band:
        # Opening USB serial can reset either board. Let setup() finish, then
        # discard boot banners so only new hits are forwarded.
        time.sleep(startup_delay)
        teensy.reset_input_buffer()
        band.reset_input_buffer()
        print("Bridge ready. Press Ctrl-C to stop.")

        while True:
            raw = teensy.readline().strip()
            match = BAND_EVENT.match(raw)
            if match:
                hand, mask = match.groups()
                payload = b"BAND " + hand + b" " + mask + b"\n"
                band.write(payload)
                band.flush()
                print(f"hit hand={hand.decode()} mask={mask.decode()}")
            drain_lines(band, "band>")


def test_motors(band_path: str, baud: int, startup_delay: float):
    print(f"Band: {band_path}")
    with serial.Serial(band_path, baud, timeout=0.2) as band:
        time.sleep(startup_delay)
        band.reset_input_buffer()
        print("Each Enter key vibrates the next raw PCA9685 motor channel.")
        for motor in range(8):
            input(f"Press Enter to test motor {motor}...")
            band.write(f"TEST {motor}\n".encode())
            band.flush()
            time.sleep(0.15)
            drain_lines(band, "band>")
        print("Motor test complete.")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--teensy", help="Teensy serial port, e.g. /dev/cu.usbmodem123")
    parser.add_argument("--band", help="MKR1000 serial port, e.g. /dev/cu.usbmodem456")
    parser.add_argument("--baud", type=int, default=115200)
    parser.add_argument("--startup-delay", type=float, default=3.5)
    parser.add_argument("--list", action="store_true", help="list serial ports and exit")
    parser.add_argument("--test-motors", action="store_true", help="test raw band motors 0..7")
    args = parser.parse_args()

    try:
        teensy_path, band_path = resolve_ports(args, need_teensy=not args.test_motors)
        if args.test_motors:
            test_motors(band_path, args.baud, args.startup_delay)
        else:
            bridge(teensy_path, band_path, args.baud, args.startup_delay)
    except KeyboardInterrupt:
        print("\nBridge stopped.")
    except serial.SerialException as exc:
        print(f"Serial error: {exc}", file=sys.stderr)
        raise SystemExit(1)


if __name__ == "__main__":
    main()
