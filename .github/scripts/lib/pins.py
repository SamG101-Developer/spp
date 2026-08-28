#!/usr/bin/env python3
"""Single reader for .github/dependencies.toml.

Every version, commit and digest the pipeline uses comes through here, so the rules about what a pin may look like are
stated once instead of once per consumer.

Subcommands:
    env         NAME=value lines for GITHUB_ENV (pins and install prefixes)
    libraries   one tab-separated record per small CMake library: name, repo, commit, space-joined cmake flags
    runners     one tab-separated record per canonical runner image
    get NAME    print one exported value, for scripts that want a single pin without loading the lot
    set PATH V  rewrite one value in place, preserving comments and layout; used by refresh-pins.sh
    check       validate the file and print what it exports
"""

from __future__ import annotations

import argparse
import os
import re
import sys
from pathlib import Path

try:
    import tomllib
except ModuleNotFoundError:  # pragma: no cover - python < 3.11
    sys.exit("error: pins.py needs Python 3.11 or newer for tomllib")

MANIFEST = Path(".github/dependencies.toml")

# Keys inside a [pin.*] table that become environment variables. Anything else in such a table is metadata for the
# install or refresh logic and is deliberately not exported.
EXPORTED = re.compile(r"^(version|commit|tag|asset|developer-dir(-[a-z0-9-]+)?|sha256(-[a-z0-9-]+)?)$")

# A pin is a version, a tag, a commit, a digest or one release asset filename. Notably no whitespace, so nothing here
# can inject a second line into GITHUB_ENV.
VALUE = re.compile(r"^[A-Za-z0-9._:/+-]+$")

NAME = re.compile(r"^[A-Z][A-Z0-9_]*$")
HEX40 = re.compile(r"^[0-9a-f]{40}$")
HEX64 = re.compile(r"^[0-9a-f]{64}$")
TABLES = {"pin", "prefix", "library", "runner"}

# Runner images are not exported: `runs-on` cannot read the env context, and RUNNER_* is GitHub's own namespace. The
# manifest is the canonical list and check_workflows() enforces that the YAML agrees with it.
IMAGE = re.compile(r"^(ubuntu|macos|windows)-[a-z0-9.-]+$")
IMAGE_IN_TEXT = re.compile(r"\b(?:ubuntu|macos|windows)-(?:latest|\d+(?:\.\d+)*)(?:-arm(?:64)?)?\b")
WORKFLOW_DIRS = (Path(".github/workflows"), Path(".github/actions"))


def fail(message: str) -> None:
    print(f"::error::{message}", file=sys.stderr)
    sys.exit(1)


def env_name(table: str, key: str) -> str:
    return f"{table}_{key}".upper().replace("-", "_")


def load() -> dict:
    if not MANIFEST.is_file():
        fail(f"{MANIFEST} not found; run from the repository root")
    with MANIFEST.open("rb") as handle:
        return tomllib.load(handle)
    return None


def exports(data: dict) -> dict[str, str]:
    """Every NAME=value pair the manifest publishes, validated on the way out."""
    unknown = set(data) - TABLES
    if unknown:
        fail(f"unexpected top-level table(s) in {MANIFEST}: {', '.join(sorted(unknown))}")

    out: dict[str, str] = {}
    home = os.environ.get("HOME") or str(Path.home())

    for pin, table in data.get("pin", {}).items():
        if not isinstance(table, dict):
            fail(f"[pin.{pin}] must be a table")
        for key, value in table.items():
            if not EXPORTED.match(key):
                continue
            if not isinstance(value, str):
                fail(f"pin.{pin}.{key} must be a string")
            if not VALUE.match(value):
                fail(f"pin.{pin}.{key} is not a version, tag, commit, digest or asset name: {value!r}")
            if key.startswith("sha256") and not HEX64.match(value):
                fail(f"pin.{pin}.{key} is not a sha256 digest: {value!r}")
            if key == "commit" and not HEX40.match(value):
                fail(f"pin.{pin}.{key} is not a full 40-character commit: {value!r}")
            out[env_name(pin, key)] = value

    # Prefixes carry {home} rather than $HOME because they land in GITHUB_ENV, where a shell variable would never be
    # expanded by anything.
    for key, value in data.get("prefix", {}).items():
        if not isinstance(value, str):
            fail(f"prefix.{key} must be a string")
        path = value.replace("{home}", home)
        if "\n" in path or "\r" in path:
            fail(f"prefix.{key} contains a newline")
        out[f"SPP_{key}_PREFIX".upper().replace("-", "_")] = path

    for name in out:
        if not NAME.match(name):
            fail(f"{name} is not a usable environment variable name")
    return out


def libraries(data: dict) -> list[dict]:
    seen: set[str] = set()
    for entry in data.get("library", []):
        for field in ("name", "repo", "commit"):
            if field not in entry:
                fail(f"a [[library]] entry is missing '{field}'")
        name, repo, commit = entry["name"], entry["repo"], entry["commit"]
        if name in seen:
            fail(f"[[library]] {name} is listed twice")
        seen.add(name)
        if not repo.startswith("https://") or not repo.endswith(".git"):
            fail(f"[[library]] {name} repo must be an https .git URL: {repo!r}")
        if not HEX40.match(commit):
            fail(f"[[library]] {name} commit is not a full 40-character commit: {commit!r}")
        for flag in entry.get("cmake-flags", []):
            if not isinstance(flag, str) or not flag.startswith("-"):
                fail(f"[[library]] {name} has a cmake flag that is not a flag: {flag!r}")
            if any(c.isspace() for c in flag):
                fail(f"[[library]] {name} cmake flag contains whitespace, which would split on read: {flag!r}")
    return list(data.get("library", []))


def runners(data: dict) -> dict[str, str]:
    out: dict[str, str] = {}
    for key, value in data.get("runner", {}).items():
        if not isinstance(value, str) or not IMAGE.match(value):
            fail(f"runner.{key} is not a runner image: {value!r}")
        if value.endswith("-latest"):
            fail(f"runner.{key} is a floating image: {value!r}; name the version")
        out[key] = value
    return out


def check_workflows(data: dict) -> int:
    """Every runner image named in the workflows must be one the manifest lists."""
    allowed = set(runners(data).values())
    if not allowed:
        fail("[runner] is empty, so there is nothing to check the workflows against")

    problems = 0
    for directory in WORKFLOW_DIRS:
        for path in sorted(directory.rglob("*.y*ml")):
            for number, line in enumerate(path.read_text().splitlines(), start=1):
                for found in IMAGE_IN_TEXT.findall(line):
                    if found not in allowed:
                        print(f"::error file={path},line={number}::{found} is not in [runner]: {line.strip()}")
                        problems += 1
    return problems


def cmd_runners(data: dict) -> None:
    for key, value in runners(data).items():
        print(f"{key}\t{value}")


def cmd_env(data: dict) -> None:
    for name, value in exports(data).items():
        print(f"{name}={value}")


def cmd_libraries(data: dict) -> None:
    for entry in libraries(data):
        flags = " ".join(entry.get("cmake-flags", []))
        print(f"{entry['name']}\t{entry['repo']}\t{entry['commit']}\t{flags}")


def cmd_get(data: dict, name: str) -> None:
    """Accept either an exported name (GCC_VERSION) or a dotted path (pin.gcc.version).

    refresh-pins.sh addresses everything by path, because that is what it writes back; other scripts think in
    environment variables. Both reach the same value.
    """
    if "." in name:
        node: object = data
        for part in name.split("."):
            if part.isdigit() and isinstance(node, list):
                node = node[int(part)]
                continue
            if isinstance(node, list):
                node = next((e for e in node if e.get("name") == part), None)
            elif isinstance(node, dict):
                node = node.get(part)
            else:
                node = None
            if node is None:
                fail(f"{name} does not resolve in {MANIFEST}")
        if not isinstance(node, str):
            fail(f"{name} is not a single value in {MANIFEST}")
        print(node)
        return

    values = exports(data)
    if name not in values:
        fail(f"{name} is not exported by {MANIFEST}")
    print(values[name])


def cmd_check(data: dict, quiet: bool) -> None:
    values = exports(data)
    libs = libraries(data)
    images = runners(data)
    if not quiet:
        for name, value in sorted(values.items()):
            print(f"  {name}={value}")
        for key, value in images.items():
            print(f"  runner.{key}={value}")
    problems = check_workflows(data)
    summary = f"{MANIFEST}: {len(values)} pins, {len(libs)} libraries, {len(images)} runner images"
    if problems:
        fail(f"{summary}; {problems} workflow reference(s) disagree with [runner]")
    if not quiet:
        print(summary)


def cmd_set(path: str, value: str) -> None:
    """Rewrite one value, leaving comments and ordering alone.

    tomllib is read-only and the formatting-preserving writers are third-party, so this edits the text the way
    refresh-pins.sh always has: anchored on the table header, then on the key inside it.
    """
    if not VALUE.match(value):
        fail(f"refusing to write {path}={value!r}: not a version, tag, commit, digest or asset name")

    parts = path.split(".")
    header = ""
    key = ""
    if len(parts) == 3 and parts[0] == "pin":
        header, key = f"[pin.{parts[1]}]", parts[2]
    elif len(parts) == 3 and parts[0] == "library":
        header, key = f'name = "{parts[1]}"', parts[2]
    else:
        fail(f"unsupported path {path!r}; expected pin.<name>.<key> or library.<name>.<key>")

    text = MANIFEST.read_text()
    lines = text.splitlines(keepends=True)

    start = next((i for i, line in enumerate(lines) if line.strip() == header), None)
    if start is None:
        fail(f"{header} not found in {MANIFEST}")

    # The table runs to the next table header, so a key is only ever rewritten inside the entry that owns it.
    end = len(lines)
    for i in range(start + 1, len(lines)):
        if lines[i].lstrip().startswith("["):
            end = i
            break

    pattern = re.compile(rf'^(\s*{re.escape(key)}\s*=\s*)"[^"]*"(\s*)$')
    for i in range(start, end):
        match = pattern.match(lines[i])
        if match:
            old = lines[i]
            lines[i] = f'{match.group(1)}"{value}"{match.group(2)}'
            if lines[i] != old:
                MANIFEST.write_text("".join(lines))
            return
    fail(f"{header} has no '{key}' key to rewrite")


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    sub = parser.add_subparsers(dest="command", required=True)
    sub.add_parser("env")
    sub.add_parser("libraries")
    sub.add_parser("runners")
    check = sub.add_parser("check")
    check.add_argument("-q", "--quiet", action="store_true")
    get = sub.add_parser("get")
    get.add_argument("name")
    setter = sub.add_parser("set")
    setter.add_argument("path")
    setter.add_argument("value")

    args = parser.parse_args()
    if args.command == "set":
        cmd_set(args.path, args.value)
        return

    data = load()
    if args.command == "env":
        cmd_env(data)
    elif args.command == "libraries":
        cmd_libraries(data)
    elif args.command == "runners":
        cmd_runners(data)
    elif args.command == "get":
        cmd_get(data, args.name)
    else:
        cmd_check(data, args.quiet)


if __name__ == "__main__":
    main()
