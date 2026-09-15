#include "../test_macros.hpp"

SPP_TEST_SHOULD_PASS_SEMANTIC(
  AstPostfixExpressionOperatorStaticMemberAccessAst,
  test_valid_field_on_type, R"(
    cls Point {
        x: S32
        y: S32
    }

    sup Point {
        fun f() -> Void { }
    }

    fun f() -> Void {
        Point::f()
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
  AstPostfixExpressionOperatorStaticMemberAccessAst,
  test_invalid_field_on_type,
  SppIdentifierUnknownError, R"(
    cls Point {
        x: S32
        y: S32
    }

    fun f(p: Point) -> Void {
        Point::f()
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  AstPostfixExpressionOperatorStaticMemberAccessAst,
  test_valid_method_on_type_with_explicit_self, R"(
    cls Point {
        x: S32
        y: S32
    }

    sup Point {
        fun m(&self) -> Void { }
    }

    fun f(p: Point) -> Void {
        Point::m(&p)
        std::mem::ops::drop(p)
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
  AstPostfixExpressionOperatorStaticMemberAccessAst,
  test_invalid_method_on_type_without_self,
  SppFunctionCallNoValidSignaturesError, R"(
    cls Point {
        x: S32
        y: S32
    }

    sup Point {
        fun m(&self) -> Void { }
    }

    fun f() -> Void {
        Point::m()
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
  AstPostfixExpressionOperatorStaticMemberAccessAst,
  test_invalid_std_method_on_type_without_self,
  SppFunctionCallNoValidSignaturesError, R"(
    fun f() -> Void {
        let next = std::random::Rng::next_u64()
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  AstPostfixExpressionOperatorStaticMemberAccessAst,
  test_valid_field_on_namespace, R"(
    fun f() -> Void {
        std::console::print("hello")
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
  AstPostfixExpressionOperatorStaticMemberAccessAst,
  test_invalid_field_on_namespace,
  SppIdentifierUnknownError, R"(
    fun f() -> Void {
        std::console::ppp(&"hello")
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
  AstPostfixExpressionOperatorStaticMemberAccessAst,
  test_invalid_static_operator_application_on_variable,
  SppMemberAccessRuntimeOperatorExpectedError, R"(
    cls Point {
        x: S32
        y: S32
    }

    fun f(p: Point) -> Void {
        let x = p::x
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
  AstPostfixExpressionOperatorStaticMemberAccessAst,
  test_invalid_private_static_member_on_type_diff_ctx,
  SppAccessViolationError, R"(
    cls MyType { }
    sup MyType {
        !private cmp n: S32 = 123
    }

    fun f() -> Void {
        let x = MyType::n
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  AstPostfixExpressionOperatorStaticMemberAccessAst,
  test_valid_private_static_member_on_type_same_ctx, R"(
    cls MyType { }
    sup MyType {
        !private cmp n: S32 = 123

        fun f() -> Void {
            let x = MyType::n
        }
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  AstPostfixExpressionOperatorStaticMemberAccessAst,
  test_valid_attribute_and_constant_of_one_name_coexist, R"(
    cls Point {
        !public x: U32
    }

    sup Point {
        !public cmp x: Bool = true
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  AstPostfixExpressionOperatorStaticMemberAccessAst,
  test_valid_constant_read_through_the_type, R"(
    cls Point { }

    sup Point {
        !public cmp x: Bool = true
    }

    fun f() -> Void {
        let a: Bool = Point::x
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  AstPostfixExpressionOperatorStaticMemberAccessAst,
  test_valid_constant_read_through_the_type_over_an_attribute_of_one_name, R"(
    cls Point {
        !public x: U32
    }

    sup Point {
        !public cmp x: Bool = true
    }

    fun f() -> Void {
        let a: Bool = Point::x
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
  AstPostfixExpressionOperatorStaticMemberAccessAst,
  test_invalid_attribute_read_through_the_type,
  SppMemberAccessRuntimeOperatorExpectedError, R"(
    cls Point {
        !public x: U32
    }

    fun f() -> Void {
        let a = Point::x
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  AstPostfixExpressionOperatorStaticMemberAccessAst,
  test_valid_constant_on_a_base_and_attribute_on_the_derived_type, R"(
    cls Base { }

    sup Base {
        !public cmp x: Bool = true
    }

    cls Point {
        !public x: U32
    }

    sup Point ext Base { }

    fun f() -> Void {
        let p = Point(x=1_u32)
        let a: U32 = p.x
        let b: Bool = Point::x
        drop(p)
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  AstPostfixExpressionOperatorStaticMemberAccessAst,
  test_valid_attribute_on_a_base_and_constant_on_the_derived_type, R"(
    cls Base {
        !public x: U32
    }

    cls Point { }

    sup Point ext Base { }

    sup Point {
        !public cmp x: Bool = true
    }

    fun f() -> Void {
        let p = Point(x=1_u32)
        let a: U32 = p.x
        let b: Bool = Point::x
        drop(p)
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
  AstPostfixExpressionOperatorStaticMemberAccessAst,
  test_invalid_two_inherited_constants_of_one_name,
  SppAmbiguousMemberAccessError, R"(
    cls B { }

    sup B {
        !public cmp x: Bool = true
    }

    cls C { }

    sup C {
        !public cmp x: Bool = false
    }

    cls A { }

    sup A ext B { }
    sup A ext C { }

    fun f() -> Void {
        let a = A::x
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  AstPostfixExpressionOperatorStaticMemberAccessAst,
  test_valid_method_as_value_through_a_generic_parameter, R"(
    cls Counter {
        n: S32
    }

    sup Counter {
        !public fun bump(&self, x: S32) -> S32 { ret self.n + x }
    }

    fun apply[F: std::function::FunRef[(&Counter, S32), S32]](f: F, c: &Counter, x: S32) -> S32 { ret f(c, x) }

    fun f(c: Counter) -> Void {
        let r = apply(Counter::bump, &c, 2)
        std::mem::ops::drop(c)
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  AstPostfixExpressionOperatorStaticMemberAccessAst,
  test_valid_method_as_value_through_a_function_type, R"(
    cls Counter {
        n: S32
    }

    sup Counter {
        !public fun bump(&self, x: S32) -> S32 { ret self.n + x }
    }

    fun apply(f: std::function::FunRef[(&Counter, S32), S32], c: &Counter, x: S32) -> S32 { ret f(c, x) }

    fun f(c: Counter) -> Void {
        let r = apply(Counter::bump, &c, 2)
        std::mem::ops::drop(c)
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  AstPostfixExpressionOperatorStaticMemberAccessAst,
  test_valid_static_method_as_value, R"(
    cls Counter {
        n: S32
    }

    sup Counter {
        !public fun make(n: S32) -> Counter { ret Counter(n=n) }
    }

    fun apply(f: std::function::FunRef[(S32,), Counter], n: S32) -> Counter { ret f(n) }

    fun f() -> Void {
        std::mem::ops::drop(apply(Counter::make, 42))
    }
)");

// A method value on a generic owner is named through the instantiated owner ("Box[S32]::$Get").
SPP_TEST_SHOULD_PASS_SEMANTIC(
  AstPostfixExpressionOperatorStaticMemberAccessAst,
  test_valid_method_as_value_on_a_generic_owner, R"(
    cls Box[T] {
        val: T
    }

    sup [T: Copy] Box[T] {
        !public fun get(&self) -> T { ret self.val }
    }

    fun apply(f: std::function::FunRef[(&Box[S32],), S32], b: &Box[S32]) -> S32 { ret f(b) }

    fun f(b: Box[S32]) -> Void {
        let r = apply(Box[S32]::get, &b)
        std::mem::ops::drop(b)
    }
)");

// Overloads written in two "sup" blocks get a mock each, and each mock is given the other's overloads, so naming
// "Counter::$Make" reaches both - through a function type, and through a generic parameter (called via the owner).
SPP_TEST_SHOULD_PASS_SEMANTIC(
  AstPostfixExpressionOperatorStaticMemberAccessAst,
  test_valid_method_as_value_overloaded_across_sup_blocks, R"(
    cls Counter {
        n: S32
    }

    sup Counter {
        !public fun make(n: S32) -> Counter { ret Counter(n=n) }
    }

    sup Counter {
        !public fun make(flag: Bool) -> Counter { ret Counter(n=0) }
    }

    fun apply(f: std::function::FunRef[(Bool,), Counter]) -> Counter { ret f(true) }

    fun f() -> Void {
        std::mem::ops::drop(apply(Counter::make))
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  AstPostfixExpressionOperatorStaticMemberAccessAst,
  test_valid_method_as_value_overloaded_across_sup_blocks_through_a_generic_parameter, R"(
    cls Counter {
        n: S32
    }

    sup Counter {
        !public fun make(n: S32) -> Counter { ret Counter(n=n) }
    }

    sup Counter {
        !public fun make(flag: Bool) -> Counter { ret Counter(n=0) }
    }

    fun apply[F: std::function::FunRef[(Bool,), Counter]](f: F) -> Counter { ret f(true) }

    fun f() -> Void {
        std::mem::ops::drop(apply(Counter::make))
    }
)");

// Todo: red - calling a "Self"-returning method through a method value segfaults the compiler.
SPP_TEST_SHOULD_PASS_SEMANTIC(
  MethodAsValueSelfReturn,
  test_valid_method_value_returning_self, R"(
    cls A { }
    sup A ext std::copy::Copy { }
    sup A {
        !public fun make() -> Self { ret A() }
    }
    fun f() -> Void {
        let g = A::make
        let a: A = g()
    }
)");
