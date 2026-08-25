module;
#include <spp/macros.hpp>
#include <spp/analyse/macros.hpp>

module spp.asts.cmp_statement_ast;
import spp.analyse.errors.semantic_error;
import spp.analyse.errors.semantic_error_builder;
import spp.analyse.scopes.scope;
import spp.analyse.scopes.scope_block_name;
import spp.analyse.scopes.scope_manager;
import spp.analyse.scopes.symbols;
import spp.analyse.utils.mem_utils;
import spp.analyse.utils.type_utils;
import spp.asts.annotation_ast;
import spp.asts.convention_ast;
import spp.asts.generic_argument_comp_keyword_ast;
import spp.asts.identifier_ast;
import spp.asts.local_variable_single_identifier_ast;
import spp.asts.token_ast;
import spp.asts.type_ast;
import spp.asts.type_identifier_ast;
import spp.asts.utils.ast_utils;
import spp.codegen.llvm_mangle;
import spp.codegen.llvm_type;
import spp.lex.tokens;
import genex;
import llvm;

SPP_MOD_BEGIN
spp::asts::CmpStatementAst::CmpStatementAst(
  decltype(Annotations) &&annotations,
  decltype(TokCmp) &&tok_cmp,
  decltype(Name) name,
  decltype(TokColon) &&tok_colon,
  decltype(Type) type,
  decltype(TokAssign) &&tok_assign,
  decltype(Value) &&value) :
  Annotations(std::move(annotations)),
  TokCmp(std::move(tok_cmp)),
  Name(std::move(name)),
  TokColon(std::move(tok_colon)),
  Type(std::move(type)),
  TokAssign(std::move(tok_assign)),
  Value(std::move(value)) {
  SPP_SET_AST_TO_DEFAULT_IF_NULLPTR(this->TokCmp, lex::SppTokenType::KW_CMP, "cmp");
  SPP_SET_AST_TO_DEFAULT_IF_NULLPTR(this->TokColon, lex::SppTokenType::TK_COLON, ":");
  SPP_SET_AST_TO_DEFAULT_IF_NULLPTR(this->TokAssign, lex::SppTokenType::TK_ASSIGN, "=");
  Source.OriginalType = AstClone(Type);
}

spp::asts::CmpStatementAst::~CmpStatementAst() = default;

auto spp::asts::CmpStatementAst::PosStart() const
  -> std::size_t {
  // Use the name.
  return TokCmp->PosStart();
}

auto spp::asts::CmpStatementAst::PosEnd() const
  -> std::size_t {
  // Use the value.
  return Value->PosEnd();
}

auto spp::asts::CmpStatementAst::Clone() const
  -> Unique<Ast> {
  // Clone all the members of the ast.
  auto ast = MakeUnique<CmpStatementAst>(
    AstCloneVec(Annotations),
    AstClone(TokCmp),
    AstClone(Name),
    AstClone(TokColon),
    AstClone(Type),
    AstClone(TokAssign),
    AstClone(Value));
  ast->Visibility = Visibility;
  ast->_Ctx = _Ctx;
  ast->_Scope = _Scope;
  for (auto const &a : ast->Annotations) { a->SetAstCtx(ast.get()); }
  return ast;
}

auto spp::asts::CmpStatementAst::ToString() const
  -> Str {
  SPP_STRING_START;
  SPP_STRING_EXTEND(Annotations, "\n");
  SPP_STRING_APPEND_RAW(not Annotations.IsEmpty() ? "\n" : "");
  SPP_STRING_APPEND(TokCmp).append(" ");
  SPP_STRING_APPEND(Name);
  SPP_STRING_APPEND(TokColon).append(" ");
  SPP_STRING_APPEND(Type).append(" ");
  SPP_STRING_APPEND(TokAssign).append(" ");
  SPP_STRING_APPEND(Value);
  SPP_STRING_END;
}

auto spp::asts::CmpStatementAst::Stage1_PreProcess(
  Ast *ctx)
  -> void {
  // No pre-processing needed for cmp statements.
  Ast::Stage1_PreProcess(ctx);
  for (auto const &a : Annotations) { a->Stage1_PreProcess(this); }
}

auto spp::asts::CmpStatementAst::Stage2_GenTopLvlScopes(
  ScopeManager *sm,
  CompilerMetaData *meta)
  -> void {
  //
  for (auto const &a : Annotations) { a->Stage2_GenTopLvlScopes(sm, meta); }

  // Create a symbol for this constant declaration, pin to
  // prevent moving. Add the symbol to the current scope,
  // not the new one for the cmp statement; needs to be
  // accessible from the module/sup block.
  _AliasSym = MakeShared<analyse::scopes::VariableSymbol>(
    Name, Type, sm->CurrentScope, false, false, Visibility.first);
  _AliasSym->MemInfo->AstCompTime = AstClone(this);
  _AliasSym->MemInfo->InitializedBy(*this, sm->CurrentScope);
  _AliasSym->CompTimeValue = AstClone(Value);
  sm->CurrentScope->AddVarSymbolCheckConflict(_AliasSym);

  // Create a scope for the value. This provides a space for
  // the rhs expression to be placed into; it could be a "case"
  // expression for example. Make a uniform 1-scope for the
  // statement, like type statements get.
  auto scope_name = analyse::scopes::ScopeBlockName::FromParts(
    "cmp-stmt", {Name.get()}, PosStart());
  sm->CreateAndMoveIntoNewScope(std::move(scope_name), nullptr);
  Ast::Stage2_GenTopLvlScopes(sm, meta);
  sm->MoveOutOfCurrentScope();
}

auto spp::asts::CmpStatementAst::Stage3_GenTopLvlAliases(
  ScopeManager *sm,
  CompilerMetaData *meta)
  -> void {
  // Nothing to alias, but the value's scope is still stepped
  // over, so that the walk stays in step with the tree.
  sm->MoveToNextScope();
  SPP_ASSERT(sm->CurrentScope == _Scope);
  for (auto const &a : Annotations) { a->Stage3_GenTopLvlAliases(sm, meta); }
  sm->MoveOutOfCurrentScope();
}

auto spp::asts::CmpStatementAst::Stage4_QualifyTypes(
  ScopeManager *sm,
  CompilerMetaData *meta)
  -> void {
  //
  using analyse::utils::type_utils::IsTypeBorrowed;
  for (auto const &a : Annotations) { a->Stage4_QualifyTypes(sm, meta); }
  sm->MoveToNextScope();
  SPP_ASSERT(sm->CurrentScope == _Scope);

  // Qualify the type.
  Type->Stage4_QualifyTypes(sm, meta);
  Type->Stage7_AnalyseSemantics(sm, meta);

  if (not _FromUseStatement and not Type->IsSelfType()) {
    Type = sm->CurrentScope->GetTypeSymbol(Type.get())->FqName()->WithConvention(AstClone(Type->GetConvention()));
    _AliasSym->Type = Type;
  }
  sm->MoveOutOfCurrentScope();
}

auto spp::asts::CmpStatementAst::Stage5_LoadSupScopes(
  ScopeManager *sm,
  CompilerMetaData *meta)
  -> void {
  for (auto const &a : Annotations) { a->Stage5_LoadSupScopes(sm, meta); }
  sm->MoveToNextScope();
  SPP_ASSERT(sm->CurrentScope == _Scope);

  // Check the type exists before attaching super scopes
  // type->Stage7_AnalyseSemantics(sm, meta);

  if (_AliasSym != nullptr and not Type->IsCompilerGeneratedType()) {
    _AliasSym->Visibility = Visibility.first;
    _AliasSym->VisibilityAnnotation = Visibility.second;
  }
  sm->MoveOutOfCurrentScope();
}

auto spp::asts::CmpStatementAst::Stage6_PreAnalyseSemantics(
  ScopeManager *sm,
  CompilerMetaData *)
  -> void {
  // Nothing to pre-analyse, but the value's scope is still
  // stepped over.
  sm->MoveToNextScope();
  SPP_ASSERT(sm->CurrentScope == _Scope);
  sm->MoveOutOfCurrentScope();
}

auto spp::asts::CmpStatementAst::Stage7_AnalyseSemantics(
  ScopeManager *sm,
  CompilerMetaData *meta)
  -> void {
  //
  using analyse::errors::SppTypeMismatchError;
  using analyse::utils::type_utils::TypeEq;
  for (auto const &a : Annotations) { a->Stage7_AnalyseSemantics(sm, meta); }
  sm->MoveToNextScope();
  SPP_ASSERT(sm->CurrentScope == _Scope);

  // Analyse the type and value.
  meta->Save();
  meta->ReturnTypeOverloadResolverType = Type; // Todo: Add this to unit tests.
  Value->Stage7_AnalyseSemantics(sm, meta);
  meta->Restore();

  // Check the value's type is the same as the given type.
  const auto inferred_type = Value->InferType(sm, meta);

  RaiseIf<SppTypeMismatchError>(
    not IsFromUseStatement()
    and not TypeEq(*Type, *inferred_type, *sm->CurrentScope, *sm->CurrentScope),
    {sm->CurrentScope}, ERR_ARGS(*Source.OriginalType, *Type, *Value, *inferred_type));
  sm->MoveOutOfCurrentScope();
}

auto spp::asts::CmpStatementAst::Stage8_CheckMemory(
  ScopeManager *sm,
  CompilerMetaData *meta)
  -> void {
  // Check the memory of the type.
  using analyse::utils::mem_utils::ValidateSymbolMemory;
  sm->MoveToNextScope();
  SPP_ASSERT(sm->CurrentScope == _Scope);
  Value->Stage8_CheckMemory(sm, meta);
  ValidateSymbolMemory(*Value, *Value, *sm, true, true, true, true, meta);

  //
  if (not _FromUseStatement) {
    const auto var_sym = sm->CurrentScope->GetVarSymbol(Name.get());
    var_sym->CompTimeValue = AstClone(Value);
  }
  sm->MoveOutOfCurrentScope();
}

auto spp::asts::CmpStatementAst::Stage9_CompTimeResolve(
  ScopeManager *sm,
  CompilerMetaData *meta)
  -> void {
  //
  for (auto const &a : Annotations) { a->Stage9_CompTimeResolve(sm, meta); }
  sm->MoveToNextScope();
  SPP_ASSERT(sm->CurrentScope == _Scope);

  // Generate the value and assign it to the variable symbol's compile-time value.
  if (not Type->IsCompilerGeneratedType()) {
    const auto var_sym = sm->CurrentScope->GetVarSymbol(Name.get());

    // Because the comp-time resolution takes the first branch
    // that matches, it leaves the resulting "case" branches'
    // scopes as unwalked, meaning that the tree becomes non-synced.
    // Use a new scope manager to unsync, then exhaust the scope
    // in the main manager afterwards.
    auto tm = analyse::scopes::ScopeManager(sm->GlobalScope, sm->CurrentScope);
    tm.Reset(sm->CurrentScope);
    Value->Stage9_CompTimeResolve(&tm, meta);
    Value = AstClone(meta->CmpResult);
    var_sym->CompTimeValue = std::move(meta->CmpResult);
  }
  sm->ExhaustScope();
  sm->MoveOutOfCurrentScope();
}

auto spp::asts::CmpStatementAst::Stage10_PreCodeGen(
  ScopeManager *sm,
  CompilerMetaData *meta,
  codegen::LlvmCtx *ctx)
  -> llvm::Value* {
  // A "cmp" generic parameter builds one of these on the
  // spot to get its storage allocated and calls only this
  // stage on it, so there is no scope from stage 2 to
  // step into. Ignore scope logic in that case.
  const auto owns_scope = _Scope != nullptr;
  if (owns_scope) {
    sm->MoveToNextScope();
    SPP_ASSERT(sm->CurrentScope == _Scope);
  }

  // No generation for $ types.
  const auto llvm_type = codegen::GetLlvmTypeOf(
    *Type, *sm->CurrentScope, ctx);

  // Generate the value in a constant context. A "cmp" generic
  // parameter reaches here through a placeholder with no "Value"
  // of its own, so what it was bound to has to be read back
  // off the symbol the instantiation's own scope registered.
  ctx->InConstantContext = true;
  const auto var_sym = sm->CurrentScope->GetVarSymbol(Name.get());
  const auto generic_arg = Value == nullptr and var_sym->MemInfo->AstCompTime != nullptr
    ? var_sym->MemInfo->AstCompTime->To<GenericArgumentCompKeywordAst>()
    : nullptr;

  // A binding that is still a name stands for another parameter
  // rather than for a value, so there is nothing to emit for it.
  // The same test that decides an instantiation is not concrete.
  const auto bound_val = generic_arg != nullptr and generic_arg->Val->To<IdentifierAst>() == nullptr
    ? static_cast<Ast*>(generic_arg->Val.get())
    : Value != nullptr
    ? var_sym->CompTimeValue.get()
    : nullptr;

  const auto val = bound_val != nullptr
    ? bound_val->Stage11_CodeGen(sm, meta, ctx)
    : llvm::Constant::getNullValue(llvm_type);
  ctx->InConstantContext = false;

  // Create the global variable for the constant.
  const auto llvm_global_var = new llvm::GlobalVariable(
    *ctx->Module, llvm_type, true, llvm::GlobalValue::ExternalLinkage,
    llvm::cast<llvm::Constant>(val),
    codegen::mangle::mangle_cmp_name(*sm->CurrentScope, *this));

  // Register in the llvm info. Nothing here descends into
  // the value - the constant is emitted from what comp-time
  // resolution already folded it to - so the scopes the
  // value owns are stepped over rather than walked.
  var_sym->LlvmInfo->Alloca = llvm_global_var;
  if (owns_scope) {
    sm->ExhaustScope();
    sm->MoveOutOfCurrentScope();
  }
  return nullptr;
}

auto spp::asts::CmpStatementAst::Stage11_CodeGen(
  ScopeManager *sm,
  CompilerMetaData *,
  codegen::LlvmCtx *)
  -> llvm::Value* {
  // Everything a "cmp" statement emits was already emitted by
  // stage 10, but the scope stage 2 gave it (and the scopes its
  // value owns) still have to be stepped over here, or every
  // sibling after it walks into the wrong scope.
  if (_Scope != nullptr) {
    sm->MoveToNextScope();
    SPP_ASSERT(sm->CurrentScope == _Scope);
    sm->ExhaustScope();
    sm->MoveOutOfCurrentScope();
  }
  return nullptr;
}

auto spp::asts::CmpStatementAst::MarkFromUseStatement()
  -> void {
  _FromUseStatement = true;
}

auto spp::asts::CmpStatementAst::IsFromUseStatement() const
  -> bool {
  return _FromUseStatement;
}

SPP_MOD_END
