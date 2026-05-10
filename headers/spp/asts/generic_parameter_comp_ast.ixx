module;
#include <spp/macros.hpp>

export module spp.asts.generic_parameter_comp_ast;
import spp.asts.generic_parameter_ast;
import spp.asts.utils.orderable;
import spp.codegen.llvm_ctx;
import spp.utils.types;
import llvm;
import std;

namespace spp::asts {
    SPP_EXP_CLS struct GenericParameterCompAst;
    SPP_EXP_CLS struct TokenAst;
    SPP_EXP_CLS struct TypeAst;
}

SPP_EXP_CLS struct spp::asts::GenericParameterCompAst : GenericParameterAst {
    SPP_GCC_VTABLE_FIX

    /**
     * The @c cmp token that represents the generic comp parameter. This is used to indicate that the parameter is a
     * comp generic and not a type generic.
     */
    Unique<TokenAst> TokCmp;

    /**
     * The token that represents the @code :@endcode colon in the generic parameter. This separates the parameter name
     * from the type.
     */
    Unique<TokenAst> TokColon;

    /**
     * The type of the parameter. This is used to specify the type of the generic comp parameter, such as @c I32 or
     * @c F64 . This is a required field, as the type of the parameter must be known at compile time.
     */
    Shared<TypeAst> Type;

    struct {
        Shared<TypeAst> OriginalType;
    } Source;

    /**
     * Construct the GenericParameterCompAst with the arguments matching the members.
     * @param tok_cmp The @c cmp token that represents the generic comp parameter.
     * @param name The value of the generic comp parameter.
     * @param tok_colon The token that represents the @c : colon in the generic parameter.
     * @param type The type of the parameter.
     * @param order_tag The order tag for this generic parameter, used to enforce ordering rules.
     */
    GenericParameterCompAst(
        decltype(TokCmp) &&tok_cmp,
        decltype(Name) name,
        decltype(TokColon) &&tok_colon,
        decltype(Type) type,
        utils::OrderableTag order_tag);

    ~GenericParameterCompAst() override;

    auto Stage2_GenTopLvlScopes(ScopeManager *sm, CompilerMetaData *) -> void override;

    auto Stage4_QualifyTypes(ScopeManager *sm, CompilerMetaData *meta) -> void override;

    auto Stage7_AnalyseSemantics(ScopeManager *sm, CompilerMetaData *meta) -> void override;

    auto Stage11_CodeGen(ScopeManager *sm, CompilerMetaData *meta, codegen::LLvmCtx *ctx) -> llvm::Value* override;
};

SPP_GCC_VTABLE_FIX_IMPL(spp::asts::GenericParameterCompAst)
