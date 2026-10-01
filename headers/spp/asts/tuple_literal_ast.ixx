module;
#include <spp/macros.hpp>

export module spp.asts.tuple_literal_ast;
import spp.asts.ast_kind;
import spp.asts.literal_ast;
import spp.codegen.llvm_ctx;
import spp.utils.types;
import llvm;
import std;

SPP_AST_COMMON_FWD_DECL(TupleLiteralAst);
use(spp::asts, struct GenericArgumentAst);
use(spp::asts, struct TokenAst);
use(spp::asts, struct TypeAst);

SPP_EXP_CLS struct spp::asts::TupleLiteralAst final : LiteralAst {
  SPP_GCC_VTABLE_FIX;
  SPP_AST_KEY_FUNCTIONS(TupleLiteralAst);

  /// The "(" token that starts the tuple literal.
  Unique<TokenAst> TokL;

  /// The elements of the tuple literal.
  Vec<Unique<ExpressionAst>> Elems;

  /// The ")" token that ends the tuple literal.
  Unique<TokenAst> TokR;

  TupleLiteralAst(
    decltype(TokL) &&tok_l,
    decltype(Elems) &&elements,
    decltype(TokR) &&tok_r);

  ~TupleLiteralAst() override;

  SPP_ATTR_NODISCARD auto EqualsTupleLiteral(TupleLiteralAst const &) const -> Ordering override;

  SPP_ATTR_NODISCARD auto Equals(ExpressionAst const &other) const -> Ordering override;

  auto Stage7_AnalyseSemantics(ScopeManager *sm, CompilerMetaData *meta) -> void override;

  auto Stage8_CheckMemory(ScopeManager *sm, CompilerMetaData *meta) -> void override;

  auto Stage9_CompTimeResolve(ScopeManager *sm, CompilerMetaData *meta) -> void override;

  auto Stage11_CodeGen(ScopeManager *sm, CompilerMetaData *meta, codegen::LlvmCtx *ctx) -> llvm::Value* override;

  auto InferType(ScopeManager *sm, CompilerMetaData *meta) -> Shared<TypeAst> override;

  SPP_ATTR_NODISCARD auto SubstituteGenericsExpr(
    Vec<GenericArgumentAst*> const &args) const
    -> Shared<ExpressionAst> override;

  SPP_ATTR_NODISCARD auto IsAllowedInDefault() const -> bool override;

  /// The type a tuple literal of these elements has, analysed:
  /// what a call's variadic arguments are typed as before any
  /// literal holding them is built.
  static auto TypeOfElements(
    Vec<ExpressionAst*> const &elems,
    std::size_t pos,
    ScopeManager *sm,
    CompilerMetaData *meta)
    -> Shared<TypeAst>;

private:
  /// The type, built and analysed once in stage 7. Rebuilding it
  /// for every "InferType" analysed it again each time, codegen
  /// included, and re-decided it in whatever context the caller
  /// happened to be in.
  Shared<TypeAst> _InferredType;

  /// Build and analyse the type from the elements.
  auto _BuildType(ScopeManager *sm, CompilerMetaData *meta) -> Shared<TypeAst>;
};

SPP_GCC_VTABLE_FIX_IMPL(spp::asts::TupleLiteralAst)
