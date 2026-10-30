#!/usr/bin/env bash
set -euo pipefail
XIP_HOME="${AROSE_XIP_HOME:-/tmp/arose-waveshare-xip}"
if [[ -x /Library/Developer/CommandLineTools/usr/bin/python3 ]]; then
  DEFAULT_PYTHON=/Library/Developer/CommandLineTools/usr/bin/python3
else
  DEFAULT_PYTHON="$(command -v python3)"
fi
BASE_PYTHON="${AROSE_XIP_BASE_PYTHON:-$DEFAULT_PYTHON}"
[[ "$XIP_HOME" != *" "* ]] || { echo 'Pro SDK zvolte cestu bez mezer.' >&2; exit 1; }
mkdir -p "$XIP_HOME"
XIP_HOME="$(cd "$XIP_HOME" && pwd -P)"
if [[ ! -d "$XIP_HOME/esp-idf" ]]; then
  git clone --depth 1 --branch v5.1.4 https://github.com/espressif/esp-idf.git "$XIP_HOME/esp-idf"
fi
[[ "$(git -C "$XIP_HOME/esp-idf" rev-parse HEAD)" == d7b0a45ddbddbac53afb4fc28168f9f9259dbb79 ]]
git -C "$XIP_HOME/esp-idf" submodule update --init --recursive --depth 1 --jobs 1
"$BASE_PYTHON" -m venv "$XIP_HOME/python"
"$XIP_HOME/python/bin/python" -m pip install 'cmake==3.31.10' 'ninja==1.13.2' 'esptool==4.12.0'
export IDF_TOOLS_PATH="$XIP_HOME/tools"
"$BASE_PYTHON" "$XIP_HOME/esp-idf/tools/idf_tools.py" install --targets esp32s3
"$BASE_PYTHON" "$XIP_HOME/esp-idf/tools/idf_tools.py" install-python-env
PY_VERSION="$("$BASE_PYTHON" -c 'import sys; print("%s.%s" % sys.version_info[:2])')"
# Newer ruamel distributions use names the IDF 5.1 dependency checker cannot resolve.
"$IDF_TOOLS_PATH/python_env/idf5.1_py${PY_VERSION}_env/bin/python" -m pip install \
  'ruamel.yaml==0.18.6' 'ruamel.yaml.clib==0.2.8'
