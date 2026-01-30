module;
#include <spp/macros.hpp>

export module spp.asts.generic_parameter_comp_ast;
import spp.asts.generic_parameter_ast;
import spp.asts.token_ast;
import spp.asts.utils.orderable;
import spp.codegen.llvm_ctx;
import llvm;
import std;

namespace spp::asts {
    SPP_EXP_CLS struct GenericParameterCompAst;
    SPP_EXP_CLS struct TypeAst;
}


SPP_EXP_CLS struct spp::asts::GenericParameterCompAst : GenericParameterAst {
    /**
     * The @c cmp token that represents the generic comp parameter. This is used to indicate that the parameter is a
     * comp generic and not a type generic.
     */
    std::unique_ptr<TokenAst> tok_cmp;

    /**
     * The token that represents the @code :@endcode colon in the generic parameter. This separates the parameter name
     * from the type.
     */
    std::unique_ptr<TokenAst> tok_colon;

    /**
     * The type of the parameter. This is used to specify the type of the generic comp parameter, such as @c I32 or
     * @c F64 . This is a required field, as the type of the parameter must be known at compile time.
     */
    std::shared_ptr<TypeAst> type;

    /**
     * Construct the GenericParameterCompAst with the arguments matching the members.
     * @param tok_cmp The @c cmp token that represents the generic comp parameter.
     * @param name The value of the generic comp parameter.
     * @param tok_colon The token that represents the @c : colon in the generic parameter.
     * @param type The type of the parameter.
     * @param order_tag The order tag for this generic parameter, used to enforce ordering rules.
     */
    GenericParameterCompAst(
        decltype(tok_cmp) &&tok_cmp,
        decltype(name) name,
        decltype(tok_colon) &&tok_colon,
        decltype(type) type,
        utils::OrderableTag order_tag);

    ~GenericParameterCompAst() override;

    auto stage_2_gen_top_level_scopes(ScopeManager *sm, CompilerMetaData *) -> void override;

    auto stage_4_qualify_types(ScopeManager *sm, CompilerMetaData *meta) -> void override;

    auto stage_7_analyse_semantics(ScopeManager *sm, CompilerMetaData *meta) -> void override;

    auto stage_11_code_gen_2(ScopeManager *sm, CompilerMetaData *meta, codegen::LLvmCtx *ctx) -> llvm::Value* override;
};


spp::asts::GenericParameterCompAst::~GenericParameterCompAst() = default;
