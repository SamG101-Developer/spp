#!/usr/bin/env python3

from __future__ import annotations

import re
import sys
import tomllib
from pathlib import Path

RULES_PATH = Path(__file__).with_name("banned_idioms.toml")


def load_rules(path: Path) -> list[dict]:
    with path.open("rb") as f:
        data = tomllib.load(f)

    rules = []
    for raw in data.get("rule", []):
        if not raw.get("enabled", True):
            continue
        rule_id = raw["id"]
        rules.append(
            {
                "id": rule_id,
                "message": raw["message"],
                "pattern": re.compile(raw["pattern"]),
                "files": re.compile(raw["files"]) if "files" in raw else None,
                "allow": tuple(raw.get("allow", ())),
                "escape": re.compile(rf"allow\(\s*{re.escape(rule_id)}\s*\)"),
            }
        )
    return rules


def exempt(path: str, allow: tuple[str, ...]) -> bool:
    """A rule's "allow" entry names either the file itself or a directory containing it."""
    return any(path == a or path.startswith(a.rstrip("/") + "/") for a in allow)


def scan(path: str, rules: list[dict]) -> list[tuple]:
    text = Path(path).read_text(encoding="utf-8", errors="replace")
    lines = text.split("\n")

    findings = []
    for rule in rules:
        if rule["files"] is not None and not rule["files"].search(path):
            continue
        if exempt(path, rule["allow"]):
            continue
        for n, line in enumerate(lines, start=1):
            if rule["escape"].search(line):
                continue
            match = rule["pattern"].search(line)
            if match is not None:
                findings.append((n, match.start() + 1, rule, line.strip()))

    return sorted(findings, key=lambda f: (f[0], f[1]))


def main(argv: list[str]) -> int:
    rules = load_rules(RULES_PATH)
    hit_ids = set()

    for path in argv[1:]:
        posix = Path(path).as_posix()
        for line_no, col, rule, source in scan(posix, rules):
            print(f"{posix}:{line_no}:{col}: {rule['id']}: {rule['message']}")
            print(f"    {source}")
            hit_ids.add(rule["id"])

    if not hit_ids:
        return 0

    hatch = " or ".join(sorted(f"// allow({rule_id}): <reason>" for rule_id in hit_ids))
    print(f"\na use that genuinely has no replacement is exempted inline with {hatch}")
    return 1


if __name__ == "__main__":
    raise SystemExit(main(sys.argv))
