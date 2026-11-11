---
name: hardening-checks-fail-closed
description: "A hardening check on an artefact refuses what it cannot read: Expected<Opt<Str>, Str>, never a silent pass"
updated: 2026-11-11
---

The artefact checks in `llvm_artefact.cpp` (`UnfortifiedImport`, `UnhardenedLink`, `UnhardenedSegments`) answer
`Expected<Opt<Str>, Str>`: none = nothing found (or not ELF, so nothing to check), a string = the finding, an error =
the file could not be read. The callers (`ffi::Check*`, the executable checks in `compiler_link.cpp`) stop the build on
the error, with "cannot read '<library>'". Until 2026-11-09 the fortify and RELRO checks answered `Opt<Str>` and
turned every read error into none, so an unreadable library passed them; segments already failed closed, except when
`ELFFile::create` failed (`VisitElf` then said "not ELF").

One exception, by design: the fortify check skips a single symbol whose name or flags can't be read, as its answer
turns on whether any `__*_chk` import exists. `ImportsAtomicLibcalls` still fails open: it chooses whether to link
libatomic, not whether a library is safe, and a wrong answer is a link error.

**Why:** a check that cannot vouch for a library must not let it through.
**How to apply:** a new artefact check returns the same shape and goes through `ElfFinding` if it reads ELF; its
codegen check gets a `check_unreadable_ffi` case (`tests/codegen/lib.sh`). Related: [[hardening-needs-feature-gate]].
