#pragma once
#include <map>

#include <spp/asts/_fwd.hpp>
#include <spp/lex/tokens.hpp>


namespace spp::analyse::scopes {
    class ScopeManager;
}


namespace spp::analyse::utils::bin_utils {
    const static auto BIN_OP_PRECEDENCE = std::map<lex::SppTokenType, std::uint8_t>{
        {lex::SppTokenType::KW_OR, 1},
        {lex::SppTokenType::KW_AND, 2},
        {lex::SppTokenType::TK_EQ, 3},
        {lex::SppTokenType::TK_NE, 3},
        {lex::SppTokenType::TK_LT, 3},
        {lex::SppTokenType::TK_LE, 3},
        {lex::SppTokenType::TK_GT, 3},
        {lex::SppTokenType::TK_GE, 3},
        {lex::SppTokenType::TK_BIT_IOR, 4},
        {lex::SppTokenType::TK_BIT_XOR, 5},
        {lex::SppTokenType::TK_BIT_AND, 6},
        {lex::SppTokenType::TK_BIT_SHL, 7},
        {lex::SppTokenType::TK_BIT_SHR, 7},
        {lex::SppTokenType::TK_ADD, 8},
        {lex::SppTokenType::TK_SUB, 8},
        {lex::SppTokenType::TK_MUL, 9},
        {lex::SppTokenType::TK_DIV, 9},
        {lex::SppTokenType::TK_REM, 9},
        {lex::SppTokenType::TK_POW, 9}
    };

    const static auto BIN_METHODS = std::map<lex::SppTokenType, std::string>{
        {lex::SppTokenType::KW_OR, "ior_"},
        {lex::SppTokenType::KW_AND, "and_"},
        {lex::SppTokenType::TK_EQ, "eq"},
        {lex::SppTokenType::TK_NE, "ne"},
        {lex::SppTokenType::TK_LT, "lt"},
        {lex::SppTokenType::TK_LE, "le"},
        {lex::SppTokenType::TK_GT, "gt"},
        {lex::SppTokenType::TK_GE, "ge"},
        {lex::SppTokenType::TK_BIT_IOR, "bit_ior"},
        {lex::SppTokenType::TK_BIT_XOR, "bit_xor"},
        {lex::SppTokenType::TK_BIT_AND, "bit_and"},
        {lex::SppTokenType::TK_BIT_SHL, "bit_shl"},
        {lex::SppTokenType::TK_BIT_SHR, "bit_shr"},
        {lex::SppTokenType::TK_ADD, "add"},
        {lex::SppTokenType::TK_SUB, "sub"},
        {lex::SppTokenType::TK_MUL, "mul"},
        {lex::SppTokenType::TK_DIV, "div"},
        {lex::SppTokenType::TK_REM, "mod"},
        {lex::SppTokenType::TK_POW, "pow"},
        {lex::SppTokenType::TK_BIT_IOR_ASSIGN, "bit_ior_assign"},
        {lex::SppTokenType::TK_BIT_XOR_ASSIGN, "bit_xor_assign"},
        {lex::SppTokenType::TK_BIT_AND_ASSIGN, "bit_and_assign"},
        {lex::SppTokenType::TK_BIT_SHL_ASSIGN, "bit_shl_assign"},
        {lex::SppTokenType::TK_BIT_SHR_ASSIGN, "bit_shr_assign"},
        {lex::SppTokenType::TK_ADD_ASSIGN, "add_assign"},
        {lex::SppTokenType::TK_SUB_ASSIGN, "sub_assign"},
        {lex::SppTokenType::TK_MUL_ASSIGN, "mul_assign"},
        {lex::SppTokenType::TK_DIV_ASSIGN, "div_assign"},
        {lex::SppTokenType::TK_REM_ASSIGN, "mod_assign"},
        {lex::SppTokenType::TK_POW_ASSIGN, "pow_assign"}
    };

    constexpr static auto BIN_COMPOUND_ASSIGNMENT_OPS = std::array{
        lex::SppTokenType::TK_ADD_ASSIGN,
        lex::SppTokenType::TK_SUB_ASSIGN,
        lex::SppTokenType::TK_MUL_ASSIGN,
        lex::SppTokenType::TK_DIV_ASSIGN,
        lex::SppTokenType::TK_REM_ASSIGN,
        lex::SppTokenType::TK_BIT_AND_ASSIGN,
        lex::SppTokenType::TK_BIT_IOR_ASSIGN,
        lex::SppTokenType::TK_BIT_XOR_ASSIGN,
        lex::SppTokenType::TK_BIT_SHL_ASSIGN,
        lex::SppTokenType::TK_BIT_SHR_ASSIGN
    };

    constexpr static auto BIN_COMPARISON_OPS = std::array{
        lex::SppTokenType::TK_EQ,
        lex::SppTokenType::TK_NE,
        lex::SppTokenType::TK_LT,
        lex::SppTokenType::TK_GT,
        lex::SppTokenType::TK_LE,
        lex::SppTokenType::TK_GE
    };

    auto fix_associativity(
        asts::BinaryExpressionAst &bin_expr)
        -> std::unique_ptr<asts::BinaryExpressionAst>;

    auto combine_comp_ops(
        asts::BinaryExpressionAst &bin_expr)
        -> std::unique_ptr<asts::BinaryExpressionAst>;

    auto convert_bin_expr_to_function_call(
        asts::BinaryExpressionAst &bin_expr)
        -> std::unique_ptr<asts::PostfixExpressionAst>;

    auto convert_is_expr_to_function_call(
        asts::IsExpressionAst &is_expr)
        -> std::unique_ptr<asts::CaseExpressionAst>;
}
