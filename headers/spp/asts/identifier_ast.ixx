module;
#include <spp/macros.hpp>

export module spp.asts.identifier_ast;
import spp.asts.primary_expression_ast;

import std;

namespace spp::asts {
    SPP_EXP_CLS struct IdentifierAst;
    SPP_EXP_CLS struct TypeAst;
}


SPP_EXP_CLS struct spp::asts::IdentifierAst final : PrimaryExpressionAst, std::enable_shared_from_this<IdentifierAst> {
private:
    std::size_t m_pos;

public:
    std::string val;

    explicit IdentifierAst(
        std::size_t pos,
        decltype(val) val);

    IdentifierAst(IdentifierAst const &) = default;

    ~IdentifierAst() override;

    SPP_ATTR_NODISCARD auto equals_identifier(IdentifierAst const &) const -> std::strong_ordering override;
    SPP_ATTR_NODISCARD auto equals(ExpressionAst const &other) const -> std::strong_ordering override;

public:
    SPP_AST_KEY_FUNCTIONS;

    SPP_ATTR_ALWAYS_INLINE auto operator<=>(IdentifierAst const &that) const -> std::strong_ordering {
        return val <=> that.val;
    }

    SPP_ATTR_ALWAYS_INLINE auto operator==(IdentifierAst const &that) const -> bool {
        return equals(that) == std::strong_ordering::equal;
    }

    SPP_ATTR_ALWAYS_INLINE auto operator==(ExpressionAst const &that) const -> bool override {
        return equals(that) == std::strong_ordering::equal;
    }

    auto operator+(IdentifierAst const &that) const -> IdentifierAst;

    auto operator+(std::string const &that) const -> IdentifierAst;

    static auto from_type(TypeAst const &val) -> std::unique_ptr<IdentifierAst>;

    auto to_function_identifier() const -> std::unique_ptr<IdentifierAst>;

    auto stage_7_analyse_semantics(ScopeManager *sm, CompilerMetaData *meta) -> void override;

    auto infer_type(ScopeManager *sm, CompilerMetaData *meta) -> std::shared_ptr<TypeAst> override;

    auto ankerl_hash() const -> std::size_t override;
};


spp::asts::IdentifierAst::~IdentifierAst() = default;
