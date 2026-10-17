#include "../test_macros.hpp"

import spp.analyse.utils.annotation_utils;

/// A method should be able to be declared as a "virtual
/// method". Apply it to a method on a superimposition block
/// and expect no errors.
SPP_TEST_SHOULD_PASS_SEMANTIC(
  AnnotationAst_BuiltinVirtualMethod,
  test_valid_usage_on_method, R"(
    cls A { }
    sup A {
      !virtual_method
      fun f() -> Void { }
    }
)");

/// A free function should not be able to be declared as a
/// virtual method. Apply it to a free function and expect
/// an annotation error to be raised.
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
        fun f() -> Void { }
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

/// The compiler binds behaviour to an llvm annotation by
/// comparing its fully qualified name to a spelling of its own.
/// A spelling std does not define is never matched, and the
/// annotation is accepted and then silently ignored, so every
/// name the compiler checks for must resolve to a std
/// annotation. Taken from the compiler's table rather than
/// spelled again here, so a drift between the two is caught.
TEST(AnnotationAst_BuiltinLlvm, test_compiler_names_resolve_in_std) {
  using A = spp::analyse::utils::annotation_utils::BuiltinAnnotations;
  for (const auto fq_name : {A::kLlvmInline, A::kLlvmAlwaysInline, A::kLlvmNoInline, A::kLlvmHot, A::kLlvmCold}) {
    const auto fq = spp::Str(fq_name);
    const auto name = fq.substr(fq.rfind("::") + 2);
    EXPECT_NO_THROW(build_temp_project("use " + fq + "\n!" + name + "\nfun f() -> Void { }\n")) << fq;
  }
}

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
