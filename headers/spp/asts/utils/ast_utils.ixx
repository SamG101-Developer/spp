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

  SPP_EXP_FUN
  template <typename T>
  SPP_ATTR_ALWAYS_INLINE
  inline auto AstClone(Unique<T> const &ast) -> Unique<std::remove_cvref_t<T>> {
    if (ast == nullptr) { return nullptr; }
    using ResultT = std::remove_cvref_t<T>;
    return Unique<ResultT>(dynamic_cast<ResultT*>(ast->Clone().release()));
  }

  SPP_EXP_FUN
  template <typename T>

  SPP_ATTR_ALWAYS_INLINE
  inline auto AstClone(Shared<T> const &ast) -> Unique<std::remove_cvref_t<T>> {
    if (ast == nullptr) { return nullptr; }
    using ResultT = std::remove_cvref_t<T>;
    return Unique<ResultT>(dynamic_cast<ResultT*>(ast->Clone().release()));
  }

  SPP_EXP_FUN
  template <typename T>

  SPP_ATTR_ALWAYS_INLINE
  inline auto AstClone(T *ast) -> Unique<std::remove_cvref_t<T>> {
    if (ast == nullptr) { return nullptr; }
    using ResultT = std::remove_cvref_t<T>;
    return Unique<ResultT>(dynamic_cast<ResultT*>(ast->Clone().release()));
  }

  SPP_EXP_FUN
  template <typename T>

  SPP_ATTR_ALWAYS_INLINE
  inline auto AstCloneShared(Shared<T> const &ast) -> Shared<std::remove_cvref_t<T>> {
    if (ast == nullptr) { return nullptr; }
    using ResultT = std::remove_cvref_t<T>;
    return Shared<ResultT>(dynamic_cast<ResultT*>(ast->Clone().release()));
  }

  SPP_EXP_FUN
  template <typename T>

  SPP_ATTR_ALWAYS_INLINE
  inline auto AstCloneShared(T *ast) -> Shared<std::remove_cvref_t<T>> {
    if (ast == nullptr) { return nullptr; }
    using ResultT = std::remove_cvref_t<T>;
    return Shared<ResultT>(dynamic_cast<ResultT*>(ast->Clone().release()));
  }

  SPP_EXP_FUN
  template <typename T>

  SPP_ATTR_ALWAYS_INLINE
  inline auto AstCloneVec(Vec<T*> const &asts) -> Vec<Unique<T>> {
    Vec<Unique<T>> cloned_asts;
    cloned_asts.Reserve(asts.Len());
    for (auto const *x : asts) {
      cloned_asts.EmplaceBack(AstClone(x));
    }
    return cloned_asts;
  }

  SPP_EXP_FUN
  template <typename U, typename T>

  SPP_ATTR_ALWAYS_INLINE
  inline auto AstCloneVec(Vec<T*> const &asts) -> Vec<Unique<U>> {
    Vec<Unique<U>> cloned_asts;
    cloned_asts.Reserve(asts.Len());
    for (auto const *x : asts) {
      cloned_asts.EmplaceBack(AstClone(dynamic_cast<U const*>(x)));
    }
    return cloned_asts;
  }

  SPP_EXP_FUN
  template <typename T>

  SPP_ATTR_ALWAYS_INLINE
  inline auto AstCloneVec(Vec<Unique<T>> const &asts) -> Vec<Unique<T>> {
    Vec<Unique<T>> cloned_asts;
    cloned_asts.Reserve(asts.Len());
    for (auto const &x : asts) {
      cloned_asts.EmplaceBack(AstClone(x));
    }
    return cloned_asts;
  }

  SPP_EXP_FUN
  template <typename T>

  SPP_ATTR_ALWAYS_INLINE
  inline auto AstCloneVec(Vec<Shared<T>> const &asts) -> Vec<Unique<T>> {
    Vec<Unique<T>> cloned_asts;
    cloned_asts.Reserve(asts.Len());
    for (auto const &x : asts) {
      cloned_asts.EmplaceBack(AstClone(x));
    }
    return cloned_asts;
  }

  SPP_EXP_FUN
  template <typename T>

  SPP_ATTR_ALWAYS_INLINE
  inline auto AstCloneVecShared(Vec<Shared<T>> const &asts) -> Vec<Shared<T>> {
    Vec<Shared<T>> cloned_asts;
    cloned_asts.reserve(asts.Len());
    for (auto const &x : asts) {
      cloned_asts.EmplaceBack(x);
    }
    return cloned_asts;
  }

  SPP_EXP_FUN auto AstNameOrNull(Ast *ast) -> Shared<TypeAst>;

  SPP_EXP_FUN auto AstName(Ast *ast) -> Shared<TypeAst>;

  SPP_EXP_FUN auto AstBody(Ast *ast) -> Vec<Ast*>;

  /**
   * Check if an AST is a runtime member access ("a.b"), that is a @c PostfixExpressionAst whose operator is a
   * @c PostfixExpressionOperatorRuntimeMemberAccessAst. Code generation uses this to know that an expression can
   * produce the address of what it names, rather than only a value.
   * @param[in] ast The AST to check.
   * @return If the AST is a runtime member access expression.
   */
  SPP_EXP_FUN auto IsRuntimeMemberAccess(Ast const *ast) -> bool;

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
