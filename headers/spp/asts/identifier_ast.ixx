module;
#include <spp/macros.hpp>

export module spp.asts.identifier_ast;
import spp.asts.primary_expression_ast;
import spp.codegen.llvm_ctx;
import spp.utils.types;
import llvm;
import std;

namespace spp::asts {
    SPP_EXP_CLS struct IdentifierAst;
    SPP_EXP_CLS struct TypeAst;
}

SPP_EXP_CLS struct spp::asts::IdentifierAst final : PrimaryExpressionAst, EnableLocalSharedFromThis<IdentifierAst> {
    SPP_GCC_VTABLE_FIX

    Str Val;

    static auto FromType(TypeAst const &val) -> Unique<IdentifierAst>;
    explicit IdentifierAst(std::size_t pos, decltype(Val) val);
    ~IdentifierAst() override;

    auto operator<=>(IdentifierAst const &that) const -> Ordering;
    auto operator==(IdentifierAst const &that) const -> bool;
    auto operator==(ExpressionAst const &that) const -> bool;
    auto operator+(IdentifierAst const &that) const -> IdentifierAst;
    auto operator+(Str const &that) const -> IdentifierAst;

    SPP_ATTR_NODISCARD auto EqualsIdentifier(IdentifierAst const &) const -> Ordering override;

    SPP_ATTR_NODISCARD auto Equals(ExpressionAst const &other) const -> Ordering override;

    SPP_AST_KEY_FUNCTIONS;

    auto Stage7_AnalyseSemantics(ScopeManager *sm, CompilerMetaData *meta) -> void override;

    auto Stage9_CompTimeResolve(ScopeManager *sm, CompilerMetaData *meta) -> void override;

    auto Stage11_CodeGen(ScopeManager *sm, CompilerMetaData *meta, codegen::LLvmCtx *ctx) -> llvm::Value* override;

    auto InferType(ScopeManager *sm, CompilerMetaData *meta) -> Shared<TypeAst> override;

    auto ToFuncIdentifier() const -> Unique<IdentifierAst>;

    auto AnkerlHash() const -> std::size_t override;

    SPP_ATTR_NODISCARD auto ExprParts() const -> Vec<Ast*> override;

    SPP_ATTR_NODISCARD auto ToView() const noexcept -> StrView;

private:
    std::size_t _Pos;
};

SPP_GCC_VTABLE_FIX_IMPL(spp::asts::IdentifierAst)
