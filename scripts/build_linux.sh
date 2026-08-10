#!/usr/bin/env bash
set -euo pipefail

target="${TARGET:-template_debug}"
arch="${ARCH:-x86_64}"
sdk_path="${STEAMWORKS_SDK_PATH:-godotsteam/sdk}"
target_path="${TARGET_PATH:-bin}"
jobs="${JOBS:-4}"
include_runtime="${INCLUDE_STEAM_RUNTIME:-no}"

python3 -m SCons \
  platform=linux arch="$arch" target="$target" \
  redot_api_dir=redot-api/26.2 \
  steam_sdk_path="$sdk_path" \
  target_path="$target_path" target_name=godotsteam \
  include_steam_runtime="$include_runtime" -j"$jobs"
