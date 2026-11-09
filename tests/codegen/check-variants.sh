#!/usr/bin/env bash

# End-to-end check of variant widening and narrowing ("codegen::WidenVariant", "codegen::NarrowVariant"). A variant
# passed into a wider one ("Apple or Mid" into "Apple or Mid or Zed") keeps its member and payload, though the wider
# variant may number its members differently; a pattern naming a narrower variant ("is AM(..)") reads the subject as
# it, re-tagged. Each source leaves out a different member of the target, so whatever the numbering, at least one
# widening translates its tag, and the IR is checked for that. Payloads of different sizes and a valueless member
# ("None") are included. gtest builds and verifies these; only running them shows a wrong tag or a short copy.
#
#   [SPPC_LIB=path/to/libsppc.so] tests/codegen/check-variants.sh [path/to/spp]

# shellcheck source=tests/codegen/lib.sh
source "$(dirname "${BASH_SOURCE[0]}")/lib.sh" "$@"

read -r -d '' PROGRAM <<'EOF'
cls Apple { !public v: U64 }

cls Mid {
    !public a: U64
    !public b: U64
}

cls Zed { !public v: U8 }

sup Apple ext Copy { }
sup Mid ext Copy { }
sup Zed ext Copy { }

type AM = Apple or Mid
type AZ = Apple or Zed
type MZ = Mid or Zed
type AMZ = Apple or Mid or Zed
type AN = Apple or None
type ANZ = Apple or None or Zed

fun show(n: U64) -> Void {
    let s = Str::from(n)
    std::console::println(&s)
    std::mem::ops::drop(s)
}

fun show_amz(x: AMZ) -> Void {
    case x of {
        is Apple(v) { show(v) }
        is Mid(a, b) { show(a * 100_u64 + b) }
        is Zed(v) { show(U64::from(v) + 1000_u64) }
    }
}

fun show_anz(x: ANZ) -> Void {
    case x of {
        is Apple(v) { show(v) }
        is Zed(v) { show(U64::from(v) + 1000_u64) }
        else { std::console::println("none") }
    }
}

fun show_am(x: AM) -> Void {
    case x of {
        is Apple(v) { show(v) }
        is Mid(a, b) { show(a * 100_u64 + b) }
    }
}

fun show_az(x: AZ) -> Void {
    case x of {
        is Apple(v) { show(v) }
        is Zed(v) { show(U64::from(v) + 1000_u64) }
    }
}

fun show_mz(x: MZ) -> Void {
    case x of {
        is Mid(a, b) { show(a * 100_u64 + b) }
        is Zed(v) { show(U64::from(v) + 1000_u64) }
    }
}

fun show_an(x: AN) -> Void {
    case x of {
        is Apple(v) { show(v) }
        else { std::console::println("none") }
    }
}

fun widen_am(x: AM) -> AMZ { ret x }
fun widen_az(x: AZ) -> AMZ { ret x }
fun widen_mz(x: MZ) -> AMZ { ret x }
fun widen_an(x: AN) -> ANZ { ret x }

fun narrow(x: AMZ) -> Void {
    case x of {
        is AM(..) { show_am(x) }
        else { std::console::println("not am") }
    }
    case x of {
        is AZ(..) { show_az(x) }
        else { std::console::println("not az") }
    }
    case x of {
        is MZ(..) { show_mz(x) }
        else { std::console::println("not mz") }
    }
}

fun narrow_an(x: ANZ) -> Void {
    case x of {
        is AN(..) { show_an(x) }
        else { std::console::println("not an") }
    }
}

fun main() -> Void {
    show_amz(widen_am(Apple(v=1_u64)))
    show_amz(widen_am(Mid(a=2_u64, b=3_u64)))
    show_amz(widen_az(Apple(v=4_u64)))
    show_amz(widen_az(Zed(v=5_u8)))
    show_amz(widen_mz(Mid(a=6_u64, b=7_u64)))
    show_amz(widen_mz(Zed(v=8_u8)))
    show_anz(widen_an(Apple(v=9_u64)))
    show_anz(widen_an(None))

    narrow(Apple(v=10_u64))
    narrow(Mid(a=11_u64, b=12_u64))
    narrow(Zed(v=13_u8))
    narrow_an(Apple(v=14_u64))
    narrow_an(None)
    narrow_an(Zed(v=15_u8))
}
EOF

read -r -d '' WANT <<'EOF'
1
203
4
1005
607
1008
9
none
10
10
not mz
1112
not az
1112
not am
1013
1013
14
none
not an
EOF

for mode in dev rel; do
    dir="${WORK}/variants-${mode}"
    new_project "${dir}"
    printf '%s\n' "${PROGRAM}" > "${dir}/src/main.spp"
    if ! build_project "variants [${mode}]" "${dir}" "${mode}"; then
        failures=$((failures + 1))
        continue
    fi
    run_exe "${EXE}" "${dir}/run.log"
    grep -v '^  #[0-9]' "${dir}/run.log" > "${dir}/got.log"
    diff -u --label want --label got <(printf '%s\n' "${WANT}") "${dir}/got.log" > "${dir}/run.diff"
    same=$?
    if (( RC != 0 || same != 0 )); then
        fail "variants [${mode}]: printed the wrong values (exit ${RC})" "${dir}/run.diff"
    else
        ok "variants [${mode}]"
    fi

    # At dev, before optimisation folds anything: some widening ("ret.variant") and some narrowing
    # ("case.pattern.narrow") translated their tag, so the translation itself was run, not only the identity.
    if [[ "${mode}" == dev ]]; then
        for what in "ret.variant:widening" "case.pattern.narrow:narrowing"; do
            if grep -rqE "\"${what%%:*}[^\"]*\.to\.tag\" = select" "${dir}/out" --include='*.ll'; then
                ok "variants [dev]: a ${what#*:} translates its tag"
            else
                fail "variants [dev]: no ${what#*:} translated its tag, so the translation went untested" /dev/null
            fi
        done
    fi
done

finish
