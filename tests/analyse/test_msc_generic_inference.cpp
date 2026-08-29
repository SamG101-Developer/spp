#include "../test_macros.hpp"

SPP_TEST_SHOULD_PASS_SEMANTIC(
  TestGenericInference_Direct,
  test_valid_infer_single_type_param_from_arg, R"(
    fun f[T](a: T) -> T { ret a }

    fun g() -> Void {
        let mut x = f(123)
        x = 456
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  TestGenericInference_Direct,
  test_valid_infer_multiple_type_params, R"(
    fun f[T, U](a: T, b: U) -> (T, U) { ret (a, b) }

    fun g() -> Void {
        let mut x = f(123, true)
        x.0 = 456
        x.1 = false
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  TestGenericInference_Direct,
  test_valid_infer_same_param_consistent, R"(
    fun f[T](a: T, b: T) -> T {
        std::mem::ops::drop(b)
        ret a
    }

    fun g() -> Void {
        let mut x = f(123, 456)
        x = 789
    }
)");

//
// Inference through structural / nested generic types.
//

SPP_TEST_SHOULD_PASS_SEMANTIC(
  TestGenericInference_Nested,
  test_valid_infer_from_nested_generic_type, R"(
    fun f[T](a: Vec[T]) -> T {
        std::mem::ops::drop(a)
        ret T()
    }

    fun g() -> Void {
        let mut x = f(Vec[S32]())
        x = 123
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  TestGenericInference_Nested,
  test_valid_infer_from_tuple, R"(
    fun f[T, U](a: (T, U)) -> (T, U) { ret a }

    fun g() -> Void {
        let mut x = f((123, true))
        x = (456, false)
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  TestGenericInference_Nested,
  test_valid_infer_from_immutable_borrow, R"(
    fun f[T](a: &T) -> T { ret T() }

    fun g(x: S32) -> Void {
        let mut y = f(&x)
        y = 123
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  TestGenericInference_Nested,
  test_valid_infer_from_mutable_borrow, R"(
    fun f[T](a: &mut T) -> T { ret T() }

    fun g(mut x: S32) -> Void {
        let mut y = f(&mut x)
        y = 123
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  TestGenericInference_Nested,
  test_valid_infer_comp_from_array_size, R"(
    fun f[T, cmp n: USize](a: Arr[T, n]) -> Arr[T, n + 1] { }

    fun g() -> Void {
        let mut x = f([1, 2, 3])
        x = [4, 5, 6, 7]
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  TestGenericInference_Optional,
  test_valid_infer_optional_default_applied, R"(
    fun f[T, U = Bool](a: T) -> U {
        std::mem::ops::drop(a)
        ret U()
    }

    fun g() -> Void {
        let mut x = f(123)
        x = true
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  TestGenericInference_Optional,
  test_valid_infer_optional_default_override, R"(
    fun f[T, U = Bool](a: T) -> U {
        std::mem::ops::drop(a)
        ret U()
    }

    fun g() -> Void {
        let mut x = f[U=Str](123)
        x = Str::from("hello")
        drop(x)
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  TestGenericInference_Mixed,
  test_valid_infer_mixed_explicit_and_inferred, R"(
    fun f[T, U](a: U) -> Void {
        std::mem::ops::drop(a)
    }

    fun g() -> Void {
        f[S32](true)
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  TestGenericInference_ObjInit,
  test_valid_infer_class_generic_from_object_initializer, R"(
    cls Box[T] {
        !public val: T
    }

    fun g() -> Void {
        let x = Box(val=123)
        std::mem::ops::drop(x)
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
  TestGenericInference_Conflict,
  test_invalid_infer_conflicting_type,
  SppFunctionCallNoValidSignaturesError, R"(
    fun f[T](a: T, b: T) -> Void { }

    fun g() -> Void {
        f(1, true)
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
  TestGenericInference_Conflict,
  test_valid_infer_same_param_across_different_nesting_depths_low_high,
  SppFunctionCallNoValidSignaturesError, R"(
    fun f[T](a: T, b: Vec[T]) -> T { ret T() }

    fun g() -> Void {
        f(false, Vec[S32]())
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
  TestGenericInference_Conflict,
  test_valid_infer_same_param_across_different_nesting_depths_high_low,
  SppFunctionCallNoValidSignaturesError, R"(
    fun f[T](a: Vec[T], b: T) -> T { }

    fun g() -> Void {
        f(Vec[S32](), false)
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
  TestGenericInference_Conflict,
  test_valid_infer_same_param_across_different_nesting_depths_high_high,
  SppFunctionCallNoValidSignaturesError, R"(
    fun f[T](a: Vec[T], b: Vec[T]) -> Void { }

    fun g() -> Void {
        f(Vec[S32](), Vec[Bool]())
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
  TestGenericInference_Conflict,
  test_invalid_infer_uninferrable_required_param,
  SppFunctionCallNoValidSignaturesError, R"(
    fun f[T]() -> Void { }

    fun g() -> Void {
        f()
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
  TestGenericInference_Conflict,
  test_invalid_too_many_explicit_generic_arguments,
  SppFunctionCallNoValidSignaturesError, R"(
    fun f[T](a: T) -> Void { }

    fun g() -> Void {
        f[S32, Bool](1)
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
  TestGenericInference_Conflict,
  test_invalid_infer_mixed_explicit_and_inferred_conflict,
  SppFunctionCallNoValidSignaturesError, R"(
    fun f[T](a: T) -> Void { }

    fun g() -> Void {
        f[S32](true)
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  TestGenericInference_MultiNested,
  test_valid_infer_deeply_nested_generic, R"(
    fun f[T](a: Vec[Vec[T]]) -> T {
        std::mem::ops::drop(a)
        ret T()
    }

    fun g() -> Void {
        let mut x = f(Vec[Vec[S32]]())
        x = 123
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  TestGenericInference_MultiNested,
  test_valid_infer_nested_generic_inside_tuple, R"(
    fun f[T, U](a: (Vec[T], U)) -> (T, U) {
        std::mem::ops::drop(a)
        ret (T(), U())
    }

    fun g() -> Void {
        let mut x = f((Vec[S32](), true))
        x.0 = 123
        x.1 = false
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  TestGenericInference_MultiNested,
  test_valid_infer_same_param_across_different_nesting_depths, R"(
    fun f[T](a: T, b: Vec[T]) -> T {
        std::mem::ops::drop(b)
        ret a
    }

    fun g() -> Void {
        let mut x = f(1, Vec[S32]())
        x = 123
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  TestGenericInference_Constraint,
  test_valid_infer_generic_from_constraint, R"(
    cls MyUType { }
    cls Other[U] { }
    cls Concrete { }
    sup Concrete ext Other[MyUType] { }

    fun f[U, T: Other[U]](x: T) -> U {
        std::mem::ops::drop(x)
        ret U()
    }

    fun g() -> Void {
        let mut x = f(Concrete())
        x = MyUType()
        std::mem::ops::drop(x)
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  TestGenericInference_Constraint,
  test_valid_infer_generic_from_constraint_functional, R"(
    fun f[U, F: FunRef[(), U]](f: F) -> U {
        std::mem::ops::drop(f)
        ret U()
    }

    fun g() -> Void {
        let mut x = f(() 123)
        x = 456
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  TestGenericInference_CrossApplication,
  test_valid_cross_application_type_default_references_type_param, R"(
    cls Container[T, U = Vec[T]] {
        !public a: U
    }

    fun g() -> Void {
        let mut x = Container[S32]()
        x.a = Vec[S32]()
        std::mem::ops::drop(x)
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  TestGenericInference_CrossApplication,
  test_valid_cross_application_in_function_inference, R"(
    fun f[T, U = Vec[T]](a: T) -> U {
        std::mem::ops::drop(a)
        ret U()
    }

    fun g() -> Void {
        let mut x = f(123)
        x = Vec[S32]()
        std::mem::ops::drop(x)
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  TestGenericInference_CrossApplication,
  test_valid_cross_application_type_and_comp, R"(
    cls Foo[T, cmp n: USize, U = Arr[T, n + 1]] { !public a: U }

    fun g() -> Void {
        let mut x = Foo[S32, 3_uz]()
        x.a = [1, 2, 3, 4]
        std::mem::ops::drop(x)
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  TestGenericInference_Variant,
  test_valid_infer_from_optional_argument, R"(
    fun f[T](a: Opt[T]) -> T { ret T() }

    fun g() -> Void {
        let opt: Opt[S32] = Some(val=123)
        let mut x = f(opt)
        x = 456
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  TestGenericInference_Variant,
  test_valid_infer_into_variant_return, R"(
    fun f[T](a: T) -> Opt[T] { ret Some(val=a) }

    fun g() -> Void {
        let x = f(123)
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  TestGenericInference_Nested,
  test_valid_infer_from_borrow_of_nested_generic, R"(
    fun f[T](a: &Vec[T]) -> T { ret T() }

    fun g(v: Vec[S32]) -> Void {
        let mut x = f(&v)
        x = 123
        std::mem::ops::drop(v)
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  TestGenericInference_Variadic,
  test_valid_infer_variadic_type_args_as_pack, R"(
    fun f[..Ts]() -> Void { }

    fun g() -> Void {
        f[S32, Bool]()
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  TestGenericInference_Variadic,
  test_valid_infer_type_from_variadic_function_parameter, R"(
    fun f[T](..a: T) -> Void {
        std::mem::ops::drop(a)
    }

    fun g() -> Void {
        f(1, 2, 3)
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  TestGenericInference_Constraints,
  test_valid_infer_against_constraint_with_self_in_sup_name, R"(
    fun g() -> Void {
        loop x in std::range::Range[S32]::between(0, 5) { }
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  TestGenericInference_Constraints,
  test_valid_generic_inherited_method_pins_self_to_implementer, R"(
    cls Base { }

    sup Base {
        !public !abstract_method
        fun speak(&self) -> Void { }

        !public !virtual_method
        fun speak_gen[T](&self) -> Void {
            self.speak()
        }
    }

    cls Derived { }

    sup Derived ext Base {
        fun speak(&self) -> Void { }
    }

    fun g() -> Void {
        let d = Derived()
        d.speak_gen[S32]()
        std::mem::ops::drop(d)
    }
)");

// A generic parameter is registered on a scope, and a fully bound instantiation like "Str[A=GlobalAlloc]" has one
// scope shared by the whole program. Carrying the enclosing scope's generics into that scope stranded them there:
// declaring any generic named "U" - anywhere, in any file - left a "U" registered on "Str", which was then offered
// to every call as a "U=U" argument. Being an argument it outranked inference, so a callee's own "U" never got
// inferred. It took a default value to trigger, because that is what names a concrete type from inside the scope
// the parameter lives in. The name is incidental: this is pinned with "U" because "Opt::map[U, ...]" is the shape
// that broke, but every generic name in the standard library was reachable the same way.
SPP_TEST_SHOULD_PASS_SEMANTIC(
  TestGenericInference_Constraints,
  test_valid_defaulted_generic_does_not_strand_its_name_on_the_default_type, R"(
    cls Holder[U = Str] { }

    fun g(o: Opt[S32]) -> Void {
        let m = o.map((x: S32) { x })
        std::mem::ops::drop(m)
    }
)");

// The same sweep carries comp generics in, and they are stranded on a shared instantiation the same way. They do
// less damage, because "Scope::GetGenerics" skips a comp symbol that is still an unbound parameter, so a stranded
// one is never offered as an argument. That skip has no counterpart on the type branch, which is why the type case
// above had to be stopped at the point the symbol is carried in instead. A comp parameter named like one of the
// standard library's own is the shape that would expose it if that ever stopped holding.
SPP_TEST_SHOULD_PASS_SEMANTIC(
  TestGenericInference_Constraints,
  test_valid_defaulted_comp_generic_does_not_strand_its_name_on_the_default_type, R"(
    cls HolderW[cmp w: U32 = 5_u32] { }

    fun g() -> Void {
        let a = 1_u32 + 2_u32
        let b = 7_u8 + 1_u8
        let c = 3_uz + 4_uz
    }
)");

// The gap the unbound-parameter skip does not cover: a comp symbol that is bound to a value is offered, so one
// stranded on a shared instantiation would be offered as a real binding rather than an inert identity. Nothing
// reaches it today - this pins the shape that would.
SPP_TEST_SHOULD_PASS_SEMANTIC(
  TestGenericInference_Constraints,
  test_valid_bound_comp_generic_is_not_stranded_on_a_shared_instantiation, R"(
    cls HolderB[cmp w: U32] {
        !public x: Str
    }

    fun g() -> Void {
        let a = HolderB[5_u32](x=Str::from("q"))
        std::mem::ops::drop(a)
        let b = 1_u32 + 2_u32
        let c = 3_uz + 4_uz
    }
)");
