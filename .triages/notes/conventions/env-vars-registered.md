---
name: env-vars-registered
description: "Every SPP_ environment variable is in utils/env.cpp's table; CI's own use SPP_CI_; anything else stops spp at startup"
updated: 2026-10-30
---

The compiler reads the environment only through `spp::utils::env` (`headers/spp/utils/env.ixx`), by `env::Var`; there is
no string-keyed getter. `RunCli` runs `env::Check` first, so an `SPP_` name not in the table, or a value its variable does
not accept, is a hard error before any command runs, `--help` included. Names under `SPP_CI_` are skipped: they belong to
the workflows and scripts, and reach `spp` through `GITHUB_ENV`.

**Why:** a mistyped variable used to do nothing at all (`SPP_MEMORY_LIMIT=0`), and a bad value could do the opposite of
what was asked (`SPP_MEMORY_LIMIT_KB=abc` parsed to 0, which turned the cap off).

**How to apply:**
- A new variable for the compiler, its programs or its tests: add it to `Var` and `Schema()` (it then shows in `spp
  config`). One read only by a built program counts too, as `SPP_BACKTRACE` does for sppc.
- A new variable only CI or a script reads: name it `SPP_CI_*`, including one set by hand to run a script locally
  (`SPP_CI_EXE`), since `spp` will see it in its environment.
- `check-env-vars.py` (pre-commit, and CI's lint) fails on any other `SPP_` name in scripts, and on an `SPP_` name in a C++
  string outside the module. The `no-raw-env-access` rule in `banned_idioms.toml` fails on raw `getenv`/`setenv`/`environ`.
- Test the check with `env::CheckEntries` on given entries, not by changing the process's environment.
