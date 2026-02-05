module;
#include <spp/macros.hpp>

export module spp.analyse.scopes.scope_block_name;
import std;

namespace spp::analyse::scopes {
    SPP_EXP_CLS struct ScopeBlockName;
}

namespace spp::asts {
    SPP_EXP_CLS struct Ast;
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

private:
    /**
     * The constructor for the ScopeBlockName. This takes a string and moves it into the struct. This is to avoid
     * unnecessary copies.
     * @param name The name of the scope block.
     */
    explicit ScopeBlockName(std::string &&name);

public:
    /**
     * Create a ScopeBlockName from a header and parts. The format created is "<header#part1#part2#...>". This is
     * the standard format for scope block names in SPP.
     * @param header The header of the scope block name, like "type-stmt" or "loop".
     * @param parts The parts to append to the header, typically strings representing metadata of the AST.
     * @param pos The position of the AST in the source code, to make scope names unique.
     * @return The constructed ScopeBlockName.
     */
    static auto from_parts(std::string &&header, std::vector<asts::Ast*> const &parts, std::size_t pos) -> ScopeBlockName;

    /**
     * Allow default copy constructors for easy passing around of scope block names.
     */
    ScopeBlockName(ScopeBlockName const &) = default;

    /**
     * Allow default move constructors for easy passing around of scope block names.
     */
    ScopeBlockName(ScopeBlockName &&) noexcept = default;
};
