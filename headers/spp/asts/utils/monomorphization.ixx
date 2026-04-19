module;
#include <spp/macros.hpp>

export module spp.asts.utils.monomorphization;
import spp.asts;
import spp.analyse.scopes;
import std;

namespace spp::asts::utils::monomorphization {
    /**
     * For every type discovered up until this point, attach the defined supertypes to them. At this point, all base
     * classes, and @c sup blocks, will have been injected into the symbol table, but possibly not all generic
     * substitutions of some of these type. This is fine because the @c TypeAst semantic analysis will call individual
     * sup scope attachment functions if needed.
     * @param meta The compiler metadata.
     */
    SPP_EXP_FUN auto attach_all_super_scopes(
        analyse::scopes::ScopeManager &sm,
        meta::CompilerMetaData *meta)
        -> void;

    /**
     * The public method to attach all the superscopes to an individual type, represented by its corresponding scope.
     * This is the function called by the type analysis code if the generic substitution is new. For example, of
     * @c Vec[Str] has been discovered after the "attach all" was performed, then this function will be called to attach
     * the superscopes of @c Vec[Str].
     * @param scope The scope representing the type to attach superscopes to.
     * @param meta The compiler metadata.
     */
    SPP_EXP_FUN auto attach_specific_super_scopes(
        analyse::scopes::Scope &scope,
        meta::CompilerMetaData *meta)
        -> void;

    /**
     * The implementation logic for attaching specific super scopes to a type scope. This is separated out so that the
     * public method can alter scope lists based on @c TypeSymbol vs @c AliasSymbol, where-as this function strictly
     * applies the scope changes.
     * @param scope The scope representing the type to attach superscopes to.
     * @param sup_scopes The list of super scopes to attach to the type scope. This is passed in as an rvalue reference
     * because it will typically be created on-the-fly by the public method.
     * @param meta The compiler metadata.
     */
    SPP_EXP_FUN auto attach_specific_super_scopes_impl(
        analyse::scopes::Scope &scope,
        std::vector<analyse::scopes::Scope*> &&sup_scopes,
        meta::CompilerMetaData *meta)
        -> void;

}
