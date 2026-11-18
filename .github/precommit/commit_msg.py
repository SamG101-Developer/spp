#!/usr/bin/env python3

"""Check a commit subject against the PR title rule in pr_title.yaml: "type(scope): description"."""

from __future__ import annotations

import re
import sys
from pathlib import Path

import yaml

WORKFLOW = Path(__file__).parents[1] / "workflows" / "pr_title.yaml"
EXEMPT = re.compile(r'^(Merge |Revert "|fixup! |squash! |amend! )')


def load_rule() -> tuple[list[str], re.Pattern[str]]:
    """The types list and subject pattern given to action-semantic-pull-request."""
    steps = yaml.safe_load(WORKFLOW.read_text(encoding="utf-8"))["jobs"]["pr-title"]["steps"]
    args = next(s["with"] for s in steps if "action-semantic-pull-request" in s.get("uses", ""))
    return args["types"].split(), re.compile(args["subjectPattern"])


def subject_of(path: str) -> str:
    """The first line that is not blank or a git comment."""
    for line in Path(path).read_text(encoding="utf-8").splitlines():
        if line.strip() and not line.startswith("#"):
            return line.rstrip()
    return ""


def main() -> int:
    subject = subject_of(sys.argv[1])
    if not subject or EXEMPT.match(subject):
        return 0

    types, pattern = load_rule()
    bare = re.sub(r"( \(#\d+\)| \[skip ci\])+$", "", subject)
    match = re.fullmatch(r"(?P<type>[a-z]+)(\([^()]+\))?: (?P<desc>.+)", bare)
    if match and match["type"] in types and pattern.fullmatch(match["desc"]):
        return 0

    print(f"Bad commit subject: {subject!r}", file=sys.stderr)
    print("Expected: type(optional scope): description", file=sys.stderr)
    print(f"  types: {', '.join(types)}", file=sys.stderr)
    print(f"  description: {pattern.pattern}", file=sys.stderr)
    return 1


if __name__ == "__main__":
    sys.exit(main())
