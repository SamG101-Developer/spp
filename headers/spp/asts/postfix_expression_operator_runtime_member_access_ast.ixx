module;
#include <spp/macros.hpp>

export module spp.asts.postfix_expression_operator_runtime_member_access_ast;
import spp.asts.ast_kind;
import spp.asts.postfix_expression_operator_ast;
import spp.codegen.llvm_ctx;
import spp.utils.interner;
import spp.utils.types;
import llvm;
import std;

use_common(PostfixExpressionOperatorRuntimeMemberAccessAst);
use(spp::asts, struct IdentifierAst);
use(spp::asts, struct PostfixExpressionAst);
use(spp::asts, struct TokenAst);
use(spp::analyse::scopes, struct TypeRef);
use(spp::analyse::scopes, struct TypeSymbol);
use(spp::analyse::scopes, struct VariableSymbol);
use(spp::asts, struct TypeAst);

SPP_EXP_CLS struct spp::asts::PostfixExpressionOperatorRuntimeMemberAccessAst final : PostfixExpressionOperatorAst {
  SPP_AST_KEY_FUNCTIONS(PostfixExpressionOperatorRuntimeMemberAccessAst);

  /// The "." token that indicates a runtime member access.
  Unique<TokenAst> TokDot;

  /// The identifier of the member being accessed, ie the name
  /// of the member in the class.
  Shared<IdentifierAst> Name;

  struct {
    Ast *OriginalExpr; // Original asts mapped into member accesses - (try? op for example)
    bool Lowered; // Built by a lowering (".res()", ".await") to call a method the user cannot: no visibility check
  } Source;

  explicit PostfixExpressionOperatorRuntimeMemberAccessAst(
    decltype(TokDot) &&tok_dot,
    decltype(Name) name);

  ~PostfixExpressionOperatorRuntimeMemberAccessAst() override;

  auto Stage7_AnalyseSemantics(ScopeManager *sm, CompilerMetaData *meta) -> void override;

  auto Stage11_CodeGen(ScopeManager *sm, CompilerMetaData *meta, codegen::LlvmCtx *ctx) -> llvm::Value* override;

  auto EvaluateCompTime(
    ScopeManager *sm, CompilerMetaData *meta) -> void override;

  auto InferType(
    ScopeManager *sm, CompilerMetaData *meta) -> Shared<TypeAst> override;

  auto InferTypeRef(
    ScopeManager *sm, CompilerMetaData *meta) -> TypeRef override;

  SPP_ATTR_NODISCARD auto ExprParts() const -> Vec<IdentifierAst*> override;

  /// The call that forwards the left-hand-side to the type this
  /// member was found on, ie the "x.fwd_ref()" of the mapped
  /// access, or "nullptr" when the member belongs to the
  /// left-hand-side's own type. A method call reached through
  /// forwarding needs it, because the forwarded-to value, not
  /// the forwarding object, is the "self" the method is
  /// invoked on.
  SPP_ATTR_NODISCARD auto GetFwdReceiver() const -> PostfixExpressionAst*;

  SPP_ATTR_NODISCARD auto IsAllowedInDefault() const -> bool override;

  /// Read off a temporary ("mk().a"), the rest of it is dropped as
  /// its statement ends, less this member if it is moved out (the
  /// path to it). Set by memory checking ("PostfixExpressionAst").
  Opt<Vec<Vec<spp::utils::InternedId>>> DropsReceiverRest;

  /// Another member is read off this one's value ("mk().a.b"), so
  /// this access is no end of a chain: the access at the end checks
  /// what the temporary is left missing ("PostfixExpressionAst").
  bool ReadOnwards = false;

  /// The value is only borrowed ("f(&mk().a)", or the receiver of a
  /// "&self" method) or discarded ("mk().a" as a statement): read off
  /// a temporary, nothing is moved out of it, so the temporary is
  /// dropped whole. Set by the call's argument check and by
  /// "expr_utils::DiscardValue".
  bool ReadByBorrow = false;

private:
  /// The access rewritten against the forwarded-to value
  /// ("x.fwd_ref().field"), built when the member is not found
  /// on the left-hand-side's own type but is reachable through
  /// a "FwdRef" / "FwdMut" superimposition. When set, inference
  /// and code generation both defer to it.
  Shared<PostfixExpressionAst> _MappedFwd;

  /// For a named member (not a forwarded one, nor ".0"): the type
  /// owning it, as the left-hand side's type, and the member as
  /// "." reaches it ("_Resolve"). Not cloned: a copy resolves for
  /// itself.
  TypeSymbol *_Owner = nullptr;
  VariableSymbol *_Member = nullptr;

  /// The named member, looked up once: Stage 7 sets it in its own
  /// scope, and anything asking before then resolves it here.
  auto _Resolve(ScopeManager *sm, CompilerMetaData *meta) -> VariableSymbol*;

  /// Stage 7 for an element read ("t.0"): the lhs is a tuple or an
  /// array, and the index within its bounds.
  auto _CheckIndexAccess(ScopeManager *sm, CompilerMetaData *meta) -> void;

  /// Stage 7 for a named member ("x.name"): found on the lhs's type,
  /// or reached by forwarding (rewritten, "_MappedFwd"), visible, and
  /// reached with "." (an attribute or a method, not a constant).
  auto _CheckNamedAccess(ScopeManager *sm, CompilerMetaData *meta) -> void;

  /// Stage 11: the address of the object the member is in (its lhs,
  /// whose type is "lhs_ref"), loaded out of a borrow, read off
  /// another access, or put in a temporary.
  auto _EmitBasePtr(
    analyse::scopes::TypeRef const &lhs_ref, Str const &uid, ScopeManager *sm, CompilerMetaData *meta,
    codegen::LlvmCtx *ctx) -> llvm::Value*;

  /// Stage 11: the member's address within the object at "base_ptr":
  /// a named field through the layout's field order, an element of a
  /// tuple or an array by its index.
  auto _EmitFieldPtr(
    analyse::scopes::TypeRef const &lhs_ref, llvm::Value *base_ptr, Str const &uid, ScopeManager *sm,
    codegen::LlvmCtx *ctx) -> llvm::Value*;
};
