#!/usr/bin/env python3
"""Read-only diagnostics for ARose (pyserial); PNG encoder uses stdlib only."""
import argparse
import binascii
from pathlib import Path
import struct
import time
import zlib
import serial


def png565(data, width, height):
    rows = bytearray()
    for y in range(height):
        rows.append(0)
        for x in range(width):
            value = struct.unpack_from('<H', data, (y * width + x) * 2)[0]
            rows.extend(((value >> 11) * 255 // 31, ((value >> 5) & 63) * 255 // 63, (value & 31) * 255 // 31))
    def chunk(kind, body):
        return struct.pack('>I', len(body)) + kind + body + struct.pack('>I', binascii.crc32(kind + body) & 0xffffffff)
    return (b'\x89PNG\r\n\x1a\n' + chunk(b'IHDR', struct.pack('>IIBBBBB', width, height, 8, 2, 0, 0, 0))
            + chunk(b'IDAT', zlib.compress(rows, 6)) + chunk(b'IEND', b''))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('port')
    parser.add_argument('--seconds', type=float, default=15)
    parser.add_argument('--capture', type=Path)
    parser.add_argument('--capture-delay', type=float, default=0,
                        help='Pocet sekund pred zadosti o snimek; animace behem cekani pokracuje')
    args = parser.parse_args()
    if args.capture_delay < 0:
        parser.error('--capture-delay musi byt nezaporne')
    # Request inactive control lines. Some USB bridges still reset on opening.
    port = serial.Serial(baudrate=921600, timeout=1)
    port.dtr = False
    port.rts = False
    port.port = args.port
    with port:
        time.sleep(2)
        port.reset_input_buffer()
        if args.capture:
            ready = time.monotonic() + args.capture_delay
            while time.monotonic() < ready:
                port.read(port.in_waiting or 1)
            port.reset_input_buffer()
        port.write(b'c' if args.capture else b'?')
        deadline = time.monotonic() + (20 if args.capture else args.seconds)
        while time.monotonic() < deadline:
            line = port.readline()
            if line.startswith(b'AROSE_FRAME '):
                _, width, height, size = line.split()
                width, height, size = map(int, (width, height, size))
                if (width, height, size) != (480, 480, 460800):
                    raise RuntimeError('Neocekavany format obrazu')
                payload = bytearray()
                while len(payload) < size and time.monotonic() < deadline:
                    payload.extend(port.read(min(8192, size - len(payload))))
                if len(payload) != size or port.read(len(b'\nAROSE_END\n')) != b'\nAROSE_END\n':
                    raise RuntimeError('Neuplny snimek')
                args.capture.parent.mkdir(parents=True, exist_ok=True)
                args.capture.write_bytes(png565(payload, width, height))
                print(f'Snimek ulozen: {args.capture}')
                return
            if line and all(c in (9, 10, 13) or 32 <= c < 127 for c in line):
                print(line.decode('utf-8', errors='replace').rstrip(), flush=True)
        if args.capture:
            raise RuntimeError('Zarizeni neodeslalo snimek')


if __name__ == '__main__':
    main()
