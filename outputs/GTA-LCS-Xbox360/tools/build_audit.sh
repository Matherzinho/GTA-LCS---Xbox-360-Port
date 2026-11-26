#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
SDK="${XSDK_ROOT:-$HOME/xsdk}"
BUILD="${LCS_BUILD_DIR:-$ROOT/../../work/lcs/build-xsdk}"
mkdir -p "$ROOT/logs"
python3 "$ROOT/tools/validate_data.py" > "$ROOT/logs/data-validation-console.log"
cmake -S "$ROOT" -B "$BUILD" -G Ninja -DCMAKE_TOOLCHAIN_FILE="$SDK/cmake/xsdk.cmake" -DCMAKE_BUILD_TYPE=Release > "$ROOT/logs/configure.log" 2>&1
cmake --build "$BUILD" --target lcs_gameplay lcs_gpu lcs_data_probe lcs_render_probe lcs_model_probe lcs_io_probe lcs_text_probe lcs_scene_probe lcs_audio_probe --parallel "${LCS_JOBS:-2}" > "$ROOT/logs/build-audit.log" 2>&1
printf '%s\n' 'PASS: PowerPC gameplay, native GPU, file/streaming/text libraries and seven internal diagnostics compiled.' 'No playable game XEX is produced by this stage.'
