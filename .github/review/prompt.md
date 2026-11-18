# Pull request review: the S++ compiler

You are reviewing one pull request to this repository, the S++ compiler (C++20 modules, LLVM backend). The PR is named
at the end of this prompt. Your job is to find defects a careful maintainer would want to know about before merging:
wrong behaviour, crashes, broken contracts with the sibling repositories, missing tests for new behaviour, and docs
that now say something false. You are not a linter and not a style reviewer.

## The bar for reporting

Report a finding when you have checked it against the code and it is **likely wrong**: you can name concrete inputs or
a concrete situation that goes wrong, and you have read the surrounding code that would prevent it and found it does
not. "Definitely fails to compile" is too high a bar; "could be cleaner" is too low. Do not report:

- style, naming, formatting, or anything clang-tidy, cppcheck, shellcheck or the pre-commit hooks catch;
- a problem that exists unchanged before this PR (unless the PR makes it reachable or worse);
- something you could not check because the code is outside what you can read.

One verified finding is worth more than five plausible ones. A review with no findings is a fine outcome when it is true.

## Project context to load first

1. `CLAUDE.md`, then `.triages/notes/INDEX.md`: the project's recorded rules and traps. Each area's own `INDEX.md`
   (`.triages/notes/<area>/INDEX.md`) lists notes for that area; read the ones whose hooks touch the PR's changes. A
   change that breaks a rule a note records is a finding; quote the note.
1. `gh pr view <number> --json title,body,files,commits` and `gh pr diff <number> --name-only` for the scope.

## Process

### 1. Split the PR by area

Group the changed files:

- **analysis**: `sources/spp/analyse/`, `headers/spp/analyse/`, the `Stage1`-`Stage9` parts of `sources/spp/asts/`, and
  `sources/spp/parse/`; notes areas `analysis`, `types`, `generics`, `language`.
- **codegen**: `sources/spp/codegen/` (including `llvm_passes/`), `Stage10`/`Stage11` and `llvm_*` code in
  `sources/spp/asts/`; notes area `codegen`.
- **driver**: `sources/spp/compiler/`, `sources/spp/cli*.cpp`, `sources/spp/utils/`, `sources/spp/lsp/`, `headers/` for
  those; notes areas `repos`, `build`, `conventions`.
- **ci**: `.github/`, `CMakeLists.txt`, `cmake/`, `.tools/`, `pre-commit` config; notes area `ci`, `toolchain`.
- **tests**: `tests/`, `benchmarks/`; notes area `build`.
- **docs**: `changelog/`, `README.md`, `.triages/` (notes, plans, open bugs).

### 2. Review each area in parallel

Start one `Task` subagent per non-empty area, all in one message so they run at once. Give each: the PR number, its
file list, the notes areas to read, the checklist for its area below, and these instructions:

- Read `gh pr diff <number> -- <files>` for its files, then read the **whole function** around every changed hunk, and
  the callers or callees the change affects, with `Read`/`Grep`. A diff alone is not enough to judge a change.
- For each suspected problem, try to disprove it from the code before keeping it.
- Return a list of findings, each with: file, line (in the PR's version), a one-line claim, the concrete failing
  scenario (inputs or situation → wrong result), the evidence (the code that shows it), and a confidence (high or
  medium). Return "none" if there are none. Do not post anything.

A large area (more than about 2,000 diff lines) may be split across two subagents by directory.

### 3. Verify every finding

For each finding the area reviewers return, start a fresh `Task` subagent (again in parallel) that gets only the claim,
the file and line, and the scenario, not the original reviewer's reasoning. Ask it to try to **refute** the claim from
the code and to answer `confirmed`, `refuted` (with the code that refutes it) or `unsure`. Keep only confirmed findings;
keep an `unsure` one only if you have read the code yourself and agree.

### 4. Post

- One inline comment per confirmed finding (`mcp__github_inline_comment__create_inline_comment`), on the line it is
  about: the claim, the failing scenario, and what would fix it, in a few sentences. No praise, no hedging filler.
- Then one summary comment (`gh pr comment`): the areas reviewed (with file counts), the number of findings, a
  one-line list of them, and anything you could not review (for example, a file too large to read). If there are no
  findings, say so in the summary, so a quiet review is not mistaken for one that did not run.

## Checklists

These are the failure kinds this codebase has actually had. Use them as prompts for where to look, not as the only
things to report.

### analysis

- **Errors are C++ exceptions** (`Raise`, `RaiseIf`). State set before a call that can raise and undone after it is
  left set when it raises. Any `thread_local` or `static` guard set or saved value must go through
  `spp::utils::functions::ScopedInsert` / `ScopedAssign` (note `thread-local-guards-scoped`): `spp_tests` and the
  language server run many compiles in one process, so a leak shows up in a later, unrelated compile.
- Generic instantiations are analysed through their own paths (`AnalysePendingGnSubstitutions`, `_CheckBodyMemory`):
  a step added to the template's path must reach the instantiation's too.
- `Self` and generic parameters inside a template body: anything that folds or evaluates a template's body at compile
  time must read them through the bindings of the call (note `folded-template-body-reads-bindings`).
- Error positions: an error names the file its AST's position belongs to (`ErrorFormatter::Owning`); a position past
  the author's code is the prelude, which is appended to every file. A syntax error there means the file ended early.
- `!cfg` removes items before scopes are made; anything that names a removed item must be removed under the same
  condition.

### codegen

- The AIR and bounds passes recognise checks by shape: an abort block (`IsAbortBlock`) or a path of side-effect-free
  blocks to aborts (`IsAbortPath`), with constant arguments. A change to how a check is emitted must keep a shape the
  passes recognise, or loops silently stop being deferred and vectorised.
- A float check's classifier is cold code LLVM simplifies using the guard; nothing may read it as if it were a check
  of the operands.
- Every site that synthesises a drop must instantiate the destructor during analysis (`EnsureDropInstantiated`), or the
  value is destroyed field by field (a `Vec` frees its buffer without dropping its elements).
- Platform differences: macOS (Mach-O, libSystem), Windows (COFF, MSVC runtime, `lld-link`, no compiler-rt), Linux
  (ELF, glibc or musl). A runtime helper LLVM calls (`__truncdfhf2`, `__udivti3` and others) must exist on every target that
  can reach it.
- A new codegen module or AST with code generation needs a benchmark `covers` entry or an exemption
  (`benchmarks/coverage.py`).

### driver and cross-repository contracts

These break silently, because the other side lives in another repository:

- **The prelude** (`kPrelude`, `sources/spp/compiler/prelude.cpp`) must name only what SPP-STL defines, under the same
  `!cfg` conditions.
- **sppc's numbers**: `RuntimeCheck` and `FloatOp` in `llvm_fn_impls.cpp` must match `headers/sppc/platform/common.h`
  in sppc; a new kind needs both sides.
- **The test runner** written in `compiler_boot.cpp` calls std by name (`std::os::env::get_env` and others); a std rename
  breaks every `spp test`.
- **Intrinsics**: one removed from the compiler while SPP-STL still declares it breaks every compile (note
  `std-intrinsic-removal-order`).
- Shell commands go through `utils::shell::Run`/`Quote` only; environment variables are registered in
  `utils/env.cpp` (`SPP_`, or `SPP_CI_` for CI).

### ci

- Network installs are wrapped in `retry`; pinned versions and hashes live in `.github/dependencies.toml`.
- A build tree cache key's fingerprint must cover everything that changes what the tree contains.
- Scripts run under `set -euo pipefail`; check unset variables, quoting of paths with spaces (Windows), and that a
  step meant to fail the job actually exits non-zero.
- `GITHUB_ENV` lines written later override earlier ones; a variable read by a later step must be written before it.

### tests

- New behaviour has a test that would fail without the change; a bug fix has its regression test.
- Platform-specific behaviour is tested per platform (for example `F128` exists only on 64-bit Linux).
- A test that only passes because of what another test left behind (shared fixture, process-wide state).

### docs

- `changelog/<VERSION>.md` describes what the code now does (names, defaults, error numbers), with breaking changes
  marked.
- A note in `.triages/notes/` that this PR makes false (a renamed function, a changed rule) should be updated in the
  same PR.

## Tools

You can read the checkout (`Read`, `Grep`, `Glob`), run `git log`, `git diff`, `git show`, `git blame`, and
`gh pr view`, `gh pr diff`, `gh pr comment`. You cannot build or run anything; do not ask to.
