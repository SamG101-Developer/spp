module;
#include <spp/macros.hpp>
#include <spp/analyse/macros.hpp>

module spp.asts.is_expression_ast;
import spp.analyse.errors.semantic_error;
import spp.analyse.errors.semantic_error_builder;
import spp.analyse.scopes.scope;
import spp.analyse.scopes.scope_manager;
import spp.analyse.scopes.symbols;
import spp.analyse.utils.case_utils;
import spp.analyse.utils.expr_utils;
import spp.analyse.utils.operator_desugaring;
import spp.asts.ast;
import spp.asts.case_expression_ast;
import spp.asts.case_pattern_variant_ast;
import spp.asts.identifier_ast;
import spp.asts.inner_scope_ast;
import spp.asts.let_statement_initialized_ast;
import spp.asts.token_ast;
import spp.asts.type_ast;
import spp.asts.generate.common_types_precompiled;
import spp.asts.meta.compiler_meta_data;
import spp.asts.utils.ast_utils;
import spp.lex.tokens;
import genex;

SPP_MOD_BEGIN
IsExpressionAst::IsExpressionAst(
  decltype(Lhs) &&lhs,
  decltype(TokOp) &&tok_op,
  decltype(Rhs) &&rhs) :
  Lhs(std::move(lhs)),
  TokOp(std::move(tok_op)),
  Rhs(std::move(rhs)),
  _MappedFn(nullptr) {
  SPP_SET_AST_TO_DEFAULT_IF_NULLPTR(this->TokOp, lex::SppTokenType::KW_IS, "is");
  Source.OriginalPosStart = Lhs ? Lhs->PosStart() : 0;
  Source.OriginalPosEnd = Rhs ? Rhs->PosEnd() : 0;
}

IsExpressionAst::~IsExpressionAst() = default;

auto IsExpressionAst::PosStart() const -> std::size_t {
  // Use the lhs, or the mapped function.
  return Lhs ? Lhs->PosStart() : Source.OriginalPosStart;
}

auto IsExpressionAst::PosEnd() const -> std::size_t {
  // Use the rhs, or the mapped function.
  return Rhs ? Rhs->PosEnd() : Source.OriginalPosEnd;
}

auto IsExpressionAst::Clone() const -> Unique<Ast> {
  // Clone all the members of the ast.
  auto ast = MakeUnique<IsExpressionAst>(
    AstClone(Lhs),
    AstClone(TokOp),
    AstClone(Rhs));
  ast->_MappedFn = _MappedFn;
  return ast;
}

auto IsExpressionAst::ToString() const -> Str {
  SPP_STRING_START;
  if (_MappedFn) {
    SPP_STRING_APPEND(_MappedFn);
    SPP_STRING_END;
  }
  SPP_STRING_APPEND(Lhs);
  SPP_STRING_APPEND(TokOp);
  SPP_STRING_APPEND(Rhs);
  SPP_STRING_END;
}

auto IsExpressionAst::Stage7_AnalyseSemantics(
  ScopeManager *sm, CompilerMetaData *meta) -> void {
  IMPORT_UTILS;

  _LhsAsId = AstClone(Lhs->To<IdentifierAst>());

  // Convert to a "case" destructure and analyse it.
  const auto n = sm->CurrentScope->Children.Len();
  _MappedFn = case_utils::ConvertIsExprToFnCall(*this, sm, meta);
  _MappedFn->Stage7_AnalyseSemantics(sm, meta);

  // Add the destructure symbols to the current scope. This
  // includes the lhs symbol if it's been flow typed. They are
  // recorded as the scope's bindings, as a "let"'s are, so
  // a later walk only finds them from this "is" onwards.
  _Exposed.Clear();
  if (AstAs<InnerScopeAst<Unique<Ast>>>(sm->CurrentScope->AstNode) == nullptr) {
    const auto destructure_syms = sm->CurrentScope->Children[n]->Children[0]->GetAllVarSymbols(true, true);
    for (auto const &x : destructure_syms) {
      _Exposed.EmplaceBack(x->SharedFromThis<VariableSymbol>());
      sm->CurrentScope->RecordLetBinding(_Exposed.Back());
      sm->CurrentScope->AddVarSymbol(_Exposed.Back());
      if (x->Kind == VariableKind::Local) { meta->AddedIsBindings.EmplaceBack(x); }
    }
  }
}

auto IsExpressionAst::_DeclareExposed(
  ScopeManager &sm) const -> void {
  // Bulk declare whatever is in the "exposed" list. Used in
  // stages 8, 9 and 11, and expose the bindings.
  for (auto const &x : _Exposed) {
    sm.CurrentScope->AddVarSymbol(x);
  }
}

auto IsExpressionAst::Stage8_CheckMemory(
  ScopeManager *sm, CompilerMetaData *meta) -> void {
  // Forward the memory checking to the mapped function, then
  // expose what it binds: the subject it tests is the one
  // named before the "is", not the one it narrows.
  _MappedFn->Stage8_CheckMemory(sm, meta);
  _DeclareExposed(*sm);
}

auto IsExpressionAst::Stage9_CompTimeResolve(
  ScopeManager *sm, CompilerMetaData *meta) -> void {
  // Forward the resolution to the mapped function, then expose
  // what it binds, as stage 8 does.
  _MappedFn->Stage9_CompTimeResolve(sm, meta);
  _DeclareExposed(*sm);
}

auto IsExpressionAst::Stage11_CodeGen(
  ScopeManager *sm, CompilerMetaData *meta, codegen::LlvmCtx *ctx) -> llvm::Value* {
  // As moving to the next scope clears ast-generated bindings
  // in that scope, we need to redeclare them for them for
  // narrowing analysis and codegen.
  _DeclareExposed(*sm);

  // If the lhs was an identifier, the "is" causes it to get
  // flow typed, so we need to promote the original "alloca"
  // into the flow typed symbol. Declared before the mapped
  // function is generated, as code generation has always read
  // it: the narrowed symbol shares the original's storage.
  if (_LhsAsId) {
    const auto narrow_sym = sm->CurrentScope->FindVarSymbol(_LhsAsId.get(), true);
    if (narrow_sym != nullptr) {
      auto wide_sym = sm->CurrentScope->Parent->FindVarSymbol(_LhsAsId.get());
      wide_sym = wide_sym ? wide_sym : narrow_sym; // Todo: Remove fallback?
      narrow_sym->LlvmInfo->Alloca = wide_sym->LlvmInfo->Alloca;
    }
  }

  // Forward the code generation to the mapped function.
  return _MappedFn->Stage11_CodeGen(sm, meta, ctx);
}

auto IsExpressionAst::InferTypeRef(
  ScopeManager *sm, CompilerMetaData *) -> TypeRef {
  // Always return a boolean type (successful or failed
  // match)
  using generate::common_types_precompiled::BOOL;
  return TypeRef::Of(*BOOL, *sm->CurrentScope);
}

auto IsExpressionAst::IsAllowedInDefault() const -> bool {
  // Along with "case" expressions (which an "is" maps too),
  // the "is" expression is banned in default statements. In
  // the future, they might get enabled, even if in a restricted
  // way. Also depends on "case" being enabled in defaults.
  return false;
}

SPP_MOD_END
