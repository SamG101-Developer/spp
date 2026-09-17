module;
#include <spp/macros.hpp>

module spp.asts.let_statement_uninitialized_ast;
import spp.analyse.scopes.scope;
import spp.analyse.scopes.scope_manager;
import spp.analyse.scopes.symbols;
import spp.analyse.utils.type_utils;
import spp.asts.identifier_ast;
import spp.asts.local_variable_ast;
import spp.asts.object_initializer_argument_group_ast;
import spp.asts.object_initializer_ast;
import spp.asts.token_ast;
import spp.asts.type_ast;
import spp.asts.meta.compiler_meta_data;
import spp.asts.utils.ast_utils;
import spp.lex.tokens;

SPP_MOD_BEGIN
LetStatementUninitializedAst::LetStatementUninitializedAst(
  decltype(TokLet) &&tok_let,
  decltype(Var) &&var,
  decltype(TokColon) &&tok_colon,
  decltype(Type) type) :
  TokLet(std::move(tok_let)),
  Var(std::move(var)),
  TokColon(std::move(tok_colon)),
  Type(std::move(type)) {
  //
  using lex::SppTokenType;
  SPP_SET_AST_TO_DEFAULT_IF_NULLPTR(
    this->TokLet, SppTokenType::KW_LET, "let");
}

LetStatementUninitializedAst::~LetStatementUninitializedAst() = default;

auto LetStatementUninitializedAst::PosStart() const -> std::size_t {
  // Use the "let" token.
  return TokLet->PosStart();
}

auto LetStatementUninitializedAst::PosEnd() const -> std::size_t {
  // Use the type.
  return Type->PosEnd();
}

auto LetStatementUninitializedAst::Clone() const -> Unique<Ast> {
  // Clone all the members of the ast.
  return MakeUnique<LetStatementUninitializedAst>(
    AstClone(TokLet),
    AstClone(Var),
    AstClone(TokColon),
    AstCloneShared(Type));
}

auto LetStatementUninitializedAst::ToString() const -> Str {
  SPP_STRING_START;
  SPP_STRING_APPEND(TokLet).append(" ");
  SPP_STRING_APPEND(Var);
  SPP_STRING_APPEND_RAW(":").append(" ");
  SPP_STRING_APPEND(Type);
  SPP_STRING_END;
}

auto LetStatementUninitializedAst::Stage7_AnalyseSemantics(
  ScopeManager *sm, CompilerMetaData *meta) -> void {
  using analyse::utils::type_utils::ResolveWrittenType;

  // Analyse the type, and create a mock value for analysis.
  Type = ResolveWrittenType(*Type, *sm, *meta);
  const auto mock_init = MakeUnique<ObjectInitializerAst>(
    Type, nullptr);

  // Update the meta arguments.
  const auto _meta_guard = MetaGuard(meta);
  meta->LetStatementValue = mock_init.get();
  meta->LetStatementExplicitType = Type;
  meta->LetStatementFromUninitialized = true;
  Var->Stage7_AnalyseSemantics(sm, meta);
}

auto LetStatementUninitializedAst::Stage8_CheckMemory(
  ScopeManager *sm, CompilerMetaData *meta) -> void {
  // Check the variable for memory issues.
  const auto _meta_guard = MetaGuard(meta);
  meta->LetStatementValue = nullptr;
  meta->LetStatementFromUninitialized = true;
  Var->Stage8_CheckMemory(sm, meta);

  // Mark all the parts as uninitialized.
  for (auto const &v : Var->ExtractNames()) {
    sm->CurrentScope->GetVarSymbol(v.get())->MemInfo->MovedBy(
      *this, sm->CurrentScope);
  }
}

auto LetStatementUninitializedAst::Stage11_CodeGen(
  ScopeManager *sm, CompilerMetaData *meta, codegen::LlvmCtx *ctx) -> llvm::Value* {
  // Setup a lot of meta information for the local variable to
  // correctly generate the value.
  const auto _meta_guard = MetaGuard(meta);
  meta->LetStatementValue = nullptr;
  meta->LetStatementExplicitType = Type;
  meta->LetStatementFromUninitialized = true;

  // Delegate the code generation to the variable, after setting
  // up the meta. Note that the "alloca" is returned even though
  // this isn't an expression, for parent nodes that might need it.
  // It's a hacky solution that should live on "meta" but no harm
  // in doing it this way.
  const auto alloca = Var->Stage11_CodeGen(sm, meta, ctx);
  return alloca;
}

SPP_MOD_END
