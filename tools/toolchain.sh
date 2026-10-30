#!/usr/bin/env bash
# Sourced by build/upload. Paths to installed tools can be overridden locally.
ROOT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
ARDUINO_CONFIG_FILE="${ARDUINO_CONFIG_FILE:-$ROOT_DIR/.arduino/arduino-cli.yaml}"
ARDUINO_CLI_BIN="${ARDUINO_CLI_BIN:-$ROOT_DIR/.arduino/tools/arduino-cli/arduino-cli}"
PYTHON_BIN="${PYTHON_BIN:-/Library/Developer/CommandLineTools/usr/bin/python3}"
ESPTOOL_DIR="${ESPTOOL_DIR:-$ROOT_DIR/.arduino/tools/esptool-py39}"
CTAGS_DIR="${CTAGS_DIR:-$ROOT_DIR/.arduino/tools/ctags}"
export PYTHONPATH="$ESPTOOL_DIR${PYTHONPATH:+:$PYTHONPATH}"
FQBN='esp32:esp32:esp32s3:CPUFreq=240,FlashSize=16M,FlashMode=qio,PartitionScheme=custom,PSRAM=opi,USBMode=hwcdc,CDCOnBoot=cdc'
BUILD_DIR="$ROOT_DIR/.arduino/build"
OUTPUT_DIR="$ROOT_DIR/build"
