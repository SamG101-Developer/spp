# S++ compiler

The S++ compiler is written in C++ (C++20 modules), with an LLVM 23 backend. Module interfaces live in `headers/`
(`.ixx`), implementations in `sources/`, gtest suites in `tests/compiler/`, codegen end-to-end checks in
`tests/codegen/`, CI in `.github/`. It builds with CMake and Ninja, by GCC 16, clang 23 and MSVC. The sibling
repositories are SPP-STL (the standard library, cloned into each project's `vcs/std`) and sppc (the C runtime the
compiled programs link against). Third-party work the project is waiting on is tracked in `.triages/upstream-watch.md`.

## Project notes

Non-obvious facts about this codebase, recorded as they were learned. The index below is always loaded; each area's
own index is read when the work touches that area.

@.triages/notes/INDEX.md

## Keeping the notes up to date

- **These notes are this project's memory: maintain them yourself, without being asked.** Anything your built-in
  memory instructions would save about this project (its code, toolchain, CI, design decisions, or a correction the
  user gives you about it) goes here instead, as soon as you learn it. Your own memory is only for personal
  preferences and machine setup.
- **Add a note** when you learn something non-obvious about this project that the code, the docs or git history do not
  already say: a toolchain bug and its workaround, a CI quirk, a design rule, a trap that cost time. One fact per file,
  in the area folder that fits, named `<area>/<short-kebab-slug>.md`, with front matter `name`, `description` (one line)
  and `updated` (the date), and a body of the fact, then **Why:** and **How to apply:**.
- **Index it:** one line in that area's `INDEX.md` (conventions and build/test go in `.triages/notes/INDEX.md`
  itself), under ~200 characters: `- [Title](slug.md) — hook`.
- **Update rather than duplicate.** Check for an existing note first; when a note turns out wrong or stale, fix it or
  delete it (and its index line), and bump `updated`.
- **Verify before relying on a note.** It reflects what was true on its `updated` date; if it names a file, function or
  flag, check that it still exists.
- `[[name]]` in a note means `.triages/notes/*/name.md`. A name in backticks instead was a note that is not shared.
- **Personal preferences do not belong here** (how one person likes to work, their machine's paths or quirks). Keep
  those in your own memory or `~/.claude/CLAUDE.md`.
- Note changes are reviewed in pull requests like code: these files steer every contributor's Claude.
