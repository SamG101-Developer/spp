module;
#include <spp/macros.hpp>

export module spp.asts.function_call_argument_group_ast;
import spp.asts.ast;
import spp.utils.types;
import std;

namespace spp::asts {
    SPP_EXP_CLS struct FunctionCallArgumentAst;
    SPP_EXP_CLS struct FunctionCallArgumentGroupAst;
    SPP_EXP_CLS struct FunctionCallArgumentKeywordAst;
    SPP_EXP_CLS struct FunctionCallArgumentPositionalAst;
    SPP_EXP_CLS struct TokenAst;
}

COMMON_AST_IMPORTS

/**
 * The FunctionCallArgumentGroupAst represents a group of function call arguments. It is used to group multiple
 * positional or keyword arguments together in a function call.
 */
SPP_EXP_CLS struct spp::asts::FunctionCallArgumentGroupAst final : Ast {
    SPP_AST_KEY_FUNCTIONS;

    /**
     * The token that represents the left parenthesis @code (@endcode in the function call argument group. This
     * introduces the function call argument group.
     */
    Unique<TokenAst> TokL;

    /**
     * The list of arguments in the function call argument group. This can contain both positional and keyword
     * arguments.
     */
    Vec<Unique<FunctionCallArgumentAst>> Args;

    /**
     * The token that represents the right parenthesis @code )@endcode in the function call argument group. This closes
     * the function call argument group.
     */
    Unique<TokenAst> TokR;

    static auto NewEmpty() -> Unique<FunctionCallArgumentGroupAst>;

    /**
     * Construct the FunctionCallArgumentGroupAst with the arguments matching the members.
     * @param tok_l The token that represents the left parenthesis @code (@endcode in the function call argument group.
     * @param args The list of arguments in the function call argument group.
     * @param tok_r The token that represents the right parenthesis @code )@endcode in the function call argument group.
     */
    FunctionCallArgumentGroupAst(
        decltype(TokL) &&tok_l,
        decltype(Args) &&args,
        decltype(TokR) &&tok_r);

    ~FunctionCallArgumentGroupAst() override;

    SPP_ATTR_NODISCARD auto GetAllArgs() const -> Vec<FunctionCallArgumentAst*>;

    SPP_ATTR_NODISCARD auto GetKeywordArgs() const -> Vec<FunctionCallArgumentKeywordAst*>;

    SPP_ATTR_NODISCARD auto GetPositionalArgs() const -> Vec<FunctionCallArgumentPositionalAst*>;

    auto Stage7_AnalyseSemantics(analyse::scopes::ScopeManager *sm, meta::CompilerMetaData *meta) -> void override;

    auto Stage8_CheckMemory(analyse::scopes::ScopeManager *sm, meta::CompilerMetaData *meta) -> void override;

    auto At(const char *key) const -> FunctionCallArgumentAst const*;
};
