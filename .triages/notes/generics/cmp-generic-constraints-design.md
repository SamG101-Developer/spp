---
name: cmp-generic-constraints-design
description: "cmp n: U32 and n > m" predicates; symbolic args must be implied by the caller's own predicates (syntactic), never deferred to the instance; sups filter, E60 unless provably disjoint
updated: 2026-11-19
---

Design decided 2026-11-19 (not yet implemented; full write-up in `.triages/docs_open/cmp-generic-constraints.md`):

- Syntax: the `and` chain continues after a `cmp` parameter's type, `cmp n: USize and n > 0`. The rest is one Bool
  comp-time expression. It may name any parameter of its group, `Self::` constants and `cmp fun`s.
- Concrete arguments fold and check (E78, E51 on a call). Symbolic ones (inside a generic) must be implied by the
  enclosing generic's own predicates where written, the same as a generic type symbol meeting its declared
  constraints in `UnmetConstraint`. Implication is syntactic after normalisation, with no arithmetic: unsigned
  wrap-around makes naive arithmetic unsound.
- Sups: a predicate filters, never orders. Overlap is E60 unless disjoint by the decidable fragment (fold, negation
  pairs, per-term interval intersection). Everything else counts as overlapping.
- Inference is unchanged (a predicate binds nothing). Packs check per element.

**Why:** the maintainer wants constraints to hold all the way through generic chains (no C++-style late failure in an
instance), symmetric with type constraints.

**How to apply:** don't add deferral to instantiation or a "most specific sup" ordering. Widen implication or
disjointedness only soundly (intervals, then Fourier-Motzkin/Omega), never by guessing.

Related: [[pack-constraints-pairwise]], [[spp-generic-inference-is-one-way]], [[constraint-failure-on-a-call-is-e51]].
