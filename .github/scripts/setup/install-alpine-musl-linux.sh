#!/usr/bin/env bash
# Link and run musl programs in the pinned Alpine. Two local images
# from it: one with Alpine's gcc to link, one with only libgcc (the
# one library an spp program needs besides musl and sppc) to run.
# The link goes through "x86_64-linux-musl-gcc" on PATH, the name
# spp looks for, which runs Alpine's cc with RUNNER_TEMP mounted at
# the same path.
set -euo pipefail

base="alpine:${ALPINE_VERSION}@sha256:${ALPINE_SHA256}"
bin="${RUNNER_TEMP}/musl-cc"

docker build --quiet -t spp-musl-link - <<< "FROM ${base}
RUN apk add --no-cache build-base"
docker build --quiet -t spp-musl-run - <<< "FROM ${base}
RUN apk add --no-cache libgcc"

mkdir -p "$bin"
cat > "${bin}/x86_64-linux-musl-gcc" << EOF
#!/bin/sh
exec docker run --rm --user "\$(id -u):\$(id -g)" -v '${RUNNER_TEMP}:${RUNNER_TEMP}' -w "\$PWD" spp-musl-link cc "\$@"
EOF
chmod +x "${bin}/x86_64-linux-musl-gcc"

echo "$bin" >> "$GITHUB_PATH"
echo "SPP_MUSL_RUN_IMAGE=spp-musl-run" >> "$GITHUB_ENV"
