#!/usr/bin/env python3
"""Fail if a triple in the cross codegen matrix is missing from kSuggestedTriples. Run from the repository root."""

from __future__ import annotations

import re
import sys
from pathlib import Path

MATRIX = Path(".github/workflows/_cross_codegen.yaml")
SOURCE = Path("sources/spp/codegen/llvm_target.cpp")

matrix = re.findall(r'\btriple:\s*"([^"]+)"', MATRIX.read_text(encoding="utf-8"))
block = re.search(r"kSuggestedTriples\[\]\s*=\s*\{(.*?)\};", SOURCE.read_text(encoding="utf-8"), re.DOTALL)
if not matrix or block is None:
    print(f"::error::could not read the triples from {MATRIX} or {SOURCE}")
    sys.exit(1)

suggested = set(re.findall(r'"([^"]+)"', block.group(1)))
missing = [triple for triple in matrix if triple not in suggested]
for triple in missing:
    print(f"::error file={SOURCE}::{triple} is tested in {MATRIX} but missing from kSuggestedTriples")
sys.exit(1 if missing else 0)
