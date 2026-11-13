---
name: windows-local-setup
description: .tools/setup-windows.sh runs CI's Windows MSVC setup scripts on a local machine; needs an LF clone and a Native Tools prompt
updated: 2026-11-13
---

`.tools/setup-windows.sh [--configure | --build]` installs what the Windows MSVC lane installs (pinned LLVM into
`~/llvm`, zlib/zstd/libxml2/pthreads4w into `~/llvm-deps`, the small libraries into `~/.local`) by running CI's own
scripts in order, standing in for `RUNNER_OS`/`RUNNER_ARCH`/`RUNNER_TEMP` and reading back what each step appends to
`GITHUB_ENV`/`GITHUB_PATH`. It then configures and builds `build/` through `configure.sh`/`build.sh` with sccache off.
`CC`, `CXX` and `CMAKE_PREFIX_PATH` are saved to `build/windows.env` for later hand builds.

`install-llvm-windows.sh` and `install-llvm-deps-windows.sh` redo their whole download or build on every run (CI caches
their prefixes), so the wrapper skips each when its CMake config file is already there. `RUNNER_ARCH` comes from
PowerShell's `OSArchitecture`, as Git Bash on an ARM machine may be x64 under emulation. A `python3` shim is added
when only `python` exists.

**Why:** the repo has no `.gitattributes`, so a default Windows clone checks scripts out CRLF and every bash step fails.
`cl` only exists inside a VS developer prompt; CI gets it from `ilammy/msvc-dev-cmd`.

**How to apply:** clone with `git clone -c core.autocrlf=false`, start Git Bash from the "x64 Native Tools Command
Prompt" (or ARM64), then run the wrapper. A new install step in the Windows lane gets a matching line in the wrapper.
Related: [[lf-line-endings]].
