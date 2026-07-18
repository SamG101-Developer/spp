module;
#include <spp/macros.hpp>

export module spp.asts.generic_parameter_group_ast;
import spp.asts.ast;
import spp.codegen.llvm_ctx;
import spp.utils.types;
import llvm;
import std;

namespace spp::asts {
    SPP_EXP_CLS struct GenericParameterGroupAst;
    SPP_EXP_CLS struct GenericParameterAst;
    SPP_EXP_CLS struct GenericParameterCompAst;
    SPP_EXP_CLS struct GenericParameterTypeAst;
    SPP_EXP_CLS struct TokenAst;
}


SPP_EXP_CLS struct spp::asts::GenericParameterGroupAst final : Ast {
    /**
     * The token that represents the left bracket @code [@endcode in the generic parameter group. This introduces the
     * generic parameter group.
     */
    Unique<TokenAst> TokL;

    /**
     * The list of parameters in the generic parameter group. This can contain both required and optional parameters.
     */
    Vec<Unique<GenericParameterAst>> Params;

    /**
     * The token that represents the right bracket @code ]@endcode in the generic parameter group. This closes the
     * generic parameter group.
     */
    Unique<TokenAst> TokR;

    static auto NewEmpty() -> Unique<GenericParameterGroupAst>;

    static auto NewEmptyShared() -> Shared<GenericParameterGroupAst>;

    /**
     * Construct the GenericParameterGroupAst with the arguments matching the members.
     * @param tok_l The token that represents the left bracket @code [@endcode in the generic parameter group.
     * @param params The list of parameters in the generic parameter group.
     * @param tok_r The token that represents the right bracket @code ]@endcode in the generic parameter group.
     */
    GenericParameterGroupAst(
        decltype(TokL) &&tok_l,
        decltype(Params) &&params,
        decltype(TokR) &&tok_r);

    ~GenericParameterGroupAst() override;

    auto operator+(GenericParameterGroupAst const &other) const -> Unique<GenericParameterGroupAst>;

    auto operator+=(GenericParameterGroupAst const &other) -> GenericParameterGroupAst&;

    SPP_AST_KEY_FUNCTIONS;

    auto Stage2_GenTopLvlScopes(ScopeManager *sm, CompilerMetaData *meta) -> void override;

    auto Stage4_QualifyTypes(ScopeManager *sm, CompilerMetaData *meta) -> void override;

    auto Stage7_AnalyseSemantics(ScopeManager *sm, CompilerMetaData *meta) -> void override;

    auto Stage8_CheckMemory(ScopeManager *sm, CompilerMetaData *meta) -> void override;

    auto Stage11_CodeGen(ScopeManager *sm, CompilerMetaData *meta, codegen::LLvmCtx *ctx) -> llvm::Value* override;

    auto MergeGenerics(decltype(Params) &&other_params) -> void;

    SPP_ATTR_NODISCARD auto GetRequiredParams() const -> Vec<GenericParameterAst*>;

    SPP_ATTR_NODISCARD auto GetOptionalParams() const -> Vec<GenericParameterAst*>;

    SPP_ATTR_NODISCARD auto GetVariadicParams() const -> GenericParameterAst*;

    SPP_ATTR_NODISCARD auto GetCompParams() const -> Vec<GenericParameterCompAst*>;

    SPP_ATTR_NODISCARD auto GetTypeParams() const -> Vec<GenericParameterTypeAst*>;

    SPP_ATTR_NODISCARD auto GetAllParams() const -> Vec<GenericParameterAst*>;

    SPP_ATTR_NODISCARD auto OptToReq() const -> Unique<GenericParameterGroupAst>;
};
