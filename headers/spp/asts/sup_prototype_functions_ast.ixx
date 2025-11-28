module;
#include <spp/macros.hpp>

export module spp.asts.sup_prototype_functions_ast;
import spp.asts.ast;
import spp.asts.module_member_ast;

import std;

namespace spp::asts {
    SPP_EXP_CLS struct GenericParameterGroupAst;
    SPP_EXP_CLS struct SupImplementationAst;
    SPP_EXP_CLS struct SupPrototypeFunctionsAst;
    SPP_EXP_CLS struct TokenAst;
    SPP_EXP_CLS struct TypeAst;
}


/**
 * The SupPrototypeFunctionsAst represents a superimposition of methods over a type. This is used to add behavior to a
 * type. For example, to extend the @c std::Str type with additional methods, the following code can be used:
 * @code
 * sup std::Str {
 *     fun to_upper() -> std::Str { ... }
 * }
 * @endcode
 */
SPP_EXP_CLS struct spp::asts::SupPrototypeFunctionsAst final : virtual Ast, ModuleMemberAst {
    /**
     * The @c sup keyword that represents the start of the superimposition. This is used to indicate that a type is
     * being extended with additional methods.
     */
    std::unique_ptr<TokenAst> tok_sup;

    /**
     * The generics available for this superimposition. This is used to superimpose over generic types (all generics
     * must be used by the type being extended).
     */
    std::unique_ptr<GenericParameterGroupAst> generic_param_group;

    /**
     * The name of the type that is being extended. This is the type that will gain the additional methods defined in
     * the body of this superimposition.
     */
    std::shared_ptr<TypeAst> name;

    /**
     * The body of the superimposition. This is a list of methods that are being added to the type. Each method is
     * defined as a FunctionPrototypeAst, which includes the method's name, parameters, and return type.
     */
    std::unique_ptr<SupImplementationAst> impl;

    /**
     * Construct the SupPrototypeFunctionsAst with the arguments matching the members.
     * @param tok_sup The @c sup keyword that represents the start of the superimposition.
     * @param generic_param_group The generics available for this superimposition.
     * @param name The name of the type that is being extended.
     * @param impl The body of the superimposition.
     */
    SupPrototypeFunctionsAst(
        decltype(tok_sup) &&tok_sup,
        decltype(generic_param_group) &&generic_param_group,
        decltype(name) &&name,
        decltype(impl) &&impl);

    ~SupPrototypeFunctionsAst() override;

    SPP_AST_KEY_FUNCTIONS;

    auto stage_1_pre_process(Ast *ctx) -> void override;

    auto stage_2_gen_top_level_scopes(ScopeManager *sm, CompilerMetaData *) -> void override;

    auto stage_3_gen_top_level_aliases(ScopeManager *sm, CompilerMetaData *meta) -> void override;

    auto stage_4_qualify_types(ScopeManager *sm, CompilerMetaData *meta) -> void override;

    auto stage_5_load_super_scopes(ScopeManager *sm, CompilerMetaData *meta) -> void override;

    auto stage_6_pre_analyse_semantics(ScopeManager *sm, CompilerMetaData *meta) -> void override;

    auto stage_7_analyse_semantics(ScopeManager *sm, CompilerMetaData *meta) -> void override;

    auto stage_8_check_memory(ScopeManager *sm, CompilerMetaData *meta) -> void override;
};
