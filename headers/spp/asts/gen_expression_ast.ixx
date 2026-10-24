module;
#include <spp/macros.hpp>

export module spp.asts.gen_expression_ast;
import spp.analyse.utils.linear_utils;
import spp.asts.ast_kind;
import spp.asts.primary_expression_ast;
import spp.codegen.llvm_ctx;
import spp.utils.types;
import llvm;
import std;

SPP_AST_COMMON_FWD_DECL(GenExpressionAst);
use(spp::analyse::scopes, struct TypeRef);
use(spp::asts, struct ConventionAst);
use(spp::asts, struct TokenAst);
use(spp::asts, struct TypeAst);

/// Represents a value being yielded out of a coroutine. A
/// convention can be applied to the value, to create
/// foundational structures like iterators.
SPP_EXP_CLS struct spp::asts::GenExpressionAst final : PrimaryExpressionAst {
  SPP_AST_KEY_FUNCTIONS(GenExpressionAst);

  /// The "gen" token, marking the point where the coroutine
  /// suspends its execution and yields a value.
  Unique<TokenAst> TokGen;

  /// An optional convention applied to the yielded value,
  /// such as making it an iterator.
  Unique<ConventionAst> Conv;

  /// The expression being yielded out of the coroutine; the
  /// value returned when the coroutine is resumed.
  Unique<ExpressionAst> Expr;

  struct {
    std::size_t OriginalPosEnd;
  } Source;

  GenExpressionAst(
    decltype(TokGen) &&tok_gen,
    decltype(Conv) &&conv,
    decltype(Expr) &&expr);

  ~GenExpressionAst() override;

  auto Stage7_AnalyseSemantics(ScopeManager *sm, CompilerMetaData *meta) -> void override;

  auto Stage8_CheckMemory(ScopeManager *sm, CompilerMetaData *meta) -> void override;

  auto Stage11_CodeGen(ScopeManager *sm, CompilerMetaData *meta, codegen::LlvmCtx *ctx) -> llvm::Value* override;

  auto InferTypeRef(ScopeManager *sm, CompilerMetaData *meta) -> TypeRef override;

  /// A "gen" diverges where it reads as the return: in a "GenOnce"
  /// lowered to a subroutine ("CoroutinePrototypeAst::_LowerGenOnce"),
  /// which is generated with the enclosing flavour "fun". In a
  /// coroutine it suspends, and control comes back to it.
  SPP_ATTR_NODISCARD auto Diverges(ScopeManager *sm, CompilerMetaData *meta) -> bool override;

private:
  Shared<TypeAst> _GenType;
  bool _IsOnce;

  /// The enclosing generator's "Yield", resolved at stage 7: what a
  /// yielded value is coerced into at codegen.
  Shared<TypeRef> _YieldRef;

  /// What the coroutine still owns here, for destroying its frame
  /// at this suspension to drop: per scope, from this one out to
  /// the coroutine's own, newest first ("OwnedAtSuspend"). Names,
  /// read where this is generated, so an instance finds its own.
  Vec<Vec<analyse::utils::linear_utils::OwnedValue>> _OwnedAtSuspend;
};
