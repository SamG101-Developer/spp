#pragma once
#include <spp/asts/primary_expression_ast.hpp>


struct spp::asts::IdentifierAst final : PrimaryExpressionAst {
    SPP_AST_KEY_FUNCTIONS;

    /**
     * The internal value of the identifier. This is the name of the identifier, such as @c variable or @c my_function.
     */
    std::string val;

protected:
    auto equals(ExpressionAst const &other) const -> std::strong_ordering override;

    auto equals_identifier(IdentifierAst const &) const -> std::strong_ordering override;

public:
    /**
     * Construct the IdentifierAst with the arguments matching the members.
     * @param[in] pos The position of the identifier in the source code.
     * @param[in] val The internal value of the identifier.
     */
    explicit IdentifierAst(
        std::size_t pos,
        decltype(val) val);

    IdentifierAst(IdentifierAst const &);

    ~IdentifierAst() override;

    auto operator<=>(IdentifierAst const&) const -> std::strong_ordering;

    auto operator==(IdentifierAst const&) const -> bool;

    auto operator+(IdentifierAst const &that) const -> IdentifierAst;

    auto operator+(std::string const &that) const -> IdentifierAst;

    static auto from_type(TypeAst const &val) -> std::unique_ptr<IdentifierAst>;

    auto to_function_identifier() const -> std::unique_ptr<IdentifierAst>;

    auto stage_7_analyse_semantics(ScopeManager *sm, mixins::CompilerMetaData *meta) -> void override;

    auto infer_type(ScopeManager *sm, mixins::CompilerMetaData *meta) -> std::shared_ptr<TypeAst> override;

private:
    std::size_t m_pos;
};
