module;
#include <spp/macros.hpp>

module spp.asts.type_postfix_expression_operator_nested_type_ast;
import spp.asts.type_postfix_expression_operator_ast;
import spp.asts.ast;
import spp.asts.identifier_ast;
import spp.asts.token_ast;
import spp.asts.type_identifier_ast;
import spp.asts.utils.ast_utils;
import spp.lex.tokens;

SPP_MOD_BEGIN
spp::asts::TypePostfixExpressionOperatorNestedTypeAst::TypePostfixExpressionOperatorNestedTypeAst(
    decltype(TokSep) &&tok_sep,
    decltype(Name) name) :
    TokSep(std::move(tok_sep)),
    Name(std::move(name)) {
    SPP_SET_AST_TO_DEFAULT_IF_NULLPTR(this->TokSep, lex::SppTokenType::TK_DOUBLE_COLON, "::");
}

spp::asts::TypePostfixExpressionOperatorNestedTypeAst::~TypePostfixExpressionOperatorNestedTypeAst() = default;

auto spp::asts::TypePostfixExpressionOperatorNestedTypeAst::EqualsNestedType(
    TypePostfixExpressionOperatorNestedTypeAst const &other) const
    -> Ordering {
    // Equality is based on the internal name.
    return *Name <=> *other.Name;
}

auto spp::asts::TypePostfixExpressionOperatorNestedTypeAst::Equals(
    const TypePostfixExpressionOperatorAst &other) const
    -> Ordering {
    // Reverse hook (double dispatch).
    return other.EqualsNestedType(*this);
}

auto spp::asts::TypePostfixExpressionOperatorNestedTypeAst::PosStart() const
    -> std::size_t {
    // Use the "::" token.
    return TokSep->PosStart();
}

auto spp::asts::TypePostfixExpressionOperatorNestedTypeAst::PosEnd() const
    -> std::size_t {
    // Use the name.
    return Name->PosEnd();
}

auto spp::asts::TypePostfixExpressionOperatorNestedTypeAst::Clone() const
    -> Unique<Ast> {
    // Clone all the members of the ast.
    return MakeUnique<TypePostfixExpressionOperatorNestedTypeAst>(
        AstClone(TokSep), AstCloneShared(Name));
}

auto spp::asts::TypePostfixExpressionOperatorNestedTypeAst::ToString() const
    -> Str {
    SPP_STRING_START;
    SPP_STRING_APPEND(TokSep);
    SPP_STRING_APPEND(Name);
    SPP_STRING_END;
}

auto spp::asts::TypePostfixExpressionOperatorNestedTypeAst::NsParts() const
    -> Vec<Shared<const IdentifierAst>> {
    return {};
}

auto spp::asts::TypePostfixExpressionOperatorNestedTypeAst::NsParts()
    -> Vec<Shared<IdentifierAst>> {
    return {};
}

auto spp::asts::TypePostfixExpressionOperatorNestedTypeAst::TypeParts() const
    -> Vec<Shared<const TypeIdentifierAst>> {
    return {Name};
}

auto spp::asts::TypePostfixExpressionOperatorNestedTypeAst::TypeParts()
    -> Vec<Shared<TypeIdentifierAst>> {
    return {Name};
}

SPP_MOD_END
