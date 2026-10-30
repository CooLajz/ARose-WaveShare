#!/usr/bin/env bash
set -euo pipefail
ROOT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
XIP_HOME="${AROSE_XIP_HOME:-/tmp/arose-waveshare-xip}"
PYTHON_BIN="$XIP_HOME/python/bin/python"
if [[ $# != 1 || ! "$1" =~ ^[0-9]+\.[0-9]+\.[0-9]+$ ]]; then
  echo 'Usage: ./tools/package-release.sh 0.3.0' >&2; exit 1
fi
VERSION="$1"
SOURCE="$ROOT_DIR/build/xip"
DEST="$ROOT_DIR/build/release"
mkdir -p "$DEST"
"$PYTHON_BIN" - "$ROOT_DIR" "$VERSION" <<'PY'
import pathlib, re, sys
root=pathlib.Path(sys.argv[1]); version=sys.argv[2]
assert f'"{version}-xip"' in (root/'ARoseWaveShare/FirmwareBuild.h').read_text()
config=(root/'build/xip/sdkconfig').read_text()
for name in ('SPIRAM_FETCH_INSTRUCTIONS','SPIRAM_RODATA'):
    assert re.search(r'^CONFIG_'+name+r'=y$',config,re.M),name
assert not re.search(r'^CONFIG_LCD_RGB_RESTART_IN_VSYNC=y$',config,re.M)
PY
"$PYTHON_BIN" -m esptool --chip esp32s3 merge_bin \
  --output "$DEST/ARose-WaveShare-$VERSION.factory.bin" \
  --flash_mode dio --flash_freq 80m --flash_size 16MB \
  0x0 "$SOURCE/bootloader.bin" 0x8000 "$SOURCE/partition-table.bin" \
  0xe000 "$SOURCE/ota_data_initial.bin" 0x10000 "$SOURCE/arose_xip.bin"
"$PYTHON_BIN" - "$ROOT_DIR" "$VERSION" <<'PY'
from pathlib import Path
import hashlib, json, sys, zipfile
root=Path(sys.argv[1]); version=sys.argv[2]; source=root/'build/xip'; out=root/'build/release'
name=f'ARose-WaveShare-{version}'
factory=out/(name+'.factory.bin'); data=factory.read_bytes()
assert len(data)==0x10000+(source/'arose_xip.bin').stat().st_size
assert len(data)<0xfe0000, 'Package overlaps the settings partition'
assert data[0]==0xe9 and data[2:4]==bytes([2,0x4f]), 'Expected DIO, 16 MB, 80 MHz header'
parts=[(0,'bootloader.bin'),(0x8000,'partition-table.bin'),(0xe000,'ota_data_initial.bin'),(0x10000,'arose_xip.bin')]
for offset, filename in parts:
    original=(source/filename).read_bytes()
    if offset: assert data[offset:offset+len(original)]==original,filename
with zipfile.ZipFile(out/(name+'.parts.zip'),'w',compression=zipfile.ZIP_DEFLATED) as archive:
    for offset,filename in parts:
        # Include the patched bootloader header, matching the factory image.
        archive.writestr(filename,data[offset:offset+(source/filename).stat().st_size])
    archive.writestr('OFFSETS.txt','\n'.join(f'0x{offset:06x} {filename}' for offset,filename in parts)+'\n')
    archive.write(root/'LICENSE','LICENSE')
    archive.write(root/'THIRD_PARTY_NOTICES.md','THIRD_PARTY_NOTICES.md')
manifest={'name':'ARose WaveShare','version':version,'new_install_prompt_erase':True,'new_install_improv_wait_time':0,
          'builds':[{'chipFamily':'ESP32-S3','parts':[{'path':factory.name,'offset':0}]}]}
(out/'manifest.json').write_text(json.dumps(manifest,indent=2)+'\n')
(out/'FLASHING.md').write_text((root/'docs/FLASHING.md').read_text())
assets=[factory,out/(name+'.parts.zip'),out/'manifest.json',out/'FLASHING.md']
(out/'SHA256SUMS.txt').write_text(''.join(hashlib.sha256(p.read_bytes()).hexdigest()+'  '+p.name+'\n' for p in assets))
print(f'Verified {factory.name}: {len(data)} bytes; settings partition is outside the image.')
PY
"$PYTHON_BIN" -m esptool --chip esp32s3 image_info "$SOURCE/arose_xip.bin"
