module;
#include <spp/macros.hpp>

export module spp.asts.sup_prototype_extension_ast;
import spp.analyse.scopes.scope;
import spp.analyse.scopes.scope_manager;
import spp.analyse.scopes.symbols;
import spp.asts.ast;
import spp.asts.module_member_ast;
import spp.asts.sup_member_ast;

import std;


/**
 * The SupPrototypeExtensionAst represents a superimposition of a type over a type. This is used to "inherit" a type.
 * For example, to extend the @c A type with @c B, the following code can be used:
 * @code
 * sup A ext B {
 *     # Override any abstract or virtual methods from B here.
 * }
 * @endcode
 */
SPP_EXP struct spp::asts::SupPrototypeExtensionAst final : virtual Ast, ModuleMemberAst, SupMemberAst {
    friend class spp::analyse::scopes::ScopeManager;
    friend struct spp::asts::FunctionPrototypeAst;

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
     * The @c ext keyword that represents the type that is being extended. This is used to indicate that an extension,
     * and a method block, is being defined.
     */
    std::unique_ptr<TokenAst> tok_ext;

    /**
     * The name of the super class that is this type is being extended from. The attributes and methods of this type
     * will now be available on the superimposed type.
     */
    std::shared_ptr<TypeAst> super_class;

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
     * @param tok_ext The @c ext keyword that represents the type that is being extended.
     * @param super_class The name of the super class that is this type is being extended from.
     * @param impl The body of the superimposition.
     */
    SupPrototypeExtensionAst(
        decltype(tok_sup) &&tok_sup,
        decltype(generic_param_group) &&generic_param_group,
        decltype(name) name,
        decltype(tok_ext) &&tok_ext,
        decltype(super_class) super_class,
        decltype(impl) &&impl);

    ~SupPrototypeExtensionAst() override;

    SPP_AST_KEY_FUNCTIONS;

private:
    auto m_check_cyclic_extension(
        analyse::scopes::TypeSymbol const &sup_sym,
        analyse::scopes::Scope &check_scope) const
        -> void;

    auto m_check_double_extension(
        analyse::scopes::TypeSymbol const &cls_sym,
        analyse::scopes::Scope &check_scope) const
        -> void;

    auto m_check_self_extension(
        analyse::scopes::Scope &check_scope) const
        -> void;

public:
    auto stage_1_pre_process(Ast *ctx) -> void override;

    auto stage_2_gen_top_level_scopes(ScopeManager *sm, CompilerMetaData *) -> void override;

    auto stage_3_gen_top_level_aliases(ScopeManager *sm, CompilerMetaData *meta) -> void override;

    auto stage_4_qualify_types(ScopeManager *sm, CompilerMetaData *meta) -> void override;

    auto stage_5_load_super_scopes(ScopeManager *sm, CompilerMetaData *meta) -> void override;

    auto stage_6_pre_analyse_semantics(ScopeManager *sm, CompilerMetaData *meta) -> void override;

    auto stage_7_analyse_semantics(ScopeManager *sm, CompilerMetaData *meta) -> void override;

    auto stage_8_check_memory(ScopeManager *sm, CompilerMetaData *meta) -> void override;
};
