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
import spp.analyse.utils.regions;
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
  ast->_Bound = _Bound;
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

  // The subject is owned by a place for as long as the bindings
  // viewing it last: a named one already is; anything else is
  // held by a hidden "$is" temporary, ending with the condition
  // it is in ("Scope::TemporaryHome").
  if (auto hoisted = expr_utils::HoistOnce(*Lhs, "$is", sm, meta); hoisted.Let != nullptr) {
    _SubjectLet = std::move(hoisted.Let);
    Lhs = AstClone(hoisted.Name);
  }
  auto *const subject_root = sm->CurrentScope->FindVarSymbolOutermost(*Lhs).first;

  // Convert to a "case" destructure and analyse it.
  const auto n = sm->CurrentScope->Children.size();
  _MappedFn = operator_desugaring::ConvertIsExprToCase(*this, sm, meta);
  _MappedFn->Stage7_AnalyseSemantics(sm, meta);

  // What the pattern binds, and the subject it narrows, hold where
  // it matched: what this "is" leads into declares them ("Bound"),
  // nothing else. Nothing is declared where the "is" stands.
  _Bound.clear();
  if (AstAs<InnerScopeAst<Unique<Ast>>>(sm->CurrentScope->AstNode) == nullptr) {
    const auto destructure_syms = sm->CurrentScope->Children[n]->Children[0]->GetAllVarSymbols(true, true);
    for (auto const &x : destructure_syms) { _Bound.emplace_back(x->SharedFromThis<VariableSymbol>()); }

    // What an "is" binds is a view of the part of the subject it names
    // ("regions::IsView"): the subject's owner keeps owning it, so
    // nothing drops a binding, and using one uses that part of the
    // owner. A binding is the place it was read off ("o.<Some[T]>.val.a"
    // through the narrowed subject, itself a view), through to the
    // subject. One read off anything else stands for the whole.
    if (subject_root != nullptr) {
      auto bindings = Vec<VariableSymbol*>();
      for (auto const &x : destructure_syms) {
        if ((x->Kind != VariableKind::Local and x->Kind != VariableKind::Temporary) or x->IsBorrow()) { continue; }
        x->Kind = VariableKind::FlowNarrowing;
        bindings.emplace_back(x);
      }
      const auto reaches_subject = [&](VariableSymbol const *root) {
        while (root != nullptr and root != subject_root and regions::IsView(*root)) { root = root->ViewRoot; }
        return root == subject_root;
      };
      for (auto *const x : bindings) {
        if (not reaches_subject(x->ViewRoot)) {
          x->ViewRoot = subject_root;
          x->ViewSteps.clear();
        }
      }
    }
  }
}

auto spp::asts::IsExpressionAst::Stage8_CheckMemory(
  ScopeManager *sm, CompilerMetaData *meta) -> void {
  // Forward the memory checking to the mapped function.
  if (_SubjectLet != nullptr) { _SubjectLet->Stage8_CheckMemory(sm, meta); }
  _MappedFn->Stage8_CheckMemory(sm, meta);
}

auto spp::asts::IsExpressionAst::Stage11_CodeGen(
  ScopeManager *sm, CompilerMetaData *meta, codegen::LlvmCtx *ctx) -> llvm::Value* {
  // Forward the code generation to the mapped function. The narrowed
  // subject shares the subject's storage ("LlvmInfo").
  if (_SubjectLet != nullptr) { _SubjectLet->Stage11_CodeGen(sm, meta, ctx); }
  return _MappedFn->Stage11_CodeGen(sm, meta, ctx);
}

auto spp::asts::IsExpressionAst::EvaluateCompTime(
  ScopeManager *sm, CompilerMetaData *meta) -> void {
  // Forward the resolution to the mapped function.
  if (_SubjectLet != nullptr) { _SubjectLet->EvaluateCompTime(sm, meta); }
  _MappedFn->EvaluateCompTime(sm, meta);
}

auto spp::asts::IsExpressionAst::Bound() const -> Vec<Shared<VariableSymbol>> const& {
  return _Bound;
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
