module;
#include <spp/macros.hpp>

export module spp.asts.object_initializer_argument_group_ast;
import spp.asts.ast;
import std;

namespace spp::asts {
    SPP_EXP_CLS struct ObjectInitializerArgumentAst;
    SPP_EXP_CLS struct ObjectInitializerArgumentGroupAst;
}


/**
 * The ObjectInitializerArgumentGroupAst represents a group of object initializer arguments. It is used to group
 * multiple shorthand or keyword arguments together in a object initializer.
 */
SPP_EXP_CLS struct spp::asts::ObjectInitializerArgumentGroupAst final : Ast {
    std::vector<std::unique_ptr<ObjectInitializerArgumentAst>> args;

    explicit ObjectInitializerArgumentGroupAst(
        decltype(args) &&args);
    ~ObjectInitializerArgumentGroupAst() override;
    auto to_rust() const -> std::string override;
};
