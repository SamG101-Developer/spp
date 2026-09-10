module;
#include <spp/macros.hpp>

export module spp.asts.identifier_ast;
import spp.asts.ast_kind;
import spp.asts.primary_expression_ast;
import spp.codegen.llvm_ctx;
import spp.utils.interner;
import spp.utils.types;
import llvm;
import std;

namespace spp::asts {
  SPP_EXP_CLS struct GenericArgumentAst;
  SPP_EXP_CLS struct IdentifierAst;
  SPP_EXP_CLS struct TokenAst;
  SPP_EXP_CLS struct TypeAst;
}

SPP_EXP_CLS struct spp::asts::IdentifierAst final : PrimaryExpressionAst, EnableLocalSharedFromThis<IdentifierAst> {
  SPP_GCC_VTABLE_FIX
  SPP_AST_KEY_FUNCTIONS(IdentifierAst);

  Str Val;

  static auto FromType(
    TypeAst const &val)
    -> Unique<IdentifierAst>;

  explicit IdentifierAst(
    std::size_t pos,
    decltype(Val) val);

private:
  /**
   * Use the pre-known interned identifier if there is one, for edxample from a clone.
   */
  IdentifierAst(
    std::size_t pos,
    decltype(Val) val,
    utils::InternedId name_id);

public:

  static auto MappedFromTok(
    TokenAst const &tok,
    decltype(Val) val)
    -> Unique<IdentifierAst>;

  ~IdentifierAst() override;

  auto operator<=>(
    IdentifierAst const &that) const
    -> Ordering;

  auto operator==(
    IdentifierAst const &that) const
    -> bool;

  auto operator==(
    ExpressionAst const &that) const
    -> bool;

  auto operator+(
    IdentifierAst const &that) const
    -> IdentifierAst;

  auto operator+(
    Str const &that) const -> IdentifierAst;

  SPP_ATTR_NODISCARD auto EqualsIdentifier(
    IdentifierAst const &) const
    -> Ordering override;

  SPP_ATTR_NODISCARD auto Equals(
    ExpressionAst const &other) const
    -> Ordering override;

  auto Stage7_AnalyseSemantics(ScopeManager *sm, CompilerMetaData *meta) -> void override;

  auto Stage9_CompTimeResolve(ScopeManager *sm, CompilerMetaData *meta) -> void override;

  auto Stage11_CodeGen(ScopeManager *sm, CompilerMetaData *meta, codegen::LlvmCtx *ctx) -> llvm::Value* override;

  auto InferType(ScopeManager *sm, CompilerMetaData *meta)
    -> Shared<TypeAst> override;

  SPP_ATTR_NODISCARD auto ToFuncIdentifier() const
    -> Unique<IdentifierAst>;

  SPP_ATTR_NODISCARD auto AnkerlHash() const
    -> std::size_t override;

  SPP_ATTR_NODISCARD auto ExprParts() const
    -> Vec<IdentifierAst*> override;

  SPP_ATTR_NODISCARD auto SubstituteGenericsExpr(
    Vec<GenericArgumentAst*> const &args) const
    -> Shared<ExpressionAst> override;

  SPP_ATTR_NODISCARD auto ToView() const noexcept
    -> StrView;

  /**
   * The identifier's name as an interned id. @c Val never changes once the node is built, so the id is assigned in the
   * constructor and stands for the node's lifetime. Symbol tables key on this rather than on the string.
   */
  SPP_ATTR_NODISCARD SPP_ATTR_ALWAYS_INLINE SPP_ATTR_HOT auto NameId() const noexcept
    -> utils::InternedId { return _NameId; }

  SPP_ATTR_NODISCARD auto IsAllowedInDefault() const
    -> bool override;

private:
  std::size_t _Pos;
  std::size_t _ForTok;
  utils::InternedId _NameId;
};

SPP_GCC_VTABLE_FIX_IMPL(spp::asts::IdentifierAst)
