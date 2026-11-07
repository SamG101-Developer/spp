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
use(spp::analyse::scopes, struct TypeRef);
use(spp::analyse::scopes, struct ExprSubst);
use(spp::asts, struct GenericArgumentAst);
use(spp::asts, struct LetStatementInitializedAst);
use(spp::asts, struct TokenAst);
use(spp::asts, struct TypeAst);

SPP_EXP_CLS struct spp::asts::TupleLiteralAst final : LiteralAst {
  SPP_GCC_VTABLE_FIX
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

  SPP_ATTR_NODISCARD auto EqualsTupleLiteral(
    TupleLiteralAst const &) const -> Ordering override;

  SPP_ATTR_NODISCARD auto Equals(
    ExpressionAst const &other) const -> Ordering override;

  auto Stage7_AnalyseSemantics(
    ScopeManager *sm, CompilerMetaData *meta) -> void override;

  auto Stage8_CheckMemory(
    ScopeManager *sm, CompilerMetaData *meta) -> void override;

  auto Stage11_CodeGen(
    ScopeManager *sm, CompilerMetaData *meta,
    codegen::LlvmCtx *ctx) -> llvm::Value* override;

  auto EvaluateCompTime(
    ScopeManager *sm, CompilerMetaData *meta) -> void override;

  auto InferType(
    ScopeManager *sm, CompilerMetaData *meta) -> Shared<TypeAst> override;

  auto InferTypeRef(
    ScopeManager *sm, CompilerMetaData *meta) -> TypeRef override;

  SPP_ATTR_NODISCARD auto ReadExpr(
    ExprSubst const &sub) const -> Shared<ExpressionAst> override;

  SPP_ATTR_NODISCARD auto IsAllowedInDefault() const -> bool override;

  /// The types of these elements, which is used by "_BuildType" when creating
  /// the type to cache or return (like for the type inference methods).
  static auto ElementTypes(
    Vec<ExpressionAst*> const &elems, ScopeManager *sm, CompilerMetaData *meta,
    bool strip_conventions = false) -> Vec<Shared<TypeAst>>;

private:
  /// The type, built and analysed once in stage 7. Rebuilding it for every
  /// "InferType" analysed it again each time, codegen included, and re-decided
  /// it in whatever context the caller happened to be in.
  Shared<TypeAst> _InferredType;

  /// A spread element that is no place ("(..g())") evaluated once, into a
  /// temporary its elements are read off (made before the elements).
  Vec<Unique<LetStatementInitializedAst>> _SpreadTemps;

  /// Build and analyse the type from the elements.
  auto _BuildType(ScopeManager *sm, CompilerMetaData *meta) -> Shared<TypeAst>;
};

SPP_GCC_VTABLE_FIX_IMPL(spp::asts::TupleLiteralAst)
