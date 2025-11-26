module;
#include <spp/macros.hpp>

export module spp.parse.parser_base;
import spp.lex.tokens;
import spp.parse.errors.parser_error;
import spp.parse.errors.parser_error_builder;
import spp.utils.error_formatter;
import std;


namespace spp::parse {
    SPP_EXP class ParserBase;
}


SPP_EXP class spp::parse::ParserBase {
public:
    explicit ParserBase(std::vector<lex::RawToken> tokens, std::shared_ptr<utils::errors::ErrorFormatter> const &error_formatter = nullptr);
    virtual ~ParserBase() = default;

protected:
    std::size_t m_pos = 0;
    std::vector<lex::RawToken> m_tokens = {};
    std::size_t m_tokens_len = 0;
    std::unique_ptr<errors::SyntacticErrorBuilder<errors::SppSyntaxError>> m_error_builder;
    std::shared_ptr<utils::errors::ErrorFormatter> m_error_formatter;

    template <typename T>
    using parser_method_t = std::function<std::unique_ptr<T>()>;

    template <typename T>
    using parser_method_alt_t = std::function<T()>;
};
