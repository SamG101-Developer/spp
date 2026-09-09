module;
#include <spp/macros.hpp>

module spp.asts.local_variable_ast;
import spp.asts.identifier_ast;
import spp.asts.let_statement_initialized_ast;

SPP_MOD_BEGIN
spp::asts::LocalVariableAst::LocalVariableAst() :
  _FromCasePattern(false) {
}

spp::asts::LocalVariableAst::~LocalVariableAst() = default;

auto spp::asts::LocalVariableAst::BindsByMove() const
  -> bool {
  // Only the patterns that name something take anything; every other kind is a test.
  return false;
}

auto spp::asts::LocalVariableAst::TakesRest() const
  -> bool {
  // Only the rest pattern can bind the rest, and only when it is given a name to bind it to.
  return false;
}

auto spp::asts::LocalVariableAst::ExtractName() const
  -> Shared<IdentifierAst> {
  // Default implementation returns nullptr.
  return nullptr;
}

auto spp::asts::LocalVariableAst::ExtractNames() const
  -> Vec<Shared<IdentifierAst>> {
  // Default implementation returns an empty list.
  return {};
}

auto spp::asts::LocalVariableAst::MarkFromCasePattern()
  -> void {
  // Mark this local variable as being created from a
  // case pattern.
  _FromCasePattern = true;
}

SPP_MOD_END
