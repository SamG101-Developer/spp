#include "../test_macros.hpp"


SPP_TEST_SHOULD_PASS_SEMANTIC(
    test_valid_annotation_virtual_method, R"(
    cls A { }
    sup A {
        @virtual_method
        fun f() -> std::void::Void { }
    }
)")


SPP_TEST_SHOULD_PASS_SEMANTIC(
    test_valid_annotation_abstract_method, R"(
    cls A { }
    sup A {
        @abstract_method
        fun f() -> A { }
    }
)")


SPP_TEST_SHOULD_PASS_SEMANTIC(
    test_valid_annotation_no_impl, R"(
    cls A { }
    sup A {
        @no_impl
        fun f() -> A { }
    }

    @no_impl
    fun g() -> A { }
)")


SPP_TEST_SHOULD_PASS_SEMANTIC(
    test_valid_annotation_public, R"(
    @public
    cls A { }

    sup A {
        @public
        fun f() -> std::void::Void { }
    }

    @public
    fun g() -> std::void::Void { }
)")


SPP_TEST_SHOULD_PASS_SEMANTIC(
    test_valid_annotation_protected, R"(
    @protected
    cls A { }

    sup A {
        @protected
        fun f() -> std::void::Void { }
    }

    @protected
    fun g() -> std::void::Void { }
)")


SPP_TEST_SHOULD_PASS_SEMANTIC(
    test_valid_annotation_private, R"(
    @private
    cls A { }

    sup A {
        @private
        fun f() -> std::void::Void { }
    }

    @private
    fun g() -> std::void::Void { }
)")


SPP_TEST_SHOULD_PASS_SEMANTIC(
    test_valid_annotation_cold, R"(
    cls A { }

    sup A {
        @cold
        fun f() -> std::void::Void { }
    }

    @cold
    fun g() -> std::void::Void { }
)")


SPP_TEST_SHOULD_PASS_SEMANTIC(
    test_valid_annotation_hot, R"(
    cls A { }

    sup A {
        @hot
        fun f() -> std::void::Void { }
    }

    @hot
    fun g() -> std::void::Void { }
)")


SPP_TEST_SHOULD_FAIL_SEMANTIC(
    test_invalid_annotation_virtual_method_outside_sup,
    SppAnnotationInvalidApplicationError, R"(
    cls A { }

    @virtual_method
    fun f() -> A { }
)")


SPP_TEST_SHOULD_FAIL_SEMANTIC(
    test_invalid_annotation_abstract_method_outside_sup,
    SppAnnotationInvalidApplicationError, R"(
    cls A { }

    @abstract_method
    fun f() -> A { }
)")


SPP_TEST_SHOULD_FAIL_SEMANTIC(
    test_invalid_annotation_virtual_method_on_non_function,
    SppAnnotationInvalidApplicationError, R"(
    @virtual_method
    cls A { }
)")


SPP_TEST_SHOULD_FAIL_SEMANTIC(
    test_invalid_annotation_abstract_method_on_non_function,
    SppAnnotationInvalidApplicationError, R"(
    @abstract_method
    cls A { }
)")


SPP_TEST_SHOULD_FAIL_SEMANTIC(
    test_invalid_annotation_no_impl_on_non_function,
    SppAnnotationInvalidApplicationError, R"(
    @no_impl
    cls A { }
)")


SPP_TEST_SHOULD_FAIL_SEMANTIC(
    test_invalid_annotation_cold_on_non_function,
    SppAnnotationInvalidApplicationError, R"(
    @cold
    cls A { }
)")


SPP_TEST_SHOULD_FAIL_SEMANTIC(
    test_invalid_annotation_hot_on_non_function,
    SppAnnotationInvalidApplicationError, R"(
    @hot
    cls A { }
)")


SPP_TEST_SHOULD_FAIL_SEMANTIC(
    test_invalid_annotation_access_modifier_inside_sup_ext,
    SppAnnotationInvalidApplicationError, R"(
    cls A { }
    sup A {
        @public
        fun f(&self) -> A { }
    }

    cls B { }
    sup B ext A {
        @public
        fun f(&self) -> A { }
    }
)")


SPP_TEST_SHOULD_FAIL_SEMANTIC(
    test_invalid_annotation,
    SppAnnotationInvalidError, R"(
    cls A { }

    @invalid
    fun f() -> A { }
)")


SPP_TEST_SHOULD_FAIL_SEMANTIC(
    test_invalid_annotation_conflicting_1,
    SppAnnotationConflictError, R"(
    cls A { }

    @public
    @protected
    fun f() -> A { }
)")


SPP_TEST_SHOULD_FAIL_SEMANTIC(
    test_invalid_annotation_conflicting_2,
    SppAnnotationConflictError, R"(
    cls A { }

    @protected
    @private
    fun f() -> A { }
)")


SPP_TEST_SHOULD_FAIL_SEMANTIC(
    test_invalid_annotation_conflicting_3,
    SppAnnotationConflictError, R"(
    cls A { }

    @private
    @public
    fun f() -> A { }
)")


SPP_TEST_SHOULD_FAIL_SEMANTIC(
    test_invalid_annotation_conflicting_4,
    SppAnnotationConflictError, R"(
    cls A { }

    @cold
    @hot
    fun f() -> A { }
)")


SPP_TEST_SHOULD_FAIL_SEMANTIC(
    test_invalid_annotation_conflicting_5,
    SppAnnotationConflictError, R"(
    cls A { }

    @hot
    @cold
    fun f() -> A { }
)")


SPP_TEST_SHOULD_FAIL_SEMANTIC(
    test_invalid_annotation_conflicting_6,
    SppAnnotationConflictError, R"(
    cls A { }
    sup A {
        @virtual_method
        @abstract_method
        fun f() -> A { }
    }
)")


SPP_TEST_SHOULD_FAIL_SEMANTIC(
    test_invalid_annotation_conflicting_7,
    SppAnnotationConflictError, R"(
    cls A { }
    sup A {
        @abstract_method
        @virtual_method
        fun f() -> A { }
    }
)")
