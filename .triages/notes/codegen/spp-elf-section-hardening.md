---
name: spp-elf-section-hardening
description: "Every s++ global is already an llvm constant; the missing half was the link, now full RELRO + BIND_NOW"
---

Section placement in s++ is two properties, and only one of them was ever the problem.

**Codegen was already right.** Every global the compiler emits is an llvm `constant` - `cmp` data
(`cmp_statement_ast.cpp:325`, `isConstant = true`), string literal bytes and the `{ptr,len}` view
(`string_literal_ast.cpp:121`), and the cross-module declarations (`llvm_fn.cpp:42`). Grepping `lto.ll` for
`^@.*= global` returns zero: there is no mutable global, because the language has no spelling for one. So the object
has no `.data` or `.bss` of its own at all - literal bytes land in `.rodata`, and the views land in `.data.rel.ro`
because a pointer needs a relocation.

**The link was not.** `_LinkExecutable` in `compiler_boot.cpp` shelled out to a bare `cc -o ... -lm -Wl,-rpath,...`,
so the binary inherited the host driver's defaults: partial RELRO, lazy binding, `.got.plt` writable for the life of
the process. Now gated behind `[binary.link] harden` (`ConfigKey::BinaryLinkHarden`) and passing
`-pie -Wl,-z,relro,-z,now -Wl,-z,noexecstack -Wl,-z,separate-code -Wl,-z,defs`. Verify with
`readelf -dW <bin> | grep BIND_NOW` and `readelf -SW <bin> | grep got.plt` - under the flags `.got.plt` disappears
entirely (merged into `.got`, which GNU_RELRO covers) and the only writable bytes left are libc's `completed.0`.

**How to apply:** `-z nodlopen` is *not* passed and should not be added to the exe link - ld drops it when the output
is an executable, producing no `DF_1_NOOPEN` bit, and a flag that leaves no trace in the artefact defeats the point of
auditing a binary for what it got. It belongs on a `build = "lib"` link once that exists. `-pie` is safe because
`SelectedTargetMachine` in `llvm_target.cpp` builds the target machine with `Reloc::PIC_`.

Also noted while doing this: `--dir` parses in `cli.cpp:152` but is applied nowhere - the comment says "read in main",
and `main.cpp` no longer does. Every subcommand resolves the project from `current_path()`, so `cd project` first.
See `one-hardening-measure-at-a-time`, [[spp-toml-feature-schema]].

**The rel build was broken by this measure's neighbour, not by the link.** Every `rel` binary segfaulted on the
entry point's third instruction. `ApplySafeStack` stamped *every* function, including the generated C `main`, whose
prologue reads `__safestack_unsafe_stack_ptr` - but the call that maps that stack (`sppc_unsafe_stack_up`, inside
`spp.rt.init`) is the first thing in `main`'s *body*. So `main` claimed a frame through a null pointer and wrote the
canary (which SafeStack moves onto the unsafe stack) through it. `dev` survived by luck: unoptimised `main` has no
locals, so nothing needed an unsafe frame; at `rel` the inliner pulled the s++ main into it and the frame appeared.
Fixed by a `"spp-no-split-stack"` string attribute set in `EmitCEntryPoint`/`RuntimeInitShim` and honoured in
`ApplySafeStack`, plus `noinline` on the s++ entry point - without that second half the inliner puts the whole
program into the one unsplit frame and the measure reports itself applied while the binary carries none of it.
Verify with `objdump -d project | sed -n '/<main>:/,/^$/p'`: `main` must be a bare wrapper with no
`__safestack_unsafe_stack_ptr` read, and the mangled `main#main#...` must have one.

The canary end-to-end check is `tests/codegen/check-stack-protector.sh` (2026-10-17). Overflow is 9 bytes: the victim's unsafe frame sits flush against the top of the unsafe-stack mapping, so longer writes segfault on the mapping end before the canary check.

**Section audit findings (2026-09-07).** Placement is correct: no `.data`/`.bss` of the program's own, the jump table
is PC-relative in `.rodata` (read-only from load, no relocation), no `TEXTREL`, no RWX segment, and the dynamic
export table is empty - internalize leaves nothing exported. `-z separate-code` was already the binutils default, and
the hardened link is *smaller* than the default one (18440 vs 18624 bytes) because `-z now` drops `.plt`/`.got.plt`.

Two real defects found:
1. **Literal views were not deduplicated.** `CreateGlobalString` marks the *bytes* `unnamed_addr`, so those merge,
   but `string_literal_ast.cpp` built the `{ptr,len}` view without it, so ConstantMerge refused to fold identical
   views. Four copies of one `StrView` in `.data.rel.ro`, four dynamic relocations. Fixed with
   `setUnnamedAddr(UnnamedAddr::Global)` on the view; `.data.rel.ro` 128 -> 64 bytes and RELATIVE relocs 11 -> 7 on a
   toy program.
2. **`libsppc.so` is partial RELRO with lazy binding** (fixed in sppc's CMakeLists + ctest `test_hardening` 2026-10-17; the shipped copy is not rebuilt yet, plan item G1) - no `BIND_NOW`, `.got.plt` present. The compiler hardens the
   executable and then links a runtime that is not. It is prebuilt in the sibling repo [[sppc-sibling-repo]], so the
   fix is there (or a link-time check, the doc's "fortified binaries" item). Still open.
   The duplicate shipping is **fixed**, and the ffi layout is now flat: a package is `ffi/<name>/lib<name>.so`
   beside `ffi/<name>/stub.spp`, with no `lib` subfolder. Both the extension and the name shape come from
   `utils::files::SharedLibraryExtension`/`SharedLibraryName` (`so`/`dylib`/`dll`, `lib` prefix except on Windows),
   which the sweep and the validator both ask, so the shape a project is held to cannot drift from the shape the
   linker is handed. `_FfiLibraries` sweeps for a `.so` whose parent's parent is `ffi`, `_LinkExecutable` stages that one
   file and links it, and `HandleValidate` rejects a leftover `lib` subfolder (naming the move to make) and a package with no
   `ffi/<name>/lib<name>.so`. It is one `std::filesystem::exists` on the whole expected file name - presence, not
   absence of others, because erroring on the first non-matching file reported it even when the right library was
   sitting beside it, and the extension is part of what makes a file linkable here. A package shipping only another
   platform's library therefore fails on this host rather than being skipped. Extras beside the right one are
   allowed, and are staged into `out/*/lib` and shipped, though `--as-needed` keeps them out of `DT_NEEDED`.

   Known consequence, accepted by the maintainer on 2026-09-07 ("leave it failing"): `project/ffi/test_functions` ships only
   `test_functions.dll` with no source anywhere, so the sample project does not validate or build until that package
   is resolved. Its only user is `project/src/testing/ffi_function_calls.spp`. Shipped `lib/` went 136 KB -> 68 KB. The rename in SPP-STL
   (`ffi/sppc/lib/sppc.so` -> `libsppc.so`, and `sppc.so.dbg` deleted) is staged in the `project/vcs/std` checkout
   but **not committed** - The maintainer declined the commit, so it needs committing and pushing there or the next clone
   fails validation. Note the vcs -> `project/ffi` copy merges rather than mirrors, so a stale `sppc.so` in an
   existing tree survives an update; the validator is what catches it.

`-z text` and `--error-rwx-segments` are on the exe link since 2026-10-17 (plan G3).

**`spp run` used to run a stale binary after a failed build (fixed 2026-09-07).** Four things stacked up: `HandleBuild`
returned `void`, so `HandleRun` asked "does an executable exist" instead of "did the build succeed"; the
`std::filesystem::remove(out.ExecutablePath())` that was meant to stop exactly this sits *after* the validation call,
so a validation failure never reached it; `HandleRun` then wipes the console (`\033[H\033[2J\033[3J`) before running,
scrolling the error away; and `main.cpp` discarded `RunCli`'s return so everything exited 0. Now `HandleBuild` and
`HandleRun` return `bool`, the `build`/`run`/`validate` callbacks throw `CLI::RuntimeError(1)`, `RunCli` catches
`RuntimeError` *before* `ParseError` (it is a failed command, not a mistyped one - the `ParseError` arm dumps the whole
help text over the real message), and `main.cpp` returns the status.

**Source errors used to kill the compiler (fixed 2026-09-07).** A lexical, syntactic or semantic error is raised by
throwing it (`spp::utils::errors::AbstractError : std::runtime_error`, with the whole rendered message in `what()`),
`Compiler::Compile` cleans up and *re-throws*, and nothing above it caught - so a merely-wrong program left the
compiler through `terminate`, printing the real message wrapped in "terminate called after throwing an instance of"
and dying with signal 6. `CompileReportingErrors` in `cli.cpp` now wraps the `Compile()` calls in `HandleBuild` and
`HandleTest`, prints `what()` and returns false. It is `#if SPP_DEBUG`-guarded to still let the throw escape in a
debug build, matching the same guard `Compiler::Compile` already uses so the stack is intact for a debugger.
`RunCppGoogleTest` is deliberately left uncaught - the gtest suite asserts on the error types itself.

**Syntax errors printed twice (fixed 2026-09-07).** `SyntacticErrorBuilder::Raise` in
`headers/spp/parse/errors/parser_error_builder.ixx` did `std::cout << err_msg` *and* stored the same string in
`_ErrObj->messages`, which `AbstractErrorBuilder::Raise` joins into `final_message` (what `what()` returns) - so the
whole formatted block appeared twice. The semantic builder never printed, only stored; the parser one was the odd one
out. Dropped the `cout`. Same statement also had `err_msg.replace(err_msg.find("£"), 1, ...)` - "£" is two bytes of
utf-8, so one byte was left behind as mojibake in every syntax error; now uses the placeholder's `size()`.

The expected-token list is rendered now too, as a plain loop rather than the commented-out
`transform|intersperse|join` chain (see [[genex-view-gotchas]]): "Expected '$', '&', '(', '[', 'mut', a character,
got '='". Two things it has to do - `Set` is `ankerl::unordered_dense::set`, so the names are **sorted** or the same
failure reads differently between runs; and `TokToString` returns "" for the six `LX_*` categories, so
`TokenTypeForMessage` names those ("a number", "an identifier") unquoted, since "expected 'an identifier'" reads as a
literal spelling. `LX_CHARACTER` stays "a character" even though it usually turns up where a name is wanted -
`ParseLexemeCharacter` is a generic building block, so calling it "an identifier" would be false inside a char or
string literal. `TK_DOLLAR` is suppressed outright: `ParseLexemeIdentifier` does `PARSE_OPTIONAL(ParseTokenDollar)`
before the character it actually needs, so every failed name recorded "$" as though it were an alternative when it
satisfies nothing on its own - and "$" is the namespace `utils::Uid` mints mock-type names in, never written in a
`.spp` (see [[spp-dollar-mock-types]]). Note the general shape of this: `_ErrorBuilder->Tokens` records everything
*attempted* at the failing position, including `PARSE_OPTIONAL` branches, so an optional token can always show up
looking like a requirement.

**Errors past the end of a file used to quote the prelude (fixed 2026-09-07).** `Lexer` appends `kPrelude` *after* the
module so it does not shift anyone's line numbers - but that leaves every token after it belonging to no file, so a
parse that ran off the end reported "on line 6: use std::annotations::annotation ... got 'use'" against a four-line
file whose author wrote none of it. `Lexer` now records `PreludeTokenIndex()` (the first token at or past the append
offset, taken during the lex because a `RawToken` carries no source offset), `ErrorFormatter` takes it and exposes
`IsPastUserSource(pos)` - a plain index compare, because the parser asks it on every failed token - and both ends use
it: the formatter shows `<end of file>` instead of a quoted prelude line (same shape as the existing
`ast_start_pos == 0` -> `<generated code>` placeholder), and `parse_token_primitive` says "got the end of the file"
instead of naming a token the compiler inserted. `InternalParseErrorRawPos` now returns a *six*-tuple - a `location`
phrase ("on line 4" / "at the end of the file" / "in generated code") alongside the gutter line number, because not
every position has a number and "on line ?" is not words. Both `ErrorRawPos` and `ErrorRawPosMinimal` print the
phrase; the placeholder cases pass an empty line number so the gutter is blank rather than "?". Modules lexed without a prelude (`/src/std/`, `INJECT_CODE`) get
`npos` and are unaffected.
