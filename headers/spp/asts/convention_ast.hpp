#ifndef CONVENTION_AST_HPP
#define CONVENTION_AST_HPP

#include <spp/asts/ast.hpp>
#include <spp/asts/_fwd.hpp>


/**
 * The ConventionAst class represents a convention for either a function parameter, function argument, or a generated
 * value from a coroutine. This is because the second class model restricts where borrows can be used, and the
 * convention is used to indicate that a value is borrowed (and how), or moved (lack of borrow).
 */
struct spp::asts::ConventionAst : virtual Ast {
    enum class ConventionTag { MOV, MUT, REF };

    ConventionTag tag;

    explicit ConventionAst(ConventionTag tag);
};


#endif //CONVENTION_AST_HPP
