module;
#include <spp/macros.hpp>
#include <spp/analyse/macros.hpp>

module spp.asts.is_expression_ast;
import spp.analyse.errors.semantic_error;
import spp.analyse.errors.semantic_error_builder;
import spp.analyse.scopes.scope;
import spp.analyse.scopes.scope_manager;
import spp.analyse.scopes.symbols;
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

MSVC_DEVCOM_11179619_IMPL_UNIT_USING_DECL
use_alias(spp::analyse::scopes, ScopeManager);
use_alias(spp::analyse::scopes, TypeRef);
use_alias(spp::analyse::scopes, VariableKind);
use_alias(spp::analyse::scopes, VariableSymbol);
use_alias(spp::asts::meta, CompilerMetaData);
use_alias(spp::codegen, LlvmCtx);

SPP_MOD_BEGIN
spp::asts::IsExpressionAst::IsExpressionAst(
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

spp::asts::IsExpressionAst::~IsExpressionAst() = default;

auto spp::asts::IsExpressionAst::PosStart() const -> std::size_t {
  // Use the lhs, or the mapped function.
  return Lhs ? Lhs->PosStart() : Source.OriginalPosStart;
}

auto spp::asts::IsExpressionAst::PosEnd() const -> std::size_t {
  // Use the rhs, or the mapped function.
  return Rhs ? Rhs->PosEnd() : Source.OriginalPosEnd;
}

auto spp::asts::IsExpressionAst::Clone() const -> Unique<Ast> {
  // Clone all the members of the ast.
  auto ast = MakeUnique<IsExpressionAst>(
    AstClone(Lhs),
    AstClone(TokOp),
    AstClone(Rhs));
  ast->_MappedFn = _MappedFn;
  ast->_SubjectLet = AstClone(_SubjectLet);
  return ast;
}

auto spp::asts::IsExpressionAst::ToString() const -> Str {
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

auto spp::asts::IsExpressionAst::Stage7_AnalyseSemantics(
  ScopeManager *sm, CompilerMetaData *meta) -> void {
  IMPORT_UTILS;

  _LhsAsId = AstClone(Lhs->To<IdentifierAst>());

  // The subject is owned by a place for as long as the bindings
  // viewing it last: a named one already is; anything else is
  // held by a hidden "$is" temporary, ending with the statement.
  if (auto hoisted = expr_utils::HoistOnce(*Lhs, "$is", sm, meta); hoisted.Let != nullptr) {
    _SubjectLet = std::move(hoisted.Let);
    Lhs = AstClone(hoisted.Name);
  }
  auto *const subject_root = sm->CurrentScope->FindVarSymbolOutermost(*Lhs).first;

  // Convert to a "case" destructure and analyse it.
  const auto n = sm->CurrentScope->Children.size();
  _MappedFn = operator_desugaring::ConvertIsExprToCase(*this, sm, meta);
  _MappedFn->Stage7_AnalyseSemantics(sm, meta);

  // Add the destructure symbols to the current scope. This
  // includes the lhs symbol if it's been flow typed. They are
  // recorded as the scope's bindings, as a "let"'s are, so
  // a later walk only finds them from this "is" onwards. A
  // pattern's binding lasts to the end of the statement.
  _Exposed.clear();
  if (AstAs<InnerScopeAst<Unique<Ast>>>(sm->CurrentScope->AstNode) == nullptr) {
    const auto destructure_syms = sm->CurrentScope->Children[n]->Children[0]->GetAllVarSymbols(true, true);
    for (auto const &x : destructure_syms) {
      _Exposed.emplace_back(x->SharedFromThis<VariableSymbol>());
      sm->CurrentScope->RecordLetBinding(_Exposed.back());
      if (x->Kind == VariableKind::Local or x->Kind == VariableKind::FlowNarrowing) {
        x->ExpiresWithStatement = true;
        meta->AddedIsBindings.emplace_back(x);
      }
      if (x->Kind != VariableKind::Local and x->Kind != VariableKind::Temporary) { continue; }

      // A binding is a view of the part of the subject it names, which
      // the subject's owner keeps owning: nothing drops the binding,
      // and moving it moves that part out of the owner. A part read
      // off the subject itself keeps its path; one behind a narrowing
      // (a variant's payload), or the flow-typed subject, is the whole.
      // So is the hidden "$" binding a pattern narrows the subject into.
      if (x->IsBorrow() or subject_root == nullptr) { continue; }
      if (x->ViewRoot != subject_root) { x->ViewSteps.clear(); }
      x->ViewRoot = subject_root;
      x->Kind = VariableKind::FlowNarrowing;
    }
    _DeclareExposed(*sm, meta);
  }
}

auto spp::asts::IsExpressionAst::_DeclareExposed(
  ScopeManager &sm, CompilerMetaData *meta) const -> void {
  // Bulk declare whatever is in the "exposed" list, in every
  // walk. A pattern's binding taking a name this scope already
  // holds notes the holder, which its block puts back at the
  // end of the statement.
  for (auto const &x : _Exposed) {
    // What the name means outside the statement: this scope's
    // binding of it, else one further out (a parameter, the subject
    // a flow-typed "o" narrows), put back into this scope there.
    auto *held = sm.CurrentScope->FindVarSymbol(x->Name.get(), true);
    if (held == nullptr and sm.CurrentScope->Parent != nullptr) {
      held = sm.CurrentScope->Parent->FindVarSymbol(x->Name.get());
    }
    if (x->ExpiresWithStatement and held != nullptr and held != x.get()) {
      meta->DisplacedByIsBindings.push_back({
        .In = sm.CurrentScope, .Binding = x,
        .Displaced = held->SharedFromThis<VariableSymbol>()});
    }
    sm.CurrentScope->DeclareLetBinding(x);
  }
}

auto spp::asts::IsExpressionAst::Stage8_CheckMemory(
  ScopeManager *sm, CompilerMetaData *meta) -> void {
  // Forward the memory checking to the mapped function, then
  // expose what it binds: the subject it tests is the one
  // named before the "is", not the one it narrows.
  if (_SubjectLet != nullptr) { _SubjectLet->Stage8_CheckMemory(sm, meta); }
  _MappedFn->Stage8_CheckMemory(sm, meta);
  _DeclareExposed(*sm, meta);
}

auto spp::asts::IsExpressionAst::Stage11_CodeGen(
  ScopeManager *sm, CompilerMetaData *meta, codegen::LlvmCtx *ctx) -> llvm::Value* {
  // As moving to the next scope clears ast-generated bindings
  // in that scope, we need to redeclare them for them for
  // narrowing analysis and codegen.
  _DeclareExposed(*sm, meta);

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
  if (_SubjectLet != nullptr) { _SubjectLet->Stage11_CodeGen(sm, meta, ctx); }
  return _MappedFn->Stage11_CodeGen(sm, meta, ctx);
}

auto spp::asts::IsExpressionAst::EvaluateCompTime(
  ScopeManager *sm, CompilerMetaData *meta) -> void {
  // Forward the resolution to the mapped function, then expose
  // what it binds, as stage 8 does.
  if (_SubjectLet != nullptr) { _SubjectLet->EvaluateCompTime(sm, meta); }
  _MappedFn->EvaluateCompTime(sm, meta);
  _DeclareExposed(*sm, meta);
}

auto spp::asts::IsExpressionAst::InferTypeRef(
  ScopeManager *sm, CompilerMetaData *) -> TypeRef {
  // Always return a boolean type (successful or failed
  // match)
  using generate::common_types_precompiled::BOOL;
  return TypeRef::Of(*BOOL, *sm->CurrentScope);
}

auto spp::asts::IsExpressionAst::IsAllowedInDefault() const -> bool {
  // Along with "case" expressions (which an "is" maps too),
  // the "is" expression is banned in default statements. In
  // the future, they might get enabled, even if in a restricted
  // way. Also depends on "case" being enabled in defaults.
  return false;
}

SPP_MOD_END
