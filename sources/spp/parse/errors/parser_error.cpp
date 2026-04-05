module;
#include <spp/macros.hpp>

module spp.parse.errors.parser_error;


SPP_MOD_BEGIN
spp::parse::errors::SyntacticError::SyntacticError(std::string &&header) :
    header(std::move(header)) {
}


spp::parse::errors::SppSyntaxError::SppSyntaxError(std::string &&header) :
    SyntacticError(std::move(header)),
    pos(0),
    tokens({}) {
}

SPP_MOD_END
