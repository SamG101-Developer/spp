module;
#include <spp/macros.hpp>

export module spp.asts.generic_parameter_type_ast;
import spp.asts.generic_parameter_ast;
import spp.asts.utils.orderable;
import spp.utils.types;
import std;

namespace spp::analyse::scopes {
    SPP_EXP_CLS class Scope;
}

namespace spp::asts {
    SPP_EXP_CLS struct GenericParameterTypeAst;
    SPP_EXP_CLS struct GenericParameterTypeInlineConstraintsAst;
}

SPP_EXP_CLS struct spp::asts::GenericParameterTypeAst : GenericParameterAst {
    SPP_GCC_VTABLE_FIX

    /**
     * The optional inline constraints for the generic type parameter. This is used to specify constraints on the type
     * parameter, such as @c I32 or @c F64 . An example is @code fun func[T: Copy]()@endcode, where @c T is the
     * generic type parameter and @c Copy is the constraint.
     */
    Unique<GenericParameterTypeInlineConstraintsAst> Constraints;

    /**
     * Construct the GenericParameterTypeAst with the arguments matching the members.
     * @param name The name of the generic type parameter.
     * @param constraints The optional inline constraints for the generic type parameter.
     * @param order_tag The order tag for the generic parameter.
     */
    GenericParameterTypeAst(
        decltype(Name) name,
        decltype(Constraints) &&constraints,
        utils::OrderableTag order_tag);

    ~GenericParameterTypeAst() override;

    auto Stage2_GenTopLvlScopes(ScopeManager *sm, CompilerMetaData *) -> void override;

    auto Stage4_QualifyTypes(ScopeManager *sm, CompilerMetaData *) -> void override;

    auto Stage7_AnalyseSemantics(ScopeManager *sm, CompilerMetaData *meta) -> void override;

    SPP_ATTR_NODISCARD auto GetDummyScopes() const -> std::span<analyse::scopes::Scope* const>;

private:
    Unique<Ast> _DummyAst;

    Vec<analyse::scopes::Scope*> _DummyScopes;
};

SPP_GCC_VTABLE_FIX_IMPL(spp::asts::GenericParameterTypeAst)
