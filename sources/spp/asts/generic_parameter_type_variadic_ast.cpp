module;
#include <spp/macros.hpp>

module spp.asts.generic_parameter_type_variadic_ast;
import spp.asts.generic_parameter_type_inline_constraints_ast;
import spp.asts.token_ast;
import spp.asts.type_ast;
import spp.asts.mixins.orderable_ast;
import spp.asts.utils.ast_utils;
import spp.asts.utils.orderable;

SPP_MOD_BEGIN
spp::asts::GenericParameterTypeVariadicAst::GenericParameterTypeVariadicAst(
    decltype(TokEllipsis) &&tok_ellipsis,
    decltype(Name) &&name,
    decltype(Constraints) &&constraints) :
    GenericParameterTypeAst(std::move(name), std::move(constraints), utils::OrderableTag::kVariadicParam),
    TokEllipsis(std::move(tok_ellipsis)) {
}

spp::asts::GenericParameterTypeVariadicAst::~GenericParameterTypeVariadicAst() = default;

auto spp::asts::GenericParameterTypeVariadicAst::PosStart() const
    -> std::size_t {
    // Use the ".." token.
    return TokEllipsis->PosStart();
}

auto spp::asts::GenericParameterTypeVariadicAst::PosEnd() const
    -> std::size_t {
    // Use the ".." token.
    return TokEllipsis->PosEnd();
}

auto spp::asts::GenericParameterTypeVariadicAst::Clone() const
    -> Unique<Ast> {
    // Clone all the members of the ast.
    return MakeUnique<GenericParameterTypeVariadicAst>(
        AstClone(TokEllipsis), AstCloneShared(Name), AstClone(Constraints));
}

auto spp::asts::GenericParameterTypeVariadicAst::ToString() const
    -> Str {
    SPP_STRING_START;
    SPP_STRING_APPEND(TokEllipsis);
    SPP_STRING_APPEND(Name);
    SPP_STRING_APPEND(Constraints);
    SPP_STRING_END;
}

SPP_MOD_END
