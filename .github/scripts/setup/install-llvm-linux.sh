#!/usr/bin/env bash
# Install the LLVM development libraries the project
# links against.
set -euo pipefail

# Already installed if this job's compiler *is* this
# LLVM release.
if ! [ -d "/usr/lib/llvm-${LLVM_LIB_VERSION}" ]; then
  # llvm.sh always installs lldb, and every apt.llvm.org
  # lldb ships /etc/lldb/lldbinit with no Replaces:, so
  # this release cannot unpack over the one the Clang step
  # pulled in. Nothing here uses lldb.
  mapfile -t lldb_pkgs < <(
    dpkg-query -W -f '${db:Status-Abbrev}\t${binary:Package}\n' |
      awk '$1 ~ /^i/ && $2 ~ /lldb/ { print $2 }'
  )
  if [ ${#lldb_pkgs[@]} -gt 0 ]; then
    sudo apt-get purge -y "${lldb_pkgs[@]}"
  fi
  sudo "${RUNNER_TEMP}/llvm.sh" "${LLVM_LIB_VERSION}"
fi
sudo apt-get install -y "llvm-${LLVM_LIB_VERSION}-dev"
