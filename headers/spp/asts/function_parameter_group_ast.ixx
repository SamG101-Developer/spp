module;
#include <spp/macros.hpp>

export module spp.asts.function_parameter_group_ast;
import spp.asts.ast;
import spp.codegen.llvm_ctx;
import spp.utils.types;
import llvm;
import std;

namespace spp::asts {
    SPP_EXP_CLS struct FunctionParameterGroupAst;
    SPP_EXP_CLS struct FunctionParameterAst;
    SPP_EXP_CLS struct FunctionParameterOptionalAst;
    SPP_EXP_CLS struct FunctionParameterRequiredAst;
    SPP_EXP_CLS struct FunctionParameterSelfAst;
    SPP_EXP_CLS struct FunctionParameterVariadicAst;
    SPP_EXP_CLS struct TokenAst;
}

COMMON_AST_IMPORTS

/**
 * A FunctionParameterGroupAst is used to represent a group of function parameters in a function prototype.
 */
SPP_EXP_CLS struct spp::asts::FunctionParameterGroupAst final : Ast {
    /**
     * The token that represents the left parenthesis @code (@endcode in the function parameter group. This introduces
     * the function parameter group.
     */
    Unique<TokenAst> TokL;

    /**
     * The list of parameters in the function parameter group. This can contain both required and optional parameters.
     */
    Vec<Unique<FunctionParameterAst>> Params;

    /**
     * The token that represents the right parenthesis @code )@endcode in the function parameter group. This closes
     * the function parameter group.
     */
    Unique<TokenAst> TokR;

    /**
     * Construct the FunctionParameterGroupAst with the arguments matching the members.
     * @param tok_l The token that represents the left parenthesis @code (@endcode in the function parameter group.
     * @param params The list of parameters in the function parameter group.
     * @param tok_r The token that represents the right parenthesis @code )@endcode in the function parameter group.
     */
    FunctionParameterGroupAst(
        decltype(TokL) &&tok_l,
        decltype(Params) &&params,
        decltype(TokR) &&tok_r);

    ~FunctionParameterGroupAst() override;

    SPP_AST_KEY_FUNCTIONS;

    auto Stage7_AnalyseSemantics(analyse::scopes::ScopeManager *sm, meta::CompilerMetaData *meta) -> void override;

    auto Stage8_CheckMemory(analyse::scopes::ScopeManager *sm, meta::CompilerMetaData *meta) -> void override;

    auto Stage11_CodeGen(analyse::scopes::ScopeManager *sm, meta::CompilerMetaData *meta, codegen::LLvmCtx *ctx) -> llvm::Value* override;

    SPP_ATTR_NODISCARD auto GetAllParams() const -> Vec<FunctionParameterAst*>;

    SPP_ATTR_NODISCARD auto GetSelfParam() const -> FunctionParameterSelfAst*;

    SPP_ATTR_NODISCARD auto GetRequiredParams() const -> Vec<FunctionParameterRequiredAst*>;

    SPP_ATTR_NODISCARD auto GetOptionalParams() const -> Vec<FunctionParameterOptionalAst*>;

    SPP_ATTR_NODISCARD auto GetVariadicParams() const -> FunctionParameterVariadicAst*;

    SPP_ATTR_NODISCARD auto GetNonSelfParams() const -> Vec<FunctionParameterAst*>;
};
