# Compile-time safety guarantees

This page tracks every safety property the compiler enforces or intends to enforce. The first section covers the
guarantees that fall out of the type and memory model and hold unconditionally; everything after it is a hardening
measure applied during code generation, linking, or packaging, and is gated on the build profile.

Legend: `[x]` implemented, `[ ]` planned, `[-]` considered and rejected (with the reason).

A hardening measure that is implemented is on by default, and is turned off, if at all, by naming it in the project's
`spp.toml`. The spelling is `[<group>.<area>]` with one key per measure, so the surface stays uniform as more are added:

```toml
[memory.stack]
protect = false   # no stack canaries
probe = false     # no page-by-page probing of a frame
split = false     # one frame per function, not a safe and an unsafe one
```

Nothing in "Language-level guarantees" has a key, and nothing in it ever will: those are what the language means, not a
setting. Checked arithmetic is one of them - `+` does not wrap, in any build, under any configuration, and there is no
spelling of `spp.toml` that makes it wrap. What is switchable is the hardening layered on top of that.

`spp.toml` is checked against a schema, so a mistyped key is an error rather than a measure silently left on - or,
worse, one someone believed they had turned off. `spp config` lists every section and key.

## Build profiles

- [ ] Four profiles: `dev` (fast builds, loud diagnostics), `rel` (optimised), `hard` (`rel` plus every mitigation that
  costs runtime), `ver` (proof obligations discharged statically, see "Verification and testing builds").
- [ ] No profile may weaken a language-level guarantee: the profile chooses *how* a property is enforced, never
  *whether* it is. Checked arithmetic and bounds checks are therefore present in `rel`, not just `hard`.
- [ ] Every mitigation below records itself in the binary's metadata, so a built artefact can be audited for which
  hardening was actually applied rather than which was requested.
- [ ] Any mitigation that depends on the target (MTE, PAC, CET) degrades to a documented software fallback, or a hard
  build error, never to a silent no-op.

## Language-level guarantees

These are enforced by the type system and the memory model, and need no runtime support. See the memory model page for
the full rules.

- [x] Linearity: every non-copyable value is used exactly once, so nothing leaks and nothing is released twice.
- [x] Ownership tracking: uninitialised, partially initialised and moved-from values cannot be read, borrowed or passed,
  which removes use-after-free and double-free.
- [x] Second-class borrows: a borrow can only originate at a function call site or a generator yield point, so no borrow
  can outlive the value behind it and there are no lifetimes to annotate.
- [x] The law of exclusivity: overlapping memory regions admit one mutable borrow xor n immutable borrows, which removes
  data races and iterator invalidation.
- [x] Escaping-borrow tracking: a value with a live borrow held across a suspension point cannot move, which keeps
  coroutine and async frames sound (pinning).
- [x] No null: absence is `Opt[T]`, and there is no null pointer to dereference.
- [x] Explicit, fallible destruction: a consuming method is an ordinary function that can report failure, and nothing is
  destroyed implicitly at a scope end.
- [x] Mutation is explicit: `mut` on the binding and `&mut` on the borrow, so no value is mutated through a path that
  does not say so.
- [ ] Exhaustiveness checking on `case` over variant types, with the non-exhaustive case a compile-time error rather
  than a runtime trap.

## Initialisation and residue

- [ ] Automatic variable initialisation: zero/pattern-fill every stack slot that the ownership tracker cannot prove as
  "initialised"; pattern in `dev` mode, zero in `rel` mode. See `-ftrivial-auto-var-init=[pattern|zero]`.
- [ ] Zero on drop for sensitive types: customise an allocator for this and override the deallocation method. An example
  is using the OpenSSL `secure_malloc`/`secure_free` methods.
- [ ] Register scrubbing on return: kills leaked pointers/secrets in callee-saved registers and removes "gadget"
  material. See `-fzero-call-used-regs`.
- [ ] Padding zeroed before any struct crosses an FFI boundary (or serialisation, IPC etc). Padding leaks are a CVE
  class.
- [ ] Poison-on-free/move-out in debug builds? Compile-time checks should mean this isn't needed, but can be done in the
  background anyway.

## Integer and arithmetic safety

- [x] Checked math in all builds (`dev` and `rel`): `+`, `-` and `*` abort rather than wrap.
- [x] No implicit integer conversions (always use `From::from`).
- [x] All math ops and comparisons with identical types.
- [x] Validation on shift amounts and zero division.
- [x] No pointer arithmetic (simple and safe).
- [ ] Index type discipline: distinct `USize` with restricted conversions?
- [x] Wrapping, saturating and checked behaviour only via named methods; no operator ever wraps silently.
- [ ] Overflow checks hoisted out of elementwise loops and folded into one test at the exit, so a loop over `+` can
  still vectorise. The trap then lands after the loop's stores rather than at the operation, which is a real change to
  when the abort is observed and so needs deciding rather than assuming.
- [ ] Report the failed check by name: the trap encodes which operation overflowed in its immediate, but nothing reads
  it back yet, so an overflow surfaces as `SIGILL` with no message.
- [ ] Overflow check on every size computation that feeds an allocation (`count * size_of[T]`), so a truncated size can
  never produce an undersized buffer.
- [ ] Float determinism: no fast-math, no FMA contraction, no reassociation unless the source asks for it, so results do
  not vary between profiles or targets.

**Implementation notes**

- The normal integer math intrinsics `+`, `-` and `*` are now fully checked at the llvm/asm intrinsic level.

## Bounds, indexing and slicing

- [x] Arrays and tuples are compile-time bounds checked, because the index is part of the expression: `.0`.
- [x] Indexing operations are runtime bounds checked: `[0]`.
- [ ] Bounds checks are never elided on the strength of a UB-derived assumption; elision requires a proof from the
  index's own type or a dominating comparison.
- [ ] Slice construction validates `start <= end <= len` at the point of construction, so a slice is bounds-safe by the
  time it is used.
- [ ] Bounds-check elision reported by the compiler on request, so hot loops can be audited rather than guessed at.

## Undefined behaviour policy

- [ ] Zero UB-driven optimisations anywhere.
- [ ] Strict pointer provenance (is this needed with second-class borrows?).
- [ ] Defined struct/enum validity invariants, checked at every trust boundary.
- [ ] Alignment check on all raw deref in hardened builds (just the `@` operator?).
- [ ] Aliasing metadata (`noalias` and friends) emitted only where exclusivity is actually proven for the whole call,
  not merely where a parameter is spelled `&mut`. The overlap analysis is no longer the thing standing in the way - it
  compares access paths rather than rendered expressions now, and rejects the `swap(v[mut i], v[mut j])` case that made
  `noalias` unsound - but a proof is what this needs, and pins taken through a coroutine handle with no name to hang
  them on are a second route that has not been closed.
- [ ] Defined behaviour for the operations C leaves open: signed overflow, shift past width, integer/float conversion
  out of range, null and misaligned access. Each traps or is defined, never "assume it cannot happen."

## Data integrity and immutability

- [ ] Const propagation: write all `cmp` data to `.rodata`.
- [ ] Data flow integrity: only specific sites can write to a variable, enforced by the writer set the compiler already
  computes for ownership tracking.
- [ ] Immutable-by-default globals: a global that is never mutably borrowed lands in read-only memory, and is proven so
  rather than annotated so.
- [ ] Integrity check on any structure the runtime relies on but the program can reach (vtables, type descriptors,
  allocator metadata) - see "Control flow integrity" and "Allocator hardening."

## Allocator hardening

- [ ] Out-of-line metadata: no inline heap headers adjacent to user data.
- [ ] Free-list pointer obfuscation/checksumming: double-free detection via slot state bitmaps (not possible anyway?).
- [ ] Delayed reuse quarantine and randomised slot selection.
- [ ] Guard pages between size classes and around large allocations.
- [ ] Type isolated or size-class-isolated regions so a UAF can't be groomed into a type confusion.
- [ ] Guard pages around the heap.
- [ ] Address-space quarantine: never reuse a virtual address for a different type (stronger version of above, and pairs
  with MTE).
- [ ] Zero-on-free for the whole heap, not just secrets? (performance hit).
- [ ] Fallible allocation as the default API?

## Stack hardening

- [x] Stack canaries, on every function (`sspstrong`). Not the front-line defence it is in C - bounds checking, no
  pointer arithmetic and no dangling borrows already remove the overflow that smashes a return address from safe code
  - so what this actually covers is the FFI boundary, where foreign code writes into memory this side lent it, and the
    raw-pointer core of `std` the checks are themselves built from.
- [ ] Target the canary at the frames that can actually be reached: LLVM's heuristics are C-shaped proxies and neither
  fits. The basic one looks for a character buffer and finds two functions in the whole of `std`; `sspstrong` protects
  any frame with an address-taken local, which in a language whose borrows *are* addresses of locals is all of them
  (+18% of `.text` unoptimised, +4% optimised). "Calls FFI, or handles a raw pointer" is the property worth protecting.
- [x] Stack clash protection: page-by-page probing on large frame allocation, so a frame bigger than the guard page
  cannot step over it into whatever mapping follows. This is the one overflow a canary cannot see, because nothing is
  overwritten on the way past - the frame is simply never touched where the guard is. Costs one write per page of a
  frame, and so costs nothing at all for the frames that fit in one.
- [ ] Ban `alloca`/VLA entirely or route them through a probing allocator (performance?).
- [ ] Static stack depth analysis: compute worst-case frame usage per entry point, emit max depth into the binary
  metadata, error on unbounded recursion in `!no_recursion` contexts.
- [ ] Guard pages on every thread stack, including coroutine/generator frames if and only if they are heap allocated.
- [x] Safe/split stacks: separate "safe" objects from "unsafe" ones, where unsafe objects are buffers and arrays, so an
  overflow of one cannot reach a return address or a spilled pointer. This is the measure the canary is a sampling
  approximation of - a canary notices a smashed return address after the fact, and only if the overflow crossed the one
  word it watches, whereas a split frame has no return address in front of the buffer to reach. Which side an object
  lands on is decided per object by LLVM's analysis of its uses, not by its type: on this codebase 43 of 142 functions
  end up with an unsafe frame at all, and the rest keep the single frame they had.
- [ ] Stack variable re-ordering: arrays below scalars, randomised ordering per build, alongside function randomisation.

**Implementation notes**

- Stack protection is done with `sspstrong` not `ssp`, on all functions.
- Probing is the inline sequence (`probe-stack`=`inline-asm`), not a call out to `__probestack`, so there is nothing
  extra to link and the probe cannot itself be the thing that overflows. Set as a module flag as well as a function
  attribute, so a frame a later pass creates by cloning or outlining is probed too.
- The split stack's unsafe half is a per-thread mapping with a guard page below it, and it is `sppc` that provides it:
  `sppc_unsafe_stack_up` maps one and publishes its top in `__safestack_unsafe_stack_ptr`, the emitted runtime start-up
  calls it for the main thread, and `_sppc_thread_entry` calls it for every other thread before the s++ callable runs.
  The variable is `initial-exec` thread-local, which is what makes reading it one instruction in a prologue, and also
  why the runtime has to be linked normally rather than `dlopen`ed.
- The three measures compose rather than overlap: LLVM moves the canary's own slot onto the unsafe stack, where it goes
  on guarding the objects that are still adjacent to each other, and the probe walks whichever frame is being claimed.
- Split stacks are not yet sound across the green-thread runtime: `gt_switch` swaps the safe stack but not the unsafe
  stack pointer, so two tasks on one OS thread would interleave on one unsafe stack. Latent rather than live, because
  `async` is not wired up to `sppc_async` yet - but `gt_ctx` needs to carry the unsafe pointer, and each task needs an
  unsafe stack of its own, before it is.

## Control flow integrity

- [ ] Fine-grained forward-edge CFI on the full type signature, not a coarse "is a function pointer" check.
- [ ] Backward-edge protection via shadow stacks, hardware-backed where available.
- [ ] Return address signing even where PAC isn't available, via a per-frame XOR cookie (weak but a fallback).
- [ ] Straight-line speculation hardening on returns and indirect branches; like `-mharden-sls`.
- [ ] Jump table bounds hardening, and placing tables in `.rodata` under full RELRO.
- [ ] Vtable/witness-table integrity: if vtables are used, trait object dispatch tables sealed in read-only memory, with
  a validity check on the table pointer itself.
- [ ] Gadget-aware codegen: instruction selection that avoids emitting useful unaligned gadget bytes, and constant
  blinding for attacker-influenced immediates.
- [ ] Never emit RWX (if a JIT is added, dual-map W^X with separate writable and executable views).

## Hardware-assisted mitigations

Each of these is a target capability, so each needs the fallback policy from "Build profiles."

- [ ] Indirect branch tracking: landing pads on every indirect branch target (CET IBT, AArch64 BTI).
- [ ] Shadow stacks in hardware where the target provides them (CET SS), falling back to the software shadow stack.
- [ ] Pointer authentication: signing pointers cryptographically, so a corrupted pointer fails authentication before it
  is used.
- [ ] Memory tagging / MTE: assign tags to memory allocations and pointers, and check for matching tags on access.
  Catches both linear overflow and use-after-free, and pairs with the allocator's address-space quarantine.
- [ ] Report the tag/PAC/CFI failure as a controlled abort with no attacker-visible detail, not as a raw signal.

## Speculative execution

- [ ] Speculative load hardening / index masking on bounds-checked accesses - otherwise bounds checks can be bypassed
  under speculation.
- [ ] "Retpolines" or eIBRS for indirect branches, selectable per target.
- [ ] Speculative store bypass barriers at security boundaries.
- [ ] Speculation barriers on the failure path of every validating FFI wrapper, since that is where non-trusted values
  first enter.

## Secrets and constant-time code

- [ ] A `secret` type qualifier that forbids branching on, indexing by, and variable-time operations over the value,
  statically checked.
- [ ] Constant-time comparison as the only comparison available on `secret`, so an early exit `==` cannot be written by
  accident.
- [ ] Secrets are non-copyable and zeroed on drop, and cannot reach `debug`/`format` output, logs, or a serialiser.
- [ ] `mlock` the pages holding secrets, and suppress core dumps for a process that holds any.
- [ ] Randomness comes from the OS CSPRNG only, with reseeding after `fork`, and no userspace PRNG is offered for
  security use.

## Entropy and layout randomisation

- [ ] Address space layout randomisation.
- [ ] Maximum entropy PIE: high entropy VA on 64 bits, no low-entropy fallback.
- [ ] Immutable function layouts, with function order randomised per build.
- [ ] Basic block ordering randomisation?
- [ ] Randomised hash seeds per process for all default hash maps.
- [ ] Build seed recorded in the binary so randomisation and reproducible builds coexist: same seed => same output.
- [-] Randomly order fields: loses optimisation. An opt-in annotation could still offer it per type.

## Information leak prevention

- [ ] No pointer values in default `debug`/`format` output.
- [ ] Backtrace and panic message redaction in release builds (no addresses, source paths).
- [ ] Symbol stripping with a separate debug object, and no build-path strings in the binary.
- [ ] Core dump suppression for processes holding `secret` values, plus `mlock` on those pages.
- [ ] Disable ptrace attach (`PR_SET_DUMPABLE`) as an opt-in build attribute?
- [ ] Uninitialised padding never reaches a write syscall, a socket, or a serialiser - see "Initialisation and residue."

## Failure, panic and abort policy

- [ ] No unwinding at all: a failed check aborts, so there is no exception-safety hole and no partially destroyed state
  to reason about.
- [ ] Abort is a single, auditable path: a fixed message, no allocation, no re-entrant user code, and an immediate
  `_exit` so an abort handler cannot be used as a gadget.
- [ ] Every safety check has the same failure mode in every profile, so a hardened build and a release build agree on
  which programs abort.
- [ ] Results are must-use: discarding a `Res`/`Opt` is a compile-time error, which stops the ignored-error class of bug
  at the source.

## Linking and loading

- [ ] RELRO/NX/ASLR.
- [ ] Read-only relocations: full RELRO with `BIND_NOW`, mandatory PIE,
  `-z noexecstack -z separate-code -z defs -z nodlopen -z now`.
- [ ] No text relocations ever.
- [ ] Default hidden symbol visibility, with explicit export lists for libraries.
- [ ] `mimmutable`/`mprotect` lockdown after startup - mark segments permanently non-writable where the OS supports it,
  which is what makes function layouts immutable in practice.
- [ ] MDWE (`PR_SET_MDWE`) to deny runtime W->X transitions process-wide.

## Process-level policy emission

- [ ] Seccomp-BPF filter generation from the statically reachable syscall graph: nearly free and strong.
- [ ] Landlock/pledge/unveil policy generation from declared capabilities.
- [ ] Capability declarations in the type system? For another time.
- [ ] Environment is not trusted under `AT_SECURE`: env-driven features (allocator tuning, logging, tracing) are
  disabled outright in a setuid/setgid process rather than sanitised.

## Standard library hardening

- [ ] Fortified functions: the s++ equivalent of `_FORTIFY_SOURCE` falls out of buffer types carrying their own length,
  so the fortification work is confined to the C shims that do not.
- [ ] Fortified binaries: the build fails if a linked C dependency was built without fortification, rather than silently
  inheriting an unfortified `memcpy`.
- [ ] No unchecked C string handling anywhere in `std`: no `strcpy`-shaped API is exposed, and `Str` is never assumed
  NUL-terminated across a boundary.
- [ ] Filesystem APIs are handle-relative (`openat` and friends) so a path cannot be swapped between the check and the
  use.
- [ ] Parsers in `std` (paths, numbers, formats) are fuzzed as a release gate - see "Verification and testing builds."

## Compile-time evaluation and unsafe discipline

- [ ] Machine-checkable safety contracts on unsafe functions.
- [ ] Sandboxed build scripts and macros - no ambient fs/net access during compilation?
- [ ] `cmp` evaluation is pure and bounded: no I/O, no ambient state, and a step/recursion budget so a dependency cannot
  hang or exhaust the compiler.
- [ ] Every unsafe block is attributable: the build emits the full list of unsafe sites and their contracts, so an audit
  reads a manifest rather than grepping.

## FFI

- [ ] Abort on unwinding across the boundary, in both directions.
- [ ] Validating wrappers on entry: discriminant validity, bool is 0/1, char is a valid scalar, pointers non-null and
  aligned, etc.
- [ ] Tainted pointer types: foreign pointers are a distinct type requiring explicit validation before use as a native
  borrow.
- [ ] Struct layout verification against the C header at build time (also function stubs?). Can we inspect lib/dll?
- [ ] Sandboxed FFI option - RLBox-style, compile the C dependency to WASM and run it in-process with a boundary that
  assumes it is hostile. The only FFI story that actually preserves the safety guarantees.
- [ ] CFI/shadow-stack interop policy - how to manage foreign code calling back in without landing pads.
- [ ] Ownership across the boundary is declared, not assumed: who frees what is part of the extern signature, and a
  foreign allocation can only be released by the allocator that produced it.

## Concurrency

- [ ] Enforce the `thread_hazard`/`ThreadSafe` auto types (maybe for async too?).
- [ ] Static lock ordering analysis for deadlock detection.
- [ ] No "relaxed-by-default" atomics: sequential consistency is the default.
- [ ] Coroutine cancellation safety.
- [ ] Fork safety: state that cannot survive a `fork` (locks, thread state, PRNG) is either reset in the child or
  documented as unusable there.
- [ ] Shared state is reachable only through the exclusivity rules, so there is no interior-mutability escape hatch that
  opts out of the data-race guarantee.

## Verification and testing builds

- [ ] Contracts: pre/post-conditions and invariants, compiled to runtime checks in hardened builds, discharged by SMT in
  verification builds, erased in fast builds.
- [ ] Ghost/proof code that is type checked and erased.
- [ ] An interpreter (MIRI-equivalent) for detecting UB? There shouldn't be any anyway. Leave for now.
- [ ] Sanitizer integration: ASan, MSan, TSan, UBSan, plus a hardened profile that enables everything.
- [ ] Build in structure-aware fuzzing?
- [ ] A negative test suite for the guarantees themselves: every rule above has a program that must fail to compile, so
  a regression in the checker is caught rather than assumed impossible.

## Supply chain

- [ ] Digital signatures: sign binaries, and sign packages/libraries on GitHub.
- [ ] SBOM emission and SLSA provenance attestations.
- [ ] Reproducible builds given a recorded seed.
- [ ] Transparency log publication as well as detached signatures.
- [ ] Hash-pinned lockfiles with a separate audit-state file.
- [ ] No code execution on dependency fetch or install; a dependency that needs a build step declares it, and it runs
  under the sandbox from "Compile-time evaluation and unsafe discipline."
- [ ] Bootstrap-capable compiler with diverse double compiling as a release check?
