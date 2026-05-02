module;
#include <spp/macros.hpp>

export module spp.asts.let_statement_uninitialized_ast;
import spp.asts.let_statement_ast;
import spp.codegen.llvm_ctx;
import spp.utils.types;
import llvm;
import std;

namespace spp::asts {
    SPP_EXP_CLS struct LetStatementUninitializedAst;
    SPP_EXP_CLS struct LocalVariableAst;
    SPP_EXP_CLS struct TokenAst;
    SPP_EXP_CLS struct TypeAst;
}


SPP_EXP_CLS struct spp::asts::LetStatementUninitializedAst final : LetStatementAst {
    /**
     * The @c let token that starts this statement. It is used to indicate the beginning of a let statement.
     */
    Unique<TokenAst> TokLet;

    /**
     * The variable that is being declared in the let statement. This names the symbols that will be created in the
     * scope that the @c let statement is defined in.
     */
    Unique<LocalVariableAst> Var;

    /**
     * The @c : token that indicates the type of the variable. This separates the variable name from its type in the
     * @c let statement.
     */
    Unique<TokenAst> TokColon;

    /**
     * The type of the uninitialized variable. This is used to check that values being assigned to the variable in the
     * future are of the correct type.
     */
    Shared<TypeAst> Type;

    /**
     * Construct the LetStatementUninitializedAst with the arguments matching the members.
     * @param[in] tok_let The @c let token that starts this statement.
     * @param[in] var The variable that is being declared in the let statement.
     * @param[in] tok_colon The @c : token that indicates the type of the variable.
     * @param[in] type The type of the uninitialized variable.
     */
    LetStatementUninitializedAst(
        decltype(TokLet) &&tok_let,
        decltype(Var) &&var,
        decltype(TokColon) &&tok_colon,
        decltype(Type) type);

    ~LetStatementUninitializedAst() override;

    SPP_AST_KEY_FUNCTIONS;

    auto Stage7_AnalyseSemantics(ScopeManager *sm, CompilerMetaData *meta) -> void override;

    auto Stage8_CheckMemory(ScopeManager *sm, CompilerMetaData *meta) -> void override;

    auto Stage11_CodeGen(ScopeManager *sm, CompilerMetaData *meta, codegen::LLvmCtx *ctx) -> llvm::Value* override;
};
