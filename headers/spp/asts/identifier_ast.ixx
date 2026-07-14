module;
#include <spp/macros.hpp>

export module spp.asts.identifier_ast;
import spp.asts.primary_expression_ast;
import spp.asts.mixins.compiler_stages;
import spp.codegen.llvm_ctx;
import spp.utils.types;
import llvm;
import std;

namespace spp::asts {
    SPP_EXP_CLS struct IdentifierAst;
    SPP_EXP_CLS struct TokenAst;
    SPP_EXP_CLS struct TypeAst;
}

COMMON_AST_IMPORTS

SPP_EXP_CLS struct spp::asts::IdentifierAst final : PrimaryExpressionAst {
    SPP_GCC_VTABLE_FIX

    Str Val;

    static auto FromType(TypeAst const &val) -> IdentifierAst*;
    explicit IdentifierAst(std::size_t pos, decltype(Val) val);
    static auto MappedFromTok(TokenAst const &tok, decltype(Val) val) -> Unique<IdentifierAst>;
    ~IdentifierAst() override;

    auto operator<=>(IdentifierAst const &that) const -> Ordering;
    auto operator==(IdentifierAst const &that) const -> bool;
    auto operator==(ExpressionAst const &that) const -> bool;
    auto operator+(IdentifierAst const &that) const -> IdentifierAst;
    auto operator+(Str const &that) const -> IdentifierAst;

    SPP_ATTR_NODISCARD auto EqualsIdentifier(IdentifierAst const &) const -> Ordering override;

    SPP_ATTR_NODISCARD auto Equals(ExpressionAst const &other) const -> Ordering override;

    SPP_AST_KEY_FUNCTIONS;

    auto Stage7_AnalyseSemantics(analyse::scopes::ScopeManager *sm, meta::CompilerMetaData *meta) -> void override;

    auto Stage9_CompTimeResolve(analyse::scopes::ScopeManager *sm, meta::CompilerMetaData *meta) -> void override;

    auto Stage11_CodeGen(analyse::scopes::ScopeManager *sm, meta::CompilerMetaData *meta, codegen::LLvmCtx *ctx) -> llvm::Value* override;

    auto InferType(analyse::scopes::ScopeManager *sm, meta::CompilerMetaData *meta) -> TypeAst* override;

    auto ToFuncIdentifier() const -> Unique<IdentifierAst>;

    auto AnkerlHash() const -> std::size_t override;

    SPP_ATTR_NODISCARD auto ExprParts() const -> Vec<Ast*> override;

    SPP_ATTR_NODISCARD auto ToView() const noexcept -> StrView;

private:
    std::size_t _Pos;
    std::size_t _ForTok = 0;
};

SPP_GCC_VTABLE_FIX_IMPL(spp::asts::IdentifierAst)
