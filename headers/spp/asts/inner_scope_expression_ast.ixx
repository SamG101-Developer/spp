module;
#include <spp/macros.hpp>

export module spp.asts.inner_scope_expression_ast;
import spp.asts.inner_scope_ast;
import spp.asts.primary_expression_ast;
import std;

namespace spp::asts {
    SPP_EXP_CLS template <typename T>
    struct InnerScopeExpressionAst;
}


SPP_EXP_CLS template <typename T>
struct spp::asts::InnerScopeExpressionAst final : InnerScopeAst<T>, PrimaryExpressionAst {
    using InnerScopeAst<T>::InnerScopeAst;
    ~InnerScopeExpressionAst() override;
};
