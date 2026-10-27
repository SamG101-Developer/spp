#!/usr/bin/env bash

# End-to-end check that a comp generic argument naming a "cmp" constant is the constant's value, at dev and rel: the
# instance it makes reads its parameter back as that value, and is the instance a literal of the value makes. A
# constant is named directly, through its namespace from another module, and inside a comp expression, and one is
# declared from another. Another module's constants and function are imported with "use", directly and through a
# chain of imports, one of a type only its module declares (open bug 47: the import's type was spelled before the
# alias it named was resolved). The unit tests only build, so a parameter read back as zero is only seen by running it
# (open bug 46: the constant was keyed by its spelling, and its instance emitted the parameter as zero).
#
#   tests/codegen/check-comp-args.sh [path/to/spp]

# shellcheck source=tests/codegen/lib.sh
source "$(dirname "${BASH_SOURCE[0]}")/lib.sh" "$@"

read -r -d '' OTHER <<'SPP'
!public
cmp kk: S32 = 7

!public
cmp kj: S32 = kk + 1

!public
type Num = U64

!public
cmp kn: Num = 9_u64

!public
fun twice(x: S32) -> S32 { ret x * 2 }
SPP

read -r -d '' CHAIN <<'SPP'
!public
use other::m::kj

!public
use other::m::twice
SPP

read -r -d '' PROGRAM <<'SPP'
use other::m::kk
use other::m::kn
use other::n::kj
use other::n::twice

cmp k: S32 = 5

cmp k2: S32 = k * 2

fun with_cmp[cmp n: S32]() -> S32 { ret n }

fun show(v: S32) -> Void {
    let s = Str::from(v)
    std::console::println(&s)
    std::mem::ops::drop(s)
}

fun main() -> Void {
    show(with_cmp[k]())
    show(with_cmp[5]())
    show(with_cmp[k2]())
    show(with_cmp[other::m::kk]())
    show(with_cmp[other::m::kj]())
    show(with_cmp[k + 1]())
    show(with_cmp[k2 + k]())
    show(kk)
    show(with_cmp[kj]())
    show(twice(21))
    let n: U64 = kn
    let s = Str::from(n)
    std::console::println(&s)
    std::mem::ops::drop(s)
}
SPP

read -r -d '' WANT <<'OUT'
5
5
10
7
8
6
15
7
8
42
9
OUT

for mode in dev rel; do
    dir="${WORK}/comp-${mode}/comp"
    new_project "${dir}"
    mkdir -p "${dir}/src/other"
    printf '%s\n' "${OTHER}" > "${dir}/src/other/m.spp"
    printf '%s\n' "${CHAIN}" > "${dir}/src/other/n.spp"
    printf '%s\n' "${PROGRAM}" > "${dir}/src/main.spp"
    if ! build_project "comp args [${mode}]" "${dir}" "${mode}"; then
        failures=$((failures + 1))
        continue
    fi
    run_exe "${EXE}" "${dir}/run.log"
    diff -u --label want --label got <(printf '%s\n' "${WANT}") "${dir}/run.log" > "${dir}/run.diff"
    same=$?
    instances="$(grep -c 'define .*with_cmp\[n=5_s32\]' "$(dirname "${OBJ}")/main.ll")"
    if (( RC != 0 || same != 0 )); then
        fail "comp args [${mode}]: read back the wrong values (exit ${RC})" "${dir}/run.diff"
    elif [[ "${instances}" != 1 ]]; then
        fail "comp args [${mode}]: ${instances} instances named n=5_s32 for \"with_cmp[k]\" and \"with_cmp[5]\""
    else
        ok "comp args [${mode}]"
    fi
done

finish
