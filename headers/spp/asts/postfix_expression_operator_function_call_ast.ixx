module;
#include <spp/macros.hpp>

export module spp.asts.postfix_expression_operator_function_call_ast;
import spp.analyse.scopes.type_key;
import spp.asts.ast_kind;
import spp.asts.convention_ast;
import spp.asts.postfix_expression_operator_ast;
import spp.codegen.llvm_ctx;
import spp.utils.types;
import llvm;
import std;

use_common(PostfixExpressionOperatorFunctionCallAst);
use(spp::analyse::scopes, struct ExprSubst);
use(spp::analyse::scopes, class Scope);
use(spp::asts, struct ExpressionAst);
use(spp::asts, struct FunctionCallArgumentAst);
use(spp::asts, struct FunctionCallArgumentGroupAst);
use(spp::asts, struct FunctionCallArgumentPositionalAst);
use(spp::asts, struct FunctionParameterGroupAst);
use(spp::asts, struct FoldExpressionAst);
use(spp::asts, struct FunctionPrototypeAst);
use(spp::asts, struct GenericArgumentAst);
use(spp::asts, struct GenericArgumentGroupAst);
use(spp::asts, struct IdentifierAst);
use(spp::asts, struct LetStatementInitializedAst);
use(spp::asts, struct PostfixExpressionAst);
use(spp::analyse::scopes, struct TypeRef);
use(spp::analyse::scopes, struct TypeSymbol);
use(spp::asts, struct TypeAst);
use(spp::asts, struct UnaryExpressionOperatorAsyncAst);

SPP_EXP_CLS struct spp::asts::PostfixExpressionOperatorFunctionCallAst final : PostfixExpressionOperatorAst {
  SPP_GCC_VTABLE_FIX
  SPP_AST_KEY_FUNCTIONS(PostfixExpressionOperatorFunctionCallAst);

  /// The generic arguments for the function call.
  Unique<GenericArgumentGroupAst> GnArgGroup;

  /// The function arguments for the function call.
  Unique<FunctionCallArgumentGroupAst> FnArgGroup;

  /// The optional ".." fold token. This folds all tuples in the
  /// argument group, calling the function multiple times with
  /// each tuple element as the argument for the non-tuple
  /// parameters it has mapped to.
  Unique<FoldExpressionAst> Fold;

  struct {
    Ast *OriginalExpr;
  } Source;

  explicit PostfixExpressionOperatorFunctionCallAst(
    decltype(GnArgGroup) &&generic_arg_group,
    decltype(FnArgGroup) &&arg_group,
    decltype(Fold) &&fold);

  ~PostfixExpressionOperatorFunctionCallAst() override;

  SPP_ATTR_NODISCARD auto WalksOperand() const -> bool override;

  auto Stage7_AnalyseSemantics(ScopeManager *sm, CompilerMetaData *meta) -> void override;

  auto Stage8_CheckMemory(ScopeManager *sm, CompilerMetaData *meta) -> void override;

  auto Stage11_CodeGen(ScopeManager *sm, CompilerMetaData *meta, codegen::LlvmCtx *ctx) -> llvm::Value* override;

  auto EvaluateCompTime(
    ScopeManager *sm, CompilerMetaData *meta) -> void override;

  auto InferType(
    ScopeManager *sm, CompilerMetaData *meta) -> Shared<TypeAst> override;

  auto InferTypeRef(
    ScopeManager *sm, CompilerMetaData *meta) -> TypeRef override;

  SPP_ATTR_NODISCARD auto ReadExpr(
    ExprSubst const &sub) const -> Unique<PostfixExpressionOperatorAst> override;

  auto MarkAsAsync(Ast *async_token) -> void;

  SPP_ATTR_NODISCARD auto Target() const -> FunctionPrototypeAst*;

  /// Whether this is a method called on a value ("obj.m(a)"): its
  /// receiver is the "self" argument, first, and the left-hand side
  /// ("obj.m") names the method rather than being a use of "obj".
  SPP_ATTR_NODISCARD auto HasReceiver() const -> bool;

  /// Whether "arg" is that receiver: this call's first argument.
  SPP_ATTR_NODISCARD auto IsReceiverArg(FunctionCallArgumentAst const &arg) const -> bool;

  /// The prototype the call resolved to, as resolved ("Target" is
  /// what is emitted, which for a lowered "GenOnce" is no coroutine).
  SPP_ATTR_NODISCARD auto ResolvedProto() const -> FunctionPrototypeAst*;

  /// Whether the call is made "async".
  SPP_ATTR_NODISCARD auto IsAsync() const -> bool;

  SPP_ATTR_NODISCARD auto IsAllowedInDefault() const -> bool override;

private:
  struct _OInfo {
    Scope const *OverloadScope;
    FunctionPrototypeAst *Proto;

    /// What "Self" stands for at this call, decided with the
    /// overload ("PassedOverload::SelfId").
    analyse::scopes::TypeId SelfId;

    /// "PassedOverload::HasReceiver".
    bool HasReceiver;
  };

  Opt<_OInfo> _OverloadInfo;

  /// The return type "InferType" built, analysed, in the scope it
  /// was spelled in ("_InferredIn"), kept so it is built once there.
  Shared<TypeAst> _Inferred;
  Scope const *_InferredIn = nullptr;

  /// A value called through its function type (a closure, a "FunXxx" held in
  /// a variable), resolved once at stage 7: the function type it is
  /// called through, and how the call holds it ("FunRef" borrowed,
  /// "FunMut" borrowed mutably, "FunMov" taken). "Callee" is the
  /// value's own type, without its borrow, which lays out its
  /// "{ fn_ptr, env_ptr }"; "FnSup" the function type it superimposes,
  /// whose "Out" is what the value returns.
  struct _IndirectInfo {
    Shared<TypeAst const> FnType;
    ConventionTag Conv;
    TypeSymbol const *Callee;
    Shared<TypeRef> FnSup;
    bool Borrowed;
  };

  Opt<_IndirectInfo> _Indirect;

  /// A coroutine's return type, the generator a call to it builds,
  /// resolved at stage 7 where the overload is: its handle is
  /// read out of it at codegen. Null for a subroutine.
  TypeSymbol const *_CoroRetSym = nullptr;

  /// The borrow of the callee a "FunRef" or "FunMut" call makes, as
  /// an argument group of its own, checked against what the body
  /// already holds.
  Unique<FunctionCallArgumentGroupAst> _CalleeArgGroup;

  /// A fold's calls, one per element, each a whole call over
  /// "_FoldCallee" ("PostfixExpressionAst::NewOverAnalysedLhs").
  Vec<Unique<PostfixExpressionAst>> _FoldedAsts;

  /// What a fold's calls call: the left-hand side, or for a method
  /// on a receiver that is no place ("make().m(xs)..") the method on
  /// the temporary the receiver was evaluated into, once.
  Shared<ExpressionAst> _FoldCallee;

  /// The scope a fold's arguments are analysed in, as written: only
  /// to type them and find the tuples folded over, as each folded
  /// call analyses arguments of its own. Their scopes ("?", a block)
  /// are kept in this one, which the later stages skip as one.
  Scope *_FoldArgsScope = nullptr;

  /// What a fold evaluates once, in order, before its calls
  /// ("expr_utils::HoistOnce"): a receiver, then each argument that
  /// is no place, folded or not. Each call reads the temporaries.
  Vec<Unique<LetStatementInitializedAst>> _FoldTemps;

  /// A temporary in "_FoldTemps" its calls do not consume (a
  /// receiver or argument only borrowed, or any, when no call reads
  /// it) is consumed by the fold after them, as a linear value must
  /// be ("expr_utils::DropCall").
  Vec<Unique<PostfixExpressionAst>> _FoldDrops;

  /// A template's open packs this fold is over ("j(xs).."): one call is checked, on their first element, and the memory
  /// checks take each pack whole (pack-design.md phase 7).
  Vec<Unique<IdentifierAst>> _OpenFoldPacks;

  /// The type of a fold over an open pack: the open tuple of a hidden result pack ("packs::DeclareHiddenPack"), each
  /// element the type of the call checked, as long as the pack.
  Shared<TypeAst> _OpenFoldType;

  /// A call fold whose callee returns "Void": run for its effects, so the fold is "Void", not a tuple of "Void"s, and
  /// can stand as a statement ("drop(xs)..").
  bool _FoldIsVoid = false;

  /// Step the walk past "_FoldArgsScope", before the temporaries'
  /// and the folded calls' scopes (it was created before them).
  auto _SkipFoldArgsScope(ScopeManager *sm) const -> void;

  Ast *_IsAsync;
  bool _IsCoroAndAutoResume;

  auto _HandleFnFolding(
    ScopeManager *sm,
    CompilerMetaData *meta,
    Vec<Shared<IdentifierAst>> &unconsumed)
    -> Vec<Unique<PostfixExpressionAst>>;

  /// This call again with other arguments ("group"): a fold's call
  /// per element, a spread's call per length. Fresh, so nothing this
  /// call worked out (a fold, a resolved overload) is carried.
  SPP_ATTR_NODISCARD auto _CallWith(
    Unique<FunctionCallArgumentGroupAst> group) const
    -> Unique<PostfixExpressionOperatorFunctionCallAst>;

  /// A call spreading an open tuple ("f(..b)" in a template, "b" a
  /// variadic parameter) is not resolved itself: one call per pack
  /// length is, each with that many of the pack's elements ("b.0",
  /// ...), and one more with the pack's remainder still open, which
  /// only a variadic parameter takes. Every one must resolve, to one
  /// return type, which is this call's. Each instance, where the pack
  /// is a tuple, resolves the call as written.
  Vec<Unique<PostfixExpressionOperatorFunctionCallAst>> _OpenSpreadAsts;

  /// The return type the calls in "_OpenSpreadAsts" agree on.
  Shared<TypeAst> _OpenSpreadType;

  /// Set on the last call "_ResolveOpenSpread" builds, whose open
  /// remainder must reach a variadic parameter rather than being
  /// split per length again.
  bool _IsOpenSpreadRemainder = false;

  /// Resolve a call spreading an open tuple, as "_OpenSpreadAsts"
  /// describes. "max_params" is the most parameters (not "self", not
  /// variadic) any candidate has, which bounds the lengths tried.
  auto _ResolveOpenSpread(
    std::size_t spread_index,
    std::size_t max_params,
    ScopeManager *sm,
    CompilerMetaData *meta)
    -> void;
};

SPP_GCC_VTABLE_FIX_IMPL(spp::asts::PostfixExpressionOperatorFunctionCallAst)
