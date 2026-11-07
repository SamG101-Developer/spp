#!/usr/bin/env python3
"""Check that every part of codegen has a run-time benchmark, or a reason it has none.

The parts are every AST whose source file defines Stage11_CodeGen ("ast:<file stem without _ast>") and every module in
sources/spp/codegen ("codegen:<file stem>"). A part is covered by a benchmark folder under runtime/ast named after it,
or by a "covers" entry in any benchmark's manifest.toml. Otherwise it needs a line in exempt.txt, with its reason.

Fails on a part that is neither covered nor exempt, on a covers or exempt entry naming no part, on a part both covered
and exempt (the exemption is stale), and on an exemption with no reason. A new AST with codegen therefore arrives with
its benchmark, or says why it has none.

  coverage.py [--list]     --list also prints which benchmarks cover each part
"""

import argparse
import re
import sys
import tomllib
from pathlib import Path

HERE = Path(__file__).resolve().parent
ROOT = HERE.parent
ASTS = ROOT / "sources" / "spp" / "asts"
CODEGEN = ROOT / "sources" / "spp" / "codegen"
EXEMPT = HERE / "exempt.txt"
RUNTIME = HERE / "runtime"


def parts() -> set[str]:
    asts = {
        "ast:" + path.stem.removesuffix("_ast")
        for path in ASTS.glob("*_ast.cpp")
        if "::Stage11_CodeGen(" in path.read_text(errors="replace")
    }
    return asts | {"codegen:" + path.stem for path in CODEGEN.glob("*.cpp")}


def coverage(known: set[str]) -> dict[str, list[str]]:
    covered: dict[str, list[str]] = {}
    for manifest in sorted(RUNTIME.glob("*/*/manifest.toml")):
        benchmark = manifest.parent.relative_to(RUNTIME).as_posix()
        names = list(tomllib.loads(manifest.read_text()).get("covers", []))
        if manifest.parent.parent.name == "ast" and f"ast:{manifest.parent.name}" in known:
            names.append(f"ast:{manifest.parent.name}")
        for name in names:
            covered.setdefault(name, []).append(benchmark)
    return covered


def exemptions() -> tuple[dict[str, str], list[str]]:
    exempt: dict[str, str] = {}
    problems = []
    for number, line in enumerate(EXEMPT.read_text().splitlines(), start=1):
        if not line.strip() or line.lstrip().startswith("#"):
            continue
        match = re.fullmatch(r"\s*(\S+)\s*(?:#\s*(.*))?", line)
        if not match:
            problems.append(f"exempt.txt:{number}: cannot read '{line}'")
            continue
        name, reason = match.group(1), (match.group(2) or "").strip()
        if not reason:
            problems.append(f"exempt.txt:{number}: {name} gives no reason")
        exempt[name] = reason
    return exempt, problems


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--list", action="store_true")
    args = parser.parse_args()

    known = parts()
    covered = coverage(known)
    exempt, problems = exemptions()

    problems += [f"{name} is covered by {', '.join(covered[name])} but names no AST or codegen module"
                 for name in sorted(set(covered) - known)]  # fmt: skip
    problems += [f"exempt.txt names {name}, which is no AST or codegen module" for name in sorted(set(exempt) - known)]
    problems += [f"{name} is exempt, but {', '.join(covered[name])} covers it: drop the exemption"
                 for name in sorted(set(exempt) & set(covered))]  # fmt: skip
    problems += [f"{name} has no run-time benchmark: add one that covers it, or a line in exempt.txt saying why"
                 for name in sorted(known - set(covered) - set(exempt))]  # fmt: skip

    if args.list:
        for name in sorted(known):
            where = ", ".join(covered.get(name, [])) or f"exempt: {exempt.get(name, '-')}"
            print(f"{name:<60} {where}")
    for problem in problems:
        print(f"error: {problem}", file=sys.stderr)
    if not problems:
        print(f"{len(known)} parts: {len(set(covered) & known)} covered, {len(set(exempt) & known)} exempt")
    return 1 if problems else 0


if __name__ == "__main__":
    sys.exit(main())
