module;
#include <spp/macros.hpp>

module spp.asts.generic_parameter_comp_variadic_ast;
import spp.asts.token_ast;
import spp.asts.type_ast;
import spp.asts.mixins.orderable_ast;
import spp.asts.utils.ast_utils;
import spp.asts.utils.orderable;

SPP_MOD_BEGIN
spp::asts::GenericParameterCompVariadicAst::GenericParameterCompVariadicAst(
    decltype(TokCmp) &&tok_cmp,
    decltype(TokEllipsis) &&tok_ellipsis,
    decltype(Name) name,
    decltype(TokColon) &&tok_colon,
    decltype(Type) &&type) :
    GenericParameterCompAst(std::move(tok_cmp), std::move(name), std::move(tok_colon), std::move(type), utils::OrderableTag::kVariadicParam),
    TokEllipsis(std::move(tok_ellipsis)) {
}

spp::asts::GenericParameterCompVariadicAst::~GenericParameterCompVariadicAst() = default;

auto spp::asts::GenericParameterCompVariadicAst::PosStart() const
    -> std::size_t {
    // Use the "cmp" token.
    return TokCmp->PosStart();
}

auto spp::asts::GenericParameterCompVariadicAst::PosEnd() const
    -> std::size_t {
    // Use the type.
    return Source.OriginalType->PosEnd();
}

auto spp::asts::GenericParameterCompVariadicAst::Clone() const
    -> Unique<Ast> {
    // Clone all the members of the ast.
    return MakeUnique<GenericParameterCompVariadicAst>(
        AstClone(TokCmp), AstClone(TokEllipsis), AstClone(Name), AstClone(TokColon), AstClone(Type));
}

auto spp::asts::GenericParameterCompVariadicAst::ToString() const
    -> Str {
    SPP_STRING_START;
    SPP_STRING_APPEND(TokCmp).append(" ");
    SPP_STRING_APPEND(TokEllipsis);
    SPP_STRING_APPEND(Name);
    SPP_STRING_APPEND(TokColon).append(" ");
    SPP_STRING_APPEND(Type);
    SPP_STRING_END;
}

SPP_MOD_END
