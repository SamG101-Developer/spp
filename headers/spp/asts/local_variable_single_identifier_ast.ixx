module;
#include <spp/macros.hpp>

export module spp.asts.local_variable_single_identifier_ast;
import spp.asts.local_variable_ast;
import std;

namespace spp::asts {
    SPP_EXP_CLS struct CasePatternVariantLiteralAst;
    SPP_EXP_CLS struct CasePatternVariantSingleIdentifierAst;
    SPP_EXP_CLS struct ConventionAst;
    SPP_EXP_CLS struct IdentifierAst;
    SPP_EXP_CLS struct LocalVariableSingleIdentifierAst;
    SPP_EXP_CLS struct LocalVariableSingleIdentifierAliasAst;
    SPP_EXP_CLS struct TokenAst;
}


/**
 * The LocalVariableSingleIdentifierAst represents a local variable that is defined by a single identifier. This is used
 * to define variables in the local scope, such as in function bodies. A typical example of this @code mut x@endcode.
 * This defines a local variable called @c x that is mutable.
 *
 * @n
 * This AST can be nested inside other local variable ASTs, such as a tuple destructure,
 * ie @code (mut x, y) = (1, 2)@endcode. As this AST is either used as a parameter or part of a "let" statement, it will
 * be introducing a symbol into the local scope.
 */
SPP_EXP_CLS struct spp::asts::LocalVariableSingleIdentifierAst final : LocalVariableAst {
    std::unique_ptr<ConventionAst> conv;
    std::unique_ptr<TokenAst> tok_mut;
    std::unique_ptr<IdentifierAst> name;
    std::unique_ptr<LocalVariableSingleIdentifierAliasAst> alias;

    explicit LocalVariableSingleIdentifierAst(
        decltype(tok_mut) &&tok_mut,
        decltype(name) &&name,
        decltype(alias) &&alias);
    ~LocalVariableSingleIdentifierAst() override;
    auto to_rust() const -> std::string override;
};
