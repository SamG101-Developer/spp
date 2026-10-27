#!/usr/bin/env bash
# Fail when a built binary needs a newer system than [floor] in
# .github/dependencies.toml allows. FLOOR_ADVISORY=true warns
# instead, until the release builds reach the floor (P1.4).
set -euo pipefail

level=error
[ "${FLOOR_ADVISORY:-}" = "true" ] && level=warning

exe=""
[ "$RUNNER_OS" = "Windows" ] && exe=".exe"

bins=("build/spp${exe}")
[ -f "build/tests/spp_tests${exe}" ] && bins+=("build/tests/spp_tests${exe}")

status=0
bad() {
  echo "::error::$1"
  status=1
}

# True when $1 is a newer version than $2.
newer() {
  [ "$1" != "$2" ] && [ "$(printf '%s\n%s\n' "$1" "$2" | sort -V | tail -n 1)" = "$1" ]
}

above() {
  echo "::${level}::$1"
  [ "$level" = "warning" ] || status=1
}

need() {
  local name="$1"
  [ -n "${!name:-}" ] || bad "${name} is not set; load the pins before checking the floor"
}

check_linux() {
  local bin="$1" glibc abi
  glibc="$(objdump -p "$bin" | sed -n 's/.*[[:space:]]GLIBC_\([0-9.]*\)$/\1/p' | sort -V | tail -n 1)"
  echo "${bin} needs glibc ${glibc:-<none>} (floor ${SPP_FLOOR_GLIBC})"
  if [ -n "$glibc" ] && newer "$glibc" "$SPP_FLOOR_GLIBC"; then
    above "${bin} needs glibc ${glibc}, above the floor ${SPP_FLOOR_GLIBC}: $(objdump -T "$bin" | grep -o "[^ ]*GLIBC_${glibc}).*" | head -n 3 | tr '\n' ' ')"
  fi

  abi="$(readelf -n "$bin" | sed -n 's/.*OS: Linux, ABI: \([0-9.]*\).*/\1/p' | head -n 1)"
  echo "${bin} declares Linux ${abi:-<none>} (floor ${SPP_FLOOR_LINUX})"
  if [ -n "$abi" ] && newer "$abi" "$SPP_FLOOR_LINUX"; then
    above "${bin} declares Linux ${abi}, above the floor ${SPP_FLOOR_LINUX}"
  fi
}

# LC_BUILD_VERSION carries minos; older binaries carry LC_VERSION_MIN_MACOSX's version instead.
check_macos() {
  local bin="$1" minos
  minos="$(otool -l "$bin" | awk '
    /cmd LC_BUILD_VERSION/ { b = 1 } b && $1 == "minos" { print $2; exit }
    /cmd LC_VERSION_MIN_MACOSX/ { v = 1 } v && $1 == "version" { print $2; exit }')"
  echo "${bin} needs macOS ${minos:-<none>} (floor ${SPP_FLOOR_MACOS})"
  if [ -z "$minos" ]; then
    bad "${bin} records no minimum macOS version"
  elif newer "$minos" "$SPP_FLOOR_MACOS"; then
    above "${bin} needs macOS ${minos}, above the floor ${SPP_FLOOR_MACOS}; set CMAKE_OSX_DEPLOYMENT_TARGET"
  fi
}

case "$RUNNER_OS" in
  Linux) need SPP_FLOOR_GLIBC; need SPP_FLOOR_LINUX ;;
  macOS) need SPP_FLOOR_MACOS ;;
  Windows)
    echo "no Windows floor yet: nothing to check"
    exit 0
    ;;
  *) bad "no floor check for ${RUNNER_OS}" ;;
esac
[ "$status" -eq 0 ] || exit "$status"

for bin in "${bins[@]}"; do
  if ! [ -f "$bin" ]; then
    bad "${bin} was not built"
    continue
  fi
  case "$RUNNER_OS" in
    Linux) check_linux "$bin" ;;
    macOS) check_macos "$bin" ;;
  esac
done

[ "$status" -eq 0 ] && echo "floor checked: ${bins[*]}"
exit "$status"
