#!/usr/bin/env bash
set -euo pipefail
ROOT_DIR="$(cd -- "$(dirname "$0")" && pwd)"
XIP_HOME="${AROSE_XIP_HOME:-/tmp/arose-waveshare-xip}"
PYTHON_BIN="$XIP_HOME/python/bin/python"
OUTPUT_DIR="$ROOT_DIR/build"
if [[ $# != 1 || ! -c "$1" ]]; then
  echo 'Pouziti: ./upload-xip.sh /dev/cu.usbmodem... (zvolte overeny port)' >&2
  exit 1
fi
for option in SPIRAM_FETCH_INSTRUCTIONS SPIRAM_RODATA; do
  grep -qx "CONFIG_${option}=y" "$OUTPUT_DIR/xip/sdkconfig"
done
"$PYTHON_BIN" -m esptool --chip esp32s3 --port "$1" --baud 921600 \
  --before default_reset --after hard_reset write_flash -z --flash_mode dio \
  --flash_freq 80m --flash_size 16MB \
  0x0 "$OUTPUT_DIR/xip/bootloader.bin" \
  0x8000 "$OUTPUT_DIR/xip/partition-table.bin" \
  0xe000 "$OUTPUT_DIR/xip/ota_data_initial.bin" \
  0x10000 "$OUTPUT_DIR/xip/arose_xip.bin"
