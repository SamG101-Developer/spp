module;
#include <spp/macros.hpp>

export module spp.asts.utils.ast_utils;
import spp.asts.ast;
import spp.utils.types;
import std;

namespace spp::asts {
  SPP_EXP_CLS struct Ast;
  SPP_EXP_CLS struct ExpressionAst;
  SPP_EXP_CLS struct IdentifierAst;
  SPP_EXP_CLS struct StatementAst;
  SPP_EXP_CLS struct TypeAst;

  /// A clone of a "T" is a "T", so an "Ast" result is a static
  /// cast; only a mixin, which is not an "Ast", needs the dynamic
  /// cross cast.
  template <typename T>
  SPP_ATTR_ALWAYS_INLINE
  inline auto CloneAs(Ast *clone) -> T* {
    if constexpr (std::derived_from<T, Ast>) { return static_cast<T*>(clone); }
    else { return dynamic_cast<T*>(clone); }
  }

  SPP_EXP_FUN template <typename T>
  SPP_ATTR_ALWAYS_INLINE
  inline auto AstClone(Unique<T> const &ast) -> Unique<std::remove_cvref_t<T>> {
    if (ast == nullptr) { return nullptr; }
    using ResultT = std::remove_cvref_t<T>;
    return Unique<ResultT>(CloneAs<ResultT>(ast->Clone().release()));
  }

  SPP_EXP_FUN template <typename T>
  SPP_ATTR_ALWAYS_INLINE
  inline auto AstClone(Shared<T> const &ast) -> Unique<std::remove_cvref_t<T>> {
    if (ast == nullptr) { return nullptr; }
    using ResultT = std::remove_cvref_t<T>;
    return Unique<ResultT>(CloneAs<ResultT>(ast->Clone().release()));
  }

  SPP_EXP_FUN template <typename T>
  SPP_ATTR_ALWAYS_INLINE
  inline auto AstClone(T *ast) -> Unique<std::remove_cvref_t<T>> {
    if (ast == nullptr) { return nullptr; }
    using ResultT = std::remove_cvref_t<T>;
    return Unique<ResultT>(CloneAs<ResultT>(ast->Clone().release()));
  }

  SPP_EXP_FUN template <typename T>
  SPP_ATTR_ALWAYS_INLINE
  inline auto AstCloneShared(Shared<T> const &ast) -> Shared<std::remove_cvref_t<T>> {
    if (ast == nullptr) { return nullptr; }
    using ResultT = std::remove_cvref_t<T>;
    return Shared<ResultT>(CloneAs<ResultT>(ast->Clone().release()));
  }

  SPP_EXP_FUN template <typename T>
  SPP_ATTR_ALWAYS_INLINE
  inline auto AstCloneShared(T *ast) -> Shared<std::remove_cvref_t<T>> {
    if (ast == nullptr) { return nullptr; }
    using ResultT = std::remove_cvref_t<T>;
    return Shared<ResultT>(CloneAs<ResultT>(ast->Clone().release()));
  }

  SPP_EXP_FUN template <typename T>
  SPP_ATTR_ALWAYS_INLINE
  inline auto AstCloneVec(Vec<T*> const &asts) -> Vec<Unique<T>> {
    Vec<Unique<T>> cloned_asts;
    cloned_asts.reserve(asts.size());
    for (auto const *x : asts) {
      cloned_asts.emplace_back(AstClone(x));
    }
    return cloned_asts;
  }

  SPP_EXP_FUN template <typename T>
  SPP_ATTR_ALWAYS_INLINE
  inline auto AstCloneVec(Vec<Unique<T>> const &asts) -> Vec<Unique<T>> {
    Vec<Unique<T>> cloned_asts;
    cloned_asts.reserve(asts.size());
    for (auto const &x : asts) {
      cloned_asts.emplace_back(AstClone(x));
    }
    return cloned_asts;
  }

  SPP_EXP_FUN template <typename T>
  SPP_ATTR_ALWAYS_INLINE
  inline auto AstCloneVec(Vec<Shared<T>> const &asts) -> Vec<Unique<T>> {
    Vec<Unique<T>> cloned_asts;
    cloned_asts.reserve(asts.size());
    for (auto const &x : asts) {
      cloned_asts.emplace_back(AstClone(x));
    }
    return cloned_asts;
  }

  SPP_EXP_FUN template <typename T>
  SPP_ATTR_ALWAYS_INLINE
  inline auto AstCloneVecShared(Vec<Shared<T>> const &asts) -> Vec<Shared<T>> {
    Vec<Shared<T>> cloned_asts;
    cloned_asts.reserve(asts.size());
    for (auto const &x : asts) {
      cloned_asts.emplace_back(x);
    }
    return cloned_asts;
  }

  SPP_EXP_FUN auto AstNameOrNull(Ast *ast) -> Shared<TypeAst>;

  SPP_EXP_FUN auto AstName(Ast *ast) -> Shared<TypeAst>;

  SPP_EXP_FUN auto AstBody(Ast *ast) -> Vec<Ast*>;

  /// Check if an expression is a postfix expression whose operator is a runtime
  /// member access. For example, "a.b" is.
  SPP_EXP_FUN auto IsRuntimeMemberAccess(Ast const *ast) -> bool;

  /// Check if an expression is a postfix expression whose operator is a static
  /// member access. For example, "Type::b" is.
  SPP_EXP_FUN auto IsStaticMemberAccess(Ast const *ast) -> bool;

  /// Check if an expression is a postfix expression whose operator is a deref
  /// operation. For example, "a@" is.
  SPP_EXP_FUN auto IsDeref(Ast const *ast) -> bool;

  /// The expression inside any parentheses around "ast": "(x)" is the place
  /// "x", the same storage (code generation reads it straight from "x").
  SPP_EXP_FUN auto Unparenthesised(Ast const *ast) -> Ast const*;

  /// Check if an expression is a postfix expression whose operator is a
  /// function call. For example, "a()" is.
  SPP_EXP_FUN auto IsFuncCall(Ast const *ast) -> bool;

  /// Check if an expression is a postfix expression whose operator is a not
  /// expression. For example, "a.not" is.
  SPP_EXP_FUN auto IsNotExpression(Ast const *ast) -> bool;

  /**
   * Bind an expression to a fresh local. @code let $uid = <expr>@endcode is appended to the prelude, and the expression
   * is replaced where it was by the local's name, so whatever held it now reads the local instead.
   * @param[in,out] slot The expression to bind, replaced by the local's name.
   * @param[in,out] prelude The statements the binding is appended to.
   * @param[in] pos The position given to the generated names.
   * @return A second copy of the local's name, for a capture list or similar.
   */
  SPP_EXP_FUN auto BindLocal(
    Unique<ExpressionAst> &slot,
    Vec<Unique<StatementAst>> &prelude,
    std::size_t pos)
    -> Unique<IdentifierAst>;
}
