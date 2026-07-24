module;
#include <spp/macros.hpp>

export module spp.asts.subroutine_prototype_ast;
import spp.asts.function_prototype_ast;
import spp.utils.types;
import std;

namespace spp::asts {
    SPP_EXP_CLS struct SubroutinePrototypeAst;
}

SPP_EXP_CLS struct spp::asts::SubroutinePrototypeAst final : FunctionPrototypeAst {
    SubroutinePrototypeAst(
        decltype(Annotations) &&annotations,
        decltype(TokCmp) &&tok_cmp,
        decltype(TokFun) &&tok_fun,
        decltype(Name) &&name,
        decltype(GnParamGroup) &&generic_param_group,
        decltype(FnParamGroup) &&param_group,
        decltype(TokArrow) &&tok_arrow,
        decltype(ReturnType) &&return_type,
        decltype(Impl) &&impl);

    ~SubroutinePrototypeAst() override;

    SPP_ATTR_NODISCARD auto Clone() const -> Unique<Ast> override;

    auto Stage7_AnalyseSemantics(ScopeManager *sm, CompilerMetaData *meta) -> void override;

    auto IsCoroutine() const -> bool override;
};
