#pragma once
#include <string>
#include <vector>

#include <spp/lex/tokens.hpp>
#include <spp/asts/_fwd.hpp>
#include <spp/utils/error_formatter.hpp>


namespace spp::compiler {
    struct Module;
    struct ModuleTree;
}


struct spp::compiler::Module {
    std::string path;
    std::string code;
    std::vector<lex::RawToken> tokens;
    asts::ModulePrototypeAst *module_ast;
    std::unique_ptr<utils::errors::ErrorFormatter> error_formatter;
};
