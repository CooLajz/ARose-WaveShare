#!/usr/bin/env python3
"""Explicit device test: accelerated checkpoints, NVS recycling, pause and reboot."""
import argparse
from pathlib import Path
import time
import serial


def fields(line):
    return dict(item.split('=', 1) for item in line.split()[1:] if '=' in item)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('port')
    parser.add_argument('--log', type=Path, required=True)
    parser.add_argument('--stress-limit', type=int, default=400)
    args = parser.parse_args()
    if not 1 <= args.stress_limit <= 400:
        parser.error('Zvolte 1 az 400 testovacich zapisu.')
    args.log.parent.mkdir(parents=True, exist_ok=True)
    port = serial.Serial(baudrate=921600, timeout=.1)
    port.dtr = False
    port.rts = False
    port.port = args.port
    lines = []
    with args.log.open('w') as log, port:
        def emit(line):
            lines.append(line)
            log.write(line + '\n')
            log.flush()
            print(line, flush=True)

        def read_for(seconds):
            end = time.monotonic() + seconds
            while time.monotonic() < end:
                raw = port.readline()
                if raw and all(c in (9, 10, 13) or 32 <= c < 127 for c in raw):
                    emit(raw.decode().strip())

        def last(prefix):
            return next(line for line in reversed(lines) if line.startswith(prefix))

        def command(command, delay=.3):
            port.write(command.encode())
            read_for(delay)

        def save():
            before = len(lines)
            command('s', .1)
            deadline = time.monotonic() + 5
            while time.monotonic() < deadline:
                found = [line for line in lines[before:] if line.startswith('CHECKPOINT ')]
                if found:
                    result = fields(found[-1])
                    assert result['result'] != 'failed', result
                    return result
                read_for(.1)
            raise RuntimeError('Ulozeni nebylo potvrzeno do 5 sekund.')

        # Opening CH343P may reset the board. Explicit reset makes the boot proof reproducible.
        port.rts = True
        time.sleep(.15)
        port.rts = False
        read_for(8)
        assert 'xip_instructions=1 xip_rodata=1' in last('SDK ')
        command('?')
        assert fields(last('UI '))['storage'] == '1'
        assert fields(last('UI '))['paused'] == fields(last('UI '))['menu'] == '0'
        emit('TEST regular cadence baseline')
        read_for(30)
        emit('TEST accelerated checkpoints begin')
        erases = 0
        for index in range(args.stress_limit):
            started = time.monotonic()
            saved = save()
            assert saved['result'] == 'saved', saved
            erases += int(saved['erases'])
            read_for(max(0, .5 - (time.monotonic() - started)))
            if erases >= 3:
                break
        assert erases >= 3, 'Nebyla potvrzena tri skutecna mazani NVS.'
        emit(f'TEST accelerated checkpoints complete writes={index+1} erases={erases}')
        read_for(31)
        command('?')
        status = fields(last('AROSE '))
        assert status['errors'] == status['dropped'] == '0', status
        command('p', 2.3)
        command('?')
        assert fields(last('UI '))['paused'] == '1'
        paused_frame = fields(last('AROSE '))['sim']
        saved = save()
        assert saved['frame'] == paused_frame
        unchanged = save()
        assert unchanged['result'] == 'unchanged'
        command('m?')
        assert fields(last('UI '))['menu'] == '1'
        command('m', .6)  # Save closes the menu only after the NVS acknowledgement.
        command('?')
        assert fields(last('UI '))['menu'] == '0'
        assert fields(last('UI '))['paused'] == '1'
        assert fields(last('AROSE '))['sim'] == paused_frame
        emit('TEST hardware reset and resume')
        port.rts = True
        time.sleep(.15)
        port.rts = False
        read_for(8)
        restored = fields(last('RESTORE '))
        assert restored['loaded'] == '1' and restored['frame'] == paused_frame, restored
        assert restored['brightness'] == saved['brightness'], restored
        command('?')
        assert fields(last('UI '))['paused'] == fields(last('UI '))['menu'] == '0'
        read_for(16)
        emit('TEST PASS: XIP boot, NVS recycling, pause deduplication, menu and restored generator')


if __name__ == '__main__':
    main()
