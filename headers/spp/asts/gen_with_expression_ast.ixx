module;
#include <spp/macros.hpp>

export module spp.asts.gen_with_expression_ast;
import spp.asts.primary_expression_ast;
import spp.codegen.llvm_ctx;
import spp.utils.types;
import llvm;
import std;

namespace spp::asts {
    SPP_EXP_CLS struct GenWithExpressionAst;
    SPP_EXP_CLS struct TokenAst;
    SPP_EXP_CLS struct TypeAst;
}


SPP_EXP_CLS struct spp::asts::GenWithExpressionAst final : PrimaryExpressionAst {
    /**
     * The token that represents a generation point. This is the @c gen keyword in the source code, which indicates that
     * the coroutine is suspending its execution and yielding a value.
     */
    Unique<TokenAst> TokGen;

    /**
     * The token that represents the @c with keyword in the source code. This indicates that the expression is being
     * generated with a specific context.
     */
    Unique<TokenAst> TokWith;

    /**
     * The expression that is being generated with the context. This is the value that will be used in the generation.
     */
    Unique<ExpressionAst> Expr;

    /**
     * Construct the GenWithExpressionAst with the arguments matching the members.
     * @param tok_gen The token that represents the @c gen keyword in the source code.
     * @param tok_with The token that represents the @c with keyword in the source code.
     * @param expr The expression that is being generated with the context.
     */
    GenWithExpressionAst(
        decltype(TokGen) &&tok_gen,
        decltype(TokWith) &&tok_with,
        decltype(Expr) &&expr);

    ~GenWithExpressionAst() override;

    SPP_AST_KEY_FUNCTIONS;

    auto Stage7_AnalyseSemantics(ScopeManager *sm, CompilerMetaData *meta) -> void override;

    auto Stage8_CheckMemory(ScopeManager *sm, CompilerMetaData *meta) -> void override;

    auto Stage11_CodeGen(ScopeManager *sm, CompilerMetaData *meta, codegen::LLvmCtx *ctx) -> llvm::Value* override;

    auto InferType(ScopeManager *sm, CompilerMetaData *meta) -> Shared<TypeAst> override;

private:
    Shared<TypeAst> _GenType;
};
