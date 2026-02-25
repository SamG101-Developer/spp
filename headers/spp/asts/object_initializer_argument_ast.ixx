module;
#include <spp/macros.hpp>

export module spp.asts.object_initializer_argument_ast;
import spp.asts.ast;
import std;

namespace spp::asts {
    SPP_EXP_CLS struct ExpressionAst;
    SPP_EXP_CLS struct IdentifierAst;
    SPP_EXP_CLS struct ObjectInitializerArgumentAst;
}


/**
 * The ObjectInitializerArgumentAst is the base class representing an argument in a object initialization. It is
 * inherited into the "shorthand" and "keyword" variants.
 */
SPP_EXP_CLS struct spp::asts::ObjectInitializerArgumentAst : Ast {
    std::unique_ptr<IdentifierAst> name;
    std::unique_ptr<ExpressionAst> val;

    explicit ObjectInitializerArgumentAst(
        decltype(name) &&name,
        decltype(val) &&val);
    ~ObjectInitializerArgumentAst() override;
    auto to_rust() const -> std::string override;
};
