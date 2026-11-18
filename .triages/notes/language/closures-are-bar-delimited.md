---
name: closures-are-bar-delimited
description: "Closures are |params caps ..| body (since 0.7.0); a | that starts a line never continues the line before as a bit-or"
updated: 2026-11-16
---

Closures are written `|a: S32, ..b: S32 caps c, &d| body` (with an optional `-> T` before a `{ }` body), replacing
`(...)` in 0.7.0. `||` is a closure with no parameters: the lexer gives one `TK_VERTICAL_BAR` per character, and
logical or is the `or` keyword, so there is no `||` token. `()` is now only ever the empty tuple.

**Why it is unambiguous:** closure parameters are required or variadic only, never optional, so no default
expression sits inside the bars. Types have no top-level `|` (their binary operator is `or`; a `|` inside a comp
argument is enclosed by `[...]`), patterns have none, and captures are a convention plus a name. A closure is only
tried where an operand is expected, and a bit-or `|` only after one, as in Rust. The body is greedy: `|x: U8| x | 1_u8`
has `x | 1_u8` as its body.

**The one parser rule it needed:** the binary operator lookahead (`try_bin_op` in `ParseBinaryExpression`) skips line
feeds, so a line starting with a closure after an expression statement read as `prev | x`. A `|` after a line feed
is now never a binary operator. The old `(...)` form needed the reverse guard instead: a closure's body had to start on
its parameters' line, or `let t = ()` plus the next statement parsed as one closure; that guard is gone.

**How to apply:** write closures with bars everywhere, including docs and notes. The rewrite of every existing
closure was compiler-driven: see [[syntax-rewrites-from-a-parser-dump]].
