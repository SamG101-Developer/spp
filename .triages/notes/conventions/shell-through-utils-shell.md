---
name: shell-through-utils-shell
description: "Commands run through utils::shell::Run, paths and values through shell::Quote, programs found by shell::Find*; cmd /c strips quotes from a line starting with one"
updated: 2026-11-18
---

The compiler runs commands only through `spp::utils::shell::Run`, and puts every path or value in one through
`shell::Quote`. A linker or `--runner` stays unquoted, as it is a command meant to split into words. `no-raw-shell` in
`banned_idioms.toml` rejects `std::system` and `popen` under `sources/` and `headers/`; tests may call them directly.

Finding what the shell would run lives there too (moved from `utils::files` 2026-11-18): `FindProgram` (a name on
PATH), `FindCommand` (a written command, its program found and, for a bare path, quoted), `FindNewestVersion`
(`gcc-14` over `gcc-13`) and `PathListSeparator`. `utils::files` is files and paths only.

**Why:** `cmd /c` removes the first and the last quote from a line that starts with a quote, unless the line holds exactly
two quotes and meets its other conditions. So `"C:\a b\cc.exe" -o "x y"` loses the wrong quotes. `Run` gives such a line an
outer pair for `cmd` to take. A quoted `[target.linkers]` entry hit this as soon as the link's paths were quoted.

**How to apply:** do not add a second route to the shell. `cmd` also expands `%VAR%` inside double quotes, with no reliable
escape on a `cmd /c` line, so a path holding `%...%` remains a known gap on Windows. See [[sys-names-vs-c-macros]] for why
`test_msc_shell.cpp` has its `#undef`s.
