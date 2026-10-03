module;
#include <spp/macros.hpp>
#include <spp/analyse/macros.hpp>

module spp.asts.cmp_statement_ast;
import spp.analyse.errors.semantic_error;
import spp.analyse.errors.semantic_error_builder;
import spp.analyse.scopes.instance_key;
import spp.analyse.scopes.scope;
import spp.analyse.scopes.scope_block_name;
import spp.analyse.scopes.scope_manager;
import spp.analyse.scopes.symbols;
import spp.analyse.utils.mem_utils;
import spp.analyse.utils.type_compare;
import spp.analyse.utils.type_predicates;
import spp.analyse.utils.type_resolution;
import spp.asts.annotation_ast;
import spp.asts.convention_ast;
import spp.asts.generic_argument_ast;
import spp.asts.identifier_ast;
import spp.asts.local_variable_single_identifier_ast;
import spp.asts.token_ast;
import spp.asts.type_ast;
import spp.asts.type_identifier_ast;
import spp.asts.meta.compiler_meta_data;
import spp.asts.utils.ast_utils;
import spp.codegen.llvm_mangle;
import spp.codegen.llvm_type;
import spp.lex.tokens;
import spp.lsp.resolution_index;
import genex;
import llvm;

SPP_MOD_BEGIN
CmpStatementAst::CmpStatementAst(
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
  Value(std::move(value)),
  _FromUseStatement(false),
  _AliasSymbol(nullptr) {
  SPP_SET_AST_TO_DEFAULT_IF_NULLPTR(this->TokCmp, lex::SppTokenType::KW_CMP, "cmp");
  SPP_SET_AST_TO_DEFAULT_IF_NULLPTR(this->TokColon, lex::SppTokenType::TK_COLON, ":");
  SPP_SET_AST_TO_DEFAULT_IF_NULLPTR(this->TokAssign, lex::SppTokenType::TK_ASSIGN, "=");
}

CmpStatementAst::~CmpStatementAst() = default;

auto CmpStatementAst::PosStart() const -> std::size_t {
  // Use the name.
  return TokCmp->PosStart();
}

auto CmpStatementAst::PosEnd() const -> std::size_t {
  // Use the value.
  return Value->PosEnd();
}

auto CmpStatementAst::Clone() const -> Unique<Ast> {
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
  ast->_FromUseStatement = _FromUseStatement;
  for (auto const &a : ast->Annotations) { a->SetAstCtx(ast.get()); }
  return ast;
}

auto CmpStatementAst::ToString() const -> Str {
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

auto CmpStatementAst::Stage1_PreProcess(
  Ast *ctx) -> void {
  // No pre-processing needed for cmp statements.
  Ast::Stage1_PreProcess(ctx);
  for (auto const &a : Annotations) { a->Stage1_PreProcess(this); }
}

auto CmpStatementAst::Stage2_GenTopLvlScopes(
  ScopeManager *sm, CompilerMetaData *meta) -> void {
  //
  for (auto const &a : Annotations) { a->Stage2_GenTopLvlScopes(sm, meta); }

  // Create a symbol for this constant declaration, pin to
  // prevent moving. Add the symbol to the current scope,
  // not the new one for the cmp statement; needs to be
  // accessible from the module/sup block.
  // A "use" import is a placeholder until stage 3 finds what it
  // names, and then takes that symbol's kind.
  const auto kind = _FromUseStatement
    ? VariableKind::Import
    : Type != nullptr and Type->IsCompilerGeneratedType()
    ? VariableKind::FnMock
    : VariableKind::Constant;
  _AliasSymbol = MakeShared<VariableSymbol>(
    Name, Type, sm->CurrentScope, kind, false, Visibility.first);
  _AliasSymbol->MemInfo->InitializedBy(*this, sm->CurrentScope);
  _AliasSymbol->CompTimeValue = AstClone(Value);
  sm->CurrentScope->AddVarSymbolCheckConflict(_AliasSymbol);

  // Create a scope for the value. This provides a space for
  // the rhs expression to be placed into; it could be a "case"
  // expression for example. Make a uniform 1-scope for the
  // statement, like type statements get.
  auto scope_name = ScopeBlockName::FromParts(
    "cmp-stmt", {Name.get()}, PosStart());
  sm->CreateAndMoveIntoNewScope(std::move(scope_name), nullptr);
  Ast::Stage2_GenTopLvlScopes(sm, meta);
  sm->MoveOutOfCurrentScope();
}

auto CmpStatementAst::Stage3_GenTopLvlAliases(
  ScopeManager *sm, CompilerMetaData *meta) -> void {
  // Nothing to alias, but the value's scope is still stepped
  // over, so that the walk stays in step with the tree.
  sm->MoveToNextScope();
  SPP_ASSERT(sm->CurrentScope == _Scope);
  for (auto const &a : Annotations) { a->Stage3_GenTopLvlAliases(sm, meta); }
  sm->MoveOutOfCurrentScope();
}

auto CmpStatementAst::Stage4_ResolveDeclarations(
  ScopeManager *sm, CompilerMetaData *meta) -> void {
  IMPORT_UTILS;

  //
  for (auto const &a : Annotations) { a->Stage4_ResolveDeclarations(sm, meta); }
  sm->MoveToNextScope();
  SPP_ASSERT(sm->CurrentScope == _Scope);

  // Resolve the type: a "use" import's is its target's qualified
  // name already.
  if (_FromUseStatement) {
    Type->Stage7_AnalyseSemantics(sm, meta);
  }
  else {
    // Todo: a class-typed "cmp" in a generic sup gets a global
    //  of the unbound "Unit[T=T]", which LLVM rejects as unsized
    //  SupCmpStatementGeneric.test_valid_class_typed_cmp_in_a_generic_sup.
    Type = type_resolution::AnalyseWrittenType(*Type, *sm, *meta);
    _AliasSymbol->Type = Type;
  }
  sm->MoveOutOfCurrentScope();
}

auto CmpStatementAst::Stage5_LoadSupScopes(
  ScopeManager *sm, CompilerMetaData *meta) -> void {
  for (auto const &a : Annotations) { a->Stage5_LoadSupScopes(sm, meta); }
  sm->MoveToNextScope();
  SPP_ASSERT(sm->CurrentScope == _Scope);

  if (_AliasSymbol != nullptr and not Type->IsCompilerGeneratedType()) {
    _AliasSymbol->Visibility = Visibility.first;
    _AliasSymbol->VisibilityAnnotation = Visibility.second;
  }
  sm->MoveOutOfCurrentScope();
}

auto CmpStatementAst::Stage6_PreAnalyseSemantics(
  ScopeManager *sm, CompilerMetaData *) -> void {
  // Nothing to pre-analyse, but the value's scope is still
  // stepped over.
  sm->MoveToNextScope();
  SPP_ASSERT(sm->CurrentScope == _Scope);
  sm->MoveOutOfCurrentScope();
}

auto CmpStatementAst::Stage7_AnalyseSemantics(
  ScopeManager *sm, CompilerMetaData *meta) -> void {
  IMPORT_UTILS;

  //
  for (auto const &a : Annotations) { a->Stage7_AnalyseSemantics(sm, meta); }
  sm->MoveToNextScope();
  SPP_ASSERT(sm->CurrentScope == _Scope);

  // Analyse the type and value.
  {
    const auto _meta_guard = MetaGuard(meta);
    // Todo: Add this to unit tests.
    meta->ReturnTypeOverloadResolverType = MakeShared<TypeRef>(
      TypeRef::Of(*Type, *sm->CurrentScope));
    Value->Stage7_AnalyseSemantics(sm, meta);
  }

  // Check the value's type is the same as the given type;
  // it is only spelled out for the error.
  if (not IsFromUseStatement() and not type_compare::Assignable(
    TypeRef::Of(*Type, *sm->CurrentScope), Value->InferTypeRef(sm, meta),
    *sm->CurrentScope, *sm->CurrentScope)) {
    const auto inferred_type = Value->InferType(sm, meta);
    Raise<SppTypeMismatchError>({sm->CurrentScope}, ERR_ARGS(*Type, *Type, *Value, *inferred_type));
  }
  sm->MoveOutOfCurrentScope();
}

auto CmpStatementAst::Stage8_CheckMemory(
  ScopeManager *sm, CompilerMetaData *meta) -> void {
  // Check the memory of the type.
  IMPORT_UTILS;
  sm->MoveToNextScope();
  SPP_ASSERT(sm->CurrentScope == _Scope);
  Value->Stage8_CheckMemory(sm, meta);
  mem_utils::ValidateSymbolMemory(*Value, *Value, *sm, meta);

  //
  if (not _FromUseStatement) {
    const auto var_sym = sm->CurrentScope->FindVarSymbol(Name.get());
    var_sym->CompTimeValue = AstClone(Value);
  }
  sm->MoveOutOfCurrentScope();
}

auto CmpStatementAst::Stage9_CompTimeResolve(
  ScopeManager *sm, CompilerMetaData *meta) -> void {
  //
  IMPORT_UTILS;
  for (auto const &a : Annotations) { a->Stage9_CompTimeResolve(sm, meta); }
  sm->MoveToNextScope();
  SPP_ASSERT(sm->CurrentScope == _Scope);

  // Generate the value and assign it to the variable symbol's compile-time value. One naming a parameter of a generic
  // "sup" block it is declared in ("cmp n: USize = k + 1_uz") has no value until an instantiation binds it: it is kept
  // as written, and read where it is used ("comp_generics::FindCompMemberId", the folded static access).
  if (not Type->IsCompilerGeneratedType()) {
    const auto var_sym = sm->CurrentScope->FindVarSymbol(Name.get());
    if (not analyse::scopes::IsClosedCompId(sm->CurrentScope->CompIdOf(*Value))) {
      var_sym->CompTimeValue = AstClone(Value);
      sm->ExhaustScope();
      sm->MoveOutOfCurrentScope();
      return;
    }

    // Because the comp-time resolution takes the first branch
    // that matches, it leaves the resulting "case" branches'
    // scopes as unwalked, meaning that the tree becomes non-synced.
    // Use a new scope manager to unsync, then exhaust the scope
    // in the main manager afterwards.
    auto tm = ScopeManager(sm->GlobalScope, sm->CurrentScope);
    tm.Reset(sm->CurrentScope);
    Value->Stage9_CompTimeResolve(&tm, meta);
    Value = AstClone(meta->CompTimeResult);
    var_sym->CompTimeValue = std::move(meta->CompTimeResult);

    // Use the hook to record information for the resolution and
    // completion plugin.
    lsp::resolution_index::RecordCompTimeValue(
      *Name, *sm, *meta, Value != nullptr ? Value->ToString() : Str());
  }
  sm->ExhaustScope();
  sm->MoveOutOfCurrentScope();
}

auto CmpStatementAst::Stage10_PreCodeGen(
  ScopeManager *sm, CompilerMetaData *meta, codegen::LlvmCtx *ctx) -> llvm::Value* {
  IMPORT_UTILS;
  // A "cmp" generic parameter builds one of these on the
  // spot to get its storage allocated and calls only this
  // stage on it, so there is no scope from stage 2 to step
  // into. Ignore scope logic in that case.
  const auto owns_scope = _Scope != nullptr;
  if (owns_scope) {
    sm->MoveToNextScope();
    SPP_ASSERT(sm->CurrentScope == _Scope);
  }

  // No generation for $ types.
  const auto llvm_type = codegen::GetLlvmTypeOf(
    TypeRef::Of(*Type, *sm->CurrentScope), ctx);

  // A type with no layout ("T" in an uninstantiated "sup"
  // template) has no constant to emit, and neither has one only
  // written in terms of such a parameter ("Unit[T]"): its llvm
  // struct is an unsized placeholder, not the instance's. Nor
  // has a value naming such a parameter ("k + 1_uz", Stage 9).
  // The use site folds the value instead.
  if (llvm_type == nullptr
    or not type_predicates::IsTypeConcrete(*Type, *sm->CurrentScope)
    or (Value != nullptr and not analyse::scopes::IsClosedCompId(sm->CurrentScope->CompIdOf(*Value)))) {
    if (owns_scope) {
      sm->ExhaustScope();
      sm->MoveOutOfCurrentScope();
    }
    return nullptr;
  }

  // Generate the value in a constant context. A "cmp" generic
  // parameter reaches here through a placeholder with no "Value"
  // of its own, so what it was bound to has to be read back
  // off the symbol the instantiation's own scope registered.
  ctx->InConstantContext = true;
  const auto var_sym = sm->CurrentScope->FindVarSymbol(Name.get());
  const auto generic_val = Value == nullptr ? var_sym->BoundCompVal() : nullptr;

  // A binding that is still a name stands for another parameter
  // rather than for a value, so there is nothing to emit for it.
  // The same test that decides an instantiation is not concrete.
  const auto bound_val = generic_val != nullptr and generic_val->To<IdentifierAst>() == nullptr
    ? static_cast<Ast*>(generic_val)
    : Value != nullptr
    ? var_sym->CompTimeValue.get()
    : nullptr;

  // The true val derived for the cmp statement, for stage 11 LLVM
  // IR, is based off a few different flags. Either the cmp generic
  // needs resolving though another stage9 call, or we can just use
  // the current "bound" value.
  const auto val = [&]() -> llvm::Value* {
    if (bound_val == nullptr) { return llvm::Constant::getNullValue(llvm_type); }

    // A value that is only a name ("cmp n: T = m" in a "sup" over a
    // "cmp m") names another constant, which may be a comp generic
    // with no storage at all - so it is folded, not generated.
    if (generic_val != nullptr or bound_val->To<IdentifierAst>() != nullptr) {
      auto tm = ScopeManager(sm->GlobalScope, sm->CurrentScope);
      tm.Reset(sm->CurrentScope);
      bound_val->Stage9_CompTimeResolve(&tm, meta);
      if (const auto folded = std::move(meta->CompTimeResult); folded != nullptr and folded->To<IdentifierAst>() ==
        nullptr) {
        return folded->Stage11_CodeGen(sm, meta, ctx);
      }

      // A name that does not fold here is a parameter not yet bound
      // (the "sup" block's own template), so there is nothing to emit.
      if (bound_val->To<IdentifierAst>() != nullptr) { return llvm::Constant::getNullValue(llvm_type); }
    }
    return bound_val->Stage11_CodeGen(sm, meta, ctx);
  }();
  ctx->InConstantContext = false;

  // Create the global variable for the constant.
  const auto llvm_global_var = new llvm::GlobalVariable(
    *ctx->Module, llvm_type, true, llvm::GlobalValue::ExternalLinkage,
    llvm::cast<llvm::Constant>(val),
    codegen::mangle::MangleCmpName(*sm->CurrentScope, *this));

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

auto CmpStatementAst::Stage11_CodeGen(
  ScopeManager *sm, CompilerMetaData *, codegen::LlvmCtx *) -> llvm::Value* {
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

auto CmpStatementAst::MarkFromUseStatement() -> void {
  // Setter for marking if its from a "use"-var statement.
  _FromUseStatement = true;
}

auto CmpStatementAst::IsFromUseStatement() const -> bool {
  // Getter for the marked "use"-var flag.
  return _FromUseStatement;
}

SPP_MOD_END
