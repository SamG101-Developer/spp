#!/usr/bin/env bash

# End-to-end check that dropping a value runs every destructor it owns. A closure dropped without being called drops
# what it captured by value, newest first, through the destructor its environment's header points to; one that is called
# leaves that to its body; a borrowed or copied capture is not dropped. A generator destroyed at a "gen" drops what its
# frame still owns there, innermost scope and newest first; what it yielded,
# consumed, or has not declared yet is not dropped, one partly moved loses only what is left of it, and one that
# finished drops nothing more, a local hidden by an "is" binding of the statement it is suspended in included. One
# dropped before it was ever resumed drops its by-value parameters, newest first: its body has not run, so they are all
# it owns, an instantiation of a generic coroutine included. What either drops goes through the value's own destructor,
# even one nothing written calls (a "Vec" drops its elements, not only its buffer). An array moved out element by
# element ("iter_mov", "reverse_iter_mov") yields each in order, and dropped part-way drops only the elements it has not
# yielded. A type extending two bases that each declare an attribute of one name holds and drops both, each filled from
# its own base's default. A second program covers where the compiler drops what nothing moved on (affine ownership):
# scope ends, early returns, overwrites of a local and of a field, a borrowed temporary, the rest of a temporary a part
# was read off, a part a destructure skipped, a discarded temporary, an "is" binding, the paths of a join (a "case" and
# a loop's exit and condition), a loop condition's temporary per evaluation, a "case" over a temporary it keeps, and
# "self" left whole at the end of its own "drop". The unit tests only build these, so a destructor that never runs is
# only seen by running it.
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

cls Wrap[T] {
    !public a: T
}

sup [T] Wrap[T] {
    !public
    cor take(self) -> Gen[U64] {
        gen 1_u64
        std::mem::ops::drop(self)
    }
}

cor two_params(x: Noisy, y: Noisy) -> Gen[U64] {
    gen 1_u64
    std::mem::ops::drop(x)
    std::mem::ops::drop(y)
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

cor shadowed(o: &Opt[Noisy]) -> Gen[U64] {
    let x = Noisy(n=230_u64)
    let b = o is Some[Noisy](&val as x) and {
        gen 1_u64
        true
    }
    std::mem::ops::drop(x)
}

cls BaseB {
    !public a: Noisy = Noisy(n=120_u64)
}

cls BaseC {
    !public a: Noisy = Noisy(n=121_u64)
}

cls Both { }
sup Both ext BaseB { }
sup Both ext BaseC { }

fun main() -> Void {
    std::console::println("closure dropped uncalled")
    let a = Noisy(n=1_u64)
    let c1 = |y: U64 caps a| y
    std::mem::ops::drop(c1)

    std::console::println("two captures, dropped uncalled")
    let b1 = Noisy(n=2_u64)
    let b2 = Noisy(n=3_u64)
    let c2 = |y: U64 caps b1, b2| y
    std::mem::ops::drop(c2)

    std::console::println("closure called, its body consumes")
    let d = Noisy(n=4_u64)
    let c3 = |caps d| -> U64 { consume(d) }
    let r = c3()

    std::console::println("borrowed capture")
    let e = Noisy(n=5_u64)
    let c4 = |caps &e| -> U64 { 7_u64 }
    std::mem::ops::drop(c4)
    std::mem::ops::drop(e)

    std::console::println("copied capture")
    let k = 9_u64
    let c5 = |y: U64 caps k| y + k
    std::mem::ops::drop(c5)

    std::console::println("captured vector, dropped uncalled")
    let mut tv = Vec[Trio]::new()
    tv.append(Trio(a=Noisy(n=6_u64), b=Noisy(n=7_u64), c=Noisy(n=8_u64)))
    let c6 = |y: U64 caps tv| y
    std::mem::ops::drop(c6)

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

    std::console::println("generator: never resumed")
    let g12 = param(Noisy(n=130_u64))
    std::mem::ops::drop(g12)

    std::console::println("generator: never resumed, two parameters")
    let g13 = two_params(Noisy(n=140_u64), Noisy(n=141_u64))
    std::mem::ops::drop(g13)

    std::console::println("generator: never resumed, generic")
    let g14 = generic(Noisy(n=150_u64))
    std::mem::ops::drop(g14)

    std::console::println("generator: never resumed, generic method")
    let g15 = Wrap[Noisy](a=Noisy(n=160_u64)).take()
    std::mem::ops::drop(g15)

    std::console::println("generator: never resumed, the second of a concat")
    let mut v1 = Vec[Noisy]::new()
    v1.append(Noisy(n=170_u64))
    v1.append(Noisy(n=171_u64))
    let mut v2 = Vec[Noisy]::new()
    v2.append(Noisy(n=172_u64))
    let mut g16 = v1.iter_mov().concat(v2.iter_mov())
    let r16 = g16.res()
    std::mem::ops::drop(r16)
    std::mem::ops::drop(g16)

    std::console::println("array: moved out in full")
    let a1 = [Noisy(n=180_u64), Noisy(n=181_u64)]
    loop x in a1.iter_mov() { std::mem::ops::drop(x) }

    std::console::println("array: moved out in reverse")
    let a2 = [Noisy(n=190_u64), Noisy(n=191_u64)]
    loop x in a2.reverse_iter_mov() { std::mem::ops::drop(x) }

    std::console::println("array: left after one")
    let a3 = [Noisy(n=200_u64), Noisy(n=201_u64), Noisy(n=202_u64)]
    loop x in a3.iter_mov() {
        std::mem::ops::drop(x)
        exit
    }

    std::console::println("array: left after one, reversed")
    let a4 = [Noisy(n=210_u64), Noisy(n=211_u64), Noisy(n=212_u64)]
    loop x in a4.reverse_iter_mov() {
        std::mem::ops::drop(x)
        exit
    }

    std::console::println("array: never resumed")
    let g17 = [Noisy(n=220_u64), Noisy(n=221_u64)].iter_mov()
    std::mem::ops::drop(g17)

    std::console::println("generator: a local hidden by an is binding of the suspended statement")
    let o23: Opt[Noisy] = Some(val=Noisy(n=231_u64))
    let mut g23 = shadowed(&o23)
    let r23 = g23.res()
    std::mem::ops::drop(g23)
    std::mem::ops::drop(o23)

    std::console::println("two bases sharing an attribute name")
    let both = Both()
    std::mem::ops::drop(both)
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
captured vector, dropped uncalled
dropped 8
dropped 7
dropped 6
generator: parameter
dropped 1
generator: locals declared before
dropped 11
dropped 10
generator: yielded value
dropped 20
generator: consumed before
dropped 30
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
generator: never resumed
dropped 130
generator: never resumed, two parameters
dropped 141
dropped 140
generator: never resumed, generic
dropped 150
generator: never resumed, generic method
dropped 160
generator: never resumed, the second of a concat
dropped 170
dropped 171
dropped 172
array: moved out in full
dropped 180
dropped 181
array: moved out in reverse
dropped 191
dropped 190
array: left after one
dropped 200
dropped 202
dropped 201
array: left after one, reversed
dropped 212
dropped 211
dropped 210
array: never resumed
dropped 221
dropped 220
generator: a local hidden by an is binding of the suspended statement
dropped 230
dropped 231
two bases sharing an attribute name
dropped 121
dropped 120
end
OUT

# Where the compiler drops what nothing moved on (affine ownership): each case is a function, so what it owns is
# dropped before the next label prints.
read -r -d '' AFFINE <<'SPP'
cls Noisy {
    !public n: U64
}

sup Noisy ext std::ops::drop::Drop {
    fun drop(self) -> Void {
        let s = Str::from(self.n)
        std::console::print("dropped ")
        std::console::println(&s)
        let Noisy(..) = self
    }
}

cls Pair2 {
    !public a: Noisy
    !public b: Noisy
}

cls DropsOwn {
    !public inner: Noisy
}

sup DropsOwn ext std::ops::drop::Drop {
    fun drop(self) -> Void {
        std::console::println("own drop")
    }
}

fun mk(n: U64) -> Noisy { ret Noisy(n=n) }
fun peek(x: &Noisy) -> Void { }
fun take(x: Noisy) -> Void { }
fun small(x: &Noisy) -> Bool { ret x.n < 502_u64 }

fun scope_end() -> Void {
    let x = Noisy(n=300_u64)
}

fun early(flag: Bool) -> Void {
    let a = Noisy(n=301_u64)
    case flag { ret }
    let b = Noisy(n=302_u64)
}

fun overwrite() -> Void {
    let mut o = Noisy(n=310_u64)
    o = Noisy(n=311_u64)
    std::console::println("after write")
}

fun field_overwrite() -> Void {
    let mut p = Pair2(a=Noisy(n=320_u64), b=Noisy(n=321_u64))
    p.a = Noisy(n=322_u64)
    std::console::println("written")
}

fun borrowed_temporary() -> Void {
    peek(&mk(330_u64))
    std::console::println("after statement")
}

fun part_of_temporary() -> Void {
    let a = Pair2(a=Noisy(n=340_u64), b=Noisy(n=341_u64)).a
    std::console::println("after statement")
}

fun skipped_part() -> Void {
    let p = Pair2(a=Noisy(n=350_u64), b=Noisy(n=351_u64))
    let Pair2(a, ..) = p
    std::console::println("after destructure")
}

fun discarded() -> Void {
    mk(360_u64)
    std::console::println("after")
}

fun is_binding() -> Void {
    let o: Opt[Noisy] = Some(val=Noisy(n=370_u64))
    case o is Some(val as v) { std::console::println("matched") }
    std::console::println("after")
}

fun join(flag: Bool, n: U64) -> Void {
    let x = Noisy(n=n)
    case flag { take(x) }
    std::console::println("after case")
}

fun eats(x: Noisy) -> Bool { ret false }
fun holds(x: &Noisy) -> Bool { ret true }

cls InnerN {
    !public b: Noisy
    !public c: Noisy
}

cls OuterN {
    !public a: InnerN
}

fun mk_outer() -> OuterN { ret OuterN(a=InnerN(b=Noisy(n=460_u64), c=Noisy(n=461_u64))) }

fun swap() -> Void {
    let mut a = Noisy(n=440_u64)
    let mut b = Noisy(n=441_u64)
    a, b = b, a
    std::console::println("swapped")
}

fun discarded_block() -> Void {
    let s = Noisy(n=450_u64)
    { s }
    std::console::println("after block")
}

fun field_chain() -> Void {
    let b = mk_outer().a.b
    std::console::println("after chain")
}

fun short_circuit(flag: Bool, n: U64) -> Void {
    let ok = flag and holds(&Noisy(n=n))
    std::console::println("after and")
}

fun nothing() -> Void { }

fun guard_temporary(o: Opt[U64]) -> Void {
    case o of {
        is Some(val) and holds(&Noisy(n=val)) { std::console::println("matched") }
        else { std::console::println("else") }
    }
}

fun closure_temporary() -> Void {
    let f = |x: U64| holds(&Noisy(n=x))
    let b = f(520_u64)
    std::console::println("after call")
}

fun call_mov[F: FunMov[(), Void]](f: F) -> Void {
    f()
    std::console::println("after call")
}

fun closure_captures() -> Void {
    let x = Noisy(n=530_u64)
    call_mov(|caps x| -> Void { std::console::println("in closure") })
    let y = Noisy(n=531_u64)
    let g = |caps y| -> Void { std::console::println("in local closure") }
    g()
    std::console::println("after local call")
}

fun void_tuple() -> Void {
    let t = (nothing(), Noisy(n=540_u64))
    std::console::println("built")
}

fun loop_trailing_place() -> Void {
    let mut i = 0_u64
    loop i < 2_u64 {
        let s = Noisy(n=550_u64 + i)
        i += 1_u64
        s
    }
    std::console::println("after loop")
}

cls Guarded {
    !public a: Noisy
    !public b: Noisy
}

sup Guarded ext std::ops::drop::Drop {
    fun drop(self) -> Void {
        std::console::println("Guarded.drop")
    }
}

sup Guarded {
    !public
    fun close(self) -> Void {
        let Self(..) = self
        std::console::println("closed")
    }
}

fun mk_guarded(n: U64) -> Guarded { ret Guarded(a=Noisy(n=n), b=Noisy(n=n + 1_u64)) }

fun taken_apart() -> Void {
    mk_guarded(600_u64).close()
    let y = mk_guarded(610_u64)
    let Guarded(a, ..) = y
    let Guarded(..) = mk_guarded(620_u64)
    let mut z = mk_guarded(630_u64)
    let Guarded(..) = z
    z = mk_guarded(640_u64)
    std::console::println("end of scope")
}

fun taken_apart_on_one_path(flag: Bool) -> Void {
    let w = mk_guarded(650_u64)
    case flag { let Guarded(..) = w }
    std::console::println("after case")
}

fun vec_of_boxes() -> Void {
    let mut v = Vec[std::single::Single[Noisy]]::new()
    v.append(std::single::Single::new(Noisy(n=660_u64)))
    v.append(std::single::Single::new(Noisy(n=661_u64)))
    std::console::println("filled")
}

fun is_bare(o: Opt[Noisy]) -> Void {
    let b = o is Some(val as y)
    std::console::println("after is")
}

fun is_moves_binding(o: Opt[Noisy]) -> Void {
    let b = o is Some(val as y) and takes(y)
    std::console::println("after is")
}

fun takes(x: Noisy) -> Bool { ret true }

fun mk_opt(n: U64) -> Opt[Noisy] { ret Some(val=Noisy(n=n)) }

fun is_temporary_subject() -> Void {
    let b = mk_opt(720_u64) is Some(val as y) and y.n > 0_u64
    std::console::println("after is")
    let c = mk_opt(721_u64) is Some(val as z) and takes(z)
    std::console::println("after moving is")
}

cls Guarded2 { !public a: Noisy }

sup Guarded2 ext std::ops::drop::Drop {
    fun drop(self) -> Void { std::console::println("Guarded2.drop") }
}

fun look(x: &Noisy) -> Void { }

fun borrowed_field_of_temporary() -> Void {
    look(&Guarded2(a=Noisy(n=730_u64)).a)
    std::console::println("after borrow")
    let n = Guarded2(a=Noisy(n=731_u64)).a.n
    std::console::println("after copy")
}

cls Named2 {
    !public name: Noisy
    !public id: S32
}

fun case_over_temporary_shape() -> Void {
    case Named2(name=Noisy(n=740_u64), id=1) of {
        is Named2(id=1, ..) { std::console::println("matched") }
        else { std::console::println("else") }
    }
}

fun reassign_partial() -> Void {
    let mut p = Pair2(a=Noisy(n=480_u64), b=Noisy(n=481_u64))
    let a = p.a
    p = Pair2(a=Noisy(n=482_u64), b=Noisy(n=483_u64))
    std::console::println("reassigned")
}

fun guard_moves(k: Opt[U64], n: U64) -> Void {
    let x = Noisy(n=n)
    case k of {
        is Some(val) and eats(x) { std::console::println("guarded") }
        else { std::console::println("else") }
    }
    std::console::println("after case")
}

fun loop_exit() -> Void {
    let x = Noisy(n=390_u64)
    let mut i = 0_u64
    loop i < 3_u64 {
        case i == 1_u64 {
            take(x)
            exit
        }
        i += 1_u64
    }
    std::console::println("after loop")
}

fun loop_condition_ends() -> Void {
    let x = Noisy(n=391_u64)
    let mut i = 0_u64
    loop i < 1_u64 {
        case i == 5_u64 {
            take(x)
            exit
        }
        i += 1_u64
    }
    std::console::println("after loop")
}

fun condition_temporary() -> Void {
    let mut i = 0_u64
    loop small(&mk(500_u64 + i)) { i += 1_u64 }
    std::console::println("after loop")
}

fun case_temporary() -> Void {
    case mk(420_u64) of {
        is Noisy(n=1_u64) { std::console::println("one") }
        else { std::console::println("kept") }
    }
}

fun own_drop() -> Void {
    let d = DropsOwn(inner=Noisy(n=400_u64))
}

fun main() -> Void {
    std::console::println("scope end")
    scope_end()
    std::console::println("early return")
    early(true)
    early(false)
    std::console::println("overwrite")
    overwrite()
    std::console::println("field overwrite")
    field_overwrite()
    std::console::println("borrowed temporary")
    borrowed_temporary()
    std::console::println("part of a temporary")
    part_of_temporary()
    std::console::println("skipped destructure part")
    skipped_part()
    std::console::println("discarded temporary")
    discarded()
    std::console::println("is binding")
    is_binding()
    std::console::println("join, taken")
    join(true, 380_u64)
    std::console::println("join, kept")
    join(false, 381_u64)
    std::console::println("guard moves, guard ran")
    guard_moves(Some(val=1_u64), 430_u64)
    std::console::println("guard moves, no match")
    guard_moves(None, 431_u64)
    std::console::println("swap")
    swap()
    std::console::println("discarded block")
    discarded_block()
    std::console::println("field chain off a temporary")
    field_chain()
    std::console::println("short circuit, right side runs")
    short_circuit(true, 470_u64)
    std::console::println("short circuit, right side skipped")
    short_circuit(false, 471_u64)
    std::console::println("reassign a partly moved value")
    reassign_partial()
    std::console::println("guard temporary, guard runs")
    guard_temporary(Some(val=510_u64))
    std::console::println("guard temporary, no match")
    guard_temporary(None)
    std::console::println("closure temporary")
    closure_temporary()
    std::console::println("closure captures")
    closure_captures()
    std::console::println("tuple holding a void")
    void_tuple()
    std::console::println("loop body ending on a place")
    loop_trailing_place()
    std::console::println("taken apart")
    taken_apart()
    std::console::println("taken apart on one path")
    taken_apart_on_one_path(true)
    std::console::println("kept whole on the other")
    taken_apart_on_one_path(false)
    std::console::println("vector of boxes nothing else drops")
    vec_of_boxes()
    std::console::println("is, binding a view, matched")
    is_bare(Some(val=Noisy(n=700_u64)))
    std::console::println("is, binding a view, not matched")
    is_bare(None)
    std::console::println("is, binding moved")
    is_moves_binding(Some(val=Noisy(n=710_u64)))
    is_moves_binding(None)
    std::console::println("is over a temporary")
    is_temporary_subject()
    std::console::println("field of a destructible temporary, borrowed")
    borrowed_field_of_temporary()
    std::console::println("case over a temporary, shape test")
    case_over_temporary_shape()
    std::console::println("loop exit")
    loop_exit()
    std::console::println("loop condition ends")
    loop_condition_ends()
    std::console::println("temporary in a loop condition")
    condition_temporary()
    std::console::println("case over a kept temporary")
    case_temporary()
    std::console::println("own drop, self left whole")
    own_drop()
    std::console::println("end")
}
SPP

read -r -d '' AFFINE_WANT <<'OUT'
scope end
dropped 300
early return
dropped 301
dropped 302
dropped 301
overwrite
dropped 310
after write
dropped 311
field overwrite
dropped 320
written
dropped 321
dropped 322
borrowed temporary
dropped 330
after statement
part of a temporary
dropped 341
after statement
dropped 340
skipped destructure part
after destructure
dropped 350
dropped 351
discarded temporary
dropped 360
after
is binding
matched
dropped 370
after
join, taken
dropped 380
after case
join, kept
dropped 381
after case
guard moves, guard ran
dropped 430
else
after case
guard moves, no match
dropped 431
else
after case
swap
swapped
dropped 440
dropped 441
discarded block
after block
dropped 450
field chain off a temporary
dropped 461
after chain
dropped 460
short circuit, right side runs
dropped 470
after and
short circuit, right side skipped
after and
reassign a partly moved value
dropped 481
reassigned
dropped 480
dropped 483
dropped 482
guard temporary, guard runs
dropped 510
matched
guard temporary, no match
else
closure temporary
dropped 520
after call
closure captures
in closure
dropped 530
after call
in local closure
dropped 531
after local call
tuple holding a void
built
dropped 540
loop body ending on a place
dropped 550
dropped 551
after loop
taken apart
closed
dropped 601
dropped 600
dropped 621
dropped 620
dropped 631
dropped 630
end of scope
Guarded.drop
dropped 641
dropped 640
dropped 610
dropped 611
taken apart on one path
dropped 651
dropped 650
after case
kept whole on the other
Guarded.drop
dropped 651
dropped 650
after case
vector of boxes nothing else drops
filled
dropped 660
dropped 661
is, binding a view, matched
after is
dropped 700
is, binding a view, not matched
after is
is, binding moved
dropped 710
after is
after is
is over a temporary
dropped 720
after is
dropped 721
after moving is
field of a destructible temporary, borrowed
Guarded2.drop
dropped 730
after borrow
Guarded2.drop
dropped 731
after copy
case over a temporary, shape test
matched
dropped 740
loop exit
dropped 390
after loop
loop condition ends
dropped 391
after loop
temporary in a loop condition
dropped 500
dropped 501
dropped 502
after loop
case over a kept temporary
kept
dropped 420
own drop, self left whole
own drop
dropped 400
end
OUT

# Build "program" in both modes and run it, its output against "want".
check_program() {
    local name="$1" program="$2" want="$3" mode dir same
    for mode in dev rel; do
        dir="${WORK}/${name}-${mode}"
        new_project "${dir}"
        printf '%s\n' "${program}" > "${dir}/src/main.spp"
        if ! build_project "${name} [${mode}]" "${dir}" "${mode}"; then
            failures=$((failures + 1))
            continue
        fi
        run_exe "${EXE}" "${dir}/run.log"
        grep -v '^  #[0-9]' "${dir}/run.log" > "${dir}/got.log"
        diff -u --label want --label got <(printf '%s\n' "${want}") "${dir}/got.log" > "${dir}/run.diff"
        same=$?
        if (( RC != 0 || same != 0 )); then
            fail "${name} [${mode}]: destructors ran differently (exit ${RC})" "${dir}/run.diff"
        else
            ok "${name} [${mode}]"
        fi
    done
}

check_program drops "${PROGRAM}" "${WANT}"
check_program affine-drops "${AFFINE}" "${AFFINE_WANT}"

finish
