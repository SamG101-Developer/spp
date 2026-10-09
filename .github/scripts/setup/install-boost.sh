#!/usr/bin/env bash
# Unpack the prebuilt Boost for this runner into $HOME/boost and
# export BOOST_ROOT: building it from source costs far more than
# the download. The path is the tarball's own top-level boost/,
# which is why it is not an SPP_*_PREFIX.
set -euo pipefail
source .github/scripts/lib/verified-fetch.sh

base="https://github.com/MarkusJx/prebuilt-boost/releases/download/${BOOST_VERSION}"

# Keyed on the image, not RUNNER_OS/RUNNER_ARCH: each tarball is
# built against that image's libstdc++ or MSVC runtime, and the
# two cannot tell 22.04 from 24.04.
if [ -z "${SPP_RUNNER_IMAGE:-}" ]; then
  echo "::error::SPP_RUNNER_IMAGE is not set; setup-toolchain must be given its runner-image input"
  exit 1
fi

# Keyed by the image without its dots: ubuntu-24.04-arm reads BOOST_ASSET_UBUNTU_2404_ARM.
key="$(printf '%s' "${SPP_RUNNER_IMAGE//./}" | tr 'a-z-' 'A-Z_')"
asset_var="BOOST_ASSET_${key}"
sha_var="BOOST_SHA256_${key}"
asset="${!asset_var:-}"
sha="${!sha_var:-}"

# Never guess: an unknown image is an error. Updates can be
# requested from the prebuilt-boost repository.
if [ -z "$asset" ] || [ -z "$sha" ]; then
  echo "::error::no Boost asset is pinned for ${SPP_RUNNER_IMAGE}"
  echo "::error::Add asset-${SPP_RUNNER_IMAGE//./} and sha256-${SPP_RUNNER_IMAGE//./} to [pin.boost] in .github/dependencies.toml."
  exit 1
fi

# The asset names carry a '+', which the download URL has to escape.
file="boost-${BOOST_VERSION}-${asset}.tar.gz"
url="${base}/${file//+/%2B}"

tmp="$RUNNER_TEMP"
[ "$RUNNER_OS" = "Windows" ] && tmp="$(cygpath -u "$tmp")"
verified_fetch "$url" "$tmp/boost.tar.gz" "$sha"
tar xf "$tmp/boost.tar.gz" -C "$HOME"

if [ "$RUNNER_OS" = "Windows" ]; then
  echo "BOOST_ROOT=$(cygpath -m "$HOME/boost")" >> "$GITHUB_ENV"
else
  echo "BOOST_ROOT=$HOME/boost" >> "$GITHUB_ENV"
fi
