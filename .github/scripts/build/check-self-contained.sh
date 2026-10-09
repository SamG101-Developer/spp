#!/usr/bin/env bash
# Fail unless the built binaries need nothing beyond what the OS ships.
set -euo pipefail

exe=""
[ "$RUNNER_OS" = "Windows" ] && exe=".exe"

bins=("build/spp${exe}")
[ -f "build/tests/spp_tests${exe}" ] && bins+=("build/tests/spp_tests${exe}")

status=0
bad() {
  echo "::error::$1"
  status=1
}

# An allow-list, not a location check: libz3 sits in /usr/lib on a runner with LLVM and nowhere on a fresh one.
check_linux() {
  local bin="$1" needed lib path
  needed="$(readelf -d "$bin" | sed -n 's/.*(NEEDED).*\[\(.*\)\]/\1/p')"
  echo "${bin} needs: $(tr '\n' ' ' <<< "$needed")"

  while read -r lib; do
    case "$lib" in
      ld-linux* | libc.so.* | libm.so.* | libdl.so.* | libpthread.so.* | librt.so.* | libz.so.* | libzstd.so.*) ;;
      libstdc++* | libgcc_s* | libmimalloc*) bad "${bin} links ${lib} dynamically; SPP_STATIC_RUNTIME should have absorbed it" ;;
      *) bad "${bin} links ${lib}, which a machine without this job's toolchain may not have" ;;
    esac
  done <<< "$needed"

  while read -r lib _ path _; do
    case "$path" in
      "not") bad "${bin}: ${lib} not found on the system loader path" ;;
      /lib/* | /lib64/* | /usr/lib/* | /usr/lib64/*) ;;
      *) bad "${bin}: ${lib} resolves to ${path}, outside the system library directories" ;;
    esac
  done < <(env -u LD_LIBRARY_PATH ldd "$bin" | grep '=>')
}

check_macos() {
  local bin="$1" path
  while read -r path _; do
    echo "${bin} needs: ${path}"
    case "$path" in
      /usr/lib/* | /System/*) ;;
      *) bad "${bin} links ${path}, which is not part of macOS" ;;
    esac
  done < <(otool -L "$bin" | tail -n +2)
}

# api-ms-win-* are API sets the loader maps, not files in System32.
check_windows() {
  local bin="$1" dll system32
  system32="$(cygpath -u "$SYSTEMROOT")/System32"
  if ! command -v dumpbin > /dev/null; then
    bad "dumpbin is not on PATH; cannot check ${bin}"
    return
  fi
  while read -r dll; do
    echo "${bin} needs: ${dll}"
    case "${dll,,}" in
      api-ms-win-* | ext-ms-*) continue ;;
    esac
    [ -f "${system32}/${dll}" ] || bad "${bin} links ${dll}, which is not in System32"
  done < <(dumpbin -nologo -dependents "$bin" | tr -d '\r' | sed -n 's/^ *\([^ ]*\.[dD][lL][lL]\)$/\1/p')
}

for bin in "${bins[@]}"; do
  if ! [ -f "$bin" ]; then
    bad "${bin} was not built"
    continue
  fi
  case "$RUNNER_OS" in
    Linux) check_linux "$bin" ;;
    macOS) check_macos "$bin" ;;
    Windows) check_windows "$bin" ;;
    *) bad "no self-containment check for ${RUNNER_OS}" ;;
  esac
done

[ "$status" -eq 0 ] && echo "self-contained: ${bins[*]}"
exit "$status"
