#pragma once
#include <spp/asts/literal_ast.hpp>


struct spp::asts::TupleLiteralAst final : LiteralAst {
    SPP_AST_KEY_FUNCTIONS;

    /**
     * The left parenthesis token that represents the start of the tuple literal.
     */
    std::unique_ptr<TokenAst> tok_l;

    /**
     * The elements of the tuple literal. This is a list of expressions that are contained within the tuple.
     */
    std::vector<std::unique_ptr<ExpressionAst>> elems;

    /**
     * The right parenthesis token that represents the end of the tuple literal.
     */
    std::unique_ptr<TokenAst> tok_r;

protected:
    auto equals(ExpressionAst const &other) const -> std::weak_ordering override;

    auto equals_tuple_literal(TupleLiteralAst const &) const -> std::weak_ordering override;

public:

    /**
     * Construct the TupleLiteralAst with the arguments matching the members.
     * @param tok_l The left parenthesis token.
     * @param elements The elements of the tuple literal.
     * @param tok_r The right parenthesis token.
     */
    TupleLiteralAst(
        decltype(tok_l) &&tok_l,
        decltype(elems) &&elements,
        decltype(tok_r) &&tok_r);

    ~TupleLiteralAst() override;

    auto stage_7_analyse_semantics(ScopeManager *sm, mixins::CompilerMetaData *meta) -> void override;

    auto stage_8_check_memory(ScopeManager *sm, mixins::CompilerMetaData *meta) -> void override;

    auto infer_type(ScopeManager *sm, mixins::CompilerMetaData *meta) -> std::shared_ptr<TypeAst> override;
};
