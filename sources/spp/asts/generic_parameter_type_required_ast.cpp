module;
#include <spp/macros.hpp>

module spp.asts;
import spp.asts.utils;


spp::asts::GenericParameterTypeRequiredAst::GenericParameterTypeRequiredAst(
    decltype(name) &&name,
    decltype(constraints) &&constraints) :
    GenericParameterTypeAst(std::move(name), std::move(constraints), utils::OrderableTag::REQUIRED_PARAM) {
}


spp::asts::GenericParameterTypeRequiredAst::~GenericParameterTypeRequiredAst() = default;


auto spp::asts::GenericParameterTypeRequiredAst::pos_start() const
    -> std::size_t {
    return name->pos_start();
}


auto spp::asts::GenericParameterTypeRequiredAst::pos_end() const
    -> std::size_t {
    return name->pos_end();
}


auto spp::asts::GenericParameterTypeRequiredAst::clone() const
    -> std::unique_ptr<Ast> {
    return std::make_unique<GenericParameterTypeRequiredAst>(
        ast_clone(name),
        ast_clone(constraints));
}


spp::asts::GenericParameterTypeRequiredAst::operator std::string() const {
    SPP_STRING_START;
    SPP_STRING_APPEND(name);
    SPP_STRING_APPEND(constraints);
    SPP_STRING_END;
}
