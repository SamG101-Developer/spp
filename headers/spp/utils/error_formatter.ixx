module;
#include <spp/macros.hpp>

export module spp.utils.error_formatter;
import spp.lex.tokens;
import spp.utils.types;
import std;

namespace spp::asts {
    SPP_EXP_CLS struct Ast;
}

namespace spp::utils::errors {
    SPP_EXP_CLS class ErrorFormatter;
}


SPP_EXP_CLS class spp::utils::errors::ErrorFormatter {
public:
    ErrorFormatter(Vec<lex::RawToken> tokens, Str file_path);

    auto InternalParseErrorRawPos(std::size_t ast_start_pos, std::size_t ast_size, Str &&tag_message) -> std::tuple<Str, Str, Str, Str, Str>;

    auto ErrorRawPos(std::size_t ast_start_pos, std::size_t ast_size, Str &&message, Str &&tag_message) -> Str;

    auto ErrorRawPosMinimal(std::size_t ast_start_pos, std::size_t ast_size, Str &&tag_message) -> Str;

    auto ErrorAst(asts::Ast const *ast, Str &&message, Str &&tag_message) -> Str;

    auto ErrorAstMinimal(asts::Ast const *ast, Str &&tag_message) -> Str;

private:
    Vec<lex::RawToken> _Tokens;
    Str _FilePath;
};
