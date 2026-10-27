---
name: spp-stderr-behind-progress-bar
description: "Compiler stderr is overwritten by the progress bar's \\\\r, so grep only after tr '\\\\r' '\\\\n' - otherwise instrumentation looks like it never ran"
---

Anything the compiler writes to stderr while a stage progress bar is drawing lands on the same terminal line as the bar, so a plain `grep '^MARKER'` over captured output finds nothing even though the line is there. Always pipe through `tr '\r' '\n' | sed 's/\x1b\[[0-9;]*m//g'` first, and grep for the marker without anchoring it to the start of the line.

**Why:** On 2026-09-02 a temporary `std::cerr` probe in `generic_inference.cpp` appeared to produce zero output across four runs. That was read as "my new code never executes", which pointed the investigation at the wrong file entirely and cost several full suite runs. The line had been printed every time, hidden behind the bar; `grep -a "ZZ"` after `tr` found it immediately and named the culprit in one go.

**How to apply:** For any print-instrumentation run, capture to a file and read it as `tr '\r' '\n' < log | sed 's/\x1b\[[0-9;]*m//g' | grep -a MARKER`. Treat "no output from my probe" as unproven until checked that way. Also note that `spp test -l std` compiles `project/src` too, so a broken file there aborts the whole std suite before a single test runs - see [[build-workflow]].
