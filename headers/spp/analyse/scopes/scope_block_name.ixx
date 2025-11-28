module;
#include <spp/macros.hpp>

export module spp.analyse.scopes.scope_block_name;
import std;


namespace spp::analyse::scopes {
    SPP_EXP_CLS struct ScopeBlockName;
}


/**
 * For scopes that aren't for a function or type, they don't have an @c IdentifierAst or @c TypeIdentifierAst to name
 * them. Instead, they have a simple name, which is just a string. This is used for things like loops, conditionals,
 * and other blocks of code that don't have a specific name. The string is wrapped into this struct to provide type
 * safety.
 */
SPP_EXP_CLS struct spp::analyse::scopes::ScopeBlockName {
    /**
     * The name of the scope block. This is just a string, and can be anything that makes sense for the block.
     * Typically, a scope name may be "<loop#30>" for a loop block starting at token number 30.
     */
    std::string name;

    /**
     * The constructor for the ScopeBlockName. This takes a string and moves it into the struct. This is to avoid
     * unnecessary copies.
     * @param name The name of the scope block.
     */
    explicit ScopeBlockName(std::string &&name);

    /**
     * Allow default copy constructors for easy passing around of scope block names.
     */
    ScopeBlockName(ScopeBlockName const &) = default;

    /**
     * Allow default move constructors for easy passing around of scope block names.
     */
    ScopeBlockName(ScopeBlockName &&) noexcept = default;
};
