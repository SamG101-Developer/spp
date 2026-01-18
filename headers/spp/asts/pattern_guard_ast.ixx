module;
#include <spp/macros.hpp>

export module spp.asts.pattern_guard_ast;
import spp.asts.ast;
import spp.codegen.llvm_ctx;
import llvm;
import std;

namespace spp::asts {
    SPP_EXP_CLS struct ExpressionAst;
    SPP_EXP_CLS struct PatternGuardAst;
    SPP_EXP_CLS struct TokenAst;
}


SPP_EXP_CLS struct spp::asts::PatternGuardAst final : virtual Ast {
    /**
     * The @c and keyword token. This is used to indicate that the pattern guard is being introduced, following a
     * pattern.
     */
    std::unique_ptr<TokenAst> tok_and;

    /**
     * The expression that is used as the guard for the pattern. This expression is evaluated to determine if the
     * pattern matches, and must be a boolean expression.
     */
    std::unique_ptr<ExpressionAst> expr;

    /**
     * Constructor for the @c PatternGuardAst.
     * @param tok_and The @c and keyword token.
     * @param expression The expression that is used as the guard for the pattern.
     */
    PatternGuardAst(
        decltype(tok_and) &&tok_and,
        decltype(expr) &&expression);

    ~PatternGuardAst() override;

    SPP_AST_KEY_FUNCTIONS;

    auto stage_7_analyse_semantics(ScopeManager *sm, CompilerMetaData *meta) -> void override;

    auto stage_8_check_memory(ScopeManager *sm, CompilerMetaData *meta) -> void override;

    auto stage_9_comptime_resolution(ScopeManager *sm, CompilerMetaData *meta) -> void override;

    auto stage_11_code_gen_2(ScopeManager *sm, CompilerMetaData *meta, codegen::LLvmCtx *ctx) -> llvm::Value* override;
};
