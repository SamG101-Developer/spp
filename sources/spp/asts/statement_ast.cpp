module;
#include <spp/macros.hpp>

module spp.asts.statement_ast;
import spp.asts.type_ast;
import spp.asts.generate.common_types;

SPP_MOD_BEGIN
StatementAst::StatementAst() = default;

StatementAst::~StatementAst() = default;

auto StatementAst::InferType(
  ScopeManager *, CompilerMetaData *) -> Shared<TypeAst> {
  // All statements are inferred as the Void type.
  using generate::common_types::VoidType;
  return VoidType(PosStart());
}

auto StatementAst::Terminates() const -> bool {
  // By default, statements do not terminate control flow.
  return false;
}

SPP_MOD_END
