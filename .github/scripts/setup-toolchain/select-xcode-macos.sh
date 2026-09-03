#!/usr/bin/env bash
# Point the toolchain at the Xcode pinned for this runner
# image. Keyed on the image rather than taking the default,
# because the images share no default Xcode: macos-15 boots
# on 16.4 and macos-26 on 26.6, and the SDK that comes with
# each is what clang links against.
set -euo pipefail

if [ -z "${SPP_RUNNER_IMAGE:-}" ]; then
  echo "::error::SPP_RUNNER_IMAGE is not set; setup-toolchain must be given its runner-image input"
  exit 1
fi

var="XCODE_DEVELOPER_DIR_$(printf '%s' "$SPP_RUNNER_IMAGE" | tr 'a-z-' 'A-Z_')"
dir="${!var:-}"

if [ -z "$dir" ]; then
  echo "::error::no Xcode is pinned for ${SPP_RUNNER_IMAGE}"
  echo "::error::Add developer-dir-${SPP_RUNNER_IMAGE} to [pin.xcode] in .github/dependencies.toml."
  exit 1
fi

if [ ! -d "$dir" ]; then
  echo "::error::${dir} is not on ${SPP_RUNNER_IMAGE}; the image dropped that Xcode, so repin [pin.xcode]"
  exit 1
fi

sudo xcode-select -s "$dir"
xcode-select -p
xcodebuild -version
