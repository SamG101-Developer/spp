#pragma once
#include <spp/asts/literal_ast.hpp>


struct spp::asts::StringLiteralAst final : LiteralAst {
    SPP_AST_KEY_FUNCTIONS;

    /**
     * The string value of the string literal. This is the actual string that is represented by the literal.
     */
    std::unique_ptr<TokenAst> val;

protected:
    auto equals(ExpressionAst const &other) const -> std::strong_ordering override;

    auto equals_string_literal(StringLiteralAst const &) const -> std::strong_ordering override;

public:
    /**
     * Construct the StringLiteralAst with the arguments matching the members.
     * @param[in] val The string value of the string literal.
     */
    explicit StringLiteralAst(
        decltype(val) &&val);

    ~StringLiteralAst() override;

    auto infer_type(ScopeManager *sm, mixins::CompilerMetaData *meta) -> std::shared_ptr<TypeAst> override;
};
