module;
#include <spp/macros.hpp>

export module spp.asts.convention_ast;
import spp.asts.ast;


/**
 * The ConventionAst class represents a convention for either a function parameter, function argument, or a generated
 * value from a coroutine. This is because the second class model restricts where borrows can be used, and the
 * convention is used to indicate that a value is borrowed (and how), or moved (lack of borrow).
 *
 * The MOV is defined as a tag because when convention is not present, but needs to be compared, it is the equivalent,
 * semantically, of being a "move" convention.
 */
SPP_EXP struct spp::asts::ConventionAst : virtual Ast {
    enum class ConventionTag { MOV, MUT, REF };

private:
    ConventionTag tag;

public:
    explicit ConventionAst(ConventionTag tag);

    ~ConventionAst() override;

    auto operator==(ConventionAst const *that) const -> bool;

    auto operator==(ConventionTag that_tag) const -> bool;
};
