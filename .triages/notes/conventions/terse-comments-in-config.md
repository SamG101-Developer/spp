---
name: terse-comments-in-config
description: "Do not write comments in config files at all, and keep script/workflow comments brief — the user strips verbose rationale"
updated: 2026-08-26
---

Do not add explanatory comments to config files. On 2026-08-18 the user stripped every comment I wrote from
`pyproject.toml`, `.cppcheck-suppressions` and `.github/codeql-config.yaml`, then said explicitly: "keep config
comments thinner, dont add them to config files."

They also trimmed the multi-paragraph header comment in `.github/scripts/scan/cppcheck.sh` down to one sentence, so
this extends to scripts: a short "what it does" header is wanted, a long "why, and what I considered" essay is not.

`CMakeLists.txt` counts as a config file even though it is dense with the user's own prose. On 2026-08-26 they deleted
both multi-line rationale comments I added for the pthreads4w wiring — and the pre-existing paragraph above the WIN32
`find_package` block along with them — while keeping the code verbatim. The surviving comments there are all single-line
section headers ("# Find dependencies."). Match those, not the paragraphs.

**Why:** they want config files to read as data, and treat long rationale comments as noise rather than documentation.

**How to apply:** config files (TOML/YAML/ini/suppression lists) get no comments unless the user asks. Scripts and
workflows get brief comments only — one line for a non-obvious step, not a paragraph. Put the reasoning in the chat
response instead, where it can be read once and discarded. See [[cpp-linters-local-only]].
