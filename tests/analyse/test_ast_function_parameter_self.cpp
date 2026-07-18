#include "../test_macros.hpp"

SPP_TEST_SHOULD_PASS_SEMANTIC(
    FunctionParameterSelfAst,
    test_valid_mov, R"(
    cls TestType { }
    sup TestType {
        fun f(self) -> Void { }
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    FunctionParameterSelfAst,
    test_valid_mov_mut, R"(
    cls TestType { }
    sup TestType {
        fun f(mut self) -> Void { }
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    FunctionParameterSelfAst,
    test_valid_mut, R"(
    cls TestType { }
    sup TestType {
        fun f(&mut self) -> Void { }
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    FunctionParameterSelfAst,
    test_valid_ref, R"(
    cls TestType { }
    sup TestType {
        fun f(&self) -> Void { }
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    FunctionParameterSelfAst,
    test_valid_mut_self_mutates_attribute, R"(
    cls TestType {
        !public x: Bool
    }
    sup TestType {
        fun f(&mut self) -> Void { self.x = true }
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    FunctionParameterSelfAst,
    test_valid_owned_mut_self_mutates_attribute, R"(
    cls TestType {
        !public x: Bool
    }
    sup TestType {
        fun f(mut self) -> Void { self.x = true }
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    FunctionParameterSelfAst,
    test_invalid_ref_self_mutates_attribute,
    SppInvalidMutationError, R"(
    cls TestType {
        !public x: Bool
    }
    sup TestType {
        fun f(&self) -> Void { self.x = true }
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    FunctionParameterSelfAst,
    test_invalid_owned_immutable_self_mutates_attribute,
    SppInvalidMutationError, R"(
    cls TestType {
        !public x: Bool
    }
    sup TestType {
        fun f(self) -> Void { self.x = true }
    }
)");
