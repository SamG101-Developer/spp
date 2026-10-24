#!/usr/bin/env bash

# End-to-end check that dropping a value runs every destructor it owns. A closure dropped without being called drops
# what it captured by value, newest first, through the destructor its environment's header points to; one that is
# called leaves that to its body; a borrowed or copied capture is not dropped. A generator destroyed at a "gen" drops
# what its frame still owns there, innermost scope and newest first, after the "defer"s that destroying it runs; what
# it yielded, consumed, or has not declared yet is not dropped, one partly moved loses only what is left of it, and
# one that finished drops nothing more. The unit
# tests only build these, so a destructor that never runs is only seen by running it.
#
#   [SPPC_LIB=path/to/libsppc.so] tests/codegen/check-drops.sh [path/to/spp]

# shellcheck source=tests/codegen/lib.sh
source "$(dirname "${BASH_SOURCE[0]}")/lib.sh" "$@"

read -r -d '' PROGRAM <<'SPP'
cls Noisy {
    !public n: U64
}

sup Noisy ext std::ops::drop::Drop {
    fun drop(self) -> Void {
        let s = Str::from(self.n)
        std::console::print("dropped ")
        std::console::println(&s)
        std::mem::ops::drop(s)
        let Noisy(..) = self
    }
}

cls Trio {
    !public a: Noisy
    !public b: Noisy
    !public c: Noisy
}

cls Outer {
    !public t: Trio
    !public d: Noisy
}

fun consume(x: Noisy) -> U64 {
    let n = x.n
    std::mem::ops::drop(x)
    ret n
}

cor param(x: Noisy) -> Gen[U64] {
    gen 1_u64
    std::mem::ops::drop(x)
}

cor locals() -> Gen[U64] {
    let a = Noisy(n=10_u64)
    let b = Noisy(n=11_u64)
    gen 1_u64
    let c = Noisy(n=12_u64)
    std::mem::ops::drop(a)
    std::mem::ops::drop(b)
    std::mem::ops::drop(c)
}

cor yields() -> Gen[Noisy] {
    let a = Noisy(n=20_u64)
    gen a
}

cor consumed_before() -> Gen[U64] {
    let a = Noisy(n=30_u64)
    std::mem::ops::drop(a)
    gen 1_u64
}

cor deferred() -> Gen[U64] {
    let a = Noisy(n=40_u64)
    defer std::mem::ops::drop(a)
    gen 1_u64
}

cor nested() -> Gen[U64] {
    let outer = Noisy(n=50_u64)
    {
        let inner = Noisy(n=51_u64)
        gen 1_u64
        std::mem::ops::drop(inner)
    }
    std::mem::ops::drop(outer)
}

cor in_loop() -> Gen[U64] {
    let mut i = 0_u64
    loop i < 3_u64 {
        let v = Noisy(n=60_u64 + i)
        gen i
        std::mem::ops::drop(v)
        i += 1_u64
    }
}

cor generic[T](x: T) -> Gen[U64] {
    gen 1_u64
    std::mem::ops::drop(x)
}

cor partial() -> Gen[U64] {
    let t = Trio(a=Noisy(n=100_u64), b=Noisy(n=101_u64), c=Noisy(n=102_u64))
    std::mem::ops::drop(t.b)
    gen 1_u64
    std::mem::ops::drop(t.a)
    std::mem::ops::drop(t.c)
}

cor partial_nested() -> Gen[U64] {
    let o = Outer(t=Trio(a=Noisy(n=110_u64), b=Noisy(n=111_u64), c=Noisy(n=112_u64)), d=Noisy(n=113_u64))
    std::mem::ops::drop(o.t.a)
    gen 1_u64
    std::mem::ops::drop(o.t.b)
    std::mem::ops::drop(o.t.c)
    std::mem::ops::drop(o.d)
}

fun main() -> Void {
    std::console::println("closure dropped uncalled")
    let a = Noisy(n=1_u64)
    let c1 = (y: U64 caps a) y
    std::mem::ops::drop(c1)

    std::console::println("two captures, dropped uncalled")
    let b1 = Noisy(n=2_u64)
    let b2 = Noisy(n=3_u64)
    let c2 = (y: U64 caps b1, b2) y
    std::mem::ops::drop(c2)

    std::console::println("closure called, its body consumes")
    let d = Noisy(n=4_u64)
    let c3 = (caps d) -> U64 { consume(d) }
    let r = c3()

    std::console::println("borrowed capture")
    let e = Noisy(n=5_u64)
    let c4 = (caps &e) -> U64 { 7_u64 }
    std::mem::ops::drop(c4)
    std::mem::ops::drop(e)

    std::console::println("copied capture")
    let k = 9_u64
    let c5 = (y: U64 caps k) y + k
    std::mem::ops::drop(c5)

    std::console::println("generator: parameter")
    let mut g1 = param(Noisy(n=1_u64))
    let r1 = g1.res()
    std::mem::ops::drop(g1)

    std::console::println("generator: locals declared before")
    let mut g2 = locals()
    let r2 = g2.res()
    std::mem::ops::drop(g2)

    std::console::println("generator: yielded value")
    let mut g3 = yields()
    let r3 = g3.res()
    case r3 of {
        is Noisy(..) { std::mem::ops::drop(r3) }
        else { }
    }
    std::mem::ops::drop(g3)

    std::console::println("generator: consumed before")
    let mut g4 = consumed_before()
    let r4 = g4.res()
    std::mem::ops::drop(g4)

    std::console::println("generator: deferred")
    let mut g5 = deferred()
    let r5 = g5.res()
    std::mem::ops::drop(g5)

    std::console::println("generator: nested scopes")
    let mut g6 = nested()
    let r6 = g6.res()
    std::mem::ops::drop(g6)

    std::console::println("generator: loop, destroyed on the second iteration")
    let mut g7 = in_loop()
    let r7a = g7.res()
    let r7b = g7.res()
    std::mem::ops::drop(g7)

    std::console::println("generator: generic")
    let mut g8 = generic(Noisy(n=80_u64))
    let r8 = g8.res()
    std::mem::ops::drop(g8)

    std::console::println("generator: finished")
    let mut g9 = param(Noisy(n=90_u64))
    let r9a = g9.res()
    let r9b = g9.res()
    std::mem::ops::drop(g9)

    std::console::println("generator: partly moved")
    let mut g10 = partial()
    let r10 = g10.res()
    std::mem::ops::drop(g10)

    std::console::println("generator: partly moved, nested")
    let mut g11 = partial_nested()
    let r11 = g11.res()
    std::mem::ops::drop(g11)
    std::console::println("end")
}
SPP

read -r -d '' WANT <<'OUT'
closure dropped uncalled
dropped 1
two captures, dropped uncalled
dropped 3
dropped 2
closure called, its body consumes
dropped 4
borrowed capture
dropped 5
copied capture
generator: parameter
dropped 1
generator: locals declared before
dropped 11
dropped 10
generator: yielded value
dropped 20
generator: consumed before
dropped 30
generator: deferred
dropped 40
generator: nested scopes
dropped 51
dropped 50
generator: loop, destroyed on the second iteration
dropped 60
dropped 61
generator: generic
dropped 80
generator: finished
dropped 90
generator: partly moved
dropped 101
dropped 102
dropped 100
generator: partly moved, nested
dropped 110
dropped 113
dropped 112
dropped 111
end
OUT

for mode in dev rel; do
    dir="${WORK}/drops-${mode}"
    new_project "${dir}"
    printf '%s\n' "${PROGRAM}" > "${dir}/src/main.spp"
    if ! build_project "drops [${mode}]" "${dir}" "${mode}"; then
        failures=$((failures + 1))
        continue
    fi
    run_exe "${EXE}" "${dir}/run.log"
    grep -v '^  #[0-9]' "${dir}/run.log" > "${dir}/got.log"
    diff -u --label want --label got <(printf '%s\n' "${WANT}") "${dir}/got.log" > "${dir}/run.diff"
    same=$?
    if (( RC != 0 || same != 0 )); then
        fail "drops [${mode}]: destructors ran differently (exit ${RC})" "${dir}/run.diff"
    else
        ok "drops [${mode}]"
    fi
done

finish
