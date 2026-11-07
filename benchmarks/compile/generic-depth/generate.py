#!/usr/bin/env python3
"""Deep and wide generic instantiation: monomorphisation, sup attachment, identity keys.

Each level wraps the last one in a generic class, so level k instantiates Box nested k deep, and pairs it with the
PAIRED shallower levels, so there are DEPTH * PAIRED distinct Pair instantiations besides the Box ones.

DEPTH is 16 because each level ends in a chain of k + 1 method calls, and a chain's analysis doubles per call
(open-bugs.md #48): at 24 the build was killed at 14 GB, at 16 it costs about 0.2 GB more than at 12. Raise it once #48
is fixed, which will show as this workload's counters dropping.

  generate.py <out-dir>
"""

import sys
from pathlib import Path

DEPTH = 16
PAIRED = 8

HEADER = """\
cls Box[T] {
  !public v: T
}

sup [T: std::copy::Copy] Box[T] ext std::copy::Copy { }

sup [T: std::copy::Copy] Box[T] {
  !public
  fun get(self) -> T { ret self.v }
}

cls Pair[A, B] {
  !public a: A
  !public b: B
}

sup [A: std::copy::Copy, B: std::copy::Copy] Pair[A, B] ext std::copy::Copy { }

sup [A: std::copy::Copy, B: std::copy::Copy] Pair[A, B] {
  !public
  fun first(self) -> A { ret self.a }
}

fun wrap[T: std::copy::Copy](x: T) -> Box[T] { ret Box(v=x) }

fun pair[A: std::copy::Copy, B: std::copy::Copy](a: A, b: B) -> Pair[A, B] { ret Pair(a=a, b=b) }
"""


def level(k: int) -> str:
    lines = [f"fun level_{k}(x: S32) -> S32 {{", "  let w0 = wrap(x)"]
    lines += [f"  let w{i} = wrap(w{i - 1})" for i in range(1, k + 1)]
    lines += [f"  let p{j} = pair(w{k}, w{j})" for j in range(max(0, k - PAIRED), k)]
    lines.append(f"  ret w{k}" + ".get()" * (k + 1))
    lines.append("}")
    return "\n".join(lines) + "\n"


def main() -> None:
    out = Path(sys.argv[1]) / "src"
    out.mkdir(parents=True, exist_ok=True)
    body = [HEADER] + [level(k) for k in range(DEPTH)]
    calls = "\n".join(f"  total = total + level_{k}(1)" for k in range(DEPTH))
    body.append(
        f"fun main() -> Void {{\n  let mut total = 0_s32\n{calls}\n  let text = Str::from(total)\n"
        "  std::console::println(&text)\n  std::mem::ops::drop(text)\n}\n"
    )
    (out / "main.spp").write_text("\n".join(body))


if __name__ == "__main__":
    main()
