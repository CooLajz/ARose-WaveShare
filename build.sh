#!/usr/bin/env bash
set -euo pipefail
source "$(dirname "$0")/tools/toolchain.sh"
"$ARDUINO_CLI_BIN" --config-file "$ARDUINO_CONFIG_FILE" compile \
  --fqbn "$FQBN" \
  --build-property "tools.esptool_py.path=$ESPTOOL_DIR/bin" \
  --build-property tools.esptool_py.cmd=esptool.py \
  --build-property "runtime.tools.ctags.path=$CTAGS_DIR" \
  --build-property "tools.ctags.path=$CTAGS_DIR" \
  --build-property compiler.optimization_flags=-O3 \
  --build-property upload.maximum_size=6291456 \
  --build-path "$BUILD_DIR" --output-dir "$OUTPUT_DIR" \
  "$@" "$ROOT_DIR/ARoseWaveShare"
