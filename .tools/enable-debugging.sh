#!/usr/bin/env bash
set -euo pipefail

# Let a session profile and debug the compiler as a normal user: "perf record" on it, gdb attached to a running
# build, and kernel frames named in a profile. Lasts until WSL (or the machine) restarts; "--off" puts back what this
# machine had.
#
#   .tools/enable-debugging.sh [--off]

# setting  on  off  what it allows when on
readonly SETTINGS=(
  "kernel.perf_event_paranoid 1 2 perf on a process of one's own, kernel samples included"
  "kernel.yama.ptrace_scope   0 1 gdb -p on any process of one's own, not just a child"
  "kernel.kptr_restrict       0 1 kernel symbols named in a perf report, not bare addresses"
)

column=1
[[ "${1:-}" == "--off" ]] && column=2

for entry in "${SETTINGS[@]}"; do
  read -r key on off why <<< "${entry}"
  value="${on}"
  ((column == 2)) && value="${off}"
  sudo sysctl -q "${key}=${value}"
  printf '%-28s %s  (%s)\n' "${key}" "$(sysctl -n "${key}")" "${why}"
done

# perf can record without libelf, but then names no symbols (a flame graph of bare "[spp]" frames).
if command -v perf > /dev/null; then
  if ! perf version --build-options 2> /dev/null | grep -q 'libelf: \[ on'; then
    echo "warning: $(command -v perf) has no libelf, so it will not name symbols; put one that does first on PATH"
  fi
else
  echo "warning: no perf on PATH"
fi
command -v gdb > /dev/null || echo "warning: no gdb on PATH"
