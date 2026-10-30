#!/usr/bin/env bash
set -eo pipefail
AROSE_PROJECT="$(cd -- "$(dirname "$0")" && pwd)"
export AROSE_PROJECT
# IDF's generated Ninja/Make files require a staging path without spaces.
export AROSE_XIP_HOME="${AROSE_XIP_HOME:-/tmp/arose-waveshare-xip}"
if [[ ! -d "$AROSE_XIP_HOME/esp-idf" ]]; then
  echo 'Run ./tools/setup-xip.sh first.' >&2; exit 1
fi
# IDF records installed targets by SDK path: keep /tmp symlinks canonical.
export AROSE_XIP_HOME="$(cd "$AROSE_XIP_HOME" && pwd -P)"
export IDF_PATH="$AROSE_XIP_HOME/esp-idf"
export IDF_TOOLS_PATH="$AROSE_XIP_HOME/tools"
if [[ -z "${AROSE_ARDUINO_CORE:-}" ]]; then
  cli="${ARDUINO_CLI_BIN:-$AROSE_PROJECT/.arduino/tools/arduino-cli/arduino-cli}"
  if [[ ! -x "$cli" ]]; then cli="$(command -v arduino-cli || true)"; fi
  if [[ -z "$cli" ]]; then
    echo 'Install Arduino CLI and esp32:esp32@3.0.7, or set AROSE_ARDUINO_CORE.' >&2
    exit 1
  fi
  config="${ARDUINO_CONFIG_FILE:-$AROSE_PROJECT/.arduino/arduino-cli.yaml}"
  cli_args=()
  if [[ -f "$config" ]]; then cli_args+=(--config-file "$config"); fi
  data_dir="$("$cli" "${cli_args[@]}" config get directories.data)"
  AROSE_ARDUINO_CORE="$data_dir/packages/esp32/hardware/esp32/3.0.7"
fi
if [[ ! -f "$AROSE_ARDUINO_CORE/cores/esp32/Arduino.h" || ! -f "$AROSE_ARDUINO_CORE/Kconfig.projbuild" ]]; then
  echo 'Arduino-ESP32 3.0.7 core sources were not found. See README.md.' >&2
  exit 1
fi
export AROSE_ARDUINO_CORE
export IDF_COMPONENT_MANAGER=0
export PATH="$AROSE_XIP_HOME/python/bin:$PATH"
source "$IDF_PATH/export.sh"
mkdir -p "$AROSE_XIP_HOME/project"
cp -R "$AROSE_PROJECT/idf/" "$AROSE_XIP_HOME/project/"
cp "$AROSE_ARDUINO_CORE/Kconfig.projbuild" "$AROSE_XIP_HOME/project/components/arduino/"
cp "$AROSE_PROJECT/ARoseWaveShare/partitions.csv" "$AROSE_XIP_HOME/project/"
if ! cmp -s "$AROSE_PROJECT/idf/sdkconfig.defaults" "$AROSE_XIP_HOME/last-defaults"; then
  cp "$AROSE_PROJECT/idf/sdkconfig.defaults" "$AROSE_XIP_HOME/sdkconfig"
fi
idf.py -C "$AROSE_XIP_HOME/project" -B "$AROSE_XIP_HOME/build" \
  -D "SDKCONFIG=$AROSE_XIP_HOME/sdkconfig" \
  -D "AROSE_START_FRAME=${AROSE_START_FRAME:-0}" "$@" build
cp "$AROSE_PROJECT/idf/sdkconfig.defaults" "$AROSE_XIP_HOME/last-defaults"
mkdir -p "$AROSE_PROJECT/build/xip"
cp "$AROSE_XIP_HOME/build/arose_xip.bin" "$AROSE_XIP_HOME/build/arose_xip.elf" \
   "$AROSE_XIP_HOME/build/arose_xip.map" "$AROSE_XIP_HOME/sdkconfig" "$AROSE_PROJECT/build/xip/"
cp "$AROSE_XIP_HOME/build/bootloader/bootloader.bin" \
   "$AROSE_XIP_HOME/build/partition_table/partition-table.bin" \
   "$AROSE_XIP_HOME/build/ota_data_initial.bin" "$AROSE_PROJECT/build/xip/"
