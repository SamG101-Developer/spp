---
name: ruff-format-targets-python-314
description: "ruff format follows requires-python >=3.14 and strips except-tuple parens (PEP 758); the result is a SyntaxError on 3.13"
updated: 2026-11-02
---

`pyproject.toml` says `requires-python = ">=3.14"`, and the pre-commit `ruff-format` hook formats to it. One rewrite
that follows is `except (OSError, IndexError):` becoming `except OSError, IndexError:` (PEP 758, new in 3.14). CI pins
3.14 (`[pin.python]` in `.github/dependencies.toml`), so it runs there, but a machine still on 3.13 (WSL's Debian
python3 was 3.13.5 on 2026-11-02) fails to even parse the file: `SyntaxError: multiple exception types must be
parenthesized`.

**Why:** found when `benchmarks/run.py` stopped starting locally after a `ruff format`, with no lint error to say so.

**How to apply:** a script meant to be run by hand as well as in CI (`benchmarks/`, `tests/codegen/*.py`) avoids
catching more than one exception type in one `except`; restructure, or catch a common base. After formatting, check
with `python3 -m py_compile` on the local interpreter.
