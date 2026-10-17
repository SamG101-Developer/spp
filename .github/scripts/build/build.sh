#!/usr/bin/env bash
# Build everything the configure step set up, teeing the output so a failed build can be uploaded whole.
set -euo pipefail

mkdir -p build-logs
cmake --build build --parallel 2>&1 | tee build-logs/build.log
