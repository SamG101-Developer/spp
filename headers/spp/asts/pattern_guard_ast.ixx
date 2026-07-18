module;
#include <spp/macros.hpp>

export module spp.asts.pattern_guard_ast;
import spp.asts.ast;
import spp.codegen.llvm_ctx;
import spp.utils.types;
import llvm;
import std;

namespace spp::asts {
    SPP_EXP_CLS struct ExpressionAst;
    SPP_EXP_CLS struct PatternGuardAst;
    SPP_EXP_CLS struct TokenAst;
}


SPP_EXP_CLS struct spp::asts::PatternGuardAst final : Ast {
    /**
     * The @c and keyword token. This is used to indicate that the pattern guard is being introduced, following a
     * pattern.
     */
    Unique<TokenAst> TokAnd;

    /**
     * The expression that is used as the guard for the pattern. This expression is evaluated to determine if the
     * pattern matches, and must be a boolean expression.
     */
    Unique<ExpressionAst> Expr;

    /**
     * Constructor for the @c PatternGuardAst.
     * @param tok_and The @c and keyword token.
     * @param expression The expression that is used as the guard for the pattern.
     */
    PatternGuardAst(
        decltype(TokAnd) &&tok_and,
        decltype(Expr) &&expression);

    ~PatternGuardAst() override;

    SPP_AST_KEY_FUNCTIONS;

    auto Stage7_AnalyseSemantics(ScopeManager *sm, CompilerMetaData *meta) -> void override;

    auto Stage8_CheckMemory(ScopeManager *sm, CompilerMetaData *meta) -> void override;

    auto Stage9_CompTimeResolve(ScopeManager *sm, CompilerMetaData *meta) -> void override;

    auto Stage11_CodeGen(ScopeManager *sm, CompilerMetaData *meta, codegen::LLvmCtx *ctx) -> llvm::Value* override;
};
