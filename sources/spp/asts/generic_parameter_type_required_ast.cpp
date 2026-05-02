module;
#include <spp/macros.hpp>

module spp.asts.generic_parameter_type_required_ast;
import spp.asts.generic_parameter_type_inline_constraints_ast;
import spp.asts.type_ast;
import spp.asts.mixins.orderable_ast;
import spp.asts.utils.ast_utils;
import spp.asts.utils.orderable;

SPP_MOD_BEGIN
spp::asts::GenericParameterTypeRequiredAst::GenericParameterTypeRequiredAst(
    decltype(Name) name,
    decltype(Constraints) &&constraints) :
    GenericParameterTypeAst(std::move(name), std::move(constraints), utils::OrderableTag::REQUIRED_PARAM) {
}

spp::asts::GenericParameterTypeRequiredAst::~GenericParameterTypeRequiredAst() = default;

auto spp::asts::GenericParameterTypeRequiredAst::PosStart() const
    -> std::size_t {
    // Use the "name".
    return Name->PosStart();
}

auto spp::asts::GenericParameterTypeRequiredAst::PosEnd() const
    -> std::size_t {
    // Use the "name".
    return Name->PosEnd();
}

auto spp::asts::GenericParameterTypeRequiredAst::Clone() const
    -> Unique<Ast> {
    // Clone all the members of the ast.
    return MakeUnique<GenericParameterTypeRequiredAst>(
        AstCloneShared(Name), AstClone(Constraints));
}

auto spp::asts::GenericParameterTypeRequiredAst::ToString() const
    -> Str {
    SPP_STRING_START;
    SPP_STRING_APPEND(Name);
    SPP_STRING_APPEND(Constraints);
    SPP_STRING_END;
}

SPP_MOD_END
