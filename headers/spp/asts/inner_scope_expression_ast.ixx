module;
#include <spp/macros.hpp>

export module spp.asts.inner_scope_expression_ast;
import spp.asts.inner_scope_ast;
import spp.asts.primary_expression_ast;
import spp.codegen.llvm_ctx;
import llvm;
import std;

namespace spp::asts {
    SPP_EXP_CLS template <typename T>
    struct InnerScopeExpressionAst;
    SPP_EXP_CLS struct TypeAst;
}


SPP_EXP_CLS template <typename T>
struct spp::asts::InnerScopeExpressionAst final : InnerScopeAst<T>, PrimaryExpressionAst {
    using InnerScopeAst<T>::InnerScopeAst;

    auto clone() const -> std::unique_ptr<Ast> override;

    static auto new_empty() -> std::unique_ptr<InnerScopeExpressionAst>;

    auto stage_7_analyse_semantics(ScopeManager *sm, CompilerMetaData *meta) -> void override;

    auto stage_10_code_gen_2(ScopeManager *sm, CompilerMetaData *meta, codegen::LLvmCtx *ctx) -> llvm::Value* override;

    auto infer_type(ScopeManager *sm, CompilerMetaData *meta) -> std::shared_ptr<TypeAst> override;

    SPP_ATTR_NODISCARD auto terminates() const -> bool override;
};
