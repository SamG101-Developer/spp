#pragma once
#include <spp/asts/literal_ast.hpp>


struct spp::asts::IntegerLiteralAst final : LiteralAst {
    SPP_AST_KEY_FUNCTIONS;

    /**
     * The optionally provided sign token. This can be either a @c + or @c - sign, indicating the sign of the integer
     * literal. No sign means the integer is positive by default.
     */
    std::unique_ptr<TokenAst> sign;

    /**
     * The token that represents the integer literal. This is the actual integer value in the source code.
     */
    std::unique_ptr<TokenAst> val;

    /**
     * The raw type of the integer literal. This is from the postfix tag to the literal, like "i32" or "u64".
     */
    std::string type;

protected:
    auto equals(ExpressionAst const &other) const -> std::weak_ordering override;

    auto equals_integer_literal(IntegerLiteralAst const &) const -> std::weak_ordering override;

public:
    /**
     * Construct the IntegerLiteralAst with the arguments matching the members.
     * @param[in] sign The optionally provided sign token.
     * @param[in] val The token that represents the integer literal.
     * @param[in] type The type of the integer literal.
     */
    IntegerLiteralAst(
        decltype(sign) &&sign,
        decltype(val) &&val,
        std::string &&type);

    ~IntegerLiteralAst() override;

    auto stage_7_analyse_semantics(ScopeManager *sm, mixins::CompilerMetaData *meta) -> void override;

    auto infer_type(ScopeManager *sm, mixins::CompilerMetaData *meta) -> std::shared_ptr<TypeAst> override;
};
