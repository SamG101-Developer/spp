---
name: alternative-operator-keywords
description: "Logical operators are spelled and/or/not, never &&/||/!; Sonar's S3659 is ignored, clang-tidy enforces the keywords"
updated: 2026-11-12
---

C++ logical operators are written as the alternative tokens `and`, `or` and `not`, never `&&`, `||` and `!`. `&&` stays
for rvalue and forwarding references (`Str &&msg`, `auto &&[k, v]`). Preprocessor lines (`#if !defined(...)`) still use
the symbols. The keywords would also work there, but nothing enforces them, so these lines were left as they are.

SonarCloud's `cpp:S3659` ("Alternative operators should not be used") is switched off by the `altops` entry in
`sonar-project.properties`. Sonar has no rule that requires the keywords. Instead, `.clang-tidy` enables
`readability-operators-representation` with `BinaryOperators: 'and;or;not'`, which runs only locally, through
`.tools/run-clang-tidy.sh`.

**Why:** the maintainer chose the keywords as the house style (2026-11-12). They made up 829 of SonarCloud's 1,156
maintainability issues, and only 18 symbol-form operators existed, so the code was aligned to the keywords and not the
other way round.

**How to apply:** write `and`/`or`/`not` in new code, and use `not x`, never `!x`. To scan for symbol forms when the
clang build tree is stale (clang-tidy then reports mostly `clang-diagnostic-error`), use a token-level scan instead. Strip
comments, strings and raw strings first, and count a `&&` as logical only when it has a space on both sides, because the
house reference style is `T &&name`.
