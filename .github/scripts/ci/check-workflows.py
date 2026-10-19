#!/usr/bin/env python3
"""Hold every workflow and composite action to the pipeline's security rules (docs/ci-pipeline-refactor.md).

Run from the repository root; prints one ::error per finding and exits non-zero if there are any.
"""

from __future__ import annotations

import re
import sys
from pathlib import Path

import yaml

sys.path.insert(0, str(Path(__file__).resolve().parent.parent / "lib"))
import pins  # noqa: E402

WORKFLOWS = Path(".github/workflows")
ACTIONS = Path(".github/actions")

PINNED = re.compile(r"^[A-Za-z0-9_.-]+/[A-Za-z0-9_./-]+@[0-9a-f]{40}$")
HARDEN = "step-security/harden-runner@"
CHECKOUT = "actions/checkout@"
DOWNLOAD = "actions/download-artifact@"
IMAGE_IN_TEXT = re.compile(r"\b(?:ubuntu|macos|windows)-(?:latest|\d+(?:\.\d+)*)(?:-arm(?:64)?)?\b")
LINE = "__line__"

findings = 0


class LineLoader(yaml.SafeLoader):
    """Record the line every mapping starts on, so a finding can point at it."""


def _construct_mapping(loader: LineLoader, node: yaml.MappingNode) -> dict:
    mapping = loader.construct_mapping(node, deep=True)
    mapping[LINE] = node.start_mark.line + 1
    return mapping


LineLoader.add_constructor(yaml.resolver.BaseResolver.DEFAULT_MAPPING_TAG, _construct_mapping)


def error(path: Path, node: dict | None, message: str) -> None:
    global findings
    findings += 1
    line = node.get(LINE, 1) if isinstance(node, dict) else 1
    print(f"::error file={path},line={line}::{message}")


def flat_steps(steps: list) -> list[dict]:
    """Steps in order, with the members of a `parallel:` group in place of the group."""
    out: list[dict] = []
    for step in steps or []:
        if isinstance(step, dict) and isinstance(step.get("parallel"), list):
            out.extend(flat_steps(step["parallel"]))
        elif isinstance(step, dict):
            out.append(step)
    return out


def check_step(path: Path, where: str, step: dict) -> None:
    uses = step.get("uses")
    if not isinstance(uses, str) or uses.startswith("./"):
        return
    with_ = step.get("with") or {}

    if not PINNED.match(uses):
        error(path, step, f"{where}: `{uses}` is not pinned to a full commit")
    if uses.startswith(CHECKOUT) and with_.get("persist-credentials") is not False:
        error(path, step, f"{where}: checkout without `persist-credentials: false` leaves the token in .git/config")
    if uses.startswith(DOWNLOAD):
        for key in ("run-id", "github-token"):
            if key in with_:
                error(path, step, f"{where}: download-artifact with `{key}` can fetch another run's artifact")


def check_workflow(path: Path) -> None:
    data = yaml.load(path.read_text(), Loader=LineLoader)  # noqa: S506 - LineLoader is a SafeLoader
    # The loader adds the line marker to every mapping, `{}` included.
    permissions = data.get("permissions")
    if not isinstance(permissions, dict) or set(permissions) - {LINE}:
        error(path, data, "workflow-level `permissions` must be `{}`; each job grants its own")

    for name, job in (data.get("jobs") or {}).items():
        if name == LINE or not isinstance(job, dict):
            continue
        where = f"job `{name}`"
        if "permissions" not in job:
            error(path, job, f"{where} declares no `permissions`")
        if "uses" in job:
            continue

        if "timeout-minutes" not in job:
            error(path, job, f"{where} has no `timeout-minutes`")
        steps = flat_steps(job.get("steps"))
        first = steps[0].get("uses", "") if steps else ""
        if not first.startswith(HARDEN):
            error(path, job, f"{where}: harden-runner must be the first step")
        for step in steps:
            check_step(path, where, step)


def check_action(path: Path) -> None:
    data = yaml.load(path.read_text(), Loader=LineLoader)  # noqa: S506 - LineLoader is a SafeLoader
    for step in flat_steps((data.get("runs") or {}).get("steps")):
        check_step(path, f"action `{path.parent.name}`", step)


def check_images(paths: list[Path]) -> None:
    allowed = set(pins.runners(pins.load()).values())
    if not allowed:
        pins.fail("[runner] is empty, so there is nothing to check the workflows against")
    global findings
    for path in paths:
        for number, line in enumerate(path.read_text().splitlines(), start=1):
            for found in IMAGE_IN_TEXT.findall(line):
                if found not in allowed:
                    print(f"::error file={path},line={number}::{found} is not in [runner]: {line.strip()}")
                    findings += 1


def main() -> int:
    workflows = sorted(WORKFLOWS.glob("*.y*ml"))
    actions = sorted(ACTIONS.glob("*/action.y*ml"))
    for path in workflows:
        check_workflow(path)
    for path in actions:
        check_action(path)
    check_images(workflows + actions)
    if findings:
        print(f"{findings} workflow policy finding(s)", file=sys.stderr)
    return 1 if findings else 0


if __name__ == "__main__":
    sys.exit(main())
