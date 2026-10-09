#!/usr/bin/env bash
# Check the downloaded binaries against the digests from
# `needs`, then make them executable.
set -euo pipefail
source .github/scripts/lib/verified-fetch.sh

# Overridable: the release checks a Windows build from a
# Linux runner.
exe="${BIN_EXE-}"
if [ -z "${BIN_EXE+set}" ] && [ "$RUNNER_OS" = "Windows" ]; then
  exe=".exe"
fi
dir="${BIN_DIR:-build}"

# The expected set follows the digests: spp always, spp_tests
# when the build staged it.
expected=("spp${exe}")
digests=("${SPP_SHA256:-}")
if [ -n "${TESTS_SHA256:-}" ]; then
  expected+=("tests/spp_tests${exe}")
  digests+=("$TESTS_SHA256")
fi

# Exactly that set: nothing unstaged rides along.
mapfile -t found < <(cd "$dir" && find . -type f | sed 's|^\./||' | sort)
mapfile -t wanted < <(printf '%s\n' "${expected[@]}" | sort)
if [ "${found[*]}" != "${wanted[*]}" ]; then
  echo "::error::the artifact holds [${found[*]}], not exactly [${wanted[*]}]"
  exit 1
fi

for i in "${!expected[@]}"; do
  file="${dir}/${expected[$i]}"
  want="${digests[$i]}"
  # An empty digest is a broken handover, never a reason to skip.
  if ! [[ "$want" =~ ^[0-9a-f]{64}$ ]]; then
    echo "::error::no valid sha256 was handed over for ${expected[$i]}: '${want}'"
    exit 1
  fi
  got="$(sha256_of "$file")"
  if [ "$got" != "$want" ]; then
    echo "::error::${expected[$i]} does not match the binary the build job staged"
    echo "  expected ${want}"
    echo "  actual   ${got}"
    exit 1
  fi
  # Artifacts do not keep the executable bit.
  chmod +x "$file"
  echo "verified ${expected[$i]} (sha256 ${got})"
done
