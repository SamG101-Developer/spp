#include "../test_macros.hpp"

SPP_TEST_SHOULD_PASS_SEMANTIC(
    AnnotationAst_BuiltinVirtualMethod,
    test_valid_usage_on_method, R"(
    cls A { }
    sup A {
        !virtual_method
        fun f() -> Void { }
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    AnnotationAst_BuiltinVirtualMethod,
    test_invalid_usage_on_function,
    SppCalledAnnotationAppliedToInvalidAstError, R"(
    !virtual_method
    fun f() -> Void { }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    AnnotationAst_BuiltinAbstractMethod,
    test_valid_usage_on_method, R"(
    cls A { }
    sup A {
        !abstract_method
        fun f() -> A { }
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    AnnotationAst_BuiltinAbstractMethod,
    test_invalid_usage_on_function,
    SppCalledAnnotationAppliedToInvalidAstError, R"(
    !abstract_method
    fun f() -> Void { }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    AnnotationAst_BuiltinPublic,
    test_valid_usage_on_method, R"(
    cls A { }

    sup A {
        !public
        fun f() -> Void { }
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    AnnotationAst_BuiltinPublic,
    test_valid_usage_on_function, R"(
    !public
    fun f() -> Void { }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    AnnotationAst_BuiltinPublic,
    test_valid_usage_on_class, R"(
    !public
    cls A { }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    AnnotationAst_BuiltinPublic,
    test_invalid_usage_on_ext_method,
    SppCalledAnnotationAppliedToInvalidAstError, R"(
    cls A { }
    sup A {
        !virtual_method
        fun f(&self) -> Void { }
    }

    cls B { }
    sup B ext A {
        !public
        fun f(&self) -> Void { }
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    AnnotationAst_BuiltinPackage,
    test_valid_usage_on_method, R"(
    cls A { }

    sup A {
        !package
        fun f() -> Void { }
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    AnnotationAst_BuiltinPackage,
    test_valid_usage_on_function, R"(
    !package
    fun f() -> Void { }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    AnnotationAst_BuiltinPackage,
    test_valid_usage_on_class, R"(
    !package
    cls A { }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    AnnotationAst_BuiltinPackage,
    test_invalid_usage_on_ext_method,
    SppCalledAnnotationAppliedToInvalidAstError, R"(
    cls A { }
    sup A {
        !virtual_method
        fun f(&self) -> Void { }
    }

    cls B { }
    sup B ext A {
        !package
        fun f(&self) -> Void { }
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    AnnotationAst_BuiltinProtected,
    test_valid_usage_on_method, R"(
    cls A { }

    sup A {
        !protected
        fun f() -> Void { }
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    AnnotationAst_BuiltinProtected,
    test_valid_usage_on_function, R"(
    !protected
    fun f() -> Void { }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    AnnotationAst_BuiltinProtected,
    test_valid_usage_on_class, R"(
    !protected
    cls A { }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    AnnotationAst_BuiltinProtected,
    test_invalid_usage_on_ext_method,
    SppCalledAnnotationAppliedToInvalidAstError, R"(
    cls A { }
    sup A {
        !virtual_method
        fun f(&self) -> Void { }
    }

    cls B { }
    sup B ext A {
        !protected
        fun f(&self) -> Void { }
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    AnnotationAst_BuiltinPrivate,
    test_valid_usage_on_method, R"(
    cls A { }

    sup A {
        !private
        fun f() -> Void { }
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    AnnotationAst_BuiltinPrivate,
    test_valid_usage_on_function, R"(
    !private
    fun f() -> Void { }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    AnnotationAst_BuiltinPrivate,
    test_valid_usage_on_class, R"(
    !private
    cls A { }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    AnnotationAst_BuiltinPrivate,
    test_invalid_usage_on_ext_method,
    SppCalledAnnotationAppliedToInvalidAstError, R"(
    cls A { }
    sup A {
        !virtual_method
        fun f(&self) -> Void { }
    }

    cls B { }
    sup B ext A {
        !private
        fun f(&self) -> Void { }
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    AnnotationAst_BuiltinHot,
    test_valid_usage_on_method, R"(
    use std::llvm::hot
    cls A { }

    sup A {
        !hot
        fun f() -> Void { }
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    AnnotationAst_BuiltinHot,
    test_valid_usage_on_ext_method, R"(
    use std::llvm::hot
    cls A { }
    cls B { }

    sup A {
        !virtual_method
        fun f() -> Void { }
    }

    sup B ext A {
        !hot
        fun f() -> Void { }
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    AnnotationAst_BuiltinHot,
    test_valid_usage_on_function, R"(
    use std::llvm::hot

    !hot
    fun f() -> Void { }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    AnnotationAst_BuiltinCold,
    test_valid_usage_on_method, R"(
    use std::llvm::cold
    cls A { }

    sup A {
        !cold
        fun f() -> Void { }
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    AnnotationAst_BuiltinCold,
    test_valid_usage_on_ext_method, R"(
    use std::llvm::cold
    cls A { }
    cls B { }

    sup A {
        !virtual_method
        fun f() -> Void { }
    }

    sup B ext A {
        !cold
        fun f() -> Void { }
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    AnnotationAst_BuiltinCold,
    test_valid_usage_on_function, R"(
    use std::llvm::cold

    !cold
    fun f() -> Void { }
)");

//

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    AnnotationAst_Custom,
    test_invalid_identifier,
    SppAnnotationTargetNotAnAnnotationError, R"(
    cls A { }

    fun invalid() -> Void { }

    !invalid
    fun f() -> A { }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    AnnotationAst_Custom,
    test_invalid_target_non_cmp,
    SppAnnotationTargetNotACmpFunctionError, R"(
    cls A { }

    !std::annotations::annotation
    fun aaa() -> A { }

    !aaa
    fun f() -> A { }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    AnnotationAst_Custom,
    test_invalid_target_not_an_annotation,
    SppAnnotationTargetNotAnAnnotationError, R"(
    cls A { }

    cmp fun aaa() -> A { }

    !aaa
    fun f() -> A { }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    AnnotationAst_Custom,
    test_invalid_missing_args,
    SppFunctionCallNoValidSignaturesError, R"(
    cls A { }

    !annotation(target=Annotation::function)
    cmp fun aaa(x: &StrView) -> A { }

    !aaa
    fun f() -> A { }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    AnnotationAst_Custom,
    test_invalid_extra_args,
    SppFunctionCallNoValidSignaturesError, R"(
    cls A { }

    !annotation(target=Annotation::function)
    cmp fun aaa() -> A { }

    !aaa("hello")
    fun f() -> A { }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    AnnotationAst_Custom,
    test_valid_no_args, R"(
    !annotation(target=Annotation::function)
    cmp fun my_annotation() -> Void { }

    !my_annotation
    fun f() -> Void { }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    AnnotationAst_Custom,
    test_valid_with_args, R"(
    cls A { }

    !annotation(target=Annotation::function)
    cmp fun my_annotation(a: &StrView) -> Void { }

    !my_annotation("hello world")
    fun f() -> Void { }
)");

// SPP_TEST_SHOULD_FAIL_SEMANTIC(
//     AnnotationAst,
//     test_invalid_conflicting_access_modifiers_1,
//     SppAnnotationConflictError, R"(
//     cls A { }
//
//     !public
//     !protected
//     fun f() -> A { }
// )");
//
//
// SPP_TEST_SHOULD_FAIL_SEMANTIC(
//     AnnotationAst,
//     test_invalid_conflicting_access_modifiers_2,
//     SppAnnotationConflictError, R"(
//     cls A { }
//
//     !protected
//     !private
//     fun f() -> A { }
// )");
//
//
// SPP_TEST_SHOULD_FAIL_SEMANTIC(
//     AnnotationAst,
//     test_invalid_conflicting_access_modifiers_3,
//     SppAnnotationConflictError, R"(
//     cls A { }
//
//     !private
//     !public
//     fun f() -> A { }
// )");
//
//
// SPP_TEST_SHOULD_FAIL_SEMANTIC(
//     AnnotationAst,
//     test_invalid_conflicting_temperature_1,
//     SppAnnotationConflictError, R"(
//     cls A { }
//
//     !cold
//     !hot
//     fun f() -> A { }
// )");
//
//
// SPP_TEST_SHOULD_FAIL_SEMANTIC(
//     AnnotationAst,
//     test_invalid_conflicting_temperature_2,
//     SppAnnotationConflictError, R"(
//     cls A { }
//
//     !hot
//     !cold
//     fun f() -> A { }
// )");
//
//
// SPP_TEST_SHOULD_FAIL_SEMANTIC(
//     AnnotationAst,
//     test_invalid_conflicting_function_modifier_1,
//     SppAnnotationConflictError, R"(
//     cls A { }
//     sup A {
//         !virtual_method
//         !abstract_method
//         fun f() -> A { }
//     }
// )");
//
//
// SPP_TEST_SHOULD_FAIL_SEMANTIC(
//     AnnotationAst,
//     test_invalid_conflicting_function_modifier_2,
//     SppAnnotationConflictError, R"(
//     cls A { }
//     sup A {
//         !abstract_method
//         !virtual_method
//         fun f() -> A { }
//     }
// )");
