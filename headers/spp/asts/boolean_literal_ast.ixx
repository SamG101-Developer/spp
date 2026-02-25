module;
#include <spp/macros.hpp>

export module spp.asts.boolean_literal_ast;
import spp.asts.literal_ast;
import std;

namespace spp::asts {
    SPP_EXP_CLS struct BooleanLiteralAst;
    SPP_EXP_CLS struct TokenAst;
    SPP_EXP_CLS struct TypeAst;
}


/**
 * The BooleanLiteralAst represents a boolean literal in the source code, which can be either @c true or @c false.
 * This AST is used to represent boolean values in expressions and statements.
 */
SPP_EXP_CLS struct spp::asts::BooleanLiteralAst final : LiteralAst {
    std::unique_ptr<TokenAst> tok_bool;

    explicit BooleanLiteralAst(
        decltype(tok_bool) &&tok_bool);
    ~BooleanLiteralAst() override;
    auto to_rust() const -> std::string override;
};

