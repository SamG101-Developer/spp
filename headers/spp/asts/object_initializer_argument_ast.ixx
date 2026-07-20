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

/**
 * The ObjectInitializerArgumentAst is the base class representing an argument in a object initialization. It is
 * inherited into the "shorthand" and "keyword" variants.
 */
SPP_EXP_CLS struct spp::asts::ObjectInitializerArgumentAst : Ast, mixins::TypeInferrableAst {
    /**
     * The name of the argument. This is the identifier that is used to refer to the argument in the function call. For
     * shorthand args, this is autofilled by cloning the value, and casting it to an IdentifierAst. Otherwise, it is
     * passed explicitly from the keyword arg parser.
     */
    Shared<IdentifierAst> Name;

    /**
     * The expression that is being passed as the argument to the object initialization. Both positional and keyword
     * arguments have a value.
     */
    Unique<ExpressionAst> Val;

    bool IsCompilerGenerated = false;

    /**
     * Construct the ObjectInitializerArgumentAst with the arguments matching the members.
     * @param[in] name The name of the argument.
     * @param[in] val The expression that is being passed as the argument to the object initialization.
     */
    explicit ObjectInitializerArgumentAst(
        decltype(Name) name,
        decltype(Val) &&val);

    ~ObjectInitializerArgumentAst() override;

    auto Stage7_AnalyseSemantics(ScopeManager *sm, CompilerMetaData *meta) -> void override;

    auto Stage8_CheckMemory(ScopeManager *sm, CompilerMetaData *meta) -> void override;

    auto Stage9_CompTimeResolve(ScopeManager *sm, CompilerMetaData *meta) -> void override;

    auto InferType(ScopeManager *sm, CompilerMetaData *meta) -> Shared<TypeAst> override;
};
