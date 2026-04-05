#include "../test_macros.hpp"


SPP_TEST_SHOULD_PASS_SEMANTIC(
    AnnotationAst,
    test_valid_annotation_virtual_method, R"(
    cls A { }
    sup A {
        !virtual_method
        fun f() -> std::void::Void { }
    }
)");


SPP_TEST_SHOULD_PASS_SEMANTIC(
    AnnotationAst,
    test_valid_annotation_abstract_method, R"(
    cls A { }
    sup A {
        !abstract_method
        fun f() -> A { }
    }
)");


SPP_TEST_SHOULD_PASS_SEMANTIC(
    AnnotationAst,
    test_valid_annotation_public, R"(
    !public
    cls A { }

    sup A {
        !public
        fun f() -> std::void::Void { }
    }

    !public
    fun g() -> std::void::Void { }
)");


SPP_TEST_SHOULD_PASS_SEMANTIC(
    AnnotationAst,
    test_valid_annotation_protected, R"(
    !protected
    cls A { }

    sup A {
        !protected
        fun f() -> std::void::Void { }
    }

    !protected
    fun g() -> std::void::Void { }
)");


SPP_TEST_SHOULD_PASS_SEMANTIC(
    AnnotationAst,
    test_valid_annotation_private, R"(
    !private
    cls A { }

    sup A {
        !private
        fun f() -> std::void::Void { }
    }

    !private
    fun g() -> std::void::Void { }
)");


SPP_TEST_SHOULD_PASS_SEMANTIC(
    AnnotationAst,
    test_valid_annotation_cold, R"(
    cls A { }

    sup A {
        !std::llvm::cold
        fun f() -> std::void::Void { }
    }

    !std::llvm::cold
    fun g() -> std::void::Void { }
)");


SPP_TEST_SHOULD_PASS_SEMANTIC(
    AnnotationAst,
    test_valid_annotation_hot, R"(
    cls A { }

    sup A {
        !std::llvm::hot
        fun f() -> std::void::Void { }
    }

    !std::llvm::hot
    fun g() -> std::void::Void { }
)");


SPP_TEST_SHOULD_FAIL_SEMANTIC(
    AnnotationAst,
    test_invalid_annotation_virtual_method_outside_sup,
    SppCalledAnnotationAppliedToInvalidAstError, R"(
    !virtual_method
    fun f() -> Void { }
)");


SPP_TEST_SHOULD_FAIL_SEMANTIC(
    AnnotationAst,
    test_invalid_annotation_abstract_method_outside_sup,
    SppCalledAnnotationAppliedToInvalidAstError, R"(
    !abstract_method
    fun f() -> Void { }
)");


SPP_TEST_SHOULD_FAIL_SEMANTIC(
    AnnotationAst,
    test_invalid_annotation_virtual_method_on_non_function,
    SppCalledAnnotationAppliedToInvalidAstError, R"(
    !virtual_method
    cls A { }
)");


SPP_TEST_SHOULD_FAIL_SEMANTIC(
    AnnotationAst,
    test_invalid_annotation_abstract_method_on_non_function,
    SppCalledAnnotationAppliedToInvalidAstError, R"(
    !abstract_method
    cls A { }
)");


SPP_TEST_SHOULD_FAIL_SEMANTIC(
    AnnotationAst,
    test_invalid_annotation_cold_on_non_function,
    SppCalledAnnotationAppliedToInvalidAstError, R"(
    !std::llvm::cold
    cls A { }
)");


SPP_TEST_SHOULD_FAIL_SEMANTIC(
    AnnotationAst,
    test_invalid_annotation_hot_on_non_function,
    SppCalledAnnotationAppliedToInvalidAstError, R"(
    !std::llvm::hot
    cls A { }
)");


SPP_TEST_SHOULD_FAIL_SEMANTIC(
    AnnotationAst,
    test_invalid_annotation_access_modifier_inside_sup_ext,
    SppCalledAnnotationAppliedToInvalidAstError, R"(
    cls A { }
    sup A {
        !public  # fine
        !virtual_method
        fun f(&self) -> Void { }
    }

    cls B { }
    sup B ext A {
        !public  # bad
        fun f(&self) -> Void { }
    }
)");


SPP_TEST_SHOULD_FAIL_SEMANTIC(
    AnnotationAst,
    test_invalid_annotation_annotation_identifier,
    SppAnnotationTargetNotAnAnnotationError, R"(
    cls A { }

    fun invalid() -> Void { }

    !invalid
    fun f() -> A { }
)");


SPP_TEST_SHOULD_FAIL_SEMANTIC(
    AnnotationAst,
    test_invalid_annotation_non_cmp_funcitonal,
    SppAnnotationTargetNotACmpFunctionError, R"(
    cls A { }

    !std::annotations::annotation
    fun aaa() -> A { }

    !aaa
    fun f() -> A { }
)");


SPP_TEST_SHOULD_FAIL_SEMANTIC(
    AnnotationAst,
    test_invalid_annotation_non_annotation,
    SppAnnotationTargetNotAnAnnotationError, R"(
    cls A { }

    cmp fun aaa() -> A { }

    !aaa
    fun f() -> A { }
)");


SPP_TEST_SHOULD_FAIL_SEMANTIC(
    AnnotationAst,
    test_invalid_annotation_invalid_args,
    SppFunctionCallNoValidSignaturesError, R"(
    cls A { }

    !annotation(target=Annotation::function)
    cmp fun aaa(x: StrView) -> A { }

    !aaa
    fun f() -> A { }
)");


SPP_TEST_SHOULD_PASS_SEMANTIC(
    AnnotationAst,
    test_valid_annotation_custom_no_args, R"(
    !std::annotations::annotation(target=Annotation::function)
    cmp fun my_annotation() -> Void { }

    !my_annotation
    fun f() -> Void { }
)");


SPP_TEST_SHOULD_PASS_SEMANTIC(
    AnnotationAst,
    test_valid_annotation_custom_with_args, R"(
    cls A { }

    !std::annotations::annotation(target=Annotation::function)
    cmp fun my_annotation(a: StrView) -> Void { }

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
