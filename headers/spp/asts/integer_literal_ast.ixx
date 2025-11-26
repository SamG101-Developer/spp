module;
#include <spp/macros.hpp>

export module spp.asts.integer_literal_ast;
import spp.asts.literal_ast;

import std;


SPP_EXP struct spp::asts::IntegerLiteralAst final : LiteralAst {
    /**
     * The optionally provided sign token. This can be either a @c + or @c - sign, indicating the sign of the integer
     * literal. No sign means the integer is positive by default.
     */
    std::unique_ptr<TokenAst> tok_sign;

    /**
     * The token that represents the integer literal. This is the actual integer value in the source code.
     */
    std::unique_ptr<TokenAst> val;

    /**
     * The raw type of the integer literal. This is from the postfix tag to the literal, like "i32" or "u64".
     */
    std::string type;

    /**
     * Construct the IntegerLiteralAst with the arguments matching the members.
     * @param[in] tok_sign The optionally provided sign token.
     * @param[in] val The token that represents the integer literal.
     * @param[in] type The type of the integer literal.
     */
    IntegerLiteralAst(
        decltype(tok_sign) &&tok_sign,
        decltype(val) &&val,
        std::string &&type);

    ~IntegerLiteralAst() override;

protected:
    auto equals(ExpressionAst const &other) const -> std::strong_ordering override;

    auto equals_integer_literal(IntegerLiteralAst const &) const -> std::strong_ordering override;

public:
    SPP_AST_KEY_FUNCTIONS;

    auto stage_7_analyse_semantics(ScopeManager *sm, CompilerMetaData *meta) -> void override;

    auto infer_type(ScopeManager *sm, CompilerMetaData *meta) -> std::shared_ptr<TypeAst> override;
};
