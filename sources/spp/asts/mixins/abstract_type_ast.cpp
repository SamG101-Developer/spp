module;
#include <spp/macros.hpp>

module spp.asts.mixins.abstract_type_ast;
import spp.asts.type_identifier_ast;

MSVC_DEVCOM_11179619_IMPL_UNIT_USING_DECL
use_alias(spp::analyse::scopes, ExprSubst);

SPP_MOD_BEGIN
spp::asts::mixins::AbstractTypeAst::AbstractTypeAst() = default;
spp::asts::mixins::AbstractTypeAst::~AbstractTypeAst() = default;

auto spp::asts::mixins::AbstractTypeAst::ClearPartsSourceWritten() -> void {
  // Move through each part, clearing them individually.
  for (const auto part : TypeParts()) {
    part->ClearSourceWritten();
  }
}
SPP_MOD_END
