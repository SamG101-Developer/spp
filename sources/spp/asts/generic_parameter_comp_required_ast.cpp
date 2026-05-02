module;
#include <spp/macros.hpp>

module spp.asts.generic_parameter_comp_required_ast;
import spp.asts.token_ast;
import spp.asts.type_ast;
import spp.asts.mixins.orderable_ast;
import spp.asts.utils.ast_utils;
import spp.asts.utils.orderable;

SPP_MOD_BEGIN
spp::asts::GenericParameterCompRequiredAst::GenericParameterCompRequiredAst(
    decltype(TokCmp) &&tok_cmp,
    decltype(Name) name,
    decltype(TokColon) &&tok_colon,
    decltype(Type) type) :
    GenericParameterCompAst(std::move(tok_cmp), std::move(name), std::move(tok_colon), std::move(type), utils::OrderableTag::REQUIRED_PARAM) {
}

spp::asts::GenericParameterCompRequiredAst::~GenericParameterCompRequiredAst() = default;

auto spp::asts::GenericParameterCompRequiredAst::PosStart() const
    -> std::size_t {
    // Use the "cmp" token.
    return TokCmp->PosStart();
}

auto spp::asts::GenericParameterCompRequiredAst::PosEnd() const
    -> std::size_t {
    // Use the type.
    return Type->PosEnd();
}

auto spp::asts::GenericParameterCompRequiredAst::Clone() const
    -> Unique<Ast> {
    // Clone all the members of the ast.
    return MakeUnique<GenericParameterCompRequiredAst>(
        AstClone(TokCmp), AstCloneShared(Name), AstClone(TokColon), AstCloneShared(Type));
}

auto spp::asts::GenericParameterCompRequiredAst::ToString() const
    -> Str {
    SPP_STRING_START;
    SPP_STRING_APPEND(TokCmp).append(" ");
    SPP_STRING_APPEND(Name);
    SPP_STRING_APPEND(TokColon).append(" ");
    SPP_STRING_APPEND(Type);
    SPP_STRING_END;
}

SPP_MOD_END
