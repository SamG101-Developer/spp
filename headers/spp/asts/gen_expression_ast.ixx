module;
#include <spp/macros.hpp>

export module spp.asts.gen_expression_ast;
import spp.asts.primary_expression_ast;
import spp.asts.mixins.compiler_stages;
import spp.codegen.llvm_ctx;
import spp.utils.types;
import llvm;
import std;

namespace spp::asts {
    SPP_EXP_CLS struct ConventionAst;
    SPP_EXP_CLS struct GenExpressionAst;
    SPP_EXP_CLS struct TokenAst;
    SPP_EXP_CLS struct TypeAst;
}

COMMON_AST_IMPORTS

/**
 * The GenExpressionAst represents a value being yielded out of a coroutine. A convention can be applied to the value,
 * to create foundational structures like iterators.
 */
SPP_EXP_CLS struct spp::asts::GenExpressionAst final : PrimaryExpressionAst {
    /**
     * The token that represents a generation point. This is the @c gen keyword in the source code, which indicates that
     * the coroutine is suspending its execution and yielding a value.
     */
    Unique<TokenAst> TokGen;

    /**
     * An optional convention that can be applied to the value being yielded. This allows for additional behaviour to be
     * attached to the value, such as making it an iterator.
     */
    Unique<ConventionAst> Conv;

    /**
     * The expression that is being yielded out of the coroutine. This is the value that will be returned when the
     * coroutine is resumed.
     */
    Unique<ExpressionAst> Expr;

    /**
     * Construct the GenExpressionAst with the arguments matching the members.
     * @param tok_gen The token that represents the @c gen keyword in the source code.
     * @param conv The optional convention that can be applied to the value being yielded.
     * @param expr The expression that is being yielded out of the coroutine.
     */
    GenExpressionAst(
        decltype(TokGen) &&tok_gen,
        decltype(Conv) &&conv,
        decltype(Expr) &&expr);

    ~GenExpressionAst() override;

    SPP_AST_KEY_FUNCTIONS;

    auto Stage7_AnalyseSemantics(analyse::scopes::ScopeManager *sm, meta::CompilerMetaData *meta) -> void override;

    auto Stage8_CheckMemory(analyse::scopes::ScopeManager *sm, meta::CompilerMetaData *meta) -> void override;

    auto Stage11_CodeGen(analyse::scopes::ScopeManager *sm, meta::CompilerMetaData *meta, codegen::LLvmCtx *ctx) -> llvm::Value* override;

    auto InferType(analyse::scopes::ScopeManager *sm, meta::CompilerMetaData *meta) -> TypeAst* override;

private:
    Unique<TypeAst> _GenType;
};
