module;
#include <spp/macros.hpp>
#include <spp/analyse/macros.hpp>

module spp.asts.generic_parameter_type_inline_constraints_ast;
import spp.analyse.errors.semantic_error;
import spp.analyse.errors.semantic_error_builder;
import spp.analyse.scopes.scope;
import spp.analyse.scopes.scope_manager;
import spp.analyse.scopes.symbols;
import spp.analyse.utils.type_utils;
import spp.asts.generic_argument_group_ast;
import spp.asts.token_ast;
import spp.asts.type_ast;
import spp.asts.type_identifier_ast;
import spp.asts.meta.compiler_meta_data;
import spp.asts.utils.ast_utils;
import genex;

// Todo: second-class borrow violation unit test.

SPP_MOD_BEGIN
auto GenericParameterTypeInlineConstraintsAst::NewEmpty() -> Unique<GenericParameterTypeInlineConstraintsAst> {
  // New empty constraint set.
  return MakeUnique<GenericParameterTypeInlineConstraintsAst>(
    nullptr, Vec<Unique<TypeAst>>{});
}

GenericParameterTypeInlineConstraintsAst::GenericParameterTypeInlineConstraintsAst(
  decltype(TokColon) &&tok_colon,
  Vec<Unique<TypeAst>> &&constraints) :
  TokColon(std::move(tok_colon)) {
  for (auto &&constraint : std::move(constraints)) {
    // Todo: tidy this up (simple assignment preferably).
    this->Constraints.EmplaceBack(Shared(std::move(constraint)));
  }
}

GenericParameterTypeInlineConstraintsAst::~GenericParameterTypeInlineConstraintsAst() = default;

auto GenericParameterTypeInlineConstraintsAst::PosStart() const -> std::size_t {
  // Use the ":" token.
  return TokColon->PosStart();
}

auto GenericParameterTypeInlineConstraintsAst::PosEnd() const -> std::size_t {
  // Use the last constraint.
  return Constraints.IsEmpty() ? TokColon->PosEnd() : Constraints.Back()->PosEnd();
}

auto GenericParameterTypeInlineConstraintsAst::Clone() const -> Unique<Ast> {
  // Clone all the members of the ast.
  return MakeUnique<GenericParameterTypeInlineConstraintsAst>(
    AstClone(TokColon),
    AstCloneVec(Constraints));
}

auto GenericParameterTypeInlineConstraintsAst::ToString() const -> Str {
  SPP_STRING_START;
  SPP_STRING_APPEND(TokColon);
  SPP_STRING_EXTEND(Constraints, " & ");
  SPP_STRING_END;
}

auto GenericParameterTypeInlineConstraintsAst::Stage4_ResolveDeclarations(
  ScopeManager *sm, CompilerMetaData *meta) -> void {
  using analyse::errors::SppSecondClassBorrowViolationError;
  using analyse::utils::type_utils::StampWrittenParts;

  // Analyse each constraint type.
  for (auto const &constraint : Constraints) {
    RaiseIf<SppSecondClassBorrowViolationError>(
      constraint->GetConvention() != nullptr,
      {sm->CurrentScope}, ERR_ARGS(*this, *this, "generic type constraint"));

    // A constraint names a type without ever producing a value
    // of it, so an abstract type is allowed here. This is the
    // whole point of an abstract type: "[T: Add]" accepts every
    // addable type, but never "Add" itself.
    {
      const auto _meta_guard = MetaGuard(meta);
      meta->AllowAbstractType = true;
      constraint->Stage7_AnalyseSemantics(sm, meta);
    }

    // Stamp it with what it means here, as it is read from wherever
    // the parameter is bound. A constraint imported by a "use" is
    // an alias here, which "type_utils::StampWrittenParts" follows.
    StampWrittenParts(*constraint, *sm->CurrentScope);
  }
}

SPP_MOD_END
