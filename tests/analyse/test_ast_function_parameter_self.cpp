#include "../test_macros.hpp"
import testex;


SPP_TEST_SHOULD_PASS_SEMANTIC(
    FunctionParameterSelfAst,
    test_valid_mov, R"(
    cls TestType { }
    sup TestType {
        fun f(self) -> std::void::Void { }
    }
)");;


SPP_TEST_SHOULD_PASS_SEMANTIC(
    FunctionParameterSelfAst,
    test_valid_mov_mut, R"(
    cls TestType { }
    sup TestType {
        fun f(mut self) -> std::void::Void { }
    }
)");;


SPP_TEST_SHOULD_PASS_SEMANTIC(
    FunctionParameterSelfAst,
    test_valid_mut, R"(
    cls TestType { }
    sup TestType {
        fun f(&mut self) -> std::void::Void { }
    }
)");;


SPP_TEST_SHOULD_PASS_SEMANTIC(
    FunctionParameterSelfAst,
    test_valid_ref, R"(
    cls TestType { }
    sup TestType {
        fun f(&self) -> std::void::Void { }
    }
)");;
