#!/usr/bin/env bash
# Install glibc's detached debug symbols. Ubuntu's ld.so is
# stripped, and valgrind has to find memcmp and friends in it
# before it can start; it reads build-id debug files only from
# /usr/lib/debug, hence apt rather than a home-directory copy.
set -euo pipefail

sudo apt-get update -qq
sudo apt-get install -y --no-install-recommends libc6-dbg

# Checked here so a version skew reads as this, not as
# valgrind's "cannot be set up" at the first test group.
loader="$(ldd /bin/true | awk '/ld-linux/ { print $1 }')"
build_id="$(readelf -n "$loader" | sed -n 's/^ *Build ID: //p')"
debug_file="/usr/lib/debug/.build-id/${build_id:0:2}/${build_id:2}.debug"
if ! [ -f "$debug_file" ]; then
  echo "::error::libc6-dbg has no debug file for ${loader} (build-id ${build_id}); is libc6 older than the archive's?"
  exit 1
fi
echo "debuginfo: ${loader} -> ${debug_file}"
