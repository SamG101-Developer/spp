module;
#include <spp/macros.hpp>
#include <spp/analyse/macros.hpp>

module spp.asts.local_variable_destructure_array_ast;
import spp.analyse.errors.semantic_error;
import spp.analyse.errors.semantic_error_builder;
import spp.analyse.scopes.comp_key;
import spp.analyse.scopes.scope_manager;
import spp.analyse.scopes.symbols;
import spp.analyse.utils.destructure_utils;
import spp.analyse.utils.type_predicates;
import spp.asts.array_literal_explicit_elements_ast;
import spp.asts.expression_ast;
import spp.asts.generic_argument_ast;
import spp.asts.generic_argument_group_ast;
import spp.asts.identifier_ast;
import spp.asts.integer_literal_ast;
import spp.asts.let_statement_initialized_ast;
import spp.asts.local_variable_single_identifier_ast;
import spp.asts.token_ast;
import spp.asts.type_ast;
import spp.asts.type_identifier_ast;
import spp.asts.meta.compiler_meta_data;
import spp.asts.utils.ast_utils;
import spp.lex.tokens;
import spp.utils.algorithms;
import genex;

SPP_MOD_BEGIN
LocalVariableDestructureArrayAst::LocalVariableDestructureArrayAst(
  decltype(TokL) &&tok_l,
  decltype(Elems) &&elems,
  decltype(TokR) &&tok_r) :
  TokL(std::move(tok_l)),
  Elems(std::move(elems)),
  TokR(std::move(tok_r)),
  _TmpName(nullptr) {
}

LocalVariableDestructureArrayAst::~LocalVariableDestructureArrayAst() = default;

auto LocalVariableDestructureArrayAst::PosStart() const -> std::size_t {
  // Use the "[" token.
  return TokL != nullptr ? TokL->PosStart() : Elems.Front()->PosStart();
}

auto LocalVariableDestructureArrayAst::PosEnd() const -> std::size_t {
  // Use the "]" token.
  return TokR != nullptr ? TokR->PosEnd() : Elems.Back()->PosEnd();
}

auto LocalVariableDestructureArrayAst::Clone() const -> Unique<Ast> {
  // Clone all the members of the ast.
  auto c = MakeUnique<LocalVariableDestructureArrayAst>(
    AstClone(TokL),
    AstCloneVec(Elems),
    AstClone(TokR));
  c->_NewAsts = AstCloneVec(_NewAsts);
  c->_TmpName = AstCloneShared(_TmpName);
  return c;
}

auto LocalVariableDestructureArrayAst::ToString() const -> Str {
  SPP_STRING_START;
  SPP_STRING_APPEND_RAW("[");
  SPP_STRING_EXTEND(Elems, ", ");
  SPP_STRING_APPEND_RAW("]");
  SPP_STRING_END;
}

auto LocalVariableDestructureArrayAst::BindsByMove() const -> bool {
  // A destructure binds if any of its elements does. An empty
  // one, or one made only of skips, is a shape test and takes
  // nothing.
  return genex::any_of(Elems, [](auto const &elem) { return elem->BindsByMove(); });
}

auto LocalVariableDestructureArrayAst::Stage7_AnalyseSemantics(
  ScopeManager *sm, CompilerMetaData *meta) -> void {
  IMPORT_UTILS;
  // An array, whose length is the instantiation's own binding of "n", however its argument was written.
  // Todo: Test destructuring generic array - how would that work? like Arr[Str, n] => don't allow.
  const auto shape = destructure_utils::SequenceShape{
    .CheckAndCount = [&](ExpressionAst const &val, Shared<TypeAst> const &val_type) -> std::size_t {
      RaiseIf<SppVariableArrayDestructureArrayTypeMismatchError>(
        not type_predicates::IsTypeArray(TypeRef::ForKindCheck(*val_type, *sm->CurrentScope), *sm->CurrentScope),
        {sm->CurrentScope}, ERR_ARGS(*this, val, *val_type));
      return static_cast<std::size_t>(
        analyse::scopes::U64Of(sm->CurrentScope->FindTypeSymbol(val_type.get())->CompArgId("n")).value());
    },
    .RaiseSizeMismatch = [&](const std::size_t lhs, ExpressionAst const &val, const std::size_t rhs) {
      Raise<SppVariableArrayDestructureArraySizeMismatchError>({sm->CurrentScope}, ERR_ARGS(*this, lhs, val, rhs));
    },
    .MakeRest = [](Vec<Unique<ExpressionAst>> &&elems) -> Unique<ExpressionAst> {
      return MakeUnique<ArrayLiteralExplicitElementsAst>(nullptr, std::move(elems), nullptr);
    }};
  destructure_utils::DestructureSequenceStage7(
    *this, Elems, shape, _TmpName, _NewAsts, _FromCasePattern, sm, meta);
}

auto LocalVariableDestructureArrayAst::Stage8_CheckMemory(
  ScopeManager *sm, CompilerMetaData *meta) -> void {
  // Use shared helper.
  IMPORT_UTILS;
  destructure_utils::DestructureStage8(
    *this, Elems, _NewAsts, _TmpName, nullptr, _FromCasePattern, *sm, meta);
}

auto LocalVariableDestructureArrayAst::Stage9_CompTimeResolve(
  ScopeManager *sm, CompilerMetaData *meta) -> void {
  // Use shared helper.
  IMPORT_UTILS;
  destructure_utils::DestructureStage9(_NewAsts, _TmpName, nullptr, *sm, meta);
}

auto LocalVariableDestructureArrayAst::Stage11_CodeGen(
  ScopeManager *sm, CompilerMetaData *meta, codegen::LlvmCtx *ctx) -> llvm::Value* {
  // Generate the value into the hidden temporary once,
  // before the elements index it.
  IMPORT_UTILS;

  const auto _meta_guard = MetaGuard(meta);
  const auto llvm_subject = meta->LetStatementPrecomputedValue;
  meta->LetStatementPrecomputedValue = nullptr;

  if (_TmpName != nullptr) {
    destructure_utils::DestructureTempStage11(_TmpName, llvm_subject, *sm, meta, ctx);
  }

  // Generate the "let" statements for each element.
  for (auto const &ast : _NewAsts) { ast->Stage11_CodeGen(sm, meta, ctx); }
  return nullptr;
}

auto LocalVariableDestructureArrayAst::ExtractNames() const -> Vec<Shared<IdentifierAst>> {
  // Walk the nested bindings for variable names.
  IMPORT_UTILS;
  return destructure_utils::GetNestedBindingIdentifiers(Elems);
}

auto LocalVariableDestructureArrayAst::ExtractName() const -> Shared<IdentifierAst> {
  // No single identifier for destructured bindings.
  IMPORT_UTILS;
  return destructure_utils::UnmatchableSingleIdentifier(PosStart());
}

SPP_MOD_END
