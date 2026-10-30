#!/usr/bin/env python3
"""Verify the final two-minute checkpoint interval and maximum lighting on hardware."""
import argparse
from pathlib import Path
import time
import serial
from device import png565


def fields(line):
    return dict(x.split('=',1) for x in line.split()[1:] if '=' in x)


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('port')
    parser.add_argument('--log',type=Path,required=True)
    parser.add_argument('--screenshot',type=Path,required=True)
    args=parser.parse_args()
    port=serial.Serial(baudrate=921600,timeout=.1)
    port.dtr=False; port.rts=False; port.port=args.port
    lines=[]
    with port, args.log.open('w') as log:
        def emit(line):
            lines.append((time.monotonic(),line))
            log.write(line+'\n'); log.flush(); print(line,flush=True)
        def read_for(seconds):
            until=time.monotonic()+seconds
            while time.monotonic()<until:
                raw=port.readline()
                if raw and all(c in (9,10,13) or 32<=c<127 for c in raw): emit(raw.decode().strip())
        def command(text,seconds=.2):
            port.write(text.encode()); read_for(seconds)
        def last(prefix):
            return fields(next(line for _,line in reversed(lines) if line.startswith(prefix)))
        def status():
            command('?'); return last('UI ')
        def save():
            first=len(lines); command('m',.7)
            assert any(line.startswith('SETTINGS saved=1') for _,line in lines[first:])
            assert status()['menu']=='0'
        port.rts=True; time.sleep(.15); port.rts=False; read_for(7)
        ui=status(); assert ui['menu']=='0' and ui['save_position']=='1'
        assert ui['luminous']=='0' and ui['glow']=='80'
        command('m')
        port.write(b'c'); deadline=time.monotonic()+15
        while time.monotonic()<deadline:
            raw=port.readline()
            if not raw.startswith(b'AROSE_FRAME '): continue
            _,w,h,n=raw.split(); payload=bytearray(); n=int(n)
            while len(payload)<n and time.monotonic()<deadline:
                payload.extend(port.read(min(8192,n-len(payload))))
            assert len(payload)==n and port.read(11)==b'\nAROSE_END\n'
            args.screenshot.write_bytes(png565(payload,int(w),int(h))); break
        else: raise RuntimeError('Snimek nebyl prijat.')
        read_for(.2)
        command(']'*10+'}'*2)
        ui=status(); assert ui['luminous']=='100' and ui['glow']=='100'
        save()
        started=time.monotonic(); first=len(lines)
        emit('TEST maximum luminosity=100 glow=100; expect one checkpoint after 120 seconds')
        read_for(123)
        sample=lines[first:]
        writes=[(when,fields(line)) for when,line in sample if line.startswith('CHECKPOINT ')]
        assert len(writes)==1,writes
        elapsed=writes[0][0]-started
        assert 118.5<=elapsed<=122,elapsed
        assert writes[0][1]['result']=='saved' and writes[0][1]['has_position']=='1'
        fps=[fields(line) for when,line in sample if when-started>6 and line.startswith('AROSE ')]
        assert len(fps)>=20
        assert all(float(x['fps'])>=46 and x['errors']==x['dropped']=='0' for x in fps),fps
        emit('TEST interval_seconds=%.3f min_fps=%.2f max_fps=%.2f samples=%d' % (elapsed,min(float(x['fps']) for x in fps),max(float(x['fps']) for x in fps),len(fps)))
        command('m'); command('['*10+'{'*2); save()
        ui=status(); assert ui['luminous']=='0' and ui['glow']=='80' and ui['save_position']=='1'
        emit('TEST PASS: two-minute interval, maximum light performance, defaults restored')


if __name__=='__main__':
    main()
