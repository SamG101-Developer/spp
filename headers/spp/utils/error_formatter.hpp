#pragma once
#include <spp/lex/tokens.hpp>
#include <spp/pch.hpp>


namespace spp::asts {
    struct Ast;
}

namespace spp::utils::errors {
    class ErrorFormatter;
}


class spp::utils::errors::ErrorFormatter {
public:
    ErrorFormatter(std::vector<lex::RawToken> tokens, std::string file_path);

public:
    // auto error_from_positions(std::size_t start_pos, std::string &&message = "", std::string &&tag = "", bool minimal = false) const -> std::string;
    // auto error_from_ast(asts::Ast *ast, std::string &&message = "", std::string &&tag = "", bool minimal = false) const -> std::string;


    auto internal_parse_error_raw_pos(std::size_t ast_start_pos, std::size_t ast_size, std::string &&tag_message) -> std::tuple<std::string, std::string, std::string, std::string, std::string>;
    auto error_raw_pos(std::size_t ast_start_pos, std::size_t ast_size, std::string &&message, std::string &&tag_message) -> std::string;
    auto error_raw_pow_minimal(std::size_t ast_start_pos, std::size_t ast_size, std::string &&tag_message) -> std::string;
    auto error_ast(asts::Ast const *ast, std::string &&message, std::string &&tag_message) -> std::string;
    auto error_ast_minimal(asts::Ast const *ast, std::string &&tag_message) -> std::string;

private:
    std::vector<lex::RawToken> m_tokens;
    std::string m_file_path;
};
