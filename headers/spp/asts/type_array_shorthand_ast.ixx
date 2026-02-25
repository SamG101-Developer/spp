module;
#include <spp/macros.hpp>

export module spp.asts.type_array_shorthand_ast;
import spp.asts.ast;
import std;

namespace spp::asts {
    SPP_EXP_CLS struct ExpressionAst;
    SPP_EXP_CLS struct TokenAst;
    SPP_EXP_CLS struct TypeArrayShorthandAst;
    SPP_EXP_CLS struct TypeAst;
}


SPP_EXP_CLS struct spp::asts::TypeArrayShorthandAst final : Ast {
    std::unique_ptr<TypeAst> element_type;
    std::unique_ptr<ExpressionAst> size;

    TypeArrayShorthandAst(
        decltype(element_type) &&element_type,
        decltype(size) &&size);
    ~TypeArrayShorthandAst() override;
    auto to_rust() const -> std::string override;
};
