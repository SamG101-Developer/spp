#include <fstream>
#include <iostream>
#include <sstream>

#include <spp/asts/module_prototype_ast.hpp>
#include <spp/asts/module_implementation_ast.hpp>
#include <spp/lex/lexer.hpp>
#include <spp/parse/parser_spp.hpp>
#include <spp/utils/error_formatter.hpp>
#include <spp/utils/errors.hpp>

#include <magic_enum/magic_enum.hpp>


int main() {
    auto code_stream = std::stringstream();
    auto code_file = std::ifstream("../test.spp");
    code_stream << code_file.rdbuf();
    auto code = code_stream.str();

    const auto lexer = spp::lex::Lexer(std::move(code));
    const auto tokens = lexer.lex();

    auto parser = spp::parse::ParserSpp(tokens);
    const auto root = parser.parse();
    if (root == nullptr) {
        auto error = parser.get_error();
        std::cerr << error << "\n";
        return 1;
    }

    auto printer = spp::asts::meta::AstPrinter();
    auto string = std::string();
    root->print(printer);

    std::cout << "PARSED\n";
    std::cout << "Root AST: " << string << "\n";

    return 0;
}
