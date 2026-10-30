#!/usr/bin/env bash
set -euo pipefail
source "$(dirname "$0")/tools/toolchain.sh"
if [[ $# != 1 || ! -c "$1" ]]; then
  echo 'Pouziti: ./upload.sh /dev/cu.usbmodem... (zvolte overeny port)' >&2
  exit 1
fi
"$ARDUINO_CLI_BIN" --config-file "$ARDUINO_CONFIG_FILE" upload \
  --fqbn "$FQBN" --port "$1" --input-dir "$OUTPUT_DIR" \
  --upload-property "tools.esptool_py.path=$ESPTOOL_DIR/bin" \
  --upload-property tools.esptool_py.cmd=esptool.py --verbose \
  "$ROOT_DIR/ARoseWaveShare"
