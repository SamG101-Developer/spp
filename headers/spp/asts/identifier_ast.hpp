#ifndef IDENTIFIER_AST_HPP
#define IDENTIFIER_AST_HPP

#include <spp/macros.hpp>
#include <spp/asts/primary_expression_ast.hpp>
#include <spp/asts/_fwd.hpp>


struct spp::asts::IdentifierAst final : PrimaryExpressionAst {
    SPP_AST_KEY_FUNCTIONS;

    std::string val;

    explicit IdentifierAst(
        std::string &&val);

    auto operator==(IdentifierAst const &that) const -> bool;

    auto operator+(IdentifierAst const &that) const -> IdentifierAst;

    auto operator+(std::string const &that) const -> IdentifierAst;

    static auto from_type(TypeAst const &val) -> IdentifierAst;

    static auto from_generic_identifier(GenericIdentifierAst const &val) -> IdentifierAst;

    SPP_ATTR_NODISCARD auto to_function_identifier() const -> IdentifierAst;
};


#endif //IDENTIFIER_AST_HPP
