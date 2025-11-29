module;
#include <spp/macros.hpp>

export module spp.analyse.utils.bin_utils;
import spp.lex.tokens;

import std;

namespace spp::asts {
    SPP_EXP_CLS struct BinaryExpressionAst;
    SPP_EXP_CLS struct CaseExpressionAst;
    SPP_EXP_CLS struct IsExpressionAst;
    SPP_EXP_CLS struct PostfixExpressionAst;
}

namespace spp::asts::meta {
    SPP_EXP_CLS struct CompilerMetaData;
}

namespace spp::analyse::scopes {
    SPP_EXP_CLS class ScopeManager;
    SPP_EXP_CLS struct TypeSymbol;
}


namespace spp::analyse::utils::bin_utils {
    /**
     * Define the precedence of binary operators. Higher numbers indicate higher precedence. This is used when
     * re-balancing binary expressions to ensure that the correct order of operations is maintained.
     */
    SPP_EXP_CLS const auto BIN_OP_PRECEDENCE = std::map<lex::SppTokenType, std::uint8_t>{
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

    /**
     * The map of binary operators to their corresponding method names. This is used when converting binary expressions
     * to function calls.
     */
    SPP_EXP_CLS const auto BIN_METHODS = std::map<lex::SppTokenType, std::string>{
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

    /**
     * The list of binary compound assignment operators.
     */
    SPP_EXP_CLS constexpr auto BIN_COMPOUND_ASSIGNMENT_OPS = std::array{
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

    /**
     * The list of binary comparison operators.
     */
    SPP_EXP_CLS constexpr auto BIN_COMPARISON_OPS = std::array{
        lex::SppTokenType::TK_EQ,
        lex::SppTokenType::TK_NE,
        lex::SppTokenType::TK_LT,
        lex::SppTokenType::TK_GT,
        lex::SppTokenType::TK_LE,
        lex::SppTokenType::TK_GE
    };

    /**
     * Rebalance the binary expression AST to ensure correct order of operations based on operator precedence. The
     * parser parses expressions in reverse precedence to avoid left hand recursion, so needs rebalancing.
     * @param bin_expr The binary expression AST to rebalance.
     * @param sm The scope manager for the current analysis context.
     * @param meta Metadata to pass between ASTs.
     * @return A new binary expression AST with correct order of operations.
     */
    SPP_EXP_FUN auto fix_associativity(
        asts::BinaryExpressionAst &bin_expr,
        scopes::ScopeManager *sm,
        asts::meta::CompilerMetaData *meta)
        -> std::unique_ptr<asts::BinaryExpressionAst>;

    SPP_EXP_FUN auto combine_comp_ops(
        asts::BinaryExpressionAst &bin_expr,
        scopes::ScopeManager *sm,
        asts::meta::CompilerMetaData *meta)
        -> std::unique_ptr<asts::BinaryExpressionAst>;

    SPP_EXP_FUN auto convert_bin_expr_to_function_call(
        asts::BinaryExpressionAst &bin_expr,
        scopes::ScopeManager *sm,
        asts::meta::CompilerMetaData *meta)
        -> std::unique_ptr<asts::PostfixExpressionAst>;

    SPP_EXP_FUN auto convert_is_expr_to_function_call(
        asts::IsExpressionAst &is_expr,
        scopes::ScopeManager *sm,
        asts::meta::CompilerMetaData *meta)
        -> std::unique_ptr<asts::CaseExpressionAst>;
}
