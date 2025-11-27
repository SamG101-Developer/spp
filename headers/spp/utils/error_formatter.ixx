module;
#include <spp/macros.hpp>

export module spp.utils.error_formatter;
import spp.asts._fwd;
import spp.lex.tokens;

import std;


namespace spp::utils::errors {
    SPP_EXP_CLS class ErrorFormatter;
}


SPP_EXP_CLS class spp::utils::errors::ErrorFormatter {
    std::vector<lex::RawToken> m_tokens;
    std::string m_file_path;

public:
    auto internal_parse_error_raw_pos(std::size_t ast_start_pos, std::size_t ast_size, std::string &&tag_message) -> std::tuple<std::string, std::string, std::string, std::string, std::string>;

    auto error_raw_pos(std::size_t ast_start_pos, std::size_t ast_size, std::string &&message, std::string &&tag_message) -> std::string;

    auto error_raw_pow_minimal(std::size_t ast_start_pos, std::size_t ast_size, std::string &&tag_message) -> std::string;

    auto error_ast(asts::Ast const *ast, std::string &&message, std::string &&tag_message) -> std::string;

    auto error_ast_minimal(asts::Ast const *ast, std::string &&tag_message) -> std::string;

    ErrorFormatter(std::vector<lex::RawToken> tokens, std::string file_path);
};
