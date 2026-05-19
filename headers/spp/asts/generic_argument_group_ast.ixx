module;
#include <spp/macros.hpp>

export module spp.asts.generic_argument_group_ast;
import spp.analyse.utils.func_utils; // Todo: refactor typedefs
import spp.analyse.utils.type_utils; // Todo: refactor typedefs
import spp.asts.ast;
import spp.utils.ptr;
import spp.utils.types;
import ankerl.unordered_dense;
import std;

namespace spp::asts {
    SPP_EXP_CLS struct ExpressionAst;
    SPP_EXP_CLS struct GenericArgumentGroupAst;
    SPP_EXP_CLS struct GenericArgumentAst;
    SPP_EXP_CLS struct GenericArgumentCompAst;
    SPP_EXP_CLS struct GenericArgumentCompKeywordAst;
    SPP_EXP_CLS struct GenericArgumentTypeAst;
    SPP_EXP_CLS struct GenericArgumentTypeKeywordAst;
    SPP_EXP_CLS struct GenericParameterGroupAst;
    SPP_EXP_CLS struct TokenAst;
    SPP_EXP_CLS struct TypeAst;
    SPP_EXP_CLS struct TypeIdentifierAst;
}


SPP_EXP_CLS struct spp::asts::GenericArgumentGroupAst final : Ast {
    SPP_GCC_VTABLE_FIX

    /**
     * The token that represents the left bracket @code [@endcode in the generic argument group. This introduces the
     * generic argument group.
     */
    Unique<TokenAst> TokL;

    /**
     * The list of arguments in the generic argument group. This can contain both positional and keyword arguments.
     */
    Vec<Unique<GenericArgumentAst>> Args;

    /**
     * The token that represents the right parenthesis @code ]@endcode in the generic call argument group. This closes
     * the generic argument group.
     */
    Unique<TokenAst> TokR;

    static auto NewEmpty()
        -> Unique<GenericArgumentGroupAst>;

    static auto FromParams(
        GenericParameterGroupAst const &generic_params)
        -> Unique<GenericArgumentGroupAst>;

    static auto FromMap(
        analyse::utils::type_utils::GenericInferenceMap const &map)
        -> Unique<GenericArgumentGroupAst>;

    static auto FromMap(
        analyse::utils::func_utils::InferenceFinalTypeMap const &map)
        -> Unique<GenericArgumentGroupAst>;

    /**
     * Construct the GenericArgumentGroupAst with the arguments matching the members.
     * @param tok_l The token that represents the left bracket @code [@endcode in the generic argument group.
     * @param args The list of arguments in the generic argument group.
     * @param tok_r The token that represents the right parenthesis @code ]@endcode in the generic call argument group.
     */
    GenericArgumentGroupAst(
        decltype(TokL) &&tok_l,
        decltype(Args) &&args,
        decltype(TokR) &&tok_r);

    ~GenericArgumentGroupAst() override;

    SPP_AST_KEY_FUNCTIONS;

    auto operator==(GenericArgumentGroupAst const &other) const -> bool;

    auto operator+=(const GenericArgumentGroupAst &other) -> GenericArgumentGroupAst&;

    auto operator+(const GenericArgumentGroupAst &other) const -> Unique<GenericArgumentGroupAst>;

    auto Stage4_QualifyTypes(ScopeManager *sm, CompilerMetaData *meta) -> void override;

    auto Stage7_AnalyseSemantics(ScopeManager *sm, CompilerMetaData *meta) -> void override;

    auto Stage8_CheckMemory(ScopeManager *sm, CompilerMetaData *meta) -> void override;

    auto TypeAt(const char *key) const -> GenericArgumentTypeAst const*;

    auto CompAt(const char *key) const -> GenericArgumentCompAst const*;

    auto MergeGenerics(decltype(Args) &&other_args) -> void;

    SPP_ATTR_NODISCARD auto GetTypeArgs() const -> Vec<GenericArgumentTypeAst*>;

    SPP_ATTR_NODISCARD auto GetCompArgs() const -> Vec<GenericArgumentCompAst*>;

    SPP_ATTR_NODISCARD auto GetKeywordArgs() const -> Vec<GenericArgumentAst*>;

    SPP_ATTR_NODISCARD auto GetPositionalArgs() const -> Vec<GenericArgumentAst*>;

    SPP_ATTR_NODISCARD auto GetTypeKeywordArgs() const -> Vec<GenericArgumentTypeKeywordAst*>;

    SPP_ATTR_NODISCARD auto GetCompKeywordArgs() const -> Vec<GenericArgumentCompKeywordAst*>;

    SPP_ATTR_NODISCARD auto GetAllArgs() const -> Vec<GenericArgumentAst*>;
};


SPP_GCC_VTABLE_FIX_IMPL(spp::asts::GenericArgumentGroupAst)
