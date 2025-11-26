module;
#include <spp/macros.hpp>

export module spp.asts.identifier_ast;
import spp.asts._fwd;
import spp.asts.primary_expression_ast;

import std;


SPP_EXP struct spp::asts::IdentifierAst final : PrimaryExpressionAst, std::enable_shared_from_this<IdentifierAst> {
    /**
     * The internal value of the identifier. This is the name of the identifier, such as @c variable or @c my_function.
     */
    std::string val;

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

protected:
    SPP_ATTR_ALWAYS_INLINE auto equals(ExpressionAst const &other) const -> std::strong_ordering override {
        return other.equals_identifier(*this);
    }

    SPP_ATTR_ALWAYS_INLINE auto equals_identifier(IdentifierAst const &other) const -> std::strong_ordering override {
        if (val == other.val) {
            return std::strong_ordering::equal;
        }
        return std::strong_ordering::less;
    }

public:
    SPP_AST_KEY_FUNCTIONS;

    SPP_ATTR_ALWAYS_INLINE auto operator<=>(IdentifierAst const &that) const -> std::strong_ordering {
        return val <=> that.val;
    }

    SPP_ATTR_ALWAYS_INLINE auto operator==(IdentifierAst const &that) const -> bool {
        return equals(that) == std::strong_ordering::equal;
    }

    SPP_ATTR_ALWAYS_INLINE auto operator==(ExpressionAst const &that) const -> bool {
        return equals(that) == std::strong_ordering::equal;
    }

    auto operator+(IdentifierAst const &that) const -> IdentifierAst;

    auto operator+(std::string const &that) const -> IdentifierAst;

    static auto from_type(TypeAst const &val) -> std::unique_ptr<IdentifierAst>;

    auto to_function_identifier() const -> std::unique_ptr<IdentifierAst>;

    auto stage_7_analyse_semantics(ScopeManager *sm, CompilerMetaData *meta) -> void override;

    auto infer_type(ScopeManager *sm, CompilerMetaData *meta) -> std::shared_ptr<TypeAst> override;

private:
    std::size_t m_pos;
};
