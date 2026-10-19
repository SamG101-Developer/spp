module;
#include <spp/macros.hpp>

module spp.asts.mixins.abstract_type_ast;
import spp.asts.type_identifier_ast;

SPP_MOD_BEGIN
AbstractTypeAst::AbstractTypeAst() = default;
AbstractTypeAst::~AbstractTypeAst() = default;

auto AbstractTypeAst::ClearPartsSourceWritten() -> void {
  // Move through each part, clearing them individually.
  for (const auto part : TypeParts()) {
    part->ClearSourceWritten();
  }
}
SPP_MOD_END
