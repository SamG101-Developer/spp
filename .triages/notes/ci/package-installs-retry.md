---
name: package-installs-retry
description: "apt, add-apt-repository, llvm.sh and brew installs in CI go through lib/retry.sh (3 tries, 10 s then 30 s); a re-run only works once the whole run ends"
updated: 2026-11-11
---

Every CI call that installs from a package server is wrapped in `retry` (`.github/scripts/lib/retry.sh`): three
attempts, 10 s then 30 s apart, each failure a `::warning::`, the last an `::error::` with the command's own exit code.
Downloads already retry inside `verified_fetch` (`curl --retry 3 --retry-all-errors`). `brew info` is left bare where its
failure picks the formula.

Added 2026-11-11 after post-merge's coverage build died in `install-clang-linux.sh`: `llvm.sh`'s `add-apt-repository`
had its connection to `api.launchpad.net` reset. GitHub cannot re-run one job while its run is still going (the UI
button, `gh run rerun --job` and the API all wait for the whole run), and a re-run also re-runs the jobs that need it,
so a blip early in a long run costs the rest of the run's wall time.

**Why:** a network blip should cost 10 s, not a re-run after the release lanes finish.

**How to apply:** a new install step does `source .github/scripts/lib/retry.sh` and prefixes the network command with
`retry`. Keep the helper bash 3.2 safe: `install-bash-macos.sh` runs under macOS's own bash.
