module;
#include <spp/macros.hpp>

export module spp.asts.inner_scope_ast;
import spp.asts.ast;
import std;

namespace spp::asts {
    SPP_EXP_CLS template <typename T>
    struct InnerScopeAst;
    SPP_EXP_CLS struct TokenAst;
}


/**
 * The InnerScopeAst class represents an inner scope in the abstract syntax tree. It is used to define a scope for top
 * level structure implementations (classes, functions, etc.). Bodies of expression asts, such as the @c case or @c loop
 * structures use the InnerScopeExpressionAst class, which is a specialization of this class, with support for type
 * inference etc.
 * @tparam T The type of the members in the inner scope.
 */
SPP_EXP_CLS template <typename T>
struct spp::asts::InnerScopeAst : Ast {
    std::vector<T> members;

    InnerScopeAst(
        decltype(members) &&members);
    ~InnerScopeAst() override;
    auto to_rust() const -> std::string override;
};
