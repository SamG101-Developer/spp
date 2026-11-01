---
name: shell-through-utils-shell
description: "Commands run through utils::shell::Run, paths and values through shell::Quote; cmd /c strips quotes from a line starting with one"
updated: 2026-10-30
---

The compiler runs commands only through `spp::utils::shell::Run`, and puts every path or value in one through
`shell::Quote`. A linker or `--runner` stays unquoted, as it is a command meant to split into words. `no-raw-shell` in
`banned_idioms.toml` rejects `std::system` and `popen` under `sources/` and `headers/`; tests may call them directly.

**Why:** `cmd /c` removes the first and the last quote from a line that starts with a quote, unless the line holds exactly
two quotes and meets its other conditions. So `"C:\a b\cc.exe" -o "x y"` loses the wrong quotes. `Run` gives such a line an
outer pair for `cmd` to take. A quoted `[target.linkers]` entry hit this as soon as the link's paths were quoted.

**How to apply:** do not add a second route to the shell. `cmd` also expands `%VAR%` inside double quotes, with no reliable
escape on a `cmd /c` line, so a path holding `%...%` remains a known gap on Windows. See [[sys-names-vs-c-macros]] for why
`test_msc_shell.cpp` has its `#undef`s.
