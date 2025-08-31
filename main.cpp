#include <fstream>
#include <iostream>
#include <sstream>

#include <spp/asts/module_member_ast.hpp>
#include <spp/asts/module_prototype_ast.hpp>
#include <spp/asts/module_implementation_ast.hpp>
#include <spp/lex/lexer.hpp>
#include <spp/parse/parser_spp.hpp>
#include <spp/utils/error_formatter.hpp>
#include <spp/utils/errors.hpp>


int main() {
    auto code_stream = std::stringstream();
    auto code_file = std::ifstream("../test.spp");
    code_stream << code_file.rdbuf();
    auto code = code_stream.str();

    const auto lexer = spp::lex::Lexer(std::move(code));
    const auto tokens = lexer.lex();

    auto parser = spp::parse::ParserSpp(tokens);
    const auto root = parser.parse();
    std::cout << "PARSED\n";

    return 0;
}
