module;
#include <spp/macros.hpp>

export module spp.asts.binary_expression_ast;
import spp.analyse.utils.drop_utils;
import spp.asts.ast_kind;
import spp.asts.expression_ast;
import spp.codegen.llvm_ctx;
import spp.utils.types;
import llvm;
import std;

use_common(BinaryExpressionAst);
use(spp::analyse::utils::memory_state, struct ScopeSnapshot);
use(spp::analyse::scopes, struct ExprSubst);
use(spp::asts, struct GenericArgumentAst);
use(spp::asts, struct IdentifierAst);
use(spp::asts, struct LetStatementInitializedAst);
use(spp::asts, struct PostfixExpressionAst);
use(spp::asts, struct TokenAst);
use(spp::asts, struct TypeAst);
use(spp::analyse::scopes, struct TypeRef);
use(spp::analyse::scopes, struct VariableSymbol);

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

  /// Called by a loop, a guard or a boolean "case" on its condition
  /// before checking it, whose true edge leads into a body. An "and"
  /// there keeps what its right operand's temporaries hold where it
  /// answers true, for the body to read its "is" bindings through,
  /// and drops it where the right operand answers false: after it,
  /// they are held exactly where it is true ("HeldWhenTrue").
  /// Anywhere else they end with the right operand.
  static auto MarkCondition(ExpressionAst &cond) -> void;

  /// The temporaries held where the condition "cond" answers true,
  /// for its consumer to end on that edge and pass over on the other
  /// (none unless it is an "and" marked a condition).
  SPP_ATTR_NODISCARD static auto HeldWhenTrue(
    ExpressionAst const &cond) -> Vec<analyse::scopes::VariableSymbol*>;

  /// The two operands to the binary expression, left-hand-side
  /// and right-hand-side, held until their owning pointers
  /// are moved into the function call.
  SPP_ATTR_NODISCARD auto Operands() const
    -> Pair<ExpressionAst const*, ExpressionAst const*>;

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

  /// Forward the codegen into the mapped function, or build
  /// an "and"/"or" IR injection that allows for short
  /// circuiting.
  auto Stage11_CodeGen(ScopeManager *sm, CompilerMetaData *meta, codegen::LlvmCtx *ctx) -> llvm::Value* override;

  /// Resolve the binary operator at compile time - either
  /// moving into the mapped function (must be "cmp") or
  /// manually computing the "and"/"or" (+ short circuiting)
  auto EvaluateCompTime(ScopeManager *sm, CompilerMetaData *meta) -> void override;

  /// Infer the type from the mapped function, or use Bool for
  /// the logical operators.
  auto InferType(ScopeManager *sm, CompilerMetaData *meta) -> Shared<TypeAst> override;

  auto InferTypeRef(ScopeManager *sm, CompilerMetaData *meta) -> TypeRef override;

  /// Do the substitution of the left and right side operators.
  /// Todo: Do we need to use function mapping here?
  SPP_ATTR_NODISCARD auto ReadExpr(
    ExprSubst const &sub) const -> Shared<ExpressionAst> override;

  /// Check the left and right side are safe to use in runtime
  /// default contexts.
  /// Todo: Do we need to use function mapping here?
  SPP_ATTR_NODISCARD auto IsAllowedInDefault() const -> bool override;

private:
  /// The compiler-generated function call representing the
  /// operation. Stays nullptr for the logical operators.
  Shared<PostfixExpressionAst> _MappedFn;

  /// When we collapse comparisons with temporaries, we need
  /// to store them so they aren't cloned: "a < f() < b" needs
  /// to materialize "f()". A fold's tuple operand is held the
  /// same way: ".. + f()" reads each element off one "f()".
  Vec<Unique<LetStatementInitializedAst>> _ChainTemps;

  /// A logical operator's right operand runs on one path only, and
  /// the short circuit is the other: where they meet, what one holds
  /// and the other does not is dropped on the path still holding it
  /// ("flag and take(x)" drops "x" where "flag" is false).
  Vec<analyse::utils::drop_utils::OwnedValue> _DropsOnShortCircuit;
  Vec<analyse::utils::drop_utils::OwnedValue> _DropsAfterRhs;

  /// The hidden temporaries the right operand declares ("$is" for an
  /// "is" subject that is no place) exist only on its path: they are
  /// dropped at its end, and the join never sees them. As a condition
  /// ("MarkCondition"), they are dropped only where it answers false,
  /// and held on where it answers true ("_HeldWhenTrue").
  Vec<analyse::utils::drop_utils::OwnedValue> _DropsRhsTemporaries;
  bool _IsCondition = false;
  Vec<analyse::scopes::VariableSymbol*> _HeldWhenTrue;

  /// Nest a chain of one logical operator to the right ("a and b and
  /// c" as "a and (b and c)").
  auto _NestChainRight() -> void;

  /// Stage 8 for a logical operator: join the state the left operand
  /// left ("short_circuit") with the right operand's, filling the two
  /// drop lists above.
  auto _JoinShortCircuit(
    analyse::utils::memory_state::ScopeSnapshot const &short_circuit,
    Vec<analyse::scopes::VariableSymbol*> const &rhs_temporaries, ScopeManager &sm, CompilerMetaData *meta) -> void;

  /// Check whether a logical operator has been analysed - the
  /// mirror of checking if "_MappedFn" is not nullptr.
  bool _LogicalAnalysed;

  /// Track if this binary operation is a logic "and"/"or"
  /// operation or not.
  bool _IsLogical;

  /// A template's open pack, or value of an open tuple, this fold is over
  /// (".. + xs"): checked on three elements, the type holding for any length
  /// ("_FoldElements"), and used whole by the memory checks, not element by
  /// element.
  Unique<IdentifierAst> _OpenPack;

  /// Whether folding "_OpenPack" moves its elements: each is neither copied nor
  /// a borrow.
  bool _OpenPackMoves = false;

  /// Analyse a fold's tuple operand, once: anything but a name is bound to a
  /// temporary (see "_ChainTemps"), which @p operand then names, as the fold
  /// reads an element off it per element.
  auto _BindFoldOperand(
    Unique<ExpressionAst> &operand, ScopeManager *sm,
    CompilerMetaData *meta) -> void;

  /// A fold's tuple or pack operand ("t" in ".. + t" or "t + ..")
  /// bound and checked, and each of its elements read off it in
  /// order ("t.0", "t.1", ...; a closed pack's are its parameters,
  /// "xs#0", ...), analysed. It must have at least two elements;
  /// "fold" (the "..") is named in the error too. An open pack gives
  /// three, the type each prefix folds to checked to agree.
  auto _FoldElements(
    Unique<ExpressionAst> &tuple, Ast const &fold, ScopeManager *sm,
    CompilerMetaData *meta) -> Vec<Unique<ExpressionAst>>;
};
