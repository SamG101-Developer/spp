module;
#include <spp/macros.hpp>

export module spp.asts.object_initializer_argument_group_ast;
import spp.asts.ast;
import spp.utils.types;
import std;

namespace spp::asts {
    SPP_EXP_CLS struct ObjectInitializerArgumentAst;
    SPP_EXP_CLS struct ObjectInitializerArgumentKeywordAst;
    SPP_EXP_CLS struct ObjectInitializerArgumentShorthandAst;
    SPP_EXP_CLS struct ObjectInitializerArgumentGroupAst;
    SPP_EXP_CLS struct TokenAst;
}


/**
 * The ObjectInitializerArgumentGroupAst represents a group of object initializer arguments. It is used to group
 * multiple shorthand or keyword arguments together in a object initializer.
 */
SPP_EXP_CLS struct spp::asts::ObjectInitializerArgumentGroupAst final : Ast {
    /**
     * The token that represents the left parenthesis @code (@endcode in the object initializer argument group. This
     * introduces the object initializer argument group.
     */
    Unique<TokenAst> TokL;

    /**
     * The list of arguments in the object initializer argument group. This can contain both positional and keyword
     * arguments.
     */
    Vec<Unique<ObjectInitializerArgumentAst>> Args;

    /**
     * The token that represents the right parenthesis @code )@endcode in the object initializer argument group. This
     * closes the object initializer argument group.
     */
    Unique<TokenAst> TokR;

    static auto NewEmpty() -> Unique<ObjectInitializerArgumentGroupAst>;

    /**
     * Construct the ObjectInitializerArgumentGroupAst with the arguments matching the members.
     * @param tok_l The token that represents the left parenthesis @code (@endcode in the object initializer argument
     * group.
     * @param args The list of arguments in the object initializer argument group.
     * @param tok_r The token that represents the right parenthesis @code )@endcode in the object initializer argument
     * group.
     */
    ObjectInitializerArgumentGroupAst(
        decltype(TokL) &&tok_l,
        decltype(Args) &&args,
        decltype(TokR) &&tok_r);

    ~ObjectInitializerArgumentGroupAst() override;

    SPP_AST_KEY_FUNCTIONS;

    auto Stage6_PreAnalyseSemantics(ScopeManager *sm, CompilerMetaData *meta) -> void override;

    auto Stage7_AnalyseSemantics(ScopeManager *sm, CompilerMetaData *meta) -> void override;

    auto Stage8_CheckMemory(ScopeManager *sm, CompilerMetaData *meta) -> void override;

    auto GetAllArgs() -> Vec<ObjectInitializerArgumentAst*>;

    auto GetAutoFillArg() -> ObjectInitializerArgumentShorthandAst*;

    auto GetNonAutoFillArgs() -> Vec<ObjectInitializerArgumentAst*>;

    auto GetShorthandArgs() -> Vec<ObjectInitializerArgumentShorthandAst*>;

    auto GetKeywordArgs() -> Vec<ObjectInitializerArgumentKeywordAst*>;
};
