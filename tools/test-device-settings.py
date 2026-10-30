#!/usr/bin/env python3
"""Hardware test of pause ramps, saved options and animation reset (changes settings)."""
import argparse
from pathlib import Path
import time
import serial
from device import png565


def fields(line):
    return dict(x.split('=', 1) for x in line.split()[1:] if '=' in x)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('port')
    parser.add_argument('--checkpoint-seconds', type=float, default=120)
    parser.add_argument('--log', type=Path, required=True)
    parser.add_argument('--screenshot', type=Path, required=True)
    args = parser.parse_args()
    args.log.parent.mkdir(parents=True, exist_ok=True)
    port = serial.Serial(baudrate=921600, timeout=.1)
    port.dtr = False
    port.rts = False
    port.port = args.port
    lines = []
    with port, args.log.open('w') as log:
        def emit(line):
            lines.append(line)
            log.write(line+'\n'); log.flush()
            print(line, flush=True)

        def read_for(seconds):
            end = time.monotonic()+seconds
            while time.monotonic()<end:
                raw = port.readline()
                if raw and all(c in (9,10,13) or 32<=c<127 for c in raw):
                    emit(raw.decode().strip())

        def last(prefix):
            return fields(next(x for x in reversed(lines) if x.startswith(prefix)))

        def command(text, seconds=.15):
            port.write(text.encode()); read_for(seconds)

        def status():
            command('?')
            return last('UI '), last('AROSE ')

        def reset_board():
            port.rts=True; time.sleep(.15); port.rts=False; read_for(7)

        def save_settings():
            before=len(lines)
            command('m', .5)
            assert any(x.startswith('SETTINGS saved=1') for x in lines[before:]), lines[before:]
            assert status()[0]['menu']=='0'

        reset_board()
        ui, state=status()
        assert '0.3.0' in state['version'] and ui['menu']=='0'
        command('m')
        command('++++++++++++++++++++[[[[[[[[[[{{{{{{{{{{}}}}}}}}')
        if status()[0]['draft_save']=='0': command('e')
        save_settings()
        assert status()[0]['save_position']=='1'
        emit('TEST smooth brake begins')
        command('p', .05)
        _, start=status(); initial=int(start['sim'])
        speeds=[]
        for wait in [.35,.5,.5,.8]:
            read_for(wait); ui,state=status(); speeds.append(float(ui['speed']))
        assert all(a>=b for a,b in zip(speeds,speeds[1:])) and speeds[-1]==0, speeds
        assert 40<=int(state['sim'])-initial<=53
        stopped=state['sim']
        read_for(.5); assert status()[1]['sim']==stopped
        emit('TEST smooth brake speeds='+','.join(map(str,speeds)))
        command('p', .05)
        speeds=[]
        for wait in [.35,.5,.5,.8]:
            read_for(wait); ui,state=status(); speeds.append(float(ui['speed']))
        assert all(a<=b for a,b in zip(speeds,speeds[1:])) and speeds[-1]==1, speeds
        emit('TEST smooth start speeds='+','.join(map(str,speeds)))
        command('m'); assert status()[0]['menu']=='1'
        # Capture the real framebuffer before testing options.
        port.write(b'c'); deadline=time.monotonic()+15
        while time.monotonic()<deadline:
            raw=port.readline()
            if not raw.startswith(b'AROSE_FRAME '): continue
            _,w,h,n=raw.split(); payload=bytearray(); n=int(n)
            while len(payload)<n and time.monotonic()<deadline:
                payload.extend(port.read(min(8192,n-len(payload))))
            tail=port.read(11)
            assert len(payload)==n and tail==b'\nAROSE_END\n'
            args.screenshot.parent.mkdir(parents=True,exist_ok=True)
            args.screenshot.write_bytes(png565(payload,int(w),int(h))); break
        else: raise RuntimeError('Snimek nebyl prijat.')
        read_for(.2)
        command('e------------]]]]]{{{')  # Disable position, brightness 40%.
        ui,_=status(); assert ui['draft_save']=='0' and ui['save_position']=='1' and ui['brightness']=='40' and ui['luminous']=='50' and ui['glow']=='50'
        save_settings()
        record=last('CHECKPOINT '); ui,_=status(); assert ui['luminous']=='50' and ui['glow']=='50'
        assert record['save_position']=='0' and record['has_position']=='0'
        before=sum(x.startswith('CHECKPOINT ') for x in lines)
        read_for(args.checkpoint_seconds+2)
        assert sum(x.startswith('CHECKPOINT ') for x in lines)==before
        reset_board()
        restored=last('RESTORE ')
        assert restored['loaded']=='1' and restored['frame']=='0' and restored['brightness']=='40' and restored['save_position']=='0'
        ui,_=status(); assert ui['luminous']=='50' and ui['glow']=='50'
        emit('TEST disabled position survives reboot; brightness and luminosity remain saved')
        command('p',2.3); command('m')
        ui,state=status(); frozen=state['sim']; assert ui['paused']=='1'
        command('e++++++++++++r[[[[[}}}')  # Re-enable, brightness 100%, stage reset.
        ui,state=status(); assert ui['reset_pending']=='1' and ui['draft_save']=='1' and state['sim']==frozen
        save_settings()
        committed=last('SETTINGS '); assert committed['reset']=='1' and committed['frame']=='1'
        record=last('CHECKPOINT '); assert record['has_position']=='0' and record['save_position']=='1'
        ui,state=status(); assert ui['paused']=='0' and ui['menu']=='0' and int(state['sim'])<50
        read_for(2.2); assert status()[0]['speed']=='1.000'
        reset_board()  # Before the first automatic checkpoint after reset.
        restored=last('RESTORE ')
        assert restored['frame']=='0' and restored['brightness']=='100' and restored['save_position']=='1'
        read_for(args.checkpoint_seconds+2)
        record=last('CHECKPOINT '); assert record['has_position']=='1' and int(record['frame'])>0
        ui,state=status(); assert state['errors']==state['dropped']=='0'
        assert ui['menu']==ui['paused']=='0' and ui['save_position']=='1' and ui['luminous']=='0' and ui['glow']=='80'
        emit('TEST PASS: ramps, checkbox persistence, disabled writes, reset commit and restart from zero')
        for name, keys in [('maximum',']'*10+'}'*2),('default','['*10+'{'*2)]:
            emit('TEST performance '+name)
            command(keys); read_for(6)  # Exclude the partly mixed statistics window.
            first=len(lines); read_for(31)
            samples=[fields(x) for x in lines[first:] if x.startswith('AROSE ')]
            assert len(samples)>=5 and all(float(x['fps'])>=46 and x['errors']==x['dropped']=='0' for x in samples), samples
        command('m'); save_settings()
        emit('TEST PASS: maximum luminosity/glow and default performance; defaults saved')


if __name__=='__main__':
    main()
