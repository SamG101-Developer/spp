module;
#include <spp/macros.hpp>

module spp.asts.function_parameter_variadic_ast;
import spp.asts.local_variable_ast;
import spp.asts.ast;
import spp.asts.token_ast;
import spp.asts.type_ast;
import spp.asts.mixins.orderable_ast;
import spp.asts.utils.ast_utils;
import spp.asts.utils.orderable;
import spp.lex.tokens;

SPP_MOD_BEGIN
spp::asts::FunctionParameterVariadicAst::FunctionParameterVariadicAst(
    decltype(TokEllipsis) &&tok_ellipsis,
    decltype(Var) &&var,
    decltype(TokColon) &&tok_colon,
    decltype(Type) type) :
    FunctionParameterAst(std::move(var), std::move(tok_colon), std::move(type), utils::OrderableTag::kVariadicParam),
    TokEllipsis(std::move(tok_ellipsis)) {
    SPP_SET_AST_TO_DEFAULT_IF_NULLPTR(this->TokEllipsis, lex::SppTokenType::TK_DOUBLE_DOT, "..", var != nullptr ? var->PosStart() : 0);
}

spp::asts::FunctionParameterVariadicAst::~FunctionParameterVariadicAst() = default;

auto spp::asts::FunctionParameterVariadicAst::PosStart() const
    -> std::size_t {
    // Use the ".." token.
    return TokEllipsis->PosStart();
}

auto spp::asts::FunctionParameterVariadicAst::PosEnd() const
    -> std::size_t {
    // Use the type.
    return Source.OriginalType->PosEnd();
}

auto spp::asts::FunctionParameterVariadicAst::Clone() const
    -> Unique<Ast> {
    // Clone all the members of the ast.
    return MakeUnique<FunctionParameterVariadicAst>(
        AstClone(TokEllipsis), AstClone(Var), AstClone(TokColon), AstClone(Type));
}

auto spp::asts::FunctionParameterVariadicAst::ToString() const
    -> Str {
    SPP_STRING_START;
    SPP_STRING_APPEND(TokEllipsis);
    SPP_STRING_APPEND(Var);
    SPP_STRING_APPEND(TokColon).append(" ");
    SPP_STRING_APPEND(Type);
    SPP_STRING_END;
}

SPP_MOD_END
