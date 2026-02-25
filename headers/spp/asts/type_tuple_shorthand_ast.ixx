module;
#include <spp/macros.hpp>

export module spp.asts.type_tuple_shorthand_ast;
import spp.asts.ast;
import std;

namespace spp::asts {
    SPP_EXP_CLS struct TypeTupleShorthandAst;
    SPP_EXP_CLS struct TypeAst;
}


SPP_EXP_CLS struct spp::asts::TypeTupleShorthandAst final : Ast {
    std::vector<std::unique_ptr<TypeAst>> element_types;

    explicit TypeTupleShorthandAst(
        decltype(element_types) &&element_types);
    ~TypeTupleShorthandAst() override;
    auto to_rust() const -> std::string override;
};
