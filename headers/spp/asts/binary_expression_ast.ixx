module;
#include <spp/macros.hpp>

export module spp.asts.binary_expression_ast;
import spp.asts.ast_kind;
import spp.asts.expression_ast;
import spp.codegen.llvm_ctx;
import spp.utils.types;
import llvm;
import std;

SPP_AST_COMMON_FWD_DECL(BinaryExpressionAst);
use(spp::asts, struct GenericArgumentAst);
use(spp::asts, struct LetStatementInitializedAst);
use(spp::asts, struct PostfixExpressionAst);
use(spp::asts, struct TokenAst);
use(spp::asts, struct TypeAst);
use(spp::analyse::scopes, struct TypeRef);

/// A binary expression between 2 operators, either a normal
/// "+", "*" etc, or a compound assignment operator like "+=".
/// These all map to their method like ".add". Also handles
/// comparison chain collapsing, like Python, and binary
/// folding. Note "is" has its own expression ast.
SPP_EXP_CLS struct spp::asts::BinaryExpressionAst final : ExpressionAst {
  SPP_AST_KEY_FUNCTIONS(BinaryExpressionAst);

  /// The left-hand-side value.
  Unique<ExpressionAst> Lhs;

  /// The binary operation token.
  Unique<TokenAst> TokOp;

  /// The right-hand-side value.
  Unique<ExpressionAst> Rhs;

  struct {
    std::size_t OriginalPosStart;
    std::size_t OriginalPosEnd;
  } Source;

  /// Check if this is a logical binary operator ("and"/"or"),
  /// which have some special behaviour - no function mapping,
  /// direct LLVM intrinsics, for short-circuiting.
  SPP_ATTR_NODISCARD auto IsLogicalOperator() const -> bool;

  BinaryExpressionAst(
    decltype(Lhs) &&lhs,
    decltype(TokOp) &&tok_op,
    decltype(Rhs) &&rhs);

  ~BinaryExpressionAst() override;

  /// Handle the conversion of a binary expression into a method,
  /// collapse comparison chains, and handle binary folding
  /// like "a + .." for tuples.
  auto Stage7_AnalyseSemantics(ScopeManager *sm, CompilerMetaData *meta) -> void override;

  /// Run the memory check through the mapped function. For
  /// logical operations, check the left and right are valid -
  /// we could be doing "a.b or c", and "a" must be valid.
  auto Stage8_CheckMemory(ScopeManager *sm, CompilerMetaData *meta) -> void override;

  /// Resolve the binary operator at compile time - either
  /// moving into the mapped function (must be "cmp") or
  /// manually computing the "and"/"or" (+ short circuiting)
  auto Stage9_CompTimeResolve(ScopeManager *sm, CompilerMetaData *meta) -> void override;

  /// Forward the codegen into the mapped function, or build
  /// an "and"/"or" IR injection that allows for short
  /// circuiting.
  auto Stage11_CodeGen(ScopeManager *sm, CompilerMetaData *meta, codegen::LlvmCtx *ctx) -> llvm::Value* override;

  /// Infer the type from the mapped function, or use Bool for
  /// the logical operators.
  auto InferType(ScopeManager *sm, CompilerMetaData *meta) -> Shared<TypeAst> override;

  auto InferTypeRef(ScopeManager *sm, CompilerMetaData *meta) -> TypeRef override;

  /// Do the substitution of the left and right side operators.
  /// Todo: Do we need to use function mapping here?
  SPP_ATTR_NODISCARD auto SubstituteGenericsExpr(
    Vec<GenericArgumentAst*> const &args) const
    -> Shared<ExpressionAst> override;

  /// Check the left and right side are safe to use in runtime
  /// default contexts.
  /// Todo: Do we need to use function mapping here?
  SPP_ATTR_NODISCARD auto IsAllowedInDefault() const -> bool override;

private:
  /// The compiler-generated function call representing the
  /// operation. Stays nullptr for the logical operators.
  Shared<PostfixExpressionAst> _MappedFunc;

  /// When we collapse comparisons with temporaries, we need
  /// to store them so they aren't cloned: "a < f() < b" needs
  /// to materialize "f()".
  Vec<Unique<LetStatementInitializedAst>> _ChainTemps;

  /// Check whether a logical operator has been analysed - the
  /// mirror of checking if "_MappedFunc" is not nullptr.
  bool _LogicalAnalysed;

  /// Track if this binary operation is a logic "and"/"or"
  /// operation or not.
  bool _IsLogical;
};
