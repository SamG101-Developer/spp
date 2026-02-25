module;
#include <spp/macros.hpp>

export module spp.asts.object_initializer_ast;
import spp.asts.primary_expression_ast;
import std;

namespace spp::asts {
    SPP_EXP_CLS struct ObjectInitializerAst;
    SPP_EXP_CLS struct ObjectInitializerArgumentGroupAst;
    SPP_EXP_CLS struct TypeAst;
}


SPP_EXP_CLS struct spp::asts::ObjectInitializerAst final : PrimaryExpressionAst {
    std::unique_ptr<TypeAst> type;
    std::unique_ptr<ObjectInitializerArgumentGroupAst> arg_group;

    ObjectInitializerAst(
        decltype(type) &&type,
        decltype(arg_group) &&arg_group);
    ~ObjectInitializerAst() override;
    auto to_rust() const -> std::string override;
};
