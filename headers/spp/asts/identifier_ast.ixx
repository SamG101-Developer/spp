module;
#include <spp/macros.hpp>

export module spp.asts.identifier_ast;
import spp.asts.primary_expression_ast;
import spp.codegen.llvm_ctx;
import llvm;
import std;

namespace spp::asts {
    SPP_EXP_CLS struct IdentifierAst;
    SPP_EXP_CLS struct TypeAst;
}


SPP_EXP_CLS struct spp::asts::IdentifierAst final : PrimaryExpressionAst, std::enable_shared_from_this<IdentifierAst> {
    SPP_GCC_VTABLE_FIX

    std::string val;

    static auto from_type(
        TypeAst const &val)
        -> std::unique_ptr<IdentifierAst>;

    explicit IdentifierAst(
        std::size_t pos,
        decltype(val) val);

    ~IdentifierAst() override;

    auto operator<=>(
        IdentifierAst const &that) const
        -> std::strong_ordering;

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
        std::string const &that) const
        -> IdentifierAst;

    SPP_ATTR_NODISCARD auto equals_identifier(IdentifierAst const &) const -> std::strong_ordering override;

    SPP_ATTR_NODISCARD auto equals(ExpressionAst const &other) const -> std::strong_ordering override;

    SPP_AST_KEY_FUNCTIONS;

    auto stage_7_analyse_semantics(ScopeManager *sm, CompilerMetaData *meta) -> void override;

    auto stage_9_comptime_resolution(ScopeManager *sm, CompilerMetaData *meta) -> void override;

    auto stage_11_code_gen_2(ScopeManager *sm, CompilerMetaData *meta, codegen::LLvmCtx *ctx) -> llvm::Value* override;

    auto infer_type(ScopeManager *sm, CompilerMetaData *meta) -> std::shared_ptr<TypeAst> override;

    auto to_function_identifier() const -> std::unique_ptr<IdentifierAst>;

    auto ankerl_hash() const -> std::size_t override;

    SPP_ATTR_NODISCARD auto expr_parts() const -> std::vector<Ast*> override;

    SPP_ATTR_NODISCARD auto to_string_view() const noexcept -> std::string_view;

private:
    std::size_t m_pos;
};


SPP_GCC_VTABLE_FIX_IMPL(spp::asts::IdentifierAst)
