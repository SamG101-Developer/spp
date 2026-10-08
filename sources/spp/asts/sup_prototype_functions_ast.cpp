module;
#include <spp/macros.hpp>
#include <spp/analyse/macros.hpp>

module spp.asts.sup_prototype_functions_ast;
import spp.analyse.errors.semantic_error;
import spp.analyse.errors.semantic_error_builder;
import spp.analyse.scopes.scope;
import spp.analyse.scopes.scope_block_name;
import spp.analyse.scopes.scope_manager;
import spp.analyse.scopes.symbols;
import spp.analyse.utils.fn_values;
import spp.analyse.utils.generic_inference;
import spp.analyse.utils.sup_blocks;
import spp.analyse.utils.type_members;
import spp.analyse.utils.type_predicates;
import spp.analyse.utils.type_resolution;
import spp.asts.annotation_ast;
import spp.asts.class_prototype_ast;
import spp.asts.cmp_statement_ast;
import spp.asts.convention_ast;
import spp.asts.generic_argument_ast;
import spp.asts.generic_argument_group_ast;
import spp.asts.generic_parameter_ast;
import spp.asts.generic_parameter_group_ast;
import spp.asts.identifier_ast;
import spp.asts.sup_implementation_ast;
import spp.asts.token_ast;
import spp.asts.type_ast;
import spp.asts.type_identifier_ast;
import spp.asts.type_statement_ast;
import spp.asts.generate.common_types_precompiled;
import spp.asts.meta.compiler_meta_data;
import spp.asts.utils.ast_utils;
import spp.lex.tokens;
import genex;

SPP_MOD_BEGIN
SupPrototypeFunctionsAst::SupPrototypeFunctionsAst(
  decltype(TokSup) &&tok_sup,
  decltype(GnParamGroup) &&generic_param_group,
  decltype(Name) name,
  decltype(Impl) &&impl) :
  TokSup(std::move(tok_sup)),
  GnParamGroup(std::move(generic_param_group)),
  Name(std::move(name)),
  Impl(std::move(impl)) {
  SPP_SET_AST_TO_DEFAULT_IF_NULLPTR(this->TokSup, lex::SppTokenType::KW_SUP, "sup");
  SPP_SET_AST_TO_DEFAULT_IF_NULLPTR(this->GnParamGroup);
  SPP_SET_AST_TO_DEFAULT_IF_NULLPTR(this->Impl);
}

SupPrototypeFunctionsAst::~SupPrototypeFunctionsAst() = default;

auto SupPrototypeFunctionsAst::PosStart() const -> std::size_t {
  // Use the "sup" token.
  return TokSup->PosStart();
}

auto SupPrototypeFunctionsAst::PosEnd() const -> std::size_t {
  // Use the name.
  return Name->PosEnd();
}

auto SupPrototypeFunctionsAst::Clone() const -> Unique<Ast> {
  // Clone all the members of the ast.
  auto ast = MakeUnique<SupPrototypeFunctionsAst>(
    AstClone(TokSup),
    AstClone(GnParamGroup),
    AstClone(Name),
    AstClone(Impl));
  ast->_Ctx = _Ctx;
  ast->_Scope = _Scope;
  return ast;
}

auto SupPrototypeFunctionsAst::ToString() const -> Str {
  SPP_STRING_START;
  SPP_STRING_APPEND(TokSup).append(" ");
  SPP_STRING_APPEND(GnParamGroup).append(GnParamGroup->Params.IsEmpty() ? "" : " ");
  SPP_STRING_APPEND(Name).append(" ");
  SPP_STRING_APPEND(Impl);
  SPP_STRING_END;
}

auto SupPrototypeFunctionsAst::Stage1_PreProcess(
  Ast *ctx) -> void {
  // Pre-process the AST by calling the base class method
  // and then processing the implementation.
  Ast::Stage1_PreProcess(ctx);
  Impl->Stage1_PreProcess(this);
}

auto SupPrototypeFunctionsAst::Stage2_GenTopLvlScopes(
  ScopeManager *sm, CompilerMetaData *meta) -> void {
  //
  IMPORT_UTILS;

  // Create a new scope for the superimposition extension.
  auto scope_name = ScopeBlockName::FromParts(
    "sup-prototype-functions", {Name.get()}, PosStart());
  sm->CreateAndMoveIntoNewScope(std::move(scope_name), this);
  Ast::Stage2_GenTopLvlScopes(sm, meta);

  // The generic parameters' symbols first: whether each is named in the type is read by identity.
  GnParamGroup->Stage2_GenTopLvlScopes(sm, meta);
  sup_blocks::CheckGnParams(*GnParamGroup, {Name.get()}, *sm);

  Impl->Stage2_GenTopLvlScopes(sm, meta);
  sm->MoveOutOfCurrentScope();
}

auto SupPrototypeFunctionsAst::Stage3_GenTopLvlAliases(
  ScopeManager *sm, CompilerMetaData *meta) -> void {
  IMPORT_UTILS;
  sm->MoveToNextScope();
  SPP_ASSERT(sm->CurrentScope == _Scope);
  sup_blocks::RegisterProvisionalSelf(*Name, *sm);
  Impl->Stage3_GenTopLvlAliases(sm, meta);
  sm->MoveOutOfCurrentScope();
}

auto SupPrototypeFunctionsAst::Stage4_ResolveDeclarations(
  ScopeManager *sm, CompilerMetaData *meta) -> void {
  // Forward to the implementation.
  sm->MoveToNextScope();
  SPP_ASSERT(sm->CurrentScope == _Scope);
  GnParamGroup->Stage4_ResolveDeclarations(sm, meta);
  Impl->Stage4_ResolveDeclarations(sm, meta);
  sm->MoveOutOfCurrentScope();
}

auto SupPrototypeFunctionsAst::Stage5_LoadSupScopes(
  ScopeManager *sm, CompilerMetaData *meta) -> void {
  //
  IMPORT_UTILS;

  // Move into the superimposition scope.
  sm->MoveToNextScope();
  SPP_ASSERT(sm->CurrentScope == _Scope);

  // The type superimposed over: analysed, qualified, the block filed against it, and "Self" made precise.
  sup_blocks::LoadTarget(*this, Name, false, *sm, meta);

  // Load the implementation and move out of the scope.
  Impl->Stage5_LoadSupScopes(sm, meta);
  sm->MoveOutOfCurrentScope();
}

auto SupPrototypeFunctionsAst::Stage6_PreAnalyseSemantics(
  ScopeManager *sm, CompilerMetaData *meta) -> void {
  //
  IMPORT_UTILS;

  // Move to the next scope.
  sm->MoveToNextScope();
  SPP_ASSERT(sm->CurrentScope == _Scope);

  const auto cls_sym = sm->CurrentScope->FindTypeSymbol(Name.get());
  for (auto const &member : Impl->Members) {
    if (const auto cmp_member = member->To<CmpStatementAst>()) {
      // Check the constant agrees in type with every declaration
      // of that name on the type and its super types.
      type_members::CheckShadowedCmpAgreesInType(*cmp_member, *cls_sym->LinkedScope, *sm->CurrentScope, *sm);
    }
  }

  Impl->Stage6_PreAnalyseSemantics(sm, meta);
  sm->MoveOutOfCurrentScope();
}

auto SupPrototypeFunctionsAst::Stage7_AnalyseSemantics(
  ScopeManager *sm, CompilerMetaData *meta) -> void {
  //
  IMPORT_UTILS;

  // Move to the next scope.
  sm->MoveToNextScope();
  SPP_ASSERT(sm->CurrentScope == _Scope);

  GnParamGroup->Stage7_AnalyseSemantics(sm, meta);

  {
    const auto _meta_guard = MetaGuard(meta);
    meta->AllowAbstractType = true;
    Name->ResetCache();
    Name->Stage7_AnalyseSemantics(sm, meta);
  }

  Impl->Stage7_AnalyseSemantics(sm, meta);
  sm->MoveOutOfCurrentScope();
}

auto SupPrototypeFunctionsAst::Stage8_CheckMemory(
  ScopeManager *sm, CompilerMetaData *meta) -> void {
  // Move to the next scope.
  sm->MoveToNextScope();
  SPP_ASSERT(sm->CurrentScope == _Scope);
  Impl->Stage8_CheckMemory(sm, meta);
  sm->MoveOutOfCurrentScope();
}

auto SupPrototypeFunctionsAst::Stage9_CompTimeResolve(
  ScopeManager *sm, CompilerMetaData *meta) -> void {
  // Move to the next scope.
  sm->MoveToNextScope();
  SPP_ASSERT(sm->CurrentScope == _Scope);
  Impl->Stage9_CompTimeResolve(sm, meta);
  sm->MoveOutOfCurrentScope();
}

auto SupPrototypeFunctionsAst::Stage10_PreCodeGen(
  ScopeManager *sm, CompilerMetaData *meta, codegen::LlvmCtx *ctx) -> llvm::Value* {
  // Move to the next scope.
  sm->MoveToNextScope();
  SPP_ASSERT(sm->CurrentScope == _Scope);
  Impl->Stage10_PreCodeGen(sm, meta, ctx);
  sm->MoveOutOfCurrentScope();
  return nullptr;
}

auto SupPrototypeFunctionsAst::Stage11_CodeGen(
  ScopeManager *sm, CompilerMetaData *meta, codegen::LlvmCtx *ctx) -> llvm::Value* {
  // Move to the next scope.
  sm->MoveToNextScope();
  Impl->Stage11_CodeGen(sm, meta, ctx);
  sm->MoveOutOfCurrentScope();
  return nullptr;
}

SPP_MOD_END
