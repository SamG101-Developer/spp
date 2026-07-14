module;
#include <spp/macros.hpp>

export module spp.asts.object_initializer_argument_ast;
import spp.asts.ast;
import spp.asts.mixins.type_inferrable_ast;
import spp.utils.types;
import std;

namespace spp::asts {
    SPP_EXP_CLS struct ExpressionAst;
    SPP_EXP_CLS struct IdentifierAst;
    SPP_EXP_CLS struct ObjectInitializerArgumentAst;
    SPP_EXP_CLS struct TypeAst;
}

COMMON_AST_IMPORTS

/**
 * The ObjectInitializerArgumentAst is the base class representing an argument in an object initialisation. It is
 * inherited into the "shorthand" and "keyword" variants.
 */
SPP_EXP_CLS struct spp::asts::ObjectInitializerArgumentAst : Ast, mixins::TypeInferrableAst {
    /**
     * The name of the argument. This is the identifier that is used to refer to the argument in the function call. For
     * shorthand args, this is autofilled by cloning the value, and casting it to an IdentifierAst. Otherwise, it is
     * passed explicitly from the keyword arg parser.
     */
    Unique<IdentifierAst> Name;

    /**
     * The expression that is being passed as the argument to the object initialization. Both positional and keyword
     * arguments have a value.
     */
    Unique<ExpressionAst> Val;

    /**
     * Construct the ObjectInitializerArgumentAst with the arguments matching the members.
     * @param[in] name The name of the argument.
     * @param[in] val The expression that is being passed as the argument to the object initialization.
     */
    explicit ObjectInitializerArgumentAst(
        decltype(Name) &&name,
        decltype(Val) &&val);

    ~ObjectInitializerArgumentAst() override;

    auto Stage7_AnalyseSemantics(analyse::scopes::ScopeManager *sm, meta::CompilerMetaData *meta) -> void override;

    auto Stage8_CheckMemory(analyse::scopes::ScopeManager *sm, meta::CompilerMetaData *meta) -> void override;

    auto Stage9_CompTimeResolve(analyse::scopes::ScopeManager *sm, meta::CompilerMetaData *meta) -> void override;

    auto InferType(analyse::scopes::ScopeManager *sm, meta::CompilerMetaData *meta) -> TypeAst* override;
};
