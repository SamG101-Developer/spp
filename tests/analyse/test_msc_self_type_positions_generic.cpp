#include "../test_macros.hpp"

// "Self" in every written-type position of a generic class, where it stands for "Box[T]" and must resolve to the
// receiver's instantiation. Each position rewrites its written type through its own substitute/analyse/qualify steps.

SPP_TEST_SHOULD_PASS_SEMANTIC(
  TestSelfTypePositionsGeneric,
  test_valid_self_as_let_statement_explicit_type, R"(
    !public cls Box[T] { !public v: T }
    sup [T: std::copy::Copy] Box[T] ext std::copy::Copy { }
    sup [T: std::copy::Copy] Box[T] {
        !public fun m(self) -> Self {
            let x: Self = self
            ret x
        }
    }
    fun f() -> Void {
        let b: Box[S32] = Box(v=1).m()
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  TestSelfTypePositionsGeneric,
  test_valid_self_as_let_statement_type_instantiated_twice, R"(
    !public cls Box[T] { !public v: T }
    sup [T: std::copy::Copy] Box[T] ext std::copy::Copy { }
    sup [T: std::copy::Copy] Box[T] {
        !public fun m(self) -> Self {
            let x: Self = self
            ret x
        }
    }
    fun f() -> Void {
        let a: Box[S32] = Box(v=1).m()
        let b: Box[Bool] = Box(v=true).m()
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  TestSelfTypePositionsGeneric,
  test_valid_self_with_two_class_generics, R"(
    !public cls Pair[A, B] {
        !public a: A
        !public b: B
    }
    sup [A: std::copy::Copy, B: std::copy::Copy] Pair[A, B] ext std::copy::Copy { }
    sup [A: std::copy::Copy, B: std::copy::Copy] Pair[A, B] {
        !public fun same(self) -> Self {
            let x: Self = self
            ret x
        }
    }
    fun f() -> Void {
        let p: Pair[S32, Bool] = Pair(a=1, b=true).same()
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  TestSelfTypePositionsGeneric,
  test_valid_self_as_uninitialized_let_statement_type, R"(
    !public cls Box[T] { !public v: T }
    sup [T: std::copy::Copy] Box[T] ext std::copy::Copy { }
    sup [T: std::copy::Copy] Box[T] {
        !public fun m(self) -> Self {
            let x: Self
            x = self
            ret x
        }
    }
    fun f() -> Void {
        let b: Box[S32] = Box(v=1).m()
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  TestSelfTypePositionsGeneric,
  test_valid_self_in_uninitialized_let_statement_generic_argument, R"(
    !public cls Box[T] { !public v: T }
    sup [T: std::copy::Copy] Box[T] ext std::copy::Copy { }
    sup [T: std::copy::Copy] Box[T] {
        !public fun m(&self) -> Void {
            let v: Vec[Self]
            v = Vec[Self]::new()
            std::mem::ops::drop(v)
        }
    }
    fun f() -> Void {
        let b = Box(v=1)
        b.m()
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  TestSelfTypePositionsGeneric,
  test_valid_self_in_let_statement_generic_argument, R"(
    !public cls Box[T] { !public v: T }
    sup [T: std::copy::Copy] Box[T] ext std::copy::Copy { }
    sup [T: std::copy::Copy] Box[T] {
        !public fun m(&self) -> Void {
            let v: Vec[Self] = Vec[Self]::new()
            std::mem::ops::drop(v)
        }
    }
    fun f() -> Void {
        let b = Box(v=1)
        b.m()
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  TestSelfTypePositionsGeneric,
  test_valid_self_in_function_parameter_generic_argument, R"(
    !public cls Box[T] { !public v: T }
    sup [T: std::copy::Copy] Box[T] ext std::copy::Copy { }
    sup [T: std::copy::Copy] Box[T] {
        !public fun m(&self, v: Vec[Self]) -> Void { std::mem::ops::drop(v) }
    }
    fun f() -> Void {
        let b = Box(v=1)
        b.m(Vec[Box[S32]]::new())
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  TestSelfTypePositionsGeneric,
  test_valid_self_in_return_type_generic_argument, R"(
    !public cls Box[T] { !public v: T }
    sup [T: std::copy::Copy] Box[T] ext std::copy::Copy { }
    sup [T: std::copy::Copy] Box[T] {
        !public fun many(&self) -> Vec[Self] { ret Vec[Self]::new() }
    }
    fun f() -> Void {
        let b = Box(v=1)
        let v: Vec[Box[S32]] = b.many()
        std::mem::ops::drop(v)
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  TestSelfTypePositionsGeneric,
  test_valid_self_in_class_attribute_generic_argument, R"(
    !public cls Node[T] {
        !public v: T
        !public kids: Vec[Self]
    }
    fun f() -> Void {
        let n = Node(v=1, kids=Vec[Node[S32]]::new())
        std::mem::ops::drop(n)
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  TestSelfTypePositionsGeneric,
  test_valid_self_as_a_type_alias_target, R"(
    !public cls Box[T] { !public v: T }
    sup [T: std::copy::Copy] Box[T] ext std::copy::Copy { }
    sup [T: std::copy::Copy] Box[T] {
        type Mine = Self
        !public fun m(self) -> Mine { ret self }
    }
    fun f() -> Void {
        let b: Box[S32] = Box(v=1).m()
    }
)");

// Todo: red - "Vec[Self]" aliased inside Box's own generic sup ("Self" is "Box[T]" there) nests without end, the same
// polymorphic recursion as SupTypeStatementAstGenericSelfClass.test_valid_alias_of_a_type_holding_the_sups_own_generic_
// class: it now stops with E109 (generic instantiation depth) instead of overflowing the stack.
SPP_TEST_SHOULD_PASS_SEMANTIC(
  TestSelfTypePositionsGeneric,
  test_valid_self_in_a_type_alias_generic_argument, R"(
    !public cls Box[T] { !public v: T }
    sup [T: std::copy::Copy] Box[T] ext std::copy::Copy { }
    sup [T: std::copy::Copy] Box[T] {
        type Many = Vec[Self]
        !public fun m(&self) -> Many { ret Many::new() }
    }
    fun f() -> Void {
        let b = Box(v=1)
        let v: Vec[Box[S32]] = b.m()
        std::mem::ops::drop(v)
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  TestSelfTypePositionsGeneric,
  test_valid_self_in_a_tuple_type, R"(
    !public cls Box[T] { !public v: T }
    sup [T: std::copy::Copy] Box[T] ext std::copy::Copy { }
    sup [T: std::copy::Copy] Box[T] {
        !public fun pair(self, that: Self) -> (Self, Self) { ret (self, that) }
    }
    fun f() -> Void {
        let p: (Box[S32], Box[S32]) = Box(v=1).pair(Box(v=2))
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  TestSelfTypePositionsGeneric,
  test_valid_self_in_a_variant_type, R"(
    !public cls Box[T] { !public v: T }
    sup [T: std::copy::Copy] Box[T] ext std::copy::Copy { }
    sup [T: std::copy::Copy] Box[T] {
        !public fun either(self) -> Self or S32 { ret self }
    }
    fun f() -> Void {
        let e: Box[S32] or S32 = Box(v=1).either()
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  TestSelfTypePositionsGeneric,
  test_valid_self_as_an_object_initializer_type, R"(
    !public cls Box[T] { !public v: T }
    sup [T: std::copy::Copy] Box[T] ext std::copy::Copy { }
    sup [T: std::copy::Copy] Box[T] {
        !public fun make(v: T) -> Self { ret Self(v=v) }
    }
    fun f() -> Void {
        let b: Box[S32] = Box[S32]::make(1)
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  TestSelfTypePositionsGeneric,
  test_valid_self_as_a_static_call_lhs, R"(
    !public cls Box[T] { !public v: T }
    sup [T: std::copy::Copy] Box[T] ext std::copy::Copy { }
    sup [T: std::copy::Copy] Box[T] {
        !public fun make(v: T) -> Self { ret Self(v=v) }
        !public fun again(self) -> Self { ret Self::make(self.v) }
    }
    fun f() -> Void {
        let b: Box[S32] = Box(v=1).again()
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  TestSelfTypePositionsGeneric,
  test_valid_self_as_a_let_destructure_type, R"(
    !public cls Box[T] { !public v: T }
    sup [T: std::copy::Copy] Box[T] ext std::copy::Copy { }
    sup [T: std::copy::Copy] Box[T] {
        !public fun get(self) -> T {
            let Self(v) = self
            ret v
        }
    }
    fun f() -> Void {
        let a: S32 = Box(v=1).get()
        let b: Bool = Box(v=true).get()
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  TestSelfTypePositionsGeneric,
  test_valid_self_as_a_case_pattern_type, R"(
    !public cls Box[T] { !public v: T }
    sup [T: std::copy::Copy] Box[T] ext std::copy::Copy { }
    sup [T: std::copy::Copy] Box[T] {
        !public fun get(self) -> T {
            case self is Self(v) { ret v }
            ret self.v
        }
    }
    fun f() -> Void {
        let a: S32 = Box(v=1).get()
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  TestSelfTypePositionsGeneric,
  test_valid_self_as_a_case_pattern_type_on_a_variant, R"(
    !public cls Box[T] { !public v: T }
    sup [T: std::copy::Copy] Box[T] ext std::copy::Copy { }
    sup [T: std::copy::Copy] Box[T] {
        !public fun check(x: Self or S32) -> Bool {
            case x is Self(v) { ret true }
            ret false
        }
    }
    fun f() -> Void {
        let a = Box[S32]::check(Box(v=1))
        let b = Box[S32]::check(2)
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  TestSelfTypePositionsGeneric,
  test_valid_self_as_an_is_expression_type, R"(
    !public cls Box[T] { !public v: T }
    sup [T: std::copy::Copy] Box[T] ext std::copy::Copy { }
    sup [T: std::copy::Copy] Box[T] {
        !public fun check(x: Self or S32) -> Bool { ret x is Self(v) }
    }
    fun f() -> Void {
        let a = Box[S32]::check(Box(v=1))
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  TestSelfTypePositionsGeneric,
  test_valid_self_as_a_cmp_statement_type, R"(
    !public cls P { !public x: S32 }
    sup P ext std::copy::Copy { }
    sup P {
        !public cmp origin: Self = Self(x=0)
    }
    fun f() -> Void {
        let p: P = P::origin
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  TestSelfTypePositionsGeneric,
  test_valid_self_as_a_cmp_statement_type_in_a_generic_class, R"(
    !public cls Unit[T] { }
    sup [T] Unit[T] ext std::copy::Copy { }
    sup [T] Unit[T] {
        !public cmp empty: Self = Self()
    }
    fun f() -> Void {
        let u: Unit[S32] = Unit[S32]::empty
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  TestSelfTypePositionsGeneric,
  test_valid_self_as_a_comp_generic_parameter_type, R"(
    !public cls P { !public x: S32 }
    sup P ext std::copy::Copy { }
    sup P {
        !public fun g[cmp p: Self](&self) -> Void { }
    }
    fun f() -> Void {
        let p = P(x=1)
        p.g[P(x=2)]()
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  TestSelfTypePositionsGeneric,
  test_valid_self_as_a_comp_generic_parameter_type_in_a_generic_class, R"(
    !public cls Box[T] { !public v: T }
    sup [T: std::copy::Copy] Box[T] ext std::copy::Copy { }
    sup [T: std::copy::Copy] Box[T] {
        !public fun g[cmp p: Self](&self) -> Void { }
    }
    fun f() -> Void {
        let b = Box(v=1)
        b.g[Box(v=2)]()
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  TestSelfTypePositionsGeneric,
  test_valid_self_in_a_comp_generic_parameter_generic_argument, R"(
    !public cls P { !public x: S32 }
    sup P ext std::copy::Copy { }
    sup P {
        !public fun g[cmp v: Vec[Self]](&self) -> Void { }
    }
    fun f() -> Void {
        let p = P(x=1)
        p.g[Vec[P]()]()
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  TestSelfTypePositionsGeneric,
  test_valid_self_as_a_closure_return_type, R"(
    !public cls Box[T] { !public v: T }
    sup [T: std::copy::Copy] Box[T] ext std::copy::Copy { }
    sup [T: std::copy::Copy] Box[T] {
        !public fun m(&self) -> Self {
            let c = (x: T) -> Self { ret Self(v=x) }
            ret c(self.v)
        }
    }
    fun f() -> Void {
        let b = Box(v=1)
        let c: Box[S32] = b.m()
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  TestSelfTypePositionsGeneric,
  test_valid_self_in_a_closure_return_type_generic_argument, R"(
    !public cls Box[T] { !public v: T }
    sup [T: std::copy::Copy] Box[T] ext std::copy::Copy { }
    sup [T: std::copy::Copy] Box[T] {
        !public fun m(&self) -> Vec[Self] {
            let c = () -> Vec[Self] { ret Vec[Self]::new() }
            ret c()
        }
    }
    fun f() -> Void {
        let b = Box(v=1)
        let v: Vec[Box[S32]] = b.m()
        std::mem::ops::drop(v)
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  TestSelfTypePositionsGeneric,
  test_valid_self_as_a_closure_parameter_type, R"(
    !public cls Box[T] { !public v: T }
    sup [T: std::copy::Copy] Box[T] ext std::copy::Copy { }
    sup [T: std::copy::Copy] Box[T] {
        !public fun get(self) -> T {
            let c = (x: Self) -> T { ret x.v }
            ret c(self)
        }
    }
    fun f() -> Void {
        let a: S32 = Box(v=1).get()
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  TestSelfTypePositionsGeneric,
  test_valid_self_as_a_variadic_parameter_type, R"(
    !public cls Box[T] { !public v: T }
    sup [T: std::copy::Copy] Box[T] ext std::copy::Copy { }
    sup [T: std::copy::Copy] Box[T] {
        !public fun m(&self, ..rest: Self) -> Void { std::mem::ops::drop(rest) }
    }
    fun f() -> Void {
        let b = Box(v=1)
        b.m(Box(v=2), Box(v=3))
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  TestSelfTypePositionsGeneric,
  test_valid_self_as_an_optional_parameter_type, R"(
    !public cls Unit[T] { }
    sup [T] Unit[T] ext std::copy::Copy { }
    sup [T] Unit[T] {
        !public fun m(&self, that: Self = Self()) -> Void { }
    }
    fun f() -> Void {
        let u = Unit[S32]()
        u.m()
        u.m(Unit[S32]())
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  TestSelfTypePositionsGeneric,
  test_valid_self_as_a_positional_function_generic_argument, R"(
    fun id[X](x: X) -> X { ret x }
    !public cls Box[T] { !public v: T }
    sup [T: std::copy::Copy] Box[T] ext std::copy::Copy { }
    sup [T: std::copy::Copy] Box[T] {
        !public fun m(self) -> Self { ret id[Self](self) }
    }
    fun f() -> Void {
        let b: Box[S32] = Box(v=1).m()
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  TestSelfTypePositionsGeneric,
  test_valid_self_as_a_keyword_function_generic_argument, R"(
    fun id[X](x: X) -> X { ret x }
    !public cls Box[T] { !public v: T }
    sup [T: std::copy::Copy] Box[T] ext std::copy::Copy { }
    sup [T: std::copy::Copy] Box[T] {
        !public fun m(self) -> Self { ret id[X=Self](self) }
    }
    fun f() -> Void {
        let b: Box[S32] = Box(v=1).m()
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  TestSelfTypePositionsGeneric,
  test_valid_self_nested_in_a_function_generic_argument, R"(
    fun id[X](x: X) -> X { ret x }
    !public cls Box[T] { !public v: T }
    sup [T: std::copy::Copy] Box[T] ext std::copy::Copy { }
    sup [T: std::copy::Copy] Box[T] {
        !public fun m(&self) -> Vec[Self] { ret id[Vec[Self]](Vec[Self]::new()) }
    }
    fun f() -> Void {
        let b = Box(v=1)
        let v: Vec[Box[S32]] = b.m()
        std::mem::ops::drop(v)
    }
)");

// "Self" must resolve to the receiver's instantiation, not to any "Box[T]": each of these passes a "Box[Bool]" (or
// a plain value) where "Self" is "Box[S32]", and the error must land in main.spp on written code.

SPP_TEST_SHOULD_FAIL_SEMANTIC(
  TestSelfTypePositionsGeneric,
  test_invalid_self_let_statement_type_mismatch,
  SppTypeMismatchError, R"(
    !public cls Box[T] { !public v: T }
    sup [T: std::copy::Copy] Box[T] ext std::copy::Copy { }
    sup [T: std::copy::Copy] Box[T] {
        !public fun m(&self) -> Void {
            let x: Self = 1
        }
    }
    fun f() -> Void {
        let b = Box(v=1)
        b.m()
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
  TestSelfTypePositionsGeneric,
  test_invalid_self_let_statement_generic_argument_mismatch,
  SppTypeMismatchError, R"(
    !public cls Box[T] { !public v: T }
    sup [T: std::copy::Copy] Box[T] ext std::copy::Copy { }
    sup [T: std::copy::Copy] Box[T] {
        !public fun m(&self) -> Void {
            let v: Vec[Self] = Vec[S32]::new()
            std::mem::ops::drop(v)
        }
    }
    fun f() -> Void {
        let b = Box(v=1)
        b.m()
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
  TestSelfTypePositionsGeneric,
  test_invalid_self_function_parameter_other_instantiation,
  SppFunctionCallNoValidSignaturesError, R"(
    !public cls Box[T] { !public v: T }
    sup [T: std::copy::Copy] Box[T] ext std::copy::Copy { }
    sup [T: std::copy::Copy] Box[T] {
        !public fun m(&self, that: Self) -> Void { }
    }
    fun f() -> Void {
        let b = Box(v=1)
        b.m(Box(v=true))
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
  TestSelfTypePositionsGeneric,
  test_invalid_self_function_parameter_generic_argument_other_instantiation,
  SppFunctionCallNoValidSignaturesError, R"(
    !public cls Box[T] { !public v: T }
    sup [T: std::copy::Copy] Box[T] ext std::copy::Copy { }
    sup [T: std::copy::Copy] Box[T] {
        !public fun m(&self, v: Vec[Self]) -> Void { std::mem::ops::drop(v) }
    }
    fun f() -> Void {
        let b = Box(v=1)
        b.m(Vec[Box[Bool]]::new())
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
  TestSelfTypePositionsGeneric,
  test_invalid_self_return_type_mismatch,
  SppTypeMismatchError, R"(
    !public cls Box[T] { !public v: T }
    sup [T: std::copy::Copy] Box[T] ext std::copy::Copy { }
    sup [T: std::copy::Copy] Box[T] {
        !public fun m(&self) -> Self { ret 1 }
    }
    fun f() -> Void {
        let b = Box(v=1)
        let c = b.m()
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
  TestSelfTypePositionsGeneric,
  test_invalid_self_class_attribute_generic_argument_other_instantiation,
  SppGenericParameterConflictError, R"(
    !public cls Node[T] {
        !public v: T
        !public kids: Vec[Self]
    }
    fun f() -> Void {
        let n = Node(v=1, kids=Vec[Node[Bool]]::new())
        std::mem::ops::drop(n)
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
  TestSelfTypePositionsGeneric,
  test_invalid_self_type_alias_mismatch,
  SppTypeMismatchError, R"(
    !public cls Box[T] { !public v: T }
    sup [T: std::copy::Copy] Box[T] ext std::copy::Copy { }
    sup [T: std::copy::Copy] Box[T] {
        type Mine = Self
        !public fun m(&self) -> Void {
            let x: Mine = 1
        }
    }
    fun f() -> Void {
        let b = Box(v=1)
        b.m()
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
  TestSelfTypePositionsGeneric,
  test_invalid_self_object_initializer_field_mismatch,
  SppTypeMismatchError, R"(
    !public cls Box[T] { !public v: T }
    sup [T: std::copy::Copy] Box[T] ext std::copy::Copy { }
    sup [T: std::copy::Copy] Box[T] {
        !public fun bad() -> Self { ret Self(v=true) }
    }
    fun f() -> Void {
        let b = Box[S32]::bad()
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
  TestSelfTypePositionsGeneric,
  test_invalid_self_comp_generic_parameter_other_instantiation,
  SppFunctionCallNoValidSignaturesError, R"(
    !public cls Box[T] { !public v: T }
    sup [T: std::copy::Copy] Box[T] ext std::copy::Copy { }
    sup [T: std::copy::Copy] Box[T] {
        !public fun g[cmp p: Self](&self) -> Void { }
    }
    fun f() -> Void {
        let b = Box(v=1)
        b.g[Box(v=true)]()
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
  TestSelfTypePositionsGeneric,
  test_invalid_self_closure_return_type_mismatch,
  SppTypeMismatchError, R"(
    !public cls Box[T] { !public v: T }
    sup [T: std::copy::Copy] Box[T] ext std::copy::Copy { }
    sup [T: std::copy::Copy] Box[T] {
        !public fun m(&self) -> Void {
            let c = () -> Self { ret 1 }
        }
    }
    fun f() -> Void {
        let b = Box(v=1)
        b.m()
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
  TestSelfTypePositionsGeneric,
  test_invalid_self_variadic_parameter_other_instantiation,
  SppFunctionCallNoValidSignaturesError, R"(
    !public cls Box[T] { !public v: T }
    sup [T: std::copy::Copy] Box[T] ext std::copy::Copy { }
    sup [T: std::copy::Copy] Box[T] {
        !public fun m(&self, ..rest: Self) -> Void { std::mem::ops::drop(rest) }
    }
    fun f() -> Void {
        let b = Box(v=1)
        b.m(Box(v=true))
    }
)");
