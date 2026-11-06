#!/usr/bin/env bash

# End-to-end check of functions and methods passed as values. The unit tests
# only build, so a value that builds but calls the wrong thing is only seen by
# running it. Builds two scratch projects at dev and rel and asserts each call
# through a value returns what a direct call would:
#   - a free function, one named through a namespace, and an intrinsic;
#   - a static method, a "&self" method (FunRef), a "&mut self" method (FunMut)
#     and a by-value "self" method (FunMov);
#   - a std type's static and "&self" methods, named through its full path;
#   - method values held in "let"s;
#   - a generic owner's static and "&self" methods ("Box[S32]::new"), which
#     passed the mock's empty constant on, so the call jumped to null.
#
#   [SPPC_LIB=path/to/libsppc.so] tests/codegen/check-fn-values.sh [path/to/spp]

# shellcheck source=tests/codegen/lib.sh
source "$(dirname "${BASH_SOURCE[0]}")/lib.sh" "$@"

read -r -d '' COMMON <<'SPP'
cls Counter {
    !public n: S32
}

sup Counter {
    !public fun new(n: S32) -> Counter { ret Counter(n=n) }
    !public fun get(&self) -> S32 { ret self.n }
    !public fun add(&mut self, x: S32) -> Void { self.n += x }
    !public fun take(self) -> S32 {
        let Counter(n) = self
        ret n
    }
}

cls Box[T] {
    !public val: T
}

sup [T: Copy] Box[T] {
    !public fun new(val: T) -> Box[T] { ret Box[T](val=val) }
    !public fun get(&self) -> T { ret self.val }
}

fun twice(x: S32) -> S32 { ret x * 2 }

fun check(ok: Bool, what: &StrView) -> Void {
    case ok { std::console::println(what) }
}

fun call_s32(f: FunRef[(S32,), S32], x: S32) -> S32 { ret f(x) }
fun call_bool(f: FunRef[(Bool,), Bool], b: Bool) -> Bool { ret f(b) }
SPP

read -r -d '' PLAIN <<'SPP'
fun make(f: FunRef[(S32,), Counter], n: S32) -> Counter { ret f(n) }
fun read(f: FunRef[(&Counter,), S32], c: &Counter) -> S32 { ret f(c) }
fun bump(mut f: FunMut[(&mut Counter, S32), Void], c: &mut Counter) -> Void {
    f(c, 5)
    std::mem::ops::drop(f)
}
fun consume(f: FunMov[(Counter,), S32], c: Counter) -> S32 { ret f(c) }
fun dur_make(f: FunRef[(U64,), std::time::duration::Duration], x: U64) -> std::time::duration::Duration { ret f(x) }
fun dur_read(f: FunRef[(&std::time::duration::Duration,), U64], d: &std::time::duration::Duration) -> U64 { ret f(d) }

fun main() -> Void {
    check(call_s32(twice, 21) == 42, "free")
    check(call_s32(std::hints::black_box, 41) == 41, "namespaced")
    check(call_bool(std::hints::likely, std::hints::black_box(true)), "intrinsic")

    let mut c = make(Counter::new, 1)
    check(read(Counter::get, &c) == 1, "static and ref method")
    bump(Counter::add, &mut c)
    check(c.get() == 6, "mut method")
    check(consume(Counter::take, c) == 6, "value method")

    let d = dur_make(std::time::duration::Duration::from_millis, 1500_u64)
    check(dur_read(std::time::duration::Duration::as_millis, &d) == 1500_u64, "std type methods")

    let g = Counter::get
    let h: FunRef[(S32,), Counter] = Counter::new
    let e = h(3)
    check(g(&e) == 3, "held in lets")
    std::mem::ops::drop(e)
}
SPP

read -r -d '' GENERIC <<'SPP'
fun make(f: FunRef[(S32,), Box[S32]], n: S32) -> Box[S32] { ret f(n) }
fun read(f: FunRef[(&Box[S32],), S32], b: &Box[S32]) -> S32 { ret f(b) }

fun main() -> Void {
    let b = make(Box[S32]::new, 9)
    check(b.val == 9, "generic owner static method")
    check(read(Box[S32]::get, &b) == 9, "generic owner ref method")
    std::mem::ops::drop(b)
}
SPP

# Build "name" from COMMON and "program" at "mode", run it, and expect each of
# the remaining arguments as a line of its output.
expect_lines() {
    local name="$1" program="$2" mode="$3"
    shift 3
    local dir="${WORK}/${name}-${mode}"
    new_project "${dir}"
    printf '%s\n\n%s\n' "${COMMON}" "${program}" > "${dir}/src/main.spp"
    if ! build_project "${name} (${mode})" "${dir}" "${mode}"; then
        failures=$((failures + 1))
        return
    fi
    run_exe "${EXE}" "${dir}/run.log"
    local line
    for line in "$@"; do
        if grep -qx "${line}" "${dir}/run.log"; then
            ok "${name} (${mode}): ${line}"
        else
            fail "${name} (${mode}): \"${line}\" gave the wrong value or did not run (exit ${RC})" "${dir}/run.log"
        fi
    done
}

for mode in dev rel; do
    expect_lines plain "${PLAIN}" "${mode}" \
        "free" "namespaced" "intrinsic" "static and ref method" "mut method" "value method" "std type methods" \
        "held in lets"
    expect_lines generic "${GENERIC}" "${mode}" "generic owner static method" "generic owner ref method"
done

finish
