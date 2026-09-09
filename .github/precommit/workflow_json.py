#!/usr/bin/env python3
"""Check the JSON that GitHub Actions parses at expression time."""

from __future__ import annotations

import json
import re
from pathlib import Path

import yaml

# Every input the workflows hand to fromJSON by name.
JSON_INPUTS = ("variants",)

# A fromJSON over a single-quoted literal rather than a reference. YAML doubles
# an inner quote, which is how the literal carries one through.
INLINE_JSON = re.compile(r"fromJSON\(\s*'((?:[^']|'')*)'\s*\)")

WORKFLOWS = Path(".github/workflows")
ACTIONS = Path(".github/actions")


def walk(node: object) -> list[tuple[str, str]]:
    """Every JSON-input value in a parsed workflow, with the key that named it."""
    found = []
    if isinstance(node, dict):
        for key, value in node.items():
            if key in JSON_INPUTS and isinstance(value, str):
                found.append((key, value))
            found += walk(value)
    elif isinstance(node, list):
        for item in node:
            found += walk(item)
    return found


def check(path: Path) -> list[str]:
    problems = []
    text = path.read_text(encoding="utf-8")

    try:
        document = yaml.safe_load(text)
    except yaml.YAMLError as error:
        return [f"{path}: not valid YAML: {error}"]

    payloads = walk(document)
    payloads += [("fromJSON", literal.replace("''", "'")) for literal in INLINE_JSON.findall(text)]

    for key, payload in payloads:
        try:
            json.loads(payload)
        except json.JSONDecodeError as error:
            problems.append(f"{path}: {key} is not valid JSON: {error.msg} at column {error.colno}")
            problems.append(f"    {payload}")

    return problems


def main() -> int:
    files = sorted(WORKFLOWS.glob("*.y*ml")) + sorted(ACTIONS.glob("*/action.y*ml"))
    problems = [problem for path in files for problem in check(path)]

    for problem in problems:
        print(problem)

    if problems:
        print("\nGitHub parses these at expression time, so an error here is a job that never starts")
        return 1

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
