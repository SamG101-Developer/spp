#ifndef ERROR_FORMATTER_HPP
#define ERROR_FORMATTER_HPP

#include <string>
#include <vector>

#include <spp/lex/tokens.hpp>


namespace spp::asts {
    struct Ast;
}

namespace spp::utils::errors {
    class ErrorFormatter;
}


class spp::utils::errors::ErrorFormatter {
public:
    ErrorFormatter(std::vector<lex::RawToken> tokens, std::string_view file_path);

public:
    auto error_from_positions(std::size_t start_pos, std::string &&message = "", std::string &&tag = "", bool minimal = false) const -> std::string;
    auto error_from_ast(asts::Ast *ast, std::string &&message = "", std::string &&tag = "", bool minimal = false) const -> std::string;

private:
    std::vector<lex::RawToken> m_tokens;
    std::string_view m_file_path;
};


#endif //ERROR_FORMATTER_HPP
