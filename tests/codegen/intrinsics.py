#!/usr/bin/env python3
"""The cases of check-intrinsics.sh: every numeric std intrinsic, checked at compile time and at run time.

An intrinsic with a compile-time form (a "Register" row given one) is computed three ways: as a "cmp" constant, at
run time from values the optimiser cannot see (a volatile load), and here, from the run-time semantics. Each case
reports which of the three disagree. One with no compile-time form is checked at run time against the value here.

  intrinsics.py emit <builtins.cpp> <std-src> <out-dir>
                                                write the programs and their manifest; fails on an intrinsic in the
                                                registry with neither a case nor a reason to exclude it
  intrinsics.py judge <out-dir>                 read each program's build and run, against the known failures
"""

import json
import math
import re
import struct
import sys
from collections.abc import Callable
from decimal import ROUND_CEILING, ROUND_DOWN, ROUND_FLOOR, ROUND_HALF_UP, Decimal, localcontext
from fractions import Fraction
from pathlib import Path

# -- Types ------------------------------------------------------------------------------------------------------------
# Todo: Python 3.15 "frozendict" for INTS and FLOATS.

# name: (width, signed, literal suffix)
INTS: dict[str, tuple[int, bool, str]] = {
    "S8": (8, True, "s8"),
    "S16": (16, True, "s16"),
    "S32": (32, True, "s32"),
    "S64": (64, True, "s64"),
    "S128": (128, True, "s128"),
    "S256": (256, True, "s256"),
    "SSize": (64, True, "sz"),
    "U8": (8, False, "u8"),
    "U16": (16, False, "u16"),
    "U32": (32, False, "u32"),
    "U64": (64, False, "u64"),
    "U128": (128, False, "u128"),
    "U256": (256, False, "u256"),
    "USize": (64, False, "uz"),
}

# name: (width, literal suffix, integer type of its bits)
FLOATS: dict[str, tuple[int, str, str]] = {
    "F16": (16, "f16", "U16"),
    "F32": (32, "f32", "U32"),
    "F64": (64, "f64", "U64"),
    "F128": (128, "f128", "U128"),
}

# name: (significand bits, the implicit one included, largest exponent)
FORMATS: dict[str, tuple[int, int]] = {"F16": (11, 15), "F32": (24, 127), "F64": (53, 1023), "F128": (113, 16383)}

# Decimal digits that hold every value of these formats, and any sum, difference or product of two, exactly.
EXACT = 20000


def bounds(t: str) -> tuple[int, int]:
    """Integer bounds of a given type."""
    w, signed, _ = INTS[t]
    return (-(1 << (w - 1)), (1 << (w - 1)) - 1) if signed else (0, (1 << w) - 1)


def wrap(t: str, v: int) -> int:
    w, signed, _ = INTS[t]
    v &= (1 << w) - 1
    return v - (1 << w) if signed and v >> (w - 1) else v


def int_lit(t: str, v: int) -> str:
    return f"{v}_{INTS[t][2]}"


def f_round(t: str, x: float) -> float:
    """x rounded to the float type t, as a Python float."""
    return struct.unpack("f", struct.pack("f", x))[0] if t == "F32" else x


def f_lit(t: str, x: float | Decimal) -> str:
    """The exact decimal of x, which both sides read back to x."""
    d = format(Decimal(x), "f")
    if "." not in d:
        d += ".0"
    if d.startswith("-") and Decimal(x) == 0:
        d = "-0.0"
    return f"{d}_{FLOATS[t][1]}"


def f_max(t: str) -> float:
    return struct.unpack("f", b"\xff\xff\x7f\x7f")[0] if t == "F32" else sys.float_info.max


def q_max(t: str) -> Decimal:
    """The largest finite value of the float format t, exactly."""
    p, emax = FORMATS[t]
    return Decimal(((1 << p) - 1) << (emax - p + 1))


def q_round(t: str, x: Decimal) -> Decimal | None:
    """x rounded to nearest, ties to even, in the float format t, exactly; None when it overflows."""
    if x.is_zero():
        return x
    p, emax = FORMATS[t]
    f = Fraction(x.copy_abs())
    # The exponent of f: the difference of the bit lengths, or one less.
    e = f.numerator.bit_length() - f.denominator.bit_length()
    if Fraction(2) ** e > f:
        e -= 1
    # A subnormal has the smallest exponent; round() on a Fraction rounds ties to even.
    scale = max(e, 1 - emax) - p + 1
    n = round(f / Fraction(2) ** scale)
    if n * Fraction(2) ** scale > Fraction(q_max(t)):
        return None
    with localcontext() as ctx:
        ctx.prec = EXACT
        d = Decimal(n << scale) if scale >= 0 else Decimal(n * 5**-scale).scaleb(scale)
    return d.copy_negate() if x.is_signed() else d


# -- Cases ------------------------------------------------------------------------------------------------------------
# A case: (program, name, s++ type of the result, comptime expression or None, runtime statements, runtime value
# expression, expected value). The runtime statements read their inputs through "hide".


type Value = int | float | bool | Decimal


class Case:
    def __init__(
        self,
        prog: str,
        name: str,
        ty: str,
        cmp: str | None,
        rt_stmts: list[str],
        rt_val: str,
        expected: Value | None,
        tol: float | None = None,
    ) -> None:
        self.prog, self.name, self.ty, self.cmp = prog, name, ty, cmp
        self.rt_stmts, self.rt_val, self.expected, self.tol = rt_stmts, rt_val, expected, tol


CASES: list[Case] = []
COVERED: set[str] = set()


def covers(*names: str) -> None:
    COVERED.update(f"std.intrinsics.{n}" if "." not in n else n for n in names)


def int_pairs(t: str) -> list[tuple[int, int]]:
    lo, hi = bounds(t)
    vals = [0, 1, 2, 5, 7, hi, hi - 1] + ([-1, -7, lo, lo + 1] if lo < 0 else [1 << (INTS[t][0] - 1)])
    pairs = [(a, b) for a in vals for b in vals]
    # Keep the edges, and a spread of the rest, so a program stays a few hundred cases.
    keep = [p for p in pairs if p[0] in (lo, hi) or p[1] in (lo, hi) or (p[0] in (0, 5, -7) and p[1] in (1, -1, 7, 2))]
    return sorted(set(keep))


def int_vals(t: str) -> list[int]:
    lo, hi = bounds(t)
    return sorted({0, 1, 5, hi, hi - 1, lo} | ({-1, -7, lo + 1} if lo < 0 else {1 << (INTS[t][0] - 1)}))


def tdiv(a: int, b: int) -> int:
    q = abs(a) // abs(b)
    return q if (a >= 0) == (b >= 0) else -q


INT_BIN: dict[str, tuple[str, Callable[[str, int, int], int | None]]] = {
    # name: (signed-only/unsigned-only/any, function -> value or None when out of the domain)
    "sadd": ("s", lambda t, a, b: a + b),
    "uadd": ("u", lambda t, a, b: a + b),
    "ssub": ("s", lambda t, a, b: a - b),
    "usub": ("u", lambda t, a, b: a - b),
    "smul": ("s", lambda t, a, b: a * b),
    "umul": ("u", lambda t, a, b: a * b),
    "sdiv": ("s", lambda t, a, b: None if b == 0 else tdiv(a, b)),
    "udiv": ("u", lambda t, a, b: None if b == 0 else a // b),
    "srem": ("s", lambda t, a, b: None if b == 0 or tdiv(a, b) > bounds(t)[1] else a - b * tdiv(a, b)),
    "urem": ("u", lambda t, a, b: None if b == 0 else a % b),
    "bit_and": ("*", lambda t, a, b: wrap(t, a & b)),
    "bit_ior": ("*", lambda t, a, b: wrap(t, a | b)),
    "bit_xor": ("*", lambda t, a, b: wrap(t, a ^ b)),
    "smax": ("s", lambda t, a, b: max(a, b)),
    "smin": ("s", lambda t, a, b: min(a, b)),
    "umax": ("u", lambda t, a, b: max(a, b)),
    "umin": ("u", lambda t, a, b: min(a, b)),
}
INT_CMP: dict[str, tuple[str, Callable[[int, int], bool | int]]] = {
    "eq": ("*", lambda a, b: a == b),
    "ne": ("*", lambda a, b: a != b),
    "slt": ("s", lambda a, b: a < b),
    "sle": ("s", lambda a, b: a <= b),
    "sgt": ("s", lambda a, b: a > b),
    "sge": ("s", lambda a, b: a >= b),
    "ult": ("u", lambda a, b: a < b),
    "ule": ("u", lambda a, b: a <= b),
    "ugt": ("u", lambda a, b: a > b),
    "uge": ("u", lambda a, b: a >= b),
    "scmp": ("s", lambda a, b: (a > b) - (a < b)),
    "ucmp": ("u", lambda a, b: (a > b) - (a < b)),
}


def fits(t: str, v: int | None) -> bool:
    lo, hi = bounds(t)
    return v is not None and lo <= v <= hi


def applies(kind: str, t: str) -> bool:
    return kind == "*" or (kind == "s") == INTS[t][1]


def assign_fun(op: str, t: str, args: list[str]) -> str:
    """A "cmp fun" running "<op>_assign" on a copy of its first argument, for the compile-time side."""
    params = ", ".join(f"p{i}: {a}" for i, a in enumerate(args))
    rest = "".join(f", p{i}" for i in range(1, len(args)))
    return (
        f"cmp fun asg_{op}_{t}({params}) -> {t} {{\n    let mut x = p0\n"
        f"    std::intrinsics::{op}_assign(&mut x{rest})\n    ret x\n}}\n"
    )


# program: helper name: source
HELPERS: dict[str, dict[str, str]] = {}


def int_cases() -> None:
    for t in INTS:
        prog = f"int-{t}"
        for op, (kind, fn) in INT_BIN.items():
            if not applies(kind, t):
                continue
            has_assign = op not in ("smax", "smin", "umax", "umin")
            covers(op, *([f"{op}_assign"] if has_assign else []))
            for a, b in int_pairs(t):
                e = fn(t, a, b)
                if not fits(t, e):
                    continue
                la, lb = int_lit(t, a), int_lit(t, b)
                CASES.append(
                    Case(
                        prog,
                        f"{op} {t} ({a}, {b})",
                        t,
                        f"std::intrinsics::{op}({la}, {lb})",
                        [],
                        f"std::intrinsics::{op}(hide({la}), hide({lb}))",
                        e,
                    )
                )
                if not has_assign:
                    continue
                HELPERS.setdefault(prog, {})[f"asg_{op}"] = assign_fun(op, t, [t, t])
                CASES.append(
                    Case(
                        prog,
                        f"{op}_assign {t} ({a}, {b})",
                        t,
                        f"asg_{op}_{t}({la}, {lb})",
                        [f"let mut x = hide({la})", f"std::intrinsics::{op}_assign(&mut x, hide({lb}))"],
                        "x",
                        e,
                    )
                )
        for op, (kind, fn) in INT_CMP.items():
            if not applies(kind, t):
                continue
            covers(op)
            ty = "S32" if op.endswith("cmp") else "Bool"
            for a, b in int_pairs(t):
                la, lb = int_lit(t, a), int_lit(t, b)
                e = fn(a, b)
                CASES.append(
                    Case(
                        prog,
                        f"{op} {t} ({a}, {b})",
                        ty,
                        f"std::intrinsics::{op}(&{la}, &{lb})",
                        [f"let a = hide({la})", f"let b = hide({lb})"],
                        f"std::intrinsics::{op}(&a, &b)",
                        e,
                    )
                )
        w, signed, _ = INTS[t]
        # Shifts by a "U32", within the width (at or past it is run-time only: 0 there, E91 at compile time).
        for op in ("bit_shl", "bit_shr"):
            covers(op, f"{op}_assign")
            for a in int_vals(t):
                for by in sorted({0, 1, 3, w - 1}):
                    u = a & ((1 << w) - 1)
                    e = wrap(t, u << by) if op == "bit_shl" else wrap(t, u >> by)
                    la, lb = int_lit(t, a), f"{by}_u32"
                    CASES.append(
                        Case(
                            prog,
                            f"{op} {t} ({a}, {by})",
                            t,
                            f"std::intrinsics::{op}({la}, {lb})",
                            [],
                            f"std::intrinsics::{op}(hide({la}), hide({lb}))",
                            e,
                        )
                    )
                    HELPERS.setdefault(prog, {})[f"asg_{op}"] = assign_fun(op, t, [t, "U32"])
                    CASES.append(
                        Case(
                            prog,
                            f"{op}_assign {t} ({a}, {by})",
                            t,
                            f"asg_{op}_{t}({la}, {lb})",
                            [f"let mut x = hide({la})", f"std::intrinsics::{op}_assign(&mut x, hide({lb}))"],
                            "x",
                            e,
                        )
                    )
        covers("bit_not", "bit_not_assign")
        HELPERS.setdefault(prog, {})["asg_bit_not"] = (
            f"cmp fun asg_bit_not_{t}(p0: {t}) -> {t} {{\n    let mut x = p0\n"
            f"    std::intrinsics::bit_not_assign(&mut x)\n    ret x\n}}\n"
        )
        for a in int_vals(t):
            la, e = int_lit(t, a), wrap(t, ~a)
            CASES.append(
                Case(
                    prog,
                    f"bit_not {t} ({a})",
                    t,
                    f"std::intrinsics::bit_not({la})",
                    [],
                    f"std::intrinsics::bit_not(hide({la}))",
                    e,
                )
            )
            CASES.append(
                Case(
                    prog,
                    f"bit_not_assign {t} ({a})",
                    t,
                    f"asg_bit_not_{t}({la})",
                    [f"let mut x = hide({la})", "std::intrinsics::bit_not_assign(&mut x)"],
                    "x",
                    e,
                )
            )
        if signed:
            covers("sneg")
            for a in int_vals(t):
                la = int_lit(t, a)
                for op, e in (("sneg", -a),):
                    if fits(t, e):
                        CASES.append(
                            Case(
                                prog,
                                f"{op} {t} ({a})",
                                t,
                                f"std::intrinsics::{op}({la})",
                                [],
                                f"std::intrinsics::{op}(hide({la}))",
                                e,
                            )
                        )
        covers("max_val", "min_val")
        lo, hi = bounds(t)
        for op, e in (("max_val", hi), ("min_val", lo)):
            CASES.append(
                Case(
                    prog, f"{op} {t}", t, f"std::intrinsics::{op}[{t}]()", [], f"hide(std::intrinsics::{op}[{t}]())", e
                )
            )
        covers(
            "std.num.sized_integer.SizedInteger.zero",
            "std.num.sized_integer.SizedInteger.one",
            "std.num.sized_integer.SizedInteger.two",
            "std.num.sized_integer_signed.SizedIntegerSigned.neg_one",
        )
        consts = [("zero", 0), ("one", 1), ("two", 2)] + ([("neg_one", -1)] if signed else [])
        for op, e in consts:
            # std declares "two" a plain "fun", unlike its siblings, so it cannot fold: run time only.
            cmp = None if op == "two" else f"{t}::{op}()"
            CASES.append(Case(f"num-{t}", f"{op} {t}", t, cmp, [], f"hide({t}::{op}())", e))


# As written: each is the value of its format nearest to it ("0.1" is not exactly a float of any).
FLOAT_VALS: list[str] = ["0.0", "-0.0", "1.0", "-1.0", "0.5", "1.5", "-2.5", "3.0", "0.1", "100.25"]

# Each on exact decimals (EXACT digits), rounded to the format afterwards: an IEEE operation is its exact result,
# rounded once. A decimal zero keeps its sign as an IEEE one does, except under "-", hence "copy_negate".
FLOAT_BIN: dict[str, Callable[[Decimal, Decimal], Decimal | None]] = {
    "float_add": lambda a, b: a + b,
    "float_sub": lambda a, b: a - b,
    "float_mul": lambda a, b: a * b,
    "float_div": lambda a, b: None if b == 0 else a / b,
    "float_rem": lambda a, b: None if b == 0 else a % b,
    "float_max": lambda a, b: None if a == b == 0 else max(a, b),
    "float_min": lambda a, b: None if a == b == 0 else min(a, b),
}
FLOAT_UN: dict[str, Callable[[Decimal], Decimal]] = {
    "float_neg": lambda a: a.copy_negate(),
    "float_abs": lambda a: a.copy_abs(),
    "float_floor": lambda a: a.quantize(Decimal(1), rounding=ROUND_FLOOR),
    "float_ceil": lambda a: a.quantize(Decimal(1), rounding=ROUND_CEILING),
    "float_trunc": lambda a: a.quantize(Decimal(1), rounding=ROUND_DOWN),
    "float_round": lambda a: a.quantize(Decimal(1), rounding=ROUND_HALF_UP),
}

# The same operations on the values as written, in exact decimals: where the result is not the IEEE one, a constant
# folded exactly differs from run time.
DEC_BIN: dict[str, Callable[[Decimal, Decimal], Decimal]] = {
    "float_add": lambda a, b: a + b,
    "float_sub": lambda a, b: a - b,
    "float_mul": lambda a, b: a * b,
    "float_div": lambda a, b: a / b,
    "float_rem": lambda a, b: a % b,
}


def neg_zero(x: Decimal) -> bool:
    return x.is_zero() and x.is_signed()


FLOAT_CMP: dict[str, Callable[[Decimal, Decimal], bool]] = {
    "oeq": lambda a, b: a == b,
    "one": lambda a, b: a != b,
    "olt": lambda a, b: a < b,
    "ole": lambda a, b: a <= b,
    "ogt": lambda a, b: a > b,
    "oge": lambda a, b: a >= b,
}


def float_cases() -> None:
    for t in FLOATS:
        with localcontext() as ctx:
            ctx.prec = EXACT
            float_cases_of(t)


def float_cases_of(t: str) -> None:
    prog = f"float-{t}"
    tenth = q_round(t, Decimal("0.1"))
    top = q_max(t)
    vals: list[Decimal] = [v for v in (q_round(t, Decimal(s)) for s in FLOAT_VALS) if v is not None] + [top, -top]

    # "0.1" as written, rather than as the float nearest it: what a program would write. A function, not a table:
    # 0.0 and -0.0 are one key.
    def lit(v: Decimal) -> str:
        return f"0.1_{FLOATS[t][1]}" if v == tenth else f_lit(t, v)

    def dec(v: Decimal) -> Decimal:
        return Decimal("0.1") if v == tenth else v

    # A short name for a value: the largest is thousands of digits in an "F128".
    def short(v: Decimal) -> str:
        return "0.1" if v == tenth else ("-" if v < 0 else "") + "max" if abs(v) == top else str(v)

    pairs = [(a, b) for a in vals for b in vals if abs(a) < top and abs(b) < top] + [
        (top, Decimal(1)),
        (top, Decimal("0.5")),
        (-top, Decimal(2)),
        (Decimal(1), top),
    ]
    for op, fn in FLOAT_BIN.items():
        covers(op, *([f"{op}_assign"] if op not in ("float_max", "float_min") else []))
        for a, b in pairs:
            exact = fn(a, b)
            e = None if exact is None else q_round(t, exact)
            if e is None:
                continue
            la, lb = lit(a), lit(b)
            name = f"{op} {t} ({short(a)}, {short(b)})"
            if neg_zero(e):
                name += " -> -0.0"
            elif op in DEC_BIN and DEC_BIN[op](dec(a), dec(b)) != e:
                name += " inexact"
            CASES.append(
                Case(
                    prog,
                    name,
                    t,
                    f"std::intrinsics::{op}({la}, {lb})",
                    [],
                    f"std::intrinsics::{op}(hide({la}), hide({lb}))",
                    e,
                )
            )
            if op not in ("float_max", "float_min"):
                HELPERS.setdefault(prog, {})[f"asg_{op}"] = assign_fun(op, t, [t, t])
                CASES.append(
                    Case(
                        prog,
                        name.replace(op, f"{op}_assign", 1),
                        t,
                        f"asg_{op}_{t}({la}, {lb})",
                        [f"let mut x = hide({la})", f"std::intrinsics::{op}_assign(&mut x, hide({lb}))"],
                        "x",
                        e,
                    )
                )
    for op, fn in FLOAT_UN.items():
        covers(op)
        for a in vals:
            e = q_round(t, fn(a))
            assert e is not None
            CASES.append(
                Case(
                    prog,
                    f"{op} {t} ({short(a)})" + (" -> -0.0" if neg_zero(e) else ""),
                    t,
                    f"std::intrinsics::{op}({lit(a)})",
                    [],
                    f"std::intrinsics::{op}(hide({lit(a)}))",
                    e,
                )
            )
    for op, fn in FLOAT_CMP.items():
        covers(op)
        for a, b in pairs:
            la, lb = lit(a), lit(b)
            CASES.append(
                Case(
                    prog,
                    f"{op} {t} ({short(a)}, {short(b)})",
                    "Bool",
                    f"std::intrinsics::{op}(&{la}, &{lb})",
                    [f"let a = hide({la})", f"let b = hide({lb})"],
                    f"std::intrinsics::{op}(&a, &b)",
                    fn(a, b),
                )
            )
    covers("float_max_val", "float_min_val")
    for op, e in (("float_max_val", top), ("float_min_val", -top)):
        CASES.append(
            Case(prog, f"{op} {t}", t, f"std::intrinsics::{op}[{t}]()", [], f"hide(std::intrinsics::{op}[{t}]())", e)
        )
    covers(
        "std.num.sized_floating_point.SizedFloatingPoint.zero",
        "std.num.sized_floating_point.SizedFloatingPoint.one",
        "std.num.sized_floating_point.SizedFloatingPoint.neg_one",
    )
    for op, e in (("zero", Decimal(0)), ("one", Decimal(1)), ("neg_one", Decimal(-1))):
        CASES.append(Case(f"num-{t}", f"{op} {t}", t, f"{t}::{op}()", [], f"hide({t}::{op}())", e))


# An "F8" is FP8 E4M3, stored as its bits: no infinity, and 0x7f/0xff are NaN. Each code below 0x7f is its value.
E4M3 = [Fraction(b & 7, 512) if b >> 3 == 0 else Fraction(8 + (b & 7), 1 << 10) * 2 ** (b >> 3) for b in range(127)]


def e4m3_bits(x: Fraction) -> int:
    """x rounded to nearest, ties to even, as E4M3 bits, saturating past 448 (as "fptrunc" to "F8" does)."""
    sign = 0x80 if x < 0 else 0
    a = abs(x)
    if a > E4M3[126]:
        return sign | 126
    # The even code of a tie: an even code has an even last bit of its significand.
    return sign | min(range(127), key=lambda b: (abs(E4M3[b] - a), b & 1))


def f8_cases() -> None:
    """An "F8" is converted, never computed with: its bits read back through "fpext", and "fptrunc" to one."""
    prog = "f8"
    covers("fpext", "fptrunc", "bit_cast")  # As "fpext_f8" and "fptrunc_f8", which "F8" has of its own.
    with localcontext() as ctx:
        ctx.prec = EXACT
        for b in [*range(127), *range(128, 255)]:
            value = Decimal(E4M3[b & 127].numerator) / Decimal(E4M3[b & 127].denominator)
            value = value.copy_negate() if b & 0x80 else value
            for t, (w, _, _) in FLOATS.items():
                CASES.append(
                    Case(
                        prog,
                        f"fpext F8 -> {t} (0x{b:02x})",
                        t,
                        None,
                        [f"let x = std::intrinsics::bit_cast[F8](hide({b}_u8))"],
                        f"std::intrinsics::fpext_f8[{w}_u32](x)",
                        value,
                    )
                )

        # Each tie, and the nearest value of the source either side: a wider float narrowed to nearest first would
        # land a value just short of a tie on it, and round it the wrong way.
        inputs = [Fraction(0), Fraction(500), Fraction(65504), Fraction(1, 2**20)]
        ties = [(E4M3[b] + E4M3[b + 1]) / 2 for b in range(126)]
        for t in FLOATS:
            p, _ = FORMATS[t]
            values = inputs + ties
            for mid in ties:
                e = mid.numerator.bit_length() - mid.denominator.bit_length()
                if Fraction(2) ** e > mid:
                    e -= 1
                ulp = Fraction(2) ** (e - p + 1)
                values += [mid - ulp, mid + ulp]
            if t != "F16":
                values.append(Fraction(2**100))
            for x in values + [-v for v in values if v != 0]:
                exact = Decimal(x.numerator) / Decimal(x.denominator)
                v = q_round(t, exact)
                assert v is not None and Fraction(v) == x, (t, x)
                CASES.append(
                    Case(
                        prog,
                        f"fptrunc {t} -> F8 ({exact})",
                        "U8",
                        None,
                        [],
                        f"std::intrinsics::bit_cast[U8](std::intrinsics::fptrunc_f8(hide({f_lit(t, v)})))",
                        e4m3_bits(x),
                    )
                )
            # An infinity has no "F8": it saturates.
            one, zero = f_lit(t, Decimal(1)), f_lit(t, Decimal(0))
            for sign, bits in (("", 0x7E), ("-", 0xFE)):
                inf = f"std::intrinsics::float_div(hide({sign}{one}), hide({zero}))"
                CASES.append(
                    Case(
                        prog,
                        f"fptrunc {t} -> F8 ({sign}inf)",
                        "U8",
                        None,
                        [],
                        f"std::intrinsics::bit_cast[U8](std::intrinsics::fptrunc_f8({inf}))",
                        bits,
                    )
                )


def size_cases() -> None:
    covers("std.mem.ops.size_of", "std.mem.ops.align_of")
    for t in [*INTS, *FLOATS, "F8"]:
        w = INTS[t][0] if t in INTS else FLOATS[t][0] if t in FLOATS else 8
        CASES.append(
            Case(
                "sizes",
                f"size_of {t}",
                "USize",
                f"std::mem::ops::size_of[{t}]()",
                [],
                f"hide(std::mem::ops::size_of[{t}]())",
                w // 8,
            )
        )
        # The alignment is the target's (i128 is 16 on x86-64 and arm64): compared both ways, with no expected value.
        CASES.append(
            Case(
                "sizes",
                f"align_of {t}",
                "USize",
                f"std::mem::ops::align_of[{t}]()",
                [],
                f"hide(std::mem::ops::align_of[{t}]())",
                None,
            )
        )


# -- Run time only ----------------------------------------------------------------------------------------------------


def runtime_int_cases() -> None:
    prog = "runtime-int"
    for t in ("S8", "S32", "S64", "S128", "U8", "U32", "U64", "U128"):
        w, signed, _ = INTS[t]
        lo, hi = bounds(t)
        p = "s" if signed else "u"
        vals = [0, 1, 5, hi, hi - 1, lo] + ([-1, -7] if signed else [7])
        for a in vals:
            for b in vals:
                la, lb = int_lit(t, a), int_lit(t, b)
                for op, raw in (("add", a + b), ("sub", a - b), ("mul", a * b)):
                    if op == "mul" and not signed:
                        pass
                    covers(f"{p}{op}_wrapping", f"{p}{op}_overflow")
                    CASES.append(
                        Case(
                            prog,
                            f"{p}{op}_wrapping {t} ({a}, {b})",
                            t,
                            None,
                            [],
                            f"std::intrinsics::{p}{op}_wrapping(hide({la}), hide({lb}))",
                            wrap(t, raw),
                        )
                    )
                    CASES.append(
                        Case(
                            prog,
                            f"{p}{op}_overflow {t} ({a}, {b})",
                            t,
                            None,
                            [
                                f"let (v, o) = std::intrinsics::{p}{op}_overflow(hide({la}), hide({lb}))",
                                f'case o != {"true" if not fits(t, raw) else "false"} {{ bad = bad + note("{p}{op}_overflow {t} ({a}, {b}): flag") }}',
                            ],
                            "v",
                            wrap(t, raw),
                        )
                    )
                    if op != "mul":
                        covers(f"{p}{op}_saturating")
                        CASES.append(
                            Case(
                                prog,
                                f"{p}{op}_saturating {t} ({a}, {b})",
                                t,
                                None,
                                [],
                                f"std::intrinsics::{p}{op}_saturating(hide({la}), hide({lb}))",
                                min(max(raw, lo), hi),
                            )
                        )
        for a in vals:
            la = int_lit(t, a)
            for by in (0, 1, w - 1, w, w + 1):
                if by >= (1 << w) or (signed and by > hi):
                    continue
                lb = int_lit(t, by)
                covers(f"{p}shl_saturating")
                shifted = a << by if by < w else (0 if a == 0 else a * (1 << w))
                past = " past the width" if by >= w else ""
                CASES.append(
                    Case(
                        prog,
                        f"{p}shl_saturating {t} ({a}, {by}){past}",
                        t,
                        None,
                        [],
                        f"std::intrinsics::{p}shl_saturating(hide({la}), hide({lb}))",
                        min(max(shifted, lo), hi),
                    )
                )
            # A shift at or past the width is defined at run time: 0 (E91 at compile time).
            CASES.extend(
                Case(
                    prog,
                    f"{op} {t} ({a}, {w}) past the width",
                    t,
                    None,
                    [],
                    f"std::intrinsics::{op}(hide({la}), hide({w}_u32))",
                    0,
                )
                for op in ("bit_shl", "bit_shr")
            )
            covers("bitreverse", "ctlz")
            u = a & ((1 << w) - 1)
            CASES.append(
                Case(
                    prog,
                    f"bitreverse {t} ({a})",
                    t,
                    None,
                    [],
                    f"std::intrinsics::bitreverse(hide({la}))",
                    wrap(t, int(format(u, f"0{w}b")[::-1], 2)),
                )
            )
            CASES.append(
                Case(prog, f"ctlz {t} ({a})", "U32", None, [], f"std::intrinsics::ctlz(hide({la}))", w - u.bit_length())
            )
    # Integer conversions between widths, both ways.
    covers("strunc", "utrunc", "szext", "uzext", "bit_cast")
    for src, dst, op in (
        ("S64", "S8", "strunc"),
        ("U64", "U8", "utrunc"),
        ("S8", "S64", "szext"),
        ("U8", "U64", "uzext"),
        ("S32", "U32", "bit_cast"),
        ("U32", "S32", "bit_cast"),
    ):
        CASES.extend(
            Case(
                prog,
                f"{op} {src} -> {dst} ({a})",
                dst,
                None,
                [],
                f"std::intrinsics::{op}[{dst}](hide({int_lit(src, a)}))",
                wrap(dst, a),
            )
            for a in int_vals(src)
        )


def runtime_float_cases() -> None:
    prog = "runtime-float"
    exact = {
        "float_sqrt": (math.sqrt, [0.0, 1.0, 2.0, 0.25, 1e10]),
        "float_copysign": (None, None),
    }
    approx = {
        "float_sin": math.sin,
        "float_cos": math.cos,
        "float_tan": math.tan,
        "float_asin": math.asin,
        "float_acos": math.acos,
        "float_atan": math.atan,
        "float_sinh": math.sinh,
        "float_cosh": math.cosh,
        "float_tanh": math.tanh,
        "float_exp": math.exp,
        "float_exp2": lambda x: 2.0**x,
        "float_exp10": lambda x: 10.0**x,
        "float_log": math.log,
        "float_log2": math.log2,
        "float_log10": math.log10,
    }
    # Checked against Python's double-precision maths, so only the widths it can round to.
    for t in ("F32", "F64"):
        tol = 1e-5 if t == "F32" else 1e-13
        for v in exact["float_sqrt"][1]:
            covers("float_sqrt")
            x = f_round(t, v)
            CASES.append(
                Case(
                    prog,
                    f"float_sqrt {t} ({x!r})",
                    t,
                    None,
                    [],
                    f"std::intrinsics::float_sqrt(hide({f_lit(t, x)}))",
                    f_round(t, math.sqrt(x)),
                )
            )
        for a, b in ((1.5, -1.0), (-1.5, 1.0), (2.0, -0.0), (-0.0, 1.0)):
            covers("float_copysign")
            CASES.append(
                Case(
                    prog,
                    f"float_copysign {t} ({a!r}, {b!r})",
                    t,
                    None,
                    [],
                    f"std::intrinsics::float_copysign(hide({f_lit(t, a)}), hide({f_lit(t, b)}))",
                    math.copysign(a, b),
                )
            )
        for op, fn in approx.items():
            covers(op)
            for v in (0.5, 0.25, -0.75, 0.0):
                x = f_round(t, v)
                if op in ("float_log", "float_log2", "float_log10") and x <= 0:
                    x = f_round(t, 2.5)
                CASES.append(
                    Case(
                        prog,
                        f"{op} {t} ({x!r})",
                        t,
                        None,
                        [],
                        f"std::intrinsics::{op}(hide({f_lit(t, x)}))",
                        f_round(t, fn(x)),
                        tol,
                    )
                )
        covers("float_atan2", "float_powf", "float_powi")
        for y, x in ((1.0, 1.0), (1.0, -1.0), (-0.5, 2.0)):
            CASES.append(
                Case(
                    prog,
                    f"float_atan2 {t} ({y!r}, {x!r})",
                    t,
                    None,
                    [],
                    f"std::intrinsics::float_atan2(hide({f_lit(t, y)}), hide({f_lit(t, x)}))",
                    f_round(t, math.atan2(y, x)),
                    tol,
                )
            )
        for b, e in ((2.0, 10.0), (2.5, 0.5), (9.0, -0.5)):
            CASES.append(
                Case(
                    prog,
                    f"float_powf {t} ({b!r}, {e!r})",
                    t,
                    None,
                    [],
                    f"std::intrinsics::float_powf(hide({f_lit(t, b)}), hide({f_lit(t, e)}))",
                    f_round(t, b**e),
                    tol,
                )
            )
        for b, e in ((2.0, 10), (1.5, 3), (2.0, -2)):
            CASES.append(
                Case(
                    prog,
                    f"float_powi {t} ({b!r}, {e})",
                    t,
                    None,
                    [],
                    f"std::intrinsics::float_powi(hide({f_lit(t, b)}), hide({e}_s32))",
                    f_round(t, b**e),
                    tol,
                )
            )
        # What a constant cannot hold: infinities and NaN, made at run time.
        inf, nan = (
            "std::intrinsics::float_div(hide({one}), hide({zero}))",
            "std::intrinsics::float_div(hide({zero}), hide({zero}))",
        )
        one, zero = f_lit(t, 1.0), f_lit(t, 0.0)
        mk = {"inf": inf.format(one=one, zero=zero), "nan": nan.format(zero=zero)}
        CASES.append(Case(prog, f"float_div {t} (1.0, 0.0)", t, None, [], mk["inf"], math.inf))
        CASES.append(
            Case(
                prog,
                f"float_add {t} (inf, 1.0)",
                t,
                None,
                [],
                f"std::intrinsics::float_add({mk['inf']}, hide({one}))",
                math.inf,
            )
        )
        CASES.append(
            Case(
                prog,
                f"float_max {t} (nan, 1.0)",
                t,
                None,
                [],
                f"std::intrinsics::float_max({mk['nan']}, hide({one}))",
                1.0,
            )
        )
        CASES.append(
            Case(
                prog,
                f"float_min {t} (1.0, nan)",
                t,
                None,
                [],
                f"std::intrinsics::float_min(hide({one}), {mk['nan']})",
                1.0,
            )
        )
        for op, e in (("oeq", False), ("one", False), ("olt", False), ("oge", False)):
            CASES.append(
                Case(
                    prog,
                    f"{op} {t} (nan, 1.0)",
                    "Bool",
                    None,
                    [f"let n = {mk['nan']}", f"let o = hide({one})"],
                    f"std::intrinsics::{op}(&n, &o)",
                    e,
                )
            )
        covers("fpclass")
        # The flag is a class's index (LLVM's "is.fpclass" bit): 1 quiet NaN, 5 -0, 8 +normal, 9 +inf.
        for name, val, flag, e in (
            ("nan", mk["nan"], 1, True),
            ("inf", mk["inf"], 9, True),
            ("-0.0", f"hide({f_lit(t, -0.0)})", 5, True),
            ("1.0", f"hide({one})", 8, True),
            ("1.0", f"hide({one})", 1, False),
        ):
            CASES.append(
                Case(
                    prog,
                    f"fpclass {t} ({name}, {flag})",
                    "Bool",
                    None,
                    [],
                    f"std::intrinsics::fpclass({val}, {flag}_s32)",
                    e,
                )
            )
    # Conversions between floats and integers, and between float widths.
    covers("sitofp", "uitofp", "fptosi", "fptoui", "fpext", "fptrunc")
    for a in (0, 1, -1, 7, -1024, 16777217):
        CASES.append(
            Case(
                prog,
                f"sitofp S32 -> F64 ({a})",
                "F64",
                None,
                [],
                f"std::intrinsics::sitofp[64_u32](hide({int_lit('S32', a)}))",
                float(a),
            )
        )
        CASES.append(
            Case(
                prog,
                f"sitofp S32 -> F32 ({a})",
                "F32",
                None,
                [],
                f"std::intrinsics::sitofp[32_u32](hide({int_lit('S32', a)}))",
                f_round("F32", float(a)),
            )
        )
    CASES.extend(
        Case(
            prog,
            f"uitofp U32 -> F64 ({a})",
            "F64",
            None,
            [],
            f"std::intrinsics::uitofp[64_u32](hide({int_lit('U32', a)}))",
            float(a),
        )
        for a in (0, 1, 7, 4294967295)
    )
    for x in (0.0, 1.5, -1.5, -2.75, 100.9):
        CASES.append(
            Case(
                prog,
                f"fptosi F64 -> S32 ({x!r})",
                "S32",
                None,
                [],
                f"std::intrinsics::fptosi[32_u32](hide({f_lit('F64', x)}))",
                math.trunc(x),
            )
        )
        if x >= 0:
            CASES.append(
                Case(
                    prog,
                    f"fptoui F64 -> U32 ({x!r})",
                    "U32",
                    None,
                    [],
                    f"std::intrinsics::fptoui[32_u32](hide({f_lit('F64', x)}))",
                    math.trunc(x),
                )
            )
    for x in (0.0, -0.0, 1.5, 0.1, 3.4e38):
        x32 = f_round("F32", x)
        CASES.append(
            Case(
                prog,
                f"fpext F32 -> F64 ({x32!r})",
                "F64",
                None,
                [],
                f"std::intrinsics::fpext[64_u32](hide({f_lit('F32', x32)}))",
                x32,
            )
        )
        CASES.append(
            Case(
                prog,
                f"fptrunc F64 -> F32 ({x!r})",
                "F32",
                None,
                [],
                f"std::intrinsics::fptrunc[32_u32](hide({f_lit('F64', x)}))",
                x32,
            )
        )
    covers(
        "std.num.sized_integer.SizedInteger.is_zero",
        "std.num.sized_floating_point.SizedFloatingPoint.is_zero",
        "std.num.sized_floating_point.SizedFloatingPoint.is_one",
    )


def atomic_cases() -> None:
    prog = "atomics"
    A = "std::threading::atomic::Atom"
    covers(
        "std.threading.atomic.atomic_load_inner",
        "std.threading.atomic.atomic_store_inner",
        "std.threading.atomic.atomic_compex_inner",
        "std.threading.atomic.atomic_compex_weak_inner",
        "std.threading.atomic.atomic_fence_inner",
    )
    seq = []
    # Each: (name, type, start, method, argument, returned, value after)
    for t in ("S32", "U64", "S8"):
        for meth, arg, after in (
            ("fetch_add", 5, lambda s, a: s + a),
            ("fetch_sub", 5, lambda s, a: s - a),
            ("fetch_and", 6, lambda s, a: s & a),
            ("fetch_or", 6, lambda s, a: s | a),
            ("fetch_xor", 6, lambda s, a: s ^ a),
            ("fetch_nand", 6, lambda s, a: ~(s & a)),
            ("exchange", 9, lambda s, a: a),
            ("fetch_min", 3, min),
            ("fetch_max", 30, max),
        ):
            seq.append((t, 12, meth, arg, 12, wrap(t, after(12, arg))))
    for t, start, meth, arg, ret, after in seq:
        # The registry names the min and max by signedness ("fetch_smin", "fetch_umax").
        kind = ("s" if INTS[t][1] else "u") if meth in ("fetch_min", "fetch_max") else ""
        covers(f"std.threading.atomic.Atom.{meth.replace('fetch_', 'fetch_' + kind)}")
        lt = int_lit(t, start)
        CASES.append(
            Case(
                prog,
                f"{meth} Atom[{t}] ({start}, {arg})",
                t,
                None,
                [
                    f"let at = {A}[{t}]::new(hide({lt}))",
                    f"let old = at.{meth}(hide({int_lit(t, arg)}))",
                    f'case at.load() != {int_lit(t, after)} {{ bad = bad + note("{meth} Atom[{t}] ({start}, {arg}): value after") }}',
                    "std::mem::ops::drop(at)",
                ],
                "old",
                ret,
            )
        )
    for t in ("S32",):
        CASES.append(
            Case(
                prog,
                f"compex Atom[{t}] (12, 12 -> 4)",
                t,
                None,
                [
                    f"let at = {A}[{t}]::new(hide(12_s32))",
                    "let (seen, done) = at.compex(hide(12_s32), hide(4_s32))",
                    'case done != true or at.load() != 4_s32 { bad = bad + note("compex Atom[S32]: swapped") }',
                    "std::mem::ops::drop(at)",
                ],
                "seen",
                12,
            )
        )
        CASES.append(
            Case(
                prog,
                f"compex Atom[{t}] (12, 7 -> 4)",
                t,
                None,
                [
                    f"let at = {A}[{t}]::new(hide(12_s32))",
                    "let (seen, done) = at.compex(hide(7_s32), hide(4_s32))",
                    'case done != false or at.load() != 12_s32 { bad = bad + note("compex Atom[S32]: kept") }',
                    "std::mem::ops::drop(at)",
                ],
                "seen",
                12,
            )
        )
    for t in ("F64",):
        for meth, arg, after in (
            ("fetch_add", 1.5, 3.75),
            ("fetch_sub", 1.5, 0.75),
            ("fetch_min", 1.0, 1.0),
            ("fetch_max", 9.5, 9.5),
        ):
            covers(f"std.threading.atomic.Atom.{meth.replace('fetch_', 'fetch_f')}")
            CASES.append(
                Case(
                    prog,
                    f"{meth} Atom[{t}] (2.25, {arg})",
                    t,
                    None,
                    [
                        f"let at = {A}[{t}]::new(hide({f_lit(t, 2.25)}))",
                        f"let old = at.{meth}(hide({f_lit(t, arg)}))",
                        f'case same_{t}(at.load(), {f_lit(t, after)}).not {{ bad = bad + note("{meth} Atom[{t}]: value after") }}',
                        "std::mem::ops::drop(at)",
                    ],
                    "old",
                    2.25,
                )
            )
    CASES.append(
        Case(
            prog,
            "fetch_not Atom[Bool] (false)",
            "Bool",
            None,
            [
                f"let at = {A}[Bool]::new(hide(false))",
                "let old = at.fetch_not()",
                'case at.load() != true { bad = bad + note("fetch_not Atom[Bool]: value after") }',
                "std::mem::ops::drop(at)",
            ],
            "old",
            False,
        )
    )
    covers("std.threading.atomic.Atom.fetch_not", "std.threading.atomic.Atom.is_lock_free")
    # Every "Bool" operation, each checked for what it leaves behind too: an atomic on a "Bool" works on its byte.
    ops = {
        "exchange": lambda s, a: a,
        "fetch_and": lambda s, a: s and a,
        "fetch_or": lambda s, a: s or a,
        "fetch_xor": lambda s, a: s != a,
        "fetch_nand": lambda s, a: not (s and a),
    }
    lit = {True: "true", False: "false"}
    for meth, after in ops.items():
        for start in (False, True):
            for arg in (False, True):
                name = f"{meth} Atom[Bool] ({lit[start]}, {lit[arg]})"
                CASES.append(
                    Case(
                        prog,
                        name,
                        "Bool",
                        None,
                        [
                            f"let at = {A}[Bool]::new(hide({lit[start]}))",
                            f"let old = at.{meth}(hide({lit[arg]}))",
                            f'case at.load() != {lit[after(start, arg)]} {{ bad = bad + note("{name}: value after") }}',
                            "std::mem::ops::drop(at)",
                        ],
                        "old",
                        start,
                    )
                )
    for start, expect, new_val in ((True, True, False), (True, False, False)):
        name = f"compex Atom[Bool] ({lit[start]}, {lit[expect]} -> {lit[new_val]})"
        swapped = start == expect
        CASES.append(
            Case(
                prog,
                name,
                "Bool",
                None,
                [
                    f"let at = {A}[Bool]::new(hide({lit[start]}))",
                    f"let (seen, done) = at.compex(hide({lit[expect]}), hide({lit[new_val]}))",
                    f"case done != {lit[swapped]} or at.load() != {lit[new_val if swapped else start]} "
                    f'{{ bad = bad + note("{name}: swapped") }}',
                    "std::mem::ops::drop(at)",
                ],
                "seen",
                start,
            )
        )
    CASES.append(
        Case(
            prog,
            "store Atom[Bool] (false, true)",
            "Bool",
            None,
            [
                f"let at = {A}[Bool]::new(hide(false))",
                "at.store(hide(true))",
                "let after = at.load()",
                "std::mem::ops::drop(at)",
            ],
            "after",
            True,
        )
    )
    CASES.append(Case(prog, "is_lock_free Atom[S32]", "Bool", None, [], f"{A}[S32]::is_lock_free()", True))


# -- Exclusions -------------------------------------------------------------------------------------------------------
# Intrinsics with no value to compare: they move, borrow, index or drop, and are covered by the gtest suite and the STL
# tests rather than here. Matched by prefix.
EXCLUDED: dict[str, str] = {
    "std.array.": "array storage and iteration",
    "std.vector.": "forwarding to the elements",
    "std.view.": "borrowed views",
    "std.string_view.": "borrowed slices",
    "std.generator.": "coroutine resumption",
    "std.function.": "closure storage",
    "std.cffi.": "C closures",
    "std.debug.": "a breakpoint",
    "std.mem.pointer.": "raw pointers",
    "std.mem.raw_buf.": "raw buffers",
    "std.mem.volatile.": "volatile access, which this check reads its inputs through",
    "std.mem.ops.drop": "destruction",
    "std.mem.ops.swap": "moves",
    "std.mem.ops.replace": "moves",
    "std.mem.ops.size_of_val": "the run-time form of size_of",
    "std.mem.ops.align_of_val": "the run-time form of align_of",
    "std.single.": "a singleton",
    "std.allocator.": "allocation",
    "std.abort.": "aborting",
}

# -- Known failures ---------------------------------------------------------------------------------------------------
# A case listed here reports "known" when it fails in the listed way, and fails the check once it passes, so the entry
# is removed with the fix. "build" means its program does not build: those cases get programs of their own.
# (case name pattern, "build"/"comptime"/"runtime", reason)
KNOWN: list[tuple[str, str, str]] = []


def known(case: str, side: str) -> str | None:
    for pattern, how, why in KNOWN:
        if re.search(pattern, case) and how == side:
            return why
    return None


def entries(case: str) -> list[int]:
    return [i for i, (pattern, _, _) in enumerate(KNOWN) if re.search(pattern, case)]


# -- Emit -------------------------------------------------------------------------------------------------------------

# Each float type compared by its bits, so -0.0 and 0.0 differ.
SAME = "".join(
    f"fun same_{t}(a: {t}, b: {t}) -> Bool {{\n"
    f"    ret std::intrinsics::bit_cast[{bits}](a) == std::intrinsics::bit_cast[{bits}](b)\n}}\n\n"
    for t, (_, _, bits) in FLOATS.items()
)

PRELUDE = """fun hide[T: Copy](x: T) -> T {
    let v = std::mem::volatile::Vol[T]::new(x)
    let out = v.read()
    std::mem::ops::drop(v)
    ret out
}

fun note(what: &StrView) -> S32 {
    std::console::println(what)
    ret 1_s32
}


fun near_F32(a: F32, b: F32, tol: F32) -> Bool {
    ret std::intrinsics::float_abs(a - b) <= tol * std::intrinsics::float_max(std::intrinsics::float_abs(b), 1.0_f32)
}

fun near_F64(a: F64, b: F64, tol: F64) -> Bool {
    ret std::intrinsics::float_abs(a - b) <= tol * std::intrinsics::float_max(std::intrinsics::float_abs(b), 1.0_f64)
}
"""


def value_lit(ty: str, v: Value) -> str:
    if ty == "Bool":
        return "true" if v else "false"
    if isinstance(v, Decimal):
        return f_lit(ty, v)
    if ty in FLOATS:
        if math.isinf(v):
            one, zero = f_lit(ty, -1.0 if v < 0 else 1.0), f_lit(ty, 0.0)
            return f"std::intrinsics::float_div(hide({one}), hide({zero}))"
        return f_lit(ty, v)
    assert isinstance(v, int), f"{ty} expects an integer, not {v!r}"
    return int_lit(ty, v)


def differs(ty: str, a: str, b: str) -> str:
    """An s++ expression that is true when a and b differ."""
    return f"same_{ty}({a}, {b}).not" if ty in FLOATS else f"{a} != {b}"


def emit(builtins: str, std_src: str, out: str) -> None:
    int_cases()
    float_cases()
    size_cases()
    f8_cases()
    runtime_int_cases()
    runtime_float_cases()
    atomic_cases()

    # Every intrinsic in the registry is checked here, or excluded with a reason. One std does not declare cannot be
    # called, so it is noted, and its cases dropped.
    names = re.findall(r'Register\(\s*map,\s*"([^"]+)"', Path(builtins).read_text())
    declared = {
        m for f in Path(std_src).rglob("*.spp") for m in re.findall(r'!intrinsic\(name="([^"]+)"', f.read_text())
    }
    undeclared = sorted(set(names) - declared)
    for n in undeclared:
        print(f"note: {n} is registered, but std does not declare it")
    # A case calls std's functions, which are named apart from their intrinsics ("fpext_f8" is "std.intrinsics.fpext").
    callable_fns = {
        m
        for f in Path(std_src).rglob("*.spp")
        for m in re.findall(r'!intrinsic\(name="[^"]+"\)\s*\n(?:\s*!.*\n)*\s*(?:cmp\s+)?fun\s+(\w+)', f.read_text())
    }
    CASES[:] = [
        c for c in CASES if all(x in callable_fns for x in re.findall(r"std::intrinsics::(\w+)", f"{c.cmp} {c.rt_val}"))
    ]
    missing = sorted(
        n for n in set(names) if n not in COVERED and n in declared and not any(n.startswith(p) for p in EXCLUDED)
    )
    if missing:
        print("intrinsics with neither a case nor a reason to exclude them:", file=sys.stderr)
        for n in missing:
            print(f"  {n}", file=sys.stderr)
        sys.exit(2)

    # Known build failures each get a program of their own.
    progs: dict[str, list[Case]] = {}
    for c in CASES:
        entry = next((i for i, (p, how, _) in enumerate(KNOWN) if how == "build" and re.search(p, c.name)), None)
        progs.setdefault(c.prog if entry is None else f"known{entry}-{c.prog}", []).append(c)

    out_dir = Path(out)
    out_dir.mkdir(parents=True, exist_ok=True)
    manifest: dict[str, list[str]] = {}
    for prog, cases in progs.items():
        consts: list[str] = []
        body: list[str] = []
        for i, c in enumerate(cases):
            body.append("    {")
            body.extend(f"        {s}" for s in c.rt_stmts)
            body.append(f"        let r = {c.rt_val}")
            checks: list[tuple[str, str]] = []
            if c.cmp is not None:
                consts.append(f"cmp k{i}: {c.ty} = {c.cmp}")
                checks.append((differs(c.ty, f"k{i}", "r"), "comptime and runtime differ"))
            if c.expected is not None:
                e = value_lit(c.ty, c.expected)
                if c.tol is not None:
                    checks.append(
                        (f"near_{c.ty}(r, {e}, {f_lit(c.ty, c.tol)}).not", "runtime is not the expected value")
                    )
                else:
                    checks.append((differs(c.ty, "r", e), "runtime is not the expected value"))
                if c.cmp is not None:
                    checks.append((differs(c.ty, f"k{i}", e), "comptime is not the expected value"))
            for cond, msg in checks:
                body.append(f'        case {cond} {{ bad = bad + note("{c.name}: {msg}") }}')
            body.append("    }")
        helpers = "".join(HELPERS.get(cases[0].prog, {}).values())
        src = (
            PRELUDE + SAME + helpers + "\n" + "\n".join(consts) + "\n\nfun main() -> Void {\n"
            "    let mut bad = 0_s32\n" + "\n".join(body) + "\n    case bad != 0 { std::process::exit_checked(1) }\n}\n"
        )
        (out_dir / f"{prog}.spp").write_text(src)
        manifest[prog] = [c.name for c in cases]
    (out_dir / "manifest.json").write_text(json.dumps(manifest, indent=1))
    print(
        f"{len(CASES)} cases in {len(progs)} programs; {len(COVERED)} intrinsics covered, "
        f"{len(set(names)) - len([n for n in set(names) if n in COVERED])} excluded"
    )


# -- Judge ------------------------------------------------------------------------------------------------------------

SIDE: dict[str, str] = {
    "comptime and runtime differ": "comptime",
    "comptime is not the expected value": "comptime",
    "runtime is not the expected value": "runtime",
}


def judge(out: str) -> int:
    out_dir = Path(out)
    manifest: dict[str, list[str]] = json.loads((out_dir / "manifest.json").read_text())
    failures: list[str] = []
    known_hits: set[str] = set()
    present: set[int] = set()
    hit: set[int] = set()
    for prog, cases in manifest.items():
        for c in cases:
            present.update(entries(c))
        for mode in ("dev", "rel"):
            status = (out_dir / f"{prog}.{mode}.status").read_text().strip()
            log = (out_dir / f"{prog}.{mode}.log").read_text().replace("\r", "\n")
            if status == "build-failed":
                if all(known(c, "build") for c in cases):
                    hit.update(i for c in cases for i in entries(c) if KNOWN[i][1] == "build")
                    known_hits.add(f"{prog} [{mode}]: does not build: {known(cases[0], 'build')}")
                else:
                    errors = [
                        ln
                        for ln in re.sub(r"\x1b\[[0-9;]*m", "", log).splitlines()
                        if re.match(r"^E\d+:|.*\^|.*!$", ln)
                    ]
                    failures.append(f"{prog} [{mode}]: build failed\n        " + "\n        ".join(errors[:4]))
                continue
            reported = 0
            for line in log.splitlines():
                m = re.match(r"^(.*): ([a-z ]+(?:differ|value)|flag|value after|swapped|kept)$", line)
                if not m:
                    continue
                reported += 1
                case, msg = m.group(1), m.group(2)
                why = known(case, SIDE.get(msg, "runtime"))
                if why:
                    hit.update(i for i in entries(case) if KNOWN[i][1] != "build")
                    known_hits.add(f"{case} [{mode}]: {why}")
                else:
                    failures.append(f"{line} [{mode}]")
            if status != "0" and not reported:
                failures.append(
                    f"{prog} [{mode}]: exited {status} with no report\n        "
                    + "\n        ".join(log.splitlines()[-4:])
                )
    fixed = [
        f'"{KNOWN[i][0]}" ({KNOWN[i][2]}): no case fails any more; remove the entry' for i in sorted(present - hit)
    ]
    for k in sorted(known_hits):
        print(f"known {k}")
    for f in fixed:
        print(f"FIXED {f}")
    for f in failures:
        print(f"FAIL  {f}")
    total = sum(len(c) for c in manifest.values())
    print(f"{total} cases, {len(failures)} failed, {len(known_hits)} known, {len(fixed)} fixed")
    return 1 if failures or fixed else 0


if __name__ == "__main__":
    if sys.argv[1] == "emit":
        emit(sys.argv[2], sys.argv[3], sys.argv[4])
    elif sys.argv[1] == "judge":
        sys.exit(judge(sys.argv[2]))
