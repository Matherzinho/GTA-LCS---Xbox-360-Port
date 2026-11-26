#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
SDK="${XSDK_ROOT:-$HOME/xsdk}"
BUILD="${LCS_BUILD_DIR:-$ROOT/../../work/lcs/build-xsdk}"
mkdir -p "$ROOT/logs"
cmake -S "$ROOT" -B "$BUILD" -G Ninja -DCMAKE_TOOLCHAIN_FILE="$SDK/cmake/xsdk.cmake" -DCMAKE_BUILD_TYPE=Release > "$ROOT/logs/configure-boot.log" 2>&1
cmake --build "$BUILD" --target lcs_boot --parallel "${LCS_JOBS:-2}" > "$ROOT/logs/build-boot.log" 2>&1
python3 "$ROOT/tools/check_animation_codegen.py" "$BUILD"
printf '%s\n' 'Built real game startup XEX2 (internal, native XSDK audio, basic renderer).'
