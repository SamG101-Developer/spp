module;
#include <spp/macros.hpp>

export module spp.parse.errors.parser_error;
import spp.lex.tokens;
import spp.utils.errors;
import spp.utils.types;
import std;

namespace spp::parse::errors {
    SPP_EXP_CLS struct SyntacticError;
    SPP_EXP_CLS struct SppSyntaxError;
}

SPP_EXP_CLS struct spp::parse::errors::SyntacticError : utils::errors::AbstractError {
    Str header;

    explicit SyntacticError(Str &&header);
    SyntacticError(SyntacticError const &) = default;
    ~SyntacticError() override = default;
};

SPP_EXP_CLS struct spp::parse::errors::SppSyntaxError final : SyntacticError {
    std::size_t pos;
    std::set<lex::SppTokenType> tokens;

    explicit SppSyntaxError(Str &&header);
    ~SppSyntaxError() override = default;
};
