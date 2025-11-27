module;
#include <spp/macros.hpp>

export module spp.parse.errors.parser_error_builder;
import spp.lex.tokens;
import spp.parse.errors.parser_error;
import spp.utils.errors;

import std;


namespace spp::parse::errors {
    SPP_EXP_CLS template <typename T>
    struct SyntacticErrorBuilder;
}


SPP_EXP_CLS template <typename T>
struct spp::parse::errors::SyntacticErrorBuilder final : spp::utils::errors::AbstractErrorBuilder<T> {
private:
    std::size_t pos = 0;
    std::set<lex::SppTokenType> tokens = {};

public:
    SyntacticErrorBuilder() = default;

    SPP_ATTR_NORETURN auto raise() -> void override;
};
