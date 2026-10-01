#include "../test_macros.hpp"

SPP_TEST_SHOULD_FAIL_SEMANTIC(
  TestTypeStatementAst,
  test_invalid_type_statement_old_type_convention_mut,
  SppSecondClassBorrowViolationError, R"(
    type MyType = &mut Bool
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
  TestTypeStatementAst,
  test_invalid_type_statement_old_type_convention_ref,
  SppSecondClassBorrowViolationError, R"(
    type MyType = &Bool
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
  TestTypeStatementAst,
  test_invalid_type_statement_unknown_old_type,
  SppIdentifierUnknownError, R"(
    type MyType = Unknown
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
  TestTypeStatementAst,
  test_invalid_type_statement_misspelt_namespace_in_old_type,
  SppIdentifierUnknownError, R"(
    type MyVec = std::vectr::Vec[S32]
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
  TestTypeStatementAst,
  test_invalid_type_statement_duplicate,
  SppIdentifierDuplicateError, R"(
    type MyType = Bool
    type MyType = StrView
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  TestTypeStatementAst,
  test_valid_type_statement_simple_alias, R"(
    type MyString = Str
    type MyBool = Bool

    fun f(a: MyString, b: MyBool) -> Void {
        std::mem::ops::drop(a)
    }
    fun g() -> Void { f(Str::from("hello"), true) }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  TestTypeStatementAst,
  test_valid_type_statement_local_simple_alias, R"(
    fun f() -> Void {
        type MyString = Str
        type MyBool = Bool

        let x: (MyString, MyBool)
        x = (Str::from("hello"), true)
        std::mem::ops::drop(x)
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  TestTypeStatementAst,
  test_valid_type_statement_variant, R"(
    type SomeType = Str or Bool
    fun f(a: SomeType) -> Void {
        std::mem::ops::drop(a)
    }
    fun g() -> Void { f(Str::from("hello")) }
)");

// Todo: Commented out - this crashes the compiler rather than failing.
//  segfaults on a null TypeSymbol in ObjectInitializerAst::InferType - a "type" statement declaring a variant inside a
//  function body.
// SPP_TEST_SHOULD_PASS_SEMANTIC(
//     TestTypeStatementAst,
//     test_valid_type_statement_local_variant, R"(
//     fun f() -> Void {
//         type SomeType = Str or Bool
//         let x: SomeType
//         x = Str::from("hello")
//     }
// )");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  TestTypeStatementAst,
  test_valid_type_statement_generics_alias, R"(
    type MyVec[T] = Vec[T]

    fun f[T](mut a: MyVec[T], replacement: T) -> Void {
        # let mut x = a.take_head()
        # x = replacement
        std::mem::ops::drop(replacement)
        std::mem::ops::drop(a)
    }

    fun g() -> Void {
        let x = Vec[Str]()
        f(x, Str::from("test"))
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  TestTypeStatementAst,
  test_valid_type_statement_local_generics_alias, R"(
    fun f() -> Void {
        type MyVec[T] = Vec[T]
        let x = MyVec[Str]()
        std::mem::ops::drop(x)
    }
)");

// FIXED (the test itself leaked)
SPP_TEST_SHOULD_PASS_SEMANTIC(
  TestTypeStatementAst,
  test_valid_type_statement_nested_generic_alias, R"(
    fun f[T](a: Opt[T]) -> Void { std::mem::ops::drop(a) }
    fun g() -> Void {
        let x = Some(val=123)
        f(x)
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  TestTypeStatementAst,
  test_valid_type_statement_reduction_type_generic, R"(
    type MyVec[T] = Vec[T]

    fun f[T](a: MyVec[T]) -> Void {
        std::mem::ops::drop(a)
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  TestTypeStatementAst,
  test_valid_type_statement_comp_param_alias_to_foreign_module, R"(
    type HeapArr[T, cmp n: USize] = Single[Arr[T, n]]

    fun f(a: HeapArr[Bool, 4_uz]) -> Void {
        std::mem::ops::drop(a)
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  TestTypeStatementAst,
  test_valid_type_statement_constrained_param_alias_to_foreign_module, R"(
    type CopyBox[T: Copy] = Single[T]

    fun f(a: CopyBox[Bool]) -> Void {
        std::mem::ops::drop(a)
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    TestTypeStatementAst,
    test_invalid_type_statement_recursive_through_a_generic,
    SppTypeAliasCyclicError, R"(
    type RecAlias = Vec[RecAlias]
)");

// A function-local alias runs stages 2-4 from inside stage 7, with the stage still reading as 7, so "NamedGnArgs"
// analyses the alias chain's own comp argument ("w" of "SizedIntegerSigned[w]") in the function's scope.
// FIXED
SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestTypeStatementAst,
    test_valid_local_type_statement_of_an_integer_alias, R"(
    fun f() -> Void {
        type T = S32
        let x: T = 1
    }
)");

// This segfaulted, and then read "Opt[S32]" as not "Copy": the instance the local alias made missed its
// superimpositions.
SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestTypeStatementAst,
    test_valid_local_type_statement_of_an_option, R"(
    fun f() -> Void {
        type T = Opt[S32]
        let x: T = None()
    }
)");

// A tuple alias substitutes its arguments positionally rather than by name, so a repeated or reordered parameter
// produces the wrong tuple.
SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestTypeStatementAst,
    test_valid_type_statement_tuple_repeating_a_parameter, R"(
    type TupRep[T] = (T, T)

    fun f(x: TupRep[S32]) -> (S32, S32) { ret x }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestTypeStatementAst,
    test_valid_type_statement_tuple_reordering_parameters, R"(
    type TupSwap[T, U] = (U, T)

    fun f(x: TupSwap[S32, Bool]) -> (Bool, S32) { ret x }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    TestTypeStatementAst,
    test_invalid_type_statement_too_many_generic_arguments,
    SppGenericArgumentTooManyError, R"(
    type TupPairOf[T] = (T, T)

    fun f(x: TupPairOf[S32, Bool]) -> Void { }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestTypeStatementAst,
    test_valid_type_statement_alias_of_a_class_with_a_defaulted_generic, R"(
    type MyString = Str

    fun f(a: MyString) -> Void {
        std::mem::ops::drop(a)
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestTypeStatementAst,
    test_valid_type_statement_qualified_generic_parameter_type, R"(
    fun f(o: std::option::Opt[std::boolean::Bool]) -> std::void::Void { }
)");
