module;
#include <spp/macros.hpp>

export module spp.asts.let_statement_initialized_ast;
import spp.asts.let_statement_ast;
import spp.asts.mixins.compiler_stages;
import spp.codegen.llvm_ctx;
import spp.utils.types;
import llvm;
import std;

namespace spp::asts {
    SPP_EXP_CLS struct ExpressionAst;
    SPP_EXP_CLS struct LetStatementInitializedAst;
    SPP_EXP_CLS struct LocalVariableAst;
    SPP_EXP_CLS struct TokenAst;
    SPP_EXP_CLS struct TypeAst;
}

COMMON_AST_IMPORTS

SPP_EXP_CLS struct spp::asts::LetStatementInitializedAst final : LetStatementAst {
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
     * The optionally provided type of the variable. Variable type's can always be inferred from their value, but
     * providing the type allows for variant types to be used with values of an inner type.
     */
    Unique<TypeAst> Type;

    /**
     * The @c = token that indicates the assignment of a value to the variable. This is used to indicate that the
     * variable is being initialized with a value.
     */
    Unique<TokenAst> TokAssign;

    /**
     * The value that is being assigned to the variable. This is the expression that will be evaluated and assigned to
     * the variable.
     */
    Unique<ExpressionAst> Val;

    struct {
        Unique<TypeAst> OriginalType;
    } Source;

    /**
     * Construct the LetStatementInitializedAst with the arguments matching the members.
     * @param tok_let The @c let token that starts this statement.
     * @param var The variable that is being declared in the let statement.
     * @param type The optionally provided type of the variable.
     * @param tok_assign The @c = token that indicates the assignment of a value to the variable.
     * @param val The value that is being assigned to the variable.
     */
    LetStatementInitializedAst(
        decltype(TokLet) &&tok_let,
        decltype(Var) &&var,
        decltype(Type) type,
        decltype(TokAssign) &&tok_assign,
        decltype(Val) &&val);

    ~LetStatementInitializedAst() override;

    SPP_AST_KEY_FUNCTIONS;

    auto Stage7_AnalyseSemantics(analyse::scopes::ScopeManager *sm, meta::CompilerMetaData *meta) -> void override;

    auto Stage8_CheckMemory(analyse::scopes::ScopeManager *sm, meta::CompilerMetaData *meta) -> void override;

    auto Stage9_CompTimeResolve(analyse::scopes::ScopeManager *sm, meta::CompilerMetaData *meta) -> void override;

    auto Stage11_CodeGen(analyse::scopes::ScopeManager *sm, meta::CompilerMetaData *meta, codegen::LLvmCtx *ctx) -> llvm::Value* override;
};
