#include "../test_macros.hpp"


SPP_TEST_SHOULD_FAIL_SEMANTIC(
    TestMscDefinitionConflicts,
    test_invalid_definition_type_stmt_vs_type_stmt,
    SppIdentifierDuplicateError, R"(
    type A = S32
    type A = S64
)");


SPP_TEST_SHOULD_FAIL_SEMANTIC(
    TestMscDefinitionConflicts,
    test_invalid_definition_type_stmt_vs_cls_stmt,
    SppIdentifierDuplicateError, R"(
    type A = S32
    cls A { }
)");


SPP_TEST_SHOULD_FAIL_SEMANTIC(
    TestMscDefinitionConflicts,
    test_invalid_definition_cls_stmt_vs_type_stmt,
    SppIdentifierDuplicateError, R"(
    cls A { }
    type A = S32
)");


SPP_TEST_SHOULD_FAIL_SEMANTIC(
    TestMscDefinitionConflicts,
    test_invalid_definition_cls_stmt_vs_cls_stmt,
    SppIdentifierDuplicateError, R"(
    cls A { }
    cls A { }
)");


SPP_TEST_SHOULD_FAIL_SEMANTIC(
    TestMscDefinitionConflicts,
    test_invalid_definition_cmp_stmt_vs_cmp_stmt,
    SppIdentifierDuplicateError, R"(
    cmp a: S32 = 1_s32
    cmp a: S32 = 2_s32
)");
