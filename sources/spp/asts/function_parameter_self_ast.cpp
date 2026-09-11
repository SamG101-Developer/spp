module;
#include <spp/macros.hpp>

module spp.asts.function_parameter_self_ast;
import spp.analyse.scopes.scope;
import spp.analyse.scopes.scope_manager;
import spp.analyse.scopes.symbols;
import spp.analyse.utils.mem_info_utils;
import spp.asts.convention_ast;
import spp.asts.identifier_ast;
import spp.asts.local_variable_ast;
import spp.asts.local_variable_single_identifier_ast;
import spp.asts.token_ast;
import spp.asts.type_ast;
import spp.asts.generate.common_types;
import spp.asts.mixins.orderable_ast;
import spp.asts.utils.ast_utils;
import spp.asts.utils.orderable;

SPP_MOD_BEGIN
spp::asts::FunctionParameterSelfAst::FunctionParameterSelfAst(
  decltype(Conv) &&conv,
  decltype(Var) &&var) :
  FunctionParameterAst(std::move(var), nullptr, nullptr, utils::OrderableTag::kSelfParam),
  Conv(std::move(conv)) {
  // Set the type to "Self" -> will resolve later in scope.
  using generate::common_types::SelfType;
  Type = SelfType(PosStart());
}

spp::asts::FunctionParameterSelfAst::~FunctionParameterSelfAst() = default;

auto spp::asts::FunctionParameterSelfAst::PosStart() const
  -> std::size_t {
  // Use the convention or the variable.
  return Conv != nullptr ? Conv->PosStart() : Var->PosStart();
}

auto spp::asts::FunctionParameterSelfAst::PosEnd() const
  -> std::size_t {
  // Use the token after convention (self keyword); it is mapped into local-var which calculates size differently.
  return PosStart() + 2;
}

auto spp::asts::FunctionParameterSelfAst::Clone() const
  -> Unique<Ast> {
  // Clone all the members of the ast.
  auto p = MakeUnique<FunctionParameterSelfAst>(
    AstClone(Conv),
    AstClone(Var));
  p->Type = AstCloneShared(Type);
  return p;
}

auto spp::asts::FunctionParameterSelfAst::ToString() const
  -> Str {
  SPP_STRING_START;
  SPP_STRING_APPEND(Conv);
  SPP_STRING_APPEND(Var);
  SPP_STRING_END;
}

auto spp::asts::FunctionParameterSelfAst::Stage7_AnalyseSemantics(
  analyse::scopes::ScopeManager *sm,
  meta::CompilerMetaData *meta)
  -> void {
  // Perform default analysis steps.
  FunctionParameterAst::Stage7_AnalyseSemantics(sm, meta);

  // Special mutability rules for the "self" parameter.
  const auto sym = sm->CurrentScope->GetVarSymbol(Var->ExtractName().get());
  sym->IsMutable = Var->To<LocalVariableSingleIdentifierAst>()->TokMut != nullptr
    or (Conv and *Conv == ConventionTag::MUT);

  // Apply the convention from the attribute.
  sym->Type = Type->WithConvention(AstClone(Conv));

  // And record the borrow, which the base class could not: a "self" parameter keeps its convention in "Conv" rather
  // than on "Type", so the base sees a bare type, reads no convention off it, and marks the symbol as owning its
  // value. That left "&self" and "&mut self" unregistered as borrows for the whole memory model - nothing stopped a
  // value being moved out of one.
  if (Conv != nullptr) {
    sym->MemInfo->AstBorrowed = {Conv.get(), sm->CurrentScope};
  }
}

SPP_MOD_END
