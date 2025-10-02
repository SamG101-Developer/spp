#include "../test_macros.hpp"


SPP_TEST_SHOULD_PASS_SEMANTIC(test_valid_annotation_virtual_method, R"(
    cls A { }
    sup A {
        @virtual_method
        fun f() -> std::void::Void { }
    }
)")


SPP_TEST_SHOULD_PASS_SEMANTIC(test_valid_annotation_abstract_method, R"(
    cls A { }
    sup A {
        @abstract_method
        fun f() -> A { }
    }
)")


SPP_TEST_SHOULD_PASS_SEMANTIC(test_valid_annotation_no_impl, R"(
    cls A { }
    sup A {
        @no_impl
        fun f() -> A { }
    }

    @no_impl
    fun g() -> A { }
)")


SPP_TEST_SHOULD_PASS_SEMANTIC(test_valid_annotation_public, R"(
    @public
    cls A { }

    sup A {
        @public
        fun f() -> std::void::Void { }
    }

    @public
    fun g() -> std::void::Void { }
)")


SPP_TEST_SHOULD_PASS_SEMANTIC(test_valid_annotation_protected, R"(
    @protected
    cls A { }

    sup A {
        @protected
        fun f() -> std::void::Void { }
    }

    @protected
    fun g() -> std::void::Void { }
)")


SPP_TEST_SHOULD_PASS_SEMANTIC(test_valid_annotation_private, R"(
    @private
    cls A { }

    sup A {
        @private
        fun f() -> std::void::Void { }
    }

    @private
    fun g() -> std::void::Void { }
)")


SPP_TEST_SHOULD_PASS_SEMANTIC(test_valid_annotation_cold, R"(
    cls A { }

    sup A {
        @cold
        fun f() -> std::void::Void { }
    }

    @cold
    fun g() -> std::void::Void { }
)")


SPP_TEST_SHOULD_PASS_SEMANTIC(test_valid_annotation_hot, R"(
    cls A { }

    sup A {
        @hot
        fun f() -> std::void::Void { }
    }

    @hot
    fun g() -> std::void::Void { }
)")


SPP_TEST_SHOULD_FAIL_SEMANTIC(test_invalid_annotation_virtual_method_outside_sup, R"(
    cls A { }

    @virtual_method
    fun f() -> A { }
)", SppAnnotationInvalidApplicationError)


SPP_TEST_SHOULD_FAIL_SEMANTIC(test_invalid_annotation_abstract_method_outside_sup, R"(
    cls A { }

    @abstract_method
    fun f() -> A { }
)", SppAnnotationInvalidApplicationError)


SPP_TEST_SHOULD_FAIL_SEMANTIC(test_invalid_annotation_virtual_method_on_non_function, R"(
    @virtual_method
    cls A { }
)", SppAnnotationInvalidApplicationError)


SPP_TEST_SHOULD_FAIL_SEMANTIC(test_invalid_annotation_abstract_method_on_non_function, R"(
    @abstract_method
    cls A { }
)", SppAnnotationInvalidApplicationError)


SPP_TEST_SHOULD_FAIL_SEMANTIC(test_invalid_annotation_no_impl_on_non_function, R"(
    @no_impl
    cls A { }
)", SppAnnotationInvalidApplicationError)


SPP_TEST_SHOULD_FAIL_SEMANTIC(test_invalid_annotation_cold_on_non_function, R"(
    @cold
    cls A { }
)", SppAnnotationInvalidApplicationError)


SPP_TEST_SHOULD_FAIL_SEMANTIC(test_invalid_annotation_hot_on_non_function, R"(
    @hot
    cls A { }
)", SppAnnotationInvalidApplicationError)


SPP_TEST_SHOULD_FAIL_SEMANTIC(test_invalid_annotation_access_modifier_inside_sup_ext, R"(
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
)", SppAnnotationInvalidApplicationError)


SPP_TEST_SHOULD_FAIL_SEMANTIC(test_invalid_annotation, R"(
    cls A { }

    @invalid
    fun f() -> A { }
)", SppAnnotationInvalidError)


SPP_TEST_SHOULD_FAIL_SEMANTIC(test_invalid_annotation_conflicting_1, R"(
    cls A { }

    @public
    @protected
    fun f() -> A { }
)", SppAnnotationConflictError)


SPP_TEST_SHOULD_FAIL_SEMANTIC(test_invalid_annotation_conflicting_2, R"(
    cls A { }

    @protected
    @private
    fun f() -> A { }
)", SppAnnotationConflictError)


SPP_TEST_SHOULD_FAIL_SEMANTIC(test_invalid_annotation_conflicting_3, R"(
    cls A { }

    @private
    @public
    fun f() -> A { }
)", SppAnnotationConflictError)


SPP_TEST_SHOULD_FAIL_SEMANTIC(test_invalid_annotation_conflicting_4, R"(
    cls A { }

    @cold
    @hot
    fun f() -> A { }
)", SppAnnotationConflictError)


SPP_TEST_SHOULD_FAIL_SEMANTIC(test_invalid_annotation_conflicting_5, R"(
    cls A { }

    @hot
    @cold
    fun f() -> A { }
)", SppAnnotationConflictError)


SPP_TEST_SHOULD_FAIL_SEMANTIC(test_invalid_annotation_conflicting_6, R"(
    cls A { }
    sup A {
        @virtual_method
        @abstract_method
        fun f() -> A { }
    }
)", SppAnnotationConflictError)


SPP_TEST_SHOULD_FAIL_SEMANTIC(test_invalid_annotation_conflicting_7, R"(
    cls A { }
    sup A {
        @abstract_method
        @virtual_method
        fun f() -> A { }
    }
)", SppAnnotationConflictError)
