#!/usr/bin/env python3
"""One very long function: analysis within a function, borrow and memory checking across many statements, and LLVM's
passes on a single large body, which is where a superlinear pass shows. The result is printed, or at rel the whole
function would be deleted as dead before the optimiser reached it.

  generate.py <out-dir>
"""

import sys
from pathlib import Path

STATEMENTS = 1500


def statement(i: int) -> str:
    # Kept below the S32 range, so the checked arithmetic never traps.
    return (
        f"  let v{i} = (a + {i}) % 1000_s32\n"
        f"  case v{i} > 500_s32 {{\n"
        f"    a = v{i} - 1_s32\n"
        f"  }}\n"
        f"  else {{\n"
        f"    a = v{i} + 1_s32\n"
        f"  }}\n"
    )


def main() -> None:
    out = Path(sys.argv[1]) / "src"
    out.mkdir(parents=True, exist_ok=True)
    body = "".join(statement(i) for i in range(STATEMENTS))
    (out / "main.spp").write_text(
        f"fun long(seed: S32) -> S32 {{\n  let mut a = seed\n{body}  ret a\n}}\n\n"
        "fun main() -> Void {\n  let text = Str::from(long(7_s32))\n  std::console::println(&text)\n"
        "  std::mem::ops::drop(text)\n}\n"
    )


if __name__ == "__main__":
    main()
