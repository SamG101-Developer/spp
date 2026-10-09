#!/usr/bin/env bash
# Check dist/ against the checksums from `needs`: exactly those files and digests, and an identical SHA256SUMS.
set -euo pipefail

LINE_RE='^[0-9a-f]{64}  spp-[0-9]+\.[0-9]+\.[0-9]+-[a-z0-9-]+(\.exe)?$'

if [ -z "${CHECKSUMS:-}" ]; then
  echo "::error::no checksums were handed over"
  exit 1
fi

while IFS= read -r line; do
  [[ "$line" =~ $LINE_RE ]] || { echo "::error::malformed checksum line: ${line}"; exit 1; }
done <<< "$CHECKSUMS"

# The attached SHA256SUMS is the one that was attested, byte for byte.
if [ "$(cat dist/SHA256SUMS)" != "$CHECKSUMS" ]; then
  echo "::error::dist/SHA256SUMS differs from the checksums collect-release published"
  exit 1
fi

mapfile -t found < <(cd dist && find . -type f | sed 's|^\./||' | sort)
mapfile -t wanted < <({ cut -c67- <<< "$CHECKSUMS"; echo SHA256SUMS; } | sort)
if [ "${found[*]}" != "${wanted[*]}" ]; then
  echo "::error::dist holds [${found[*]}], not exactly [${wanted[*]}]"
  exit 1
fi

(cd dist && sha256sum --check --strict <<< "$CHECKSUMS")
