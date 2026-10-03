module;
#include <spp/macros.hpp>
#include <spp/analyse/macros.hpp>

module spp.asts.local_variable_destructure_tuple_ast;
import spp.analyse.errors.semantic_error;
import spp.analyse.errors.semantic_error_builder;
import spp.analyse.scopes.scope_manager;
import spp.analyse.scopes.symbols;
import spp.analyse.utils.destructure_utils;
import spp.analyse.utils.type_predicates;
import spp.asts.expression_ast;
import spp.asts.generic_argument_group_ast;
import spp.asts.identifier_ast;
import spp.asts.let_statement_initialized_ast;
import spp.asts.local_variable_single_identifier_ast;
import spp.asts.token_ast;
import spp.asts.tuple_literal_ast;
import spp.asts.type_ast;
import spp.asts.type_identifier_ast;
import spp.asts.meta.compiler_meta_data;
import spp.asts.utils.ast_utils;
import spp.lex.tokens;
import spp.utils.algorithms;
import genex;

SPP_MOD_BEGIN
LocalVariableDestructureTupleAst::LocalVariableDestructureTupleAst(
  decltype(TokL) &&tok_l,
  decltype(Elems) &&elems,
  decltype(TokR) &&tok_r) :
  TokL(std::move(tok_l)),
  Elems(std::move(elems)),
  TokR(std::move(tok_r)),
  _TmpName(nullptr) {
  SPP_SET_AST_TO_DEFAULT_IF_NULLPTR(this->TokL, lex::SppTokenType::TK_LEFT_PARENTHESIS, "(");
  SPP_SET_AST_TO_DEFAULT_IF_NULLPTR(this->TokR, lex::SppTokenType::TK_RIGHT_PARENTHESIS, ")");
}

LocalVariableDestructureTupleAst::~LocalVariableDestructureTupleAst() = default;

auto LocalVariableDestructureTupleAst::PosStart() const -> std::size_t {
  // Use the "(" token.
  return TokL->PosStart();
}

auto LocalVariableDestructureTupleAst::PosEnd() const -> std::size_t {
  // Use the ")" token.
  return TokR->PosEnd();
}

auto LocalVariableDestructureTupleAst::Clone() const -> Unique<Ast> {
  // Clone all the members of the ast.
  auto c = MakeUnique<LocalVariableDestructureTupleAst>(
    AstClone(TokL),
    AstCloneVec(Elems),
    AstClone(TokR));
  c->_NewAsts = AstCloneVec(_NewAsts);
  c->_TmpName = AstCloneShared(_TmpName);
  return c;
}

auto LocalVariableDestructureTupleAst::ToString() const -> Str {
  SPP_STRING_START;
  SPP_STRING_APPEND(TokL);
  SPP_STRING_EXTEND(Elems, ", ");
  SPP_STRING_APPEND(TokR);
  SPP_STRING_END;
}

auto LocalVariableDestructureTupleAst::BindsByMove() const -> bool {
  // A destructure binds if any of its elements does. An empty
  // one, or one made only of skips, is a shape test and takes
  // nothing.
  return genex::any_of(Elems, [](auto const &elem) { return elem->BindsByMove(); });
}

auto LocalVariableDestructureTupleAst::Stage7_AnalyseSemantics(
  ScopeManager *sm, CompilerMetaData *meta) -> void {
  IMPORT_UTILS;
  // A tuple, whose length is its number of type arguments.
  const auto shape = destructure_utils::SequenceShape{
    .CheckAndCount = [&](ExpressionAst const &val, Shared<TypeAst> const &val_type) -> std::size_t {
      RaiseIf<SppVariableTupleDestructureTupleTypeMismatchError>(
        not type_predicates::IsTypeTuple(*val_type, *sm->CurrentScope),
        {sm->CurrentScope}, ERR_ARGS(*this, val, *val_type));
      return sm->CurrentScope->FindTypeSymbol(val_type.get())->TypeArgs().Len();
    },
    .RaiseSizeMismatch = [&](const std::size_t lhs, ExpressionAst const &val, const std::size_t rhs) {
      Raise<SppVariableTupleDestructureTupleSizeMismatchError>({sm->CurrentScope}, ERR_ARGS(*this, lhs, val, rhs));
    },
    .MakeRest = [](Vec<Unique<ExpressionAst>> &&elems) -> Unique<ExpressionAst> {
      return MakeUnique<TupleLiteralAst>(nullptr, std::move(elems), nullptr);
    }};
  destructure_utils::DestructureSequenceStage7(
    *this, Elems, shape, _TmpName, _NewAsts, _FromCasePattern, sm, meta);
}

auto LocalVariableDestructureTupleAst::Stage8_CheckMemory(
  ScopeManager *sm, CompilerMetaData *meta) -> void {
  IMPORT_UTILS;
  // Use the shared helper.
  destructure_utils::DestructureStage8(
    *this, Elems, _NewAsts, _TmpName, nullptr, _FromCasePattern, *sm, meta);
}

auto LocalVariableDestructureTupleAst::Stage9_CompTimeResolve(
  ScopeManager *sm, CompilerMetaData *meta) -> void {
  IMPORT_UTILS;
  // Use the shared helper.
  destructure_utils::DestructureStage9(
    _NewAsts, _TmpName, nullptr, *sm, meta);
}

auto LocalVariableDestructureTupleAst::Stage11_CodeGen(
  ScopeManager *sm, CompilerMetaData *meta, codegen::LlvmCtx *ctx) -> llvm::Value* {
  // Generate the value into the hidden temporary once, before
  // the elements index it.
  IMPORT_UTILS;

  const auto _meta_guard = MetaGuard(meta);
  const auto llvm_subject = meta->LetStatementPrecomputedValue;
  meta->LetStatementPrecomputedValue = nullptr;

  if (_TmpName != nullptr) {
    destructure_utils::DestructureTempStage11(_TmpName, llvm_subject, *sm, meta, ctx);
  }

  // Generate the "let" statements for each element.
  for (auto &&ast : _NewAsts) { ast->Stage11_CodeGen(sm, meta, ctx); }
  return nullptr;
}

auto LocalVariableDestructureTupleAst::ExtractNames() const -> Vec<Shared<IdentifierAst>> {
  // Walk the nested bindings for variable names.
  IMPORT_UTILS;
  return destructure_utils::GetNestedBindingIdentifiers(Elems);
}

auto LocalVariableDestructureTupleAst::ExtractName() const -> Shared<IdentifierAst> {
  // No single identifier for destructured bindings.
  IMPORT_UTILS;
  return destructure_utils::UnmatchableSingleIdentifier(PosStart());
}

SPP_MOD_END
