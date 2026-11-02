#!/usr/bin/env python3
"""Many small modules: the module tree, name resolution across modules, one LLVM module each, and linking them into
one for LTO and internalisation.

Each module calls the one before it, and main prints the last one's result, so every module is reachable and none is
dropped as dead.

  generate.py <out-dir>
"""

import sys
from pathlib import Path

MODULES = 200
GROUP = 20


def path(i: int) -> str:
    return f"mods/g{i // GROUP}/m{i}"


def module(i: int) -> str:
    previous = f"mods::g{(i - 1) // GROUP}::m{i - 1}::step(x)" if i > 0 else "x"
    return (
        "cls Item {\n  !public value: S32\n}\n\n"
        "sup Item ext std::copy::Copy { }\n\n"
        "!public\n"
        f"fun step(x: S32) -> S32 {{\n  let item = Item(value={previous})\n  ret item.value % 1000_s32 + 1_s32\n}}\n"
    )


def main() -> None:
    out = Path(sys.argv[1]) / "src"
    for i in range(MODULES):
        file = out / f"{path(i)}.spp"
        file.parent.mkdir(parents=True, exist_ok=True)
        file.write_text(module(i))
    last = f"mods::g{(MODULES - 1) // GROUP}::m{MODULES - 1}::step"
    (out / "main.spp").write_text(
        f"fun main() -> Void {{\n  let text = Str::from({last}(1_s32))\n  std::console::println(&text)\n"
        "  std::mem::ops::drop(text)\n}\n"
    )


if __name__ == "__main__":
    main()
