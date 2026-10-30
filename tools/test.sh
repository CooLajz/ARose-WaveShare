#!/usr/bin/env bash
set -euo pipefail
ROOT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
mkdir -p "$ROOT_DIR/.arduino/tests"
"${CXX:-clang++}" -std=c++17 -O1 -g -fsanitize=address,undefined -DAROSE_HOST_PARALLEL \
  -I"$ROOT_DIR/ARoseWaveShare" \
  "$ROOT_DIR/tests/native.cpp" "$ROOT_DIR/tests/features.cpp" "$ROOT_DIR/ARoseWaveShare/SettingsScreen.cpp" "$ROOT_DIR/ARoseWaveShare/RoseEngine.cpp" \
  "$ROOT_DIR/ARoseWaveShare/RoseRenderer.cpp" -o "$ROOT_DIR/.arduino/tests/native"
"$ROOT_DIR/.arduino/tests/native" "$ROOT_DIR/tests/desktop-reference.txt" "$ROOT_DIR/.arduino/tests/preview.ppm"
