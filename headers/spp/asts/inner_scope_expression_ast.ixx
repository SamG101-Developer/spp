module;
#include <spp/macros.hpp>

export module spp.asts.inner_scope_expression_ast;
import spp.asts.inner_scope_ast;
import spp.asts.primary_expression_ast;
import spp.codegen.llvm_ctx;
import spp.utils.types;
import llvm;
import std;

namespace spp::asts {
    SPP_EXP_CLS struct Ast;
    SPP_EXP_CLS struct InnerScopeExpressionAst;
    SPP_EXP_CLS struct TypeAst;
}

COMMON_AST_IMPORTS

SPP_EXP_CLS struct spp::asts::InnerScopeExpressionAst : PrimaryExpressionAst {
    /**
     * The @c { token that represents the start of the inner scope. This is used to indicate the beginning of the scope
     * and is typically followed by a list of members or statements that belong to this scope.
     */
    Unique<TokenAst> TokL;

    /**
     * The list of members in the inner scope. They are all the @c T type, or a derived type. This allows for a flexible
     * structure where different types of ASTs can be included in the same inner scope, as long as they derive from
     * a common base class.
     */
    Vec<Unique<StatementAst>> Members;

    /**
     * The @c } token that represents the end of the inner scope. This is used to indicate the end of the scope after
     * all the members or statements have been defined.
     */
    Unique<TokenAst> TokR;

    static auto NewEmpty() -> Unique<InnerScopeExpressionAst>;

    InnerScopeExpressionAst(
        decltype(TokL) &&tok_l,
        decltype(Members) &&members,
        decltype(TokR) &&tok_r);

    ~InnerScopeExpressionAst() override;

    SPP_AST_KEY_FUNCTIONS;

    auto Stage7_AnalyseSemantics(analyse::scopes::ScopeManager *sm, meta::CompilerMetaData *meta) -> void override;

    auto Stage8_CheckMemory(analyse::scopes::ScopeManager *sm, meta::CompilerMetaData *meta) -> void override;

    auto Stage9_CompTimeResolve(analyse::scopes::ScopeManager *sm, meta::CompilerMetaData *meta) -> void override;

    auto Stage11_CodeGen(analyse::scopes::ScopeManager *sm, meta::CompilerMetaData *meta, codegen::LLvmCtx *ctx) -> llvm::Value* override;

    auto InferType(analyse::scopes::ScopeManager *sm, meta::CompilerMetaData *meta) -> TypeAst* override;

    SPP_ATTR_NODISCARD auto Terminates() const -> bool override;

    SPP_ATTR_NODISCARD auto FinalMember() const -> Ast*;
};
