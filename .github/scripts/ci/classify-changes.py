#!/usr/bin/env python3
"""
Decide, per gated job, whether a diff can move that job's result, from the globs in .github/change-gates.toml.
  - classify-changes.py           classify BASE_SHA..HEAD_SHA and write one output per gate
  - classify-changes.py check     validate the gates file against the tree and the detect-changes wiring
"""

from __future__ import annotations

import fnmatch
import os
import re
import subprocess
import sys
import tomllib
from pathlib import Path

GATES = Path(".github/change-gates.toml")
ACTION = Path(".github/actions/detect-changes/action.yaml")
CALLERS = (Path(".github/workflows/pr.yaml"), Path(".github/workflows/post_merge.yaml"))
NO_BASE = {"", "0" * 40}

# A pinned `uses:`, as a step key or a list item, with the
# version comment dependabot keeps beside it.
PIN = re.compile(r"^[+-]\s*(-\s+)?uses:\s*\S+@[0-9a-fA-F]{40}(\s+#.*)?$")


class Gate:
    """
    A gate is a set of files that can be changed to trigger
    a job.
    """

    name: str  # The name of the gate.
    own: list[str]  # The files that the gate owns.
    include: list[str]  # The sets that the gate includes.
    paths: list[str]  # The combined list of files and sets.
    pinnable: bool  # If the gate can be skipped when there are no pins.
    follows_code: bool  # If the gate follows the code gate.

    def __init__(self, name: str, table: dict, sets: dict[str, list[str]]):
        # Check the table for unknown keys, error if there are
        # any.
        unknown = set(table) - {"paths", "include", "pinnable", "code"}
        if unknown:
            fail(f"[gate.{name}] has unexpected key(s): {', '.join(sorted(unknown))}")

        # Assign the name, own files and the include set(s).
        self.name = name
        self.own = list(table.get("paths", []))
        self.include = list(table.get("include", []))

        # Check that the include set(s) exist from the available
        # items under "[sets]".
        for set_name in self.include:
            if set_name not in sets:
                fail(f"[gate.{name}] includes unknown set '{set_name}'")

        # Create the combined list of files and sets, and tag the
        # pinnable and follows_code flags.
        self.paths = self.own + [p for s in self.include for p in sets[s]]
        self.pinnable = bool(table.get("pinnable", False))
        self.follows_code = bool(table.get("code", True))


def fail(message: str) -> None:
    """
    Uniform error reporting.
    :param message: The error message.
    """

    # Print the error into stderr, and exit with a non-zero
    # status.
    print(f"::error::{message}", file=sys.stderr)
    sys.exit(1)


def load() -> tuple[dict, list[Gate]]:
    """
    Load the gate file and the sets table. Report on any
    unknown keys.
    :return: The data and the gates.
    """

    # Open the gate file and load the toml into a dict.
    with GATES.open("rb") as handle:
        data = tomllib.load(handle)

    # Filter bad keys and report if there are any.
    unknown = set(data) - {"code", "pins", "sets", "gate"}
    if unknown:
        fail(f"unexpected table(s) in {GATES}: {', '.join(sorted(unknown))}")

    # Convert the data into a list of gates.
    sets = data.get("sets", {})
    gates = [Gate(name, table, sets) for name, table in data.get("gate", {}).items()]
    return data, gates


def matches(path: str, patterns: list[str]) -> bool:
    """
    Use the Unix filename pattern matching rules to see if a
    path matches any of the patterns (handles "/" and "*").
    :param path: The path to match.
    :param patterns: A list of patterns to match.
    :return: IUO True if the path matches any of the patterns.
    """

    return any(fnmatch.fnmatchcase(path, pattern) for pattern in patterns)


def git(*args: str) -> str:
    """
    The unified git command runner.
    :param args: Arguments to pass to git.
    :return: The output of the command.
    """

    # Paths and diff text need not be UTF-8; surrogateescape round-trips any byte instead of raising.
    out = subprocess.run(["git", *args], capture_output=True, check=True).stdout
    return out.decode("utf-8", "surrogateescape")


def git_paths(*args: str) -> list[str]:
    """
    Run a git command that lists paths, NUL-separated so no path is quoted or split.
    :param args: Arguments to pass to git, without -z.
    :return: The listed paths.
    """

    return [path for path in git(*args, "-z").split("\0") if path]


def emit(name: str, value: bool) -> None:
    """
    The unified output writer for the CI pipeline.
    :param name: The name of the output.
    :param value: The value of the output.
    """

    text = "true" if value else "false"
    with open(os.environ.get("GITHUB_OUTPUT") or "/dev/stdout", "a") as out:
        out.write(f"{name}={text}\n")
    print(f"  {name:<14} {text}")


def base_usable(base: str) -> bool:
    """
    Fallback to true if the base is not a valid commit. This
    then assumes everything has changed.
    :param base: The base commit.
    :return: If the base is usable.
    """

    if base in NO_BASE:
        return False
    return subprocess.run(["git", "cat-file", "-e", f"{base}^{{commit}}"], capture_output=True).returncode == 0


def pins_only(base: str, head: str, changed: list[str], scope: list[str]) -> bool:
    """
    Determine if the current set of changes are purely pinned
    version updates. This is used to determine if the CI pipeline
    should run the "ci_pins_only" job.
    :param base: The base commit.
    :param head: The head commit.
    :param changed: The changed files.
    :param scope: The scope of the pins.
    :return: If the changed files are pins only.
    """

    # If there are no changed files, or none of them match the
    # scope, then this is not a pins-only change.
    if not changed or not all(matches(path, scope) for path in changed):
        return False

    # -U0 leaves the file headers, which start with the same +/-
    # as a change, and the changes themselves.
    for line in git("diff", "-U0", base, head, "--", *changed).splitlines():
        if line.startswith(("+++ ", "--- ")) or not line.startswith(("+", "-")):
            continue
        if not PIN.match(line):
            print(f"not a pin bump: {line}")
            return False
    return True


def classify() -> None:
    """
    The function that the CI pipeline calls, which checks the
    diffs between the base sha and the head sha (current changes).
    """

    # Load the data and gates from the file and get the hashes
    # from the environment set by the CI pipeline.
    data, gates = load()
    base = os.environ.get("BASE_SHA", "")
    head = os.environ.get("HEAD_SHA", "")

    # If the base failed to resolve, then don't fail, but assume
    # everything changed - all jobs will run.
    if not base_usable(base):
        print("no usable base commit; assuming everything changed")
        emit("code", True)
        emit("ci_pins_only", False)
        for gate in gates:
            emit(gate.name, True)
        return

    # Use the git command to get the list of changed files. This
    # is a list of paths relative to the root of the repository.
    changed = git_paths("diff", "--name-only", base, head)
    print(f"changed {len(changed)} file(s)")

    # Get the "code" table. This detects whether the code gate
    # is triggered. If the code gate is triggered, then everything
    # else is triggered. If the code gate is not triggered, then
    # the code gate is triggered if any of "code.also" matches
    # any of the changed files.
    code_table = data.get("code", {})
    code = any(not matches(path, code_table.get("skip", [])) for path in changed) or any(
        matches(path, code_table.get("also", [])) for path in changed
    )
    pins = pins_only(base, head, changed, data.get("pins", {}).get("paths", []))

    emit("code", code)
    emit("ci_pins_only", pins)

    # Based on the paths that a gate contains, determine if the
    # changed paths match any of the gates.
    for gate in gates:
        touched = any(matches(path, gate.paths) for path in changed)
        if not gate.follows_code:
            emit(gate.name, touched)
        elif pins and not gate.pinnable:
            emit(gate.name, False)
        else:
            emit(gate.name, code or touched)


def check() -> None:
    """
    The pre-commit version that checks the integrity and correctness
    of the gate file.
    """

    import yaml

    # Load the data and gates from the file and prepare a "problems"
    # list for potential with the gate file.
    data, gates = load()
    problems: list[str] = []

    # Tracked files only, since a commit carries nothing else. Every
    # pattern list below is checked against them.
    files = git_paths("ls-files", "--cached")
    named = [("code.also", data.get("code", {}).get("also", [])), ("pins.paths", data.get("pins", {}).get("paths", []))]
    named += [(f"sets.{k}", v) for k, v in data.get("sets", {}).items()]
    named += [(f"gate.{g.name}", g.own) for g in gates]

    # Check for invalid files in the patterns, by comparing them to
    # the available files.
    for where, patterns in named:
        for pattern in patterns:
            if not any(fnmatch.fnmatchcase(f, pattern) for f in files):
                problems.append(f"{where}: '{pattern}' matches no file in the repository")

    # Group the gates as a set with some fixed options too. Load the
    # outputs from the action.yaml file and compare them to the
    # declared outputs.
    produced = {"code", "ci_pins_only"} | {g.name for g in gates}
    action = yaml.safe_load(ACTION.read_text())
    declared = set(action.get("outputs", {}))
    for name in sorted(produced - declared):
        problems.append(f"{ACTION} declares no output for gate '{name}'")
    for name in sorted(declared - produced):
        problems.append(f"{ACTION} declares output '{name}', which no gate produces")

    for caller in CALLERS:
        outputs = yaml.safe_load(caller.read_text())["jobs"]["changes"].get("outputs", {})
        for key, value in outputs.items():
            ref = re.search(r"steps\.filter\.outputs\.([A-Za-z0-9_]+)", str(value))
            if not ref or ref.group(1) not in declared:
                problems.append(
                    f"{caller}: changes.outputs.{key} reads '{value}', which detect-changes does not output"
                )

    for problem in problems:
        print(f"::error file={GATES}::{problem}")
    if problems:
        sys.exit(1)


if __name__ == "__main__":
    # Whether this is a check (add the "check" flag):
    if sys.argv[1:] == ["check"]:
        check()

    # Or a classification (no flags):
    elif not sys.argv[1:]:
        classify()
    else:
        fail("usage: classify-changes.py [check]")
