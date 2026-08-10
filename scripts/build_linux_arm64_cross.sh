#!/usr/bin/env bash
set -euo pipefail

target="${TARGET:-template_debug}"
sdk_path="${STEAMWORKS_SDK_PATH:-godotsteam/sdk}"
target_path="${TARGET_PATH:-bin}"
jobs="${JOBS:-4}"

python3 -m SCons \
  platform=linux arch=arm64 target="$target" \
  redot_api_dir=redot-api/26.2 \
  steam_sdk_path="$sdk_path" \
  target_path="$target_path" target_name=godotsteam \
  linux_cross_prefix=aarch64-linux-gnu- \
  include_steam_runtime=no -j"$jobs"
