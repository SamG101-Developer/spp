module;
#include <spp/macros.hpp>

export module spp.asts.closure_expression_ast;
import spp.asts.ast_kind;
import spp.asts.primary_expression_ast;
import spp.codegen.llvm_ctx;
import spp.codegen.llvm_func;
import spp.utils.types;
import llvm;
import std;

SPP_AST_COMMON_FWD_DECL(ClosureExpressionAst);
use(spp::asts, struct ClassPrototypeAst);
use(spp::asts, struct ClosureExpressionParameterAndCaptureGroupAst);
use(spp::asts, struct TokenAst);
use(spp::asts, struct TypeAst);

SPP_EXP_CLS struct spp::asts::ClosureExpressionAst final : PrimaryExpressionAst {
  SPP_AST_KEY_FUNCTIONS(ClosureExpressionAst);

  /// The optional "cor" keyword, which turns the closure into a
  /// coroutine closure. Otherwise, it defaults to "fun".
  Unique<TokenAst> Tok;

  /// The parameter and capture group of the closure: its
  /// parameters, and any variables captured from outer scopes.
  Unique<ClosureExpressionParameterAndCaptureGroupAst> PcGroup;

  /// The optional "->" token, present exactly when a return
  /// type is declared.
  Unique<TokenAst> TokArrow;

  /// The declared return type, or nullptr to infer it from the
  /// body. Declaring one lets a closure hand back a variant
  /// that its body only produces a member of, the same way
  /// "let x: Opt[S32] = Some(val=1)" does: inferring from the
  /// body gives "Some[S32]", and a caller holding it as
  /// "Opt[S32]" then reads the payload where the discriminant
  /// should be. The body is checked against and coerced into a
  /// declared type.
  Shared<TypeAst> ReturnType;

  /// The body of the closure: a single expression like
  /// "|| 1 + 2", or an inner scope for more complex closures. A
  /// declared return type requires the braced form, because
  /// that is the only one a "ret" can be written in.
  Unique<ExpressionAst> Body;

  struct {
    Shared<TypeAst> _OriginalRetType;
  } Source;

  ClosureExpressionAst(
    decltype(Tok) &&tok,
    decltype(PcGroup) &&pc_group,
    decltype(TokArrow) &&tok_arrow,
    decltype(ReturnType) return_type,
    decltype(Body) &&body);

  ~ClosureExpressionAst() override;

  SPP_ATTR_NODISCARD auto HasBorrowedCaptures() const -> bool;

  auto Stage7_AnalyseSemantics(ScopeManager *sm, CompilerMetaData *meta) -> void override;

  auto Stage8_CheckMemory(ScopeManager *sm, CompilerMetaData *meta) -> void override;

  auto Stage11_CodeGen(ScopeManager *sm, CompilerMetaData *meta, codegen::LlvmCtx *ctx) -> llvm::Value* override;

  auto InferType(ScopeManager *sm, CompilerMetaData *meta) -> Shared<TypeAst> override;

  SPP_ATTR_NODISCARD auto GetLlvmFunc() const -> Shared<codegen::LlvmFuncWrapper>;

  /// Release the class prototypes minted for closure types. They
  /// are held for the run because the scopes and symbols built
  /// against them outlive the expression that produced them, so
  /// a compile has to let go of them itself.
  static auto ClearMockAsts() -> void;

  SPP_ATTR_NODISCARD auto IsAllowedInDefault() const -> bool override;

private:
  /// The "FunRef"/"FunMut"/"FunMov" type that the closure's
  /// parameters, return type and captures decide. This is what
  /// the closure's own type superimposes, rather than what it
  /// is.
  SPP_ATTR_NODISCARD auto _FunctionalType(ScopeManager *sm, CompilerMetaData *meta) const -> Shared<TypeAst>;

  /// Mint the closure's own nominal type - a "$closure..." class
  /// superimposing "_FunctionalType" - and register it where the
  /// closure was written. Thread safety is decided by what a
  /// closure captured, and two closures of the same signature
  /// capture different things, so there is nowhere on the
  /// shared "FunMov[Args, Out]" instantiation to record it; a
  /// plain function has had a "$" mock of its own since stage 1
  /// for the same reason.
  auto _MakeMockType(ScopeManager *sm, CompilerMetaData *meta) -> Shared<TypeAst>;

  /// The class prototypes behind the minted closure types, owned
  /// for the length of the compile.
  inline static Vec<Unique<Ast>> _MockAsts = {};

  /// The closure's own type, minted in stage 7. Null until then,
  /// and null on a clone that is never re-analysed, which is why
  /// "InferType" falls back to the functional type rather than
  /// assuming it is there.
  Shared<TypeAst> _MockType;

  /// The inferred return type of the closure, determined during
  /// semantic analysis and type inference. Must be consistent
  /// with each returning value of the closure body.
  Shared<TypeAst> _TrueRetType;

  /// The LLVM function representing the closure. Generated in
  /// stage 11, and used to call the closure when it is invoked.
  Shared<codegen::LlvmFuncWrapper> _LlvmFunc;
};
