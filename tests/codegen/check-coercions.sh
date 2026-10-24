#!/usr/bin/env bash

# End-to-end check of the coercion into a slot of a declared type ("codegen::CoerceToSlot"). A variant member must
# read back as the member it was, and a named function must call that function, at every slot a value can land in:
# "let", "ret", an assignment, an argument, a default argument, an attribute, an attribute assignment, a "gen", a send
# value, a "case" branch (assigned and as an argument), a closure's body, a loop's "exit" and "else", an array element
# (written out and repeated), a tuple element, and a "cmp" (a constant). gtest builds and verifies these; only running
# them shows a value copied into the slot untagged, or a call through a function value that was never built.
#
#   [SPPC_LIB=path/to/libsppc.so] tests/codegen/check-coercions.sh [path/to/spp]

# shellcheck source=tests/codegen/lib.sh
source "$(dirname "${BASH_SOURCE[0]}")/lib.sh" "$@"

# What each program prints its results with.
read -r -d '' PRELUDE <<'EOF'
fun show(n: U64) -> Void {
    let s = Str::from(n)
    std::console::println(&s)
    std::mem::ops::drop(s)
}

fun showo(o: Opt[U64]) -> Void {
    case o of {
        is Some[U64](val) { show(val) }
        else { std::console::println("none") }
    }
}

fun g(x: U64) -> U64 { ret x + 1_u64 }

fun h(x: U64) -> U64 { ret x + 2_u64 }

fun ident[T](x: T) -> T { ret x }

fun call_ref(f: &FunRef[(U64,), U64]) -> U64 { ret f(10_u64) }
EOF

# Build "name" at "mode" from the prelude and "body", run it, and compare what it prints against "want".
check_program() {
    local name="$1" mode="$2" body="$3" want="$4"
    local dir="${WORK}/${name}-${mode}"
    new_project "${dir}"
    printf '%s\n\n%s\n' "${PRELUDE}" "${body}" > "${dir}/src/main.spp"
    if ! build_project "${name} [${mode}]" "${dir}" "${mode}"; then
        failures=$((failures + 1))
        return
    fi
    run_exe "${EXE}" "${dir}/run.log"
    grep -v '^  #[0-9]' "${dir}/run.log" > "${dir}/got.log"
    diff -u --label want --label got <(printf '%s\n' "${want}") "${dir}/got.log" > "${dir}/run.diff"
    local same=$?
    if (( RC != 0 || same != 0 )); then
        fail "${name} [${mode}]: printed the wrong values (exit ${RC})" "${dir}/run.diff"
    else
        ok "${name} [${mode}]"
    fi
}

read -r -d '' SLOTS <<'EOF'
cmp cmp_variant: Opt[U64] = Some(val=19_u64)

cmp cmp_text: U64 or &StrView = "text"

cmp cmp_tuple: (Opt[U64], U64) = (Some(val=20_u64), 21_u64)

cmp cmp_array: [Opt[U64]; 2_uz] = [Some(val=22_u64), None]

cmp cmp_none: Opt[U64] = None

cmp cmp_none_tuple: (Opt[U64], U64) = (None, 24_u64)

cmp cmp_repeated: [Opt[U64]; 2_uz] = [Some(val=25_u64); 2_uz]

cmp cmp_tuple_of_repeated: (Opt[U64], [Opt[U64]; 2_uz]) = (None, [Some(val=26_u64); 2_uz])

cmp cmp_array_of_tuples: [(Opt[U64], U64); 2_uz] = [(Some(val=27_u64), 28_u64), (None, 29_u64)]

fun ret_variant() -> Opt[U64] { ret Some(val=1_u64) }

fun ret_fn() -> FunRef[(U64,), U64] { ret h }

fun default_arg(o: Opt[U64] = Some(val=2_u64)) -> Void { showo(o) }

fun takes_fn(f: FunRef[(U64,), U64]) -> U64 { ret f(10_u64) }

cls A {
    !public
    o: Opt[U64]
    !public
    f: FunRef[(U64,), U64]
}

cor gen_variant() -> Gen[Opt[U64]] {
    gen Some(val=3_u64)
}

cor gen_fn() -> Gen[FunRef[(U64,), U64]] {
    gen h
}

cor echo() -> Gen[U64, Opt[U64]] {
    let s = gen 1_u64
    showo(s)
}

fun loop_else(c: Bool, n: Opt[U64]) -> Opt[U64] {
    let x = loop c {
        exit n
    } else {
        Some(val=4_u64)
    }
    ret x
}

fun loop_exits(b: Bool, n: Opt[U64]) -> Opt[U64] {
    let x = loop true {
        case b of {
            == true { exit n }
            else { exit Some(val=17_u64) }
        }
    }
    ret x
}

fun typed_loop_exits(b: Bool, n: Opt[U64]) -> Opt[U64] {
    let x: Opt[U64] = loop true {
        case b of {
            == true { exit Some(val=18_u64) }
            else { exit n }
        }
    }
    ret x
}

fun loop_exit_fn() -> U64 {
    let f: FunRef[(U64,), U64] = loop true {
        exit h
    }
    ret f(10_u64)
}

fun main() -> Void {
    let n: Opt[U64] = None

    let lv: Opt[U64] = Some(val=5_u64)
    showo(lv)
    let lf: FunRef[(U64,), U64] = h
    show(lf(10_u64))

    showo(ret_variant())
    show(ret_fn()(10_u64))

    let mut av: Opt[U64] = None
    av = Some(val=6_u64)
    showo(av)
    let mut af: FunRef[(U64,), U64] = g
    af = h
    show(af(10_u64))

    showo(Some(val=7_u64))
    show(takes_fn(h))
    default_arg()

    let mut a = A(o=Some(val=8_u64), f=h)
    showo(a.o)
    show(a.f(10_u64))
    a.o = Some(val=9_u64)
    showo(a.o)
    std::mem::ops::drop(a)

    let mut gv = gen_variant()
    let rv = gv.res()
    case rv of {
        is Opt[U64](..) { showo(rv) }
        else { }
    }
    std::mem::ops::drop(gv)
    let mut gf = gen_fn()
    let rf = gf.res()
    case rf of {
        is FunRef[(U64,), U64](..) { show(rf(10_u64)) }
        else { }
    }
    std::mem::ops::drop(gf)

    let mut e = echo()
    let e1 = e.res(None)
    let e2 = e.res(Some(val=10_u64))
    std::mem::ops::drop(e)

    let b = true
    let cv: Opt[U64] = case b of {
        == false { n }
        else { Some(val=11_u64) }
    }
    showo(cv)
    showo(case b of {
        == false { n }
        else { Some(val=12_u64) }
    })

    let clv = () -> Opt[U64] { Some(val=13_u64) }
    showo(clv())
    std::mem::ops::drop(clv)
    let clf = () -> FunRef[(U64,), U64] { h }
    show(clf()(10_u64))
    std::mem::ops::drop(clf)

    showo(loop_else(false, n))
    show(loop_exit_fn())
    showo(loop_exits(true, n))
    showo(loop_exits(false, n))
    showo(typed_loop_exits(true, n))
    showo(typed_loop_exits(false, n))

    let arr: [Opt[U64]; 2_uz] = [n, Some(val=14_u64)]
    let [arr0, arr1] = arr
    showo(arr0)
    showo(arr1)
    let arf: [FunRef[(U64,), U64]; 1_uz] = [h]
    show(call_ref(arf[0_uz]))
    let rep: [Opt[U64]; 2_uz] = [Some(val=15_u64); 2_uz]
    let [rep0, rep1] = rep
    showo(rep1)
    let repf: [FunRef[(U64,), U64]; 2_uz] = [h; 2_uz]
    show(call_ref(repf[1_uz]))

    let tv: (Opt[U64], U64) = (Some(val=16_u64), 1_u64)
    showo(tv.0)
    let tf: (FunRef[(U64,), U64], U64) = (h, 1_u64)
    show(call_ref(&tf.0))

    showo(cmp_variant)
    case cmp_text of {
        is &StrView(..) { std::console::println(cmp_text) }
        else { }
    }
    showo(cmp_tuple.0)
    show(cmp_tuple.1)
    let [ca0, ca1] = cmp_array
    showo(ca0)
    showo(ca1)
    showo(cmp_none)
    showo(cmp_none_tuple.0)
    show(cmp_none_tuple.1)

    let [cr0, cr1] = cmp_repeated
    showo(cr1)
    let (ctr0, ctr1) = cmp_tuple_of_repeated
    let [ctr10, ctr11] = ctr1
    showo(ctr0)
    showo(ctr11)
    let [cat0, cat1] = cmp_array_of_tuples
    showo(cat0.0)
    show(cat0.1)
    showo(cat1.0)
    show(cat1.1)

    let ltr: (Opt[U64], [Opt[U64]; 2_uz]) = (Some(val=30_u64), [Some(val=31_u64); 2_uz])
    let (ltr0, ltr1) = ltr
    let [ltr10, ltr11] = ltr1
    showo(ltr0)
    showo(ltr11)
    let lat: [(Opt[U64], U64); 2_uz] = [(Some(val=32_u64), 33_u64), (n, 34_u64)]
    let [lat0, lat1] = lat
    showo(lat0.0)
    showo(lat1.0)
    show(lat1.1)
}
EOF

read -r -d '' SLOTS_WANT <<'EOF'
5
12
1
12
6
12
7
12
2
8
12
9
3
12
10
11
12
13
12
4
12
none
17
18
none
none
14
12
15
12
16
12
19
text
20
21
22
none
none
none
24
25
none
26
27
28
none
29
30
31
32
none
34
EOF

for mode in dev rel; do
    check_program "slots" "${mode}" "${SLOTS}" "${SLOTS_WANT}"
done

# One slot each, printing 12: a generic function named as a value, instantiated where it is named
# ("fn_values::InstantiateFnValue"), and distinct functions of one signature, each kept as itself.
fn_case() {
    check_program "$1" dev "$2" "12"
}

fn_case "generic-fn-in-gen" '
cor c() -> Gen[FunRef[(U64,), U64]] {
    gen ident
}
fun main() -> Void {
    let mut i = c()
    let r = i.res()
    case r of {
        is FunRef[(U64,), U64](..) { show(r(12_u64)) }
        else { }
    }
    std::mem::ops::drop(i)
}'

fn_case "generic-fn-in-case-branch" '
fun main() -> Void {
    let b = true
    let f: FunRef[(U64,), U64] = case b of {
        == true { ident }
        else { ident }
    }
    show(f(12_u64))
}'

fn_case "generic-fn-in-closure-body" '
fun main() -> Void {
    let c = () -> FunRef[(U64,), U64] { ident }
    show(c()(12_u64))
    std::mem::ops::drop(c)
}'

fn_case "generic-fn-in-tuple" '
fun main() -> Void {
    let t: (FunRef[(U64,), U64], U64) = (ident, 1_u64)
    show(call_ref(&t.0) + 2_u64)
}'

fn_case "generic-fn-in-array" '
fun main() -> Void {
    let a: [FunRef[(U64,), U64]; 1_uz] = [ident]
    show(call_ref(a[0_uz]) + 2_u64)
}'

fn_case "distinct-fns-in-array" '
fun main() -> Void {
    let a: [FunRef[(U64,), U64]; 2_uz] = [g, h]
    show(call_ref(a[1_uz]))
}'

finish
