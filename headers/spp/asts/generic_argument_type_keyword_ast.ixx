module;
#include <spp/macros.hpp>

export module spp.asts.generic_argument_type_keyword_ast;
import spp.asts.generic_argument_type_ast;
import spp.utils.types;
import std;

namespace spp::analyse::scopes {
    SPP_EXP_CLS struct TypeSymbol;
}

namespace spp::asts {
    SPP_EXP_CLS struct GenericArgumentTypeKeywordAst;
    SPP_EXP_CLS struct TokenAst;
    SPP_EXP_CLS struct TypeAst;
}


/**
 * The GenericArgumentTypeKeywordAst represents a keyword argument in a generic argument context. It is forces the
 * argument to be matched by a keyword rather than an index.
 */
SPP_EXP_CLS struct spp::asts::GenericArgumentTypeKeywordAst final : GenericArgumentTypeAst {
    SPP_GCC_VTABLE_FIX

    /**
     * The name of the keyword argument. This is the type that is used to refer to the argument in the generic call.
     */
    Shared<TypeAst> Name;

    /**
     * The token that represents the assignment operator @c = in the keyword argument. This separates the name of the
     * argument from the expression that is being passed as the argument's value.
     */
    Unique<TokenAst> TokAssign;

    static auto FromSym(analyse::scopes::TypeSymbol const &sym) -> Unique<GenericArgumentTypeKeywordAst>;

    /**
     * Construct the GenericArgumentTypeKeywordAst with the arguments matching the members.
     * @param name The name of the keyword argument.
     * @param tok_assign The token that represents the assignment operator @c = in the keyword argument.
     * @param val The value of the generic type argument.
     */
    GenericArgumentTypeKeywordAst(
        decltype(Name) name,
        decltype(TokAssign) &&tok_assign,
        decltype(Val) val);

    ~GenericArgumentTypeKeywordAst() override;

    SPP_ATTR_NODISCARD auto EqualsGenericArgumentTypeKeyword(GenericArgumentTypeKeywordAst const &other) const -> Ordering override;

    SPP_ATTR_NODISCARD auto Equals(GenericArgumentAst const &other) const -> Ordering override;

    SPP_AST_KEY_FUNCTIONS

    auto Stage7_AnalyseSemantics(ScopeManager *sm, CompilerMetaData *meta) -> void override;

    SPP_ATTR_NODISCARD auto ViewName() const -> StrView override;
};


SPP_GCC_VTABLE_FIX_IMPL(spp::asts::GenericArgumentTypeKeywordAst)
