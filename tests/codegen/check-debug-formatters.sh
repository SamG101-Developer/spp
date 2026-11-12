#!/usr/bin/env bash

# End-to-end check that the std's debugger formatters (SPP-STL "etc/gdb/spp_std.py" and "etc/lldb/spp_std.py") read
# what the compiler's debug info describes: the layouts of "Str", "StrView", "Vec" and "Char", the owners and locks
# ("Single", "Rc", "Arc", "Weak", "Vol", "Atom", "Mutex", "RwLock"), the big numbers, times and paths ("BigUInt",
# "BigInt", "Duration", "TimePoint", "Path"), and every variant's "{tag, payload}". Builds a scratch project at dev
# for a debugger ("--for-debugger", so no local is lost to the split stack), stops it in a function holding one of
# each, and asserts that:
#   - gdb, with the printers sourced, shows each by its contents ("Some(7)", "\"héllo\"", "Vec(len=2, cap=2) =
#     {10, 20}", a borrowed view as "&\"a view\"", "'é' (U+00E9)"), and each owner and lock by what it holds, with a
#     shared owner's counts ("Rc(6, strong=1, weak=1)", a "Weak" outliving its value as "dropped", "Mutex(10)"), and
#     the rest by their text (a 100-bit "BigUInt" in decimal, "Duration(1.5s)", a "TimePoint" as a UTC date);
#   - lldb, with the formatters imported, shows the same, a vector's element by its index ("nums[1]"), and an owner's
#     value as its child ("arc.value");
#   - gdb without them shows none of it (the negative control: the expectations are the formatters' doing).
# Skipped when the std has no formatters; each debugger's part is skipped where it is not installed. Under CI's
# SPP_CI_REQUIRE_TOOLS, either fails instead. STD_ETC, if set,
# replaces the std's "etc" (formatters not yet pushed to SPP-STL).
#
#   [STD_ETC=../spp-stl/etc] tests/codegen/check-debug-formatters.sh [path/to/spp]

# shellcheck source=tests/codegen/lib.sh
source "$(dirname "${BASH_SOURCE[0]}")/lib.sh" "$@"

LLDB="$(command -v lldb-23 || command -v lldb || echo lldb-23)"
readonly LLDB

# The debugger stops on the line of "let out", after every other local is set.
read -r -d '' PROGRAM <<'SPP'
use std::hints::black_box
use std::llvm::no_inline
use std::rc::rc::Rc
use std::threading::arc::Arc
use std::mem::volatile::Vol
use std::threading::atomic::Atom
use std::threading::mutex::Mutex
use std::threading::rwlock::RwLock
use std::bignum::biguint::BigUInt
use std::bignum::bigint::BigInt
use std::time::duration::Duration
use std::time::time_point::TimePoint
use std::fs::path::Path

cls Point {
  !public x: S32
  !public y: S32
}

!no_inline
fun look(p: &Point, scale: U64, flag: Bool) -> U64 {
  let sum = p.x + p.y
  let t = (scale, flag)
  let maybe: Opt[S32] = Some(val=sum)
  let nothing: Opt[S32] = None
  let either: Bool or S32 = 5
  let arr = [1_u8, 2_u8, 3_u8]
  let byte = 100_u8
  let name = Str::from("héllo")
  let view = "a view"
  let mut nums = Vec[S32]::new()
  nums.append(10)
  nums.append(20)
  let c = 'é'
  let single = Single[Point]::new(Point(x=1, y=2))
  let rc = Rc[S32]::new(6_s32)
  let weak = rc.downgrade()
  let gone = Rc[S32]::new(0_s32)
  let orphan = gone.downgrade()
  std::mem::ops::drop(gone)
  let arc = Arc[Str]::new(Str::from("shared"))
  let vol = Vol[U32]::new(8_u32)
  let atom = Atom[U64]::new(9_u64)
  let mutex = Mutex[S32]::new(10_s32)
  let rw = RwLock[S32]::new(11_s32)
  let big = BigUInt::from(1267650600228229401496703205376_u128)
  let neg = BigInt::from(-12345678901234_s64)
  let span = Duration::from_millis(1500_u64)
  let tiny = Duration::from_micros(250_u64)
  let when = TimePoint::now()
  let path = Path::new("src/main.spp")
  let out = scale * 2_u64
  std::mem::ops::drop(nums)
  std::mem::ops::drop(name)
  std::mem::ops::drop(single)
  std::mem::ops::drop(weak)
  std::mem::ops::drop(orphan)
  std::mem::ops::drop(rc)
  std::mem::ops::drop(arc)
  std::mem::ops::drop(vol)
  std::mem::ops::drop(atom)
  std::mem::ops::drop(mutex)
  std::mem::ops::drop(rw)
  std::mem::ops::drop(big)
  std::mem::ops::drop(neg)
  std::mem::ops::drop(path)
  ret out
}

fun main() -> Void {
  let p = Point(x=3, y=4)
  let r = look(&p, black_box(7_u64), true)
  let s = Str::from(r)
  std::console::println(&s)
  std::mem::ops::drop(s)
  std::mem::ops::drop(p)
}
SPP

# What a debugger shows of each local with the formatters, as gdb and lldb write it.
readonly GDB_EXPECTED=('maybe = Some(7)' 'nothing = None' 'either = S32(5)' 'name = "héllo"' 'view = &"a view"'
    'nums = Vec(len=2, cap=2) = {10, 20}' "c = 'é' (U+00E9)" 'byte = 100'
    'single = Single(main::Point {x: 1, y: 2})' 'rc = Rc(6, strong=1, weak=1)' 'weak = Weak(6, strong=1, weak=1)'
    'orphan = Weak(dropped, strong=0, weak=1)' 'arc = Arc("shared", strong=1, ' 'vol = Vol(8)' 'atom = Atom(9)'
    'mutex = Mutex(10)' 'rw = RwLock(11)' 'big = BigUInt(1267650600228229401496703205376)'
    'neg = BigInt(-12345678901234)' 'span = Duration(1.5s)' 'tiny = Duration(250µs)' 'when = TimePoint(2'
    'path = Path("src/main.spp")')
readonly LLDB_EXPECTED=(') maybe = Some(7)' ') nothing = None' ') either = S32(5)' ') name = "héllo"' '"a view"'
    ') nums = Vec(len=2, cap=2)' "c = 'é' (U+00E9)" ') byte = 100' ') nums[1] = 20'
    ') single = Single({ x: 1, y: 2 })' ') rc = Rc(6, strong=1, weak=1)' ') weak = Weak(6, strong=1, weak=1)'
    ') orphan = Weak(dropped, strong=0, weak=1)' ') arc = Arc("shared", strong=1, ' ') vol = Vol(8)'
    ') atom = Atom(9)' ') mutex = Mutex(10)' ') rw = RwLock(11)' ') arc.value = "shared"' ') mutex.value = 10'
    ') big = BigUInt(1267650600228229401496703205376)' ') neg = BigInt(-12345678901234)' ') span = Duration(1.5s)'
    ') tiny = Duration(250µs)' ') when = TimePoint(2' ') path = Path("src/main.spp")')

# The first of "expected" missing from "log", or nothing.
first_missing() {
    local log="$1"
    shift
    local expected
    for expected in "$@"; do
        grep -qF -- "${expected}" "${log}" || { echo "${expected}"; return; }
    done
}

# gdb stopped on the "let out" line of "exe", run from the project directory "dir", printing the locals; "script" is
# sourced first when given.
gdb_locals() {
    local dir="$1" exe="$2" script="${3:-}"
    local load=()
    [[ -n "${script}" ]] && load=(-ex "source ${script}")
    (cd "${dir}" && LC_ALL=C.UTF-8 gdb -batch -nx "${load[@]}" -ex "break src/main.spp:${STOP}" -ex run \
        -ex 'info locals' "${exe}" 2>&1)
}

lldb_locals() {
    local dir="$1" exe="$2" script="$3"
    (cd "${dir}" && LC_ALL=C.UTF-8 "${LLDB}" -b -o "command script import ${script}" \
        -o "settings set target.source-map . ${dir}" -o "breakpoint set -f src/main.spp -l ${STOP}" -o run \
        -o 'frame variable' -o 'frame variable nums[1] arc.value mutex.value' "${exe}" 2>&1)
}

STOP="$(grep -n 'let out = ' <<< "${PROGRAM}" | cut -d: -f1)"
readonly STOP

dir="${WORK}/formatters"
new_project "${dir}"
printf '%s\n' "${PROGRAM}" > "${dir}/src/main.spp"
if ! build_project "dev" "${dir}" dev --for-debugger; then
    failures=$((failures + 1))
    finish
fi
if [[ -n "${STD_ETC:-}" ]]; then
    rm -rf "${dir}/vcs/std/etc"
    cp -r "${STD_ETC}" "${dir}/vcs/std/etc"
fi
GDB_SCRIPT="${dir}/vcs/std/etc/gdb/spp_std.py"
LLDB_SCRIPT="${dir}/vcs/std/etc/lldb/spp_std.py"
if [[ ! -f "${GDB_SCRIPT}" || ! -f "${LLDB_SCRIPT}" ]]; then
    if [[ -n "${SPP_CI_REQUIRE_TOOLS:-}" ]]; then
        echo "FAIL  the std has no formatters (etc/gdb, etc/lldb), and SPP_CI_REQUIRE_TOOLS is set"
        exit 1
    fi
    echo "skipped: the std has no formatters (etc/gdb, etc/lldb)"
    exit 0
fi

if have_tool gdb "gdb"; then
    gdb_locals "${dir}" "${EXE}" "${GDB_SCRIPT}" > "${dir}/gdb.log"
    missing="$(first_missing "${dir}/gdb.log" "${GDB_EXPECTED[@]}")"
    if [[ -n "${missing}" ]]; then
        fail "gdb: does not show \"${missing}\"" "${dir}/gdb.log"
    else
        ok "gdb: the std's types by their contents"
    fi

    gdb_locals "${dir}" "${EXE}" > "${dir}/gdb-raw.log"
    if ! grep -qF 'maybe = ' "${dir}/gdb-raw.log"; then
        fail "gdb without the printers: no locals shown" "${dir}/gdb-raw.log"
    elif grep -qF -e 'maybe = Some(7)' -e 'Vec(len=' -e '"héllo"' "${dir}/gdb-raw.log"; then
        fail "gdb without the printers: shows what only they should" "${dir}/gdb-raw.log"
    else
        ok "gdb without the printers: the raw fields"
    fi
fi

if have_tool "${LLDB}" "lldb"; then
    lldb_locals "${dir}" "${EXE}" "${LLDB_SCRIPT}" > "${dir}/lldb.log"
    missing="$(first_missing "${dir}/lldb.log" "${LLDB_EXPECTED[@]}")"
    if [[ -n "${missing}" ]]; then
        fail "lldb: does not show \"${missing}\"" "${dir}/lldb.log"
    else
        ok "lldb: the std's types by their contents"
    fi
fi

finish
