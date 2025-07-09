#ifndef IDENTIFIER_AST_HPP
#define IDENTIFIER_AST_HPP

#include <spp/macros.hpp>
#include <spp/asts/primary_expression_ast.hpp>
#include <spp/asts/_fwd.hpp>


struct spp::asts::IdentifierAst final : PrimaryExpressionAst {
    SPP_AST_KEY_FUNCTIONS;

    /**
     * The internal value of the identifier. This is the name of the identifier, such as @c variable or @c my_function.
     */
    icu::UnicodeString val;

    /**
     * Construct the IdentifierAst with the arguments matching the members.
     * @param[in] pos The position of the identifier in the source code.
     * @param[in] val The internal value of the identifier.
     */
    explicit IdentifierAst(
        std::size_t pos,
        decltype(val) &&val);

    auto operator==(IdentifierAst const &that) const -> bool;

    auto operator+(IdentifierAst const &that) const -> IdentifierAst;

    auto operator+(icu::UnicodeString const &that) const -> IdentifierAst;

    static auto from_type(TypeAst const &val) -> std::unique_ptr<IdentifierAst>;

    SPP_ATTR_NODISCARD auto to_function_identifier() const -> std::unique_ptr<IdentifierAst>;

private:
    std::size_t m_pos;
};


#endif //IDENTIFIER_AST_HPP
