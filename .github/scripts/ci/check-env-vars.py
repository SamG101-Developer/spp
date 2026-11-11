#!/usr/bin/env python3
"""Fail if an SPP_ name is used without being registered. Run from the repository root.

Scripts and workflows may use SPP_CI_*, a variable in sources/spp/utils/env.cpp or a CMake option. C++ names a
variable only through spp::utils::env, so an SPP_ name in a string literal outside that module is an error.
"""

from __future__ import annotations

import re
import subprocess
import sys
from pathlib import Path

SCHEMA = Path("sources/spp/utils/env.cpp")
CI_PREFIX = "SPP_CI_"
NAME = re.compile(r"(?<![A-Za-z0-9_])SPP_[A-Z0-9_]+")
STRING = re.compile(r'"(?:[^"\\\n]|\\.)*"')
OPTION = re.compile(r"\b(?:option|set)\(\s*(SPP_[A-Z0-9_]+)\b[^\n]*(?:\bCACHE\b|\")")
SCRIPTS = re.compile(r"^(?:\.github/.*\.(?:sh|ya?ml|py|toml)|\.tools/.*\.sh|tests/.*\.(?:sh|py))$")
CPP = re.compile(r"^(?:sources|headers|tests|main).*\.(?:cpp|ixx|hpp)$|^main\.cpp$")
CPP_EXEMPT = {str(SCHEMA), "tests/compiler/test_msc_env.cpp"}


def tracked() -> list[str]:
    command = ["git", "ls-files", "--cached", "--others", "--exclude-standard"]
    return [f for f in subprocess.check_output(command, text=True).split("\n") if f and Path(f).is_file()]


def main() -> int:
    files = tracked()
    registered = set(re.findall(r'\.Name = "(SPP_[A-Z0-9_]+)"', SCHEMA.read_text(encoding="utf-8")))
    if not registered:
        print(f"::error::could not read the variables from {SCHEMA}")
        return 1

    options: set[str] = set()
    for file in files:
        if file == "CMakeLists.txt" or file.endswith(".cmake"):
            options.update(OPTION.findall(Path(file).read_text(encoding="utf-8")))

    this_file = Path(__file__).resolve()
    failed = False
    for file in files:
        path = Path(file)
        if path.resolve() == this_file:
            continue
        if SCRIPTS.match(file):
            for n, line in enumerate(path.read_text(encoding="utf-8").splitlines(), start=1):
                for name in NAME.findall(line):
                    if not name.startswith(CI_PREFIX) and name not in registered | options:
                        print(
                            f"::error file={file},line={n}::{name} is not in {SCHEMA}, a CMake option or {CI_PREFIX}*"
                        )
                        failed = True
        elif CPP.match(file) and file not in CPP_EXEMPT:
            for n, line in enumerate(path.read_text(encoding="utf-8").splitlines(), start=1):
                if line.lstrip().startswith(("*", "/*")):
                    continue
                for literal in STRING.findall(line.partition("//")[0]):
                    for name in NAME.findall(literal):
                        print(f"::error file={file},line={n}::{name} in a string: use spp::utils::env")
                        failed = True
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
