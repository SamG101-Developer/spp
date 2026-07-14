module;
#include <spp/macros.hpp>

export module spp.parse.parser_base;
import spp.lex.tokens;
import spp.utils.error_formatter;
import spp.utils.types;
import std;

namespace spp::parse::errors {
    SPP_EXP_CLS struct SppSyntaxError;
    SPP_EXP_CLS template <typename T>
    struct SyntacticErrorBuilder;
}

namespace spp::parse {
    SPP_EXP_CLS class ParserBase;
}

SPP_EXP_CLS class spp::parse::ParserBase {
public:
    explicit ParserBase(Vec<lex::RawToken> tokens, Unique<utils::errors::ErrorFormatter> &&error_formatter = nullptr);
    virtual ~ParserBase();

protected:
    std::size_t _Pos = 0uz;
    Vec<lex::RawToken> _Tokens = {};
    std::size_t _TokensLen = 0uz;
    Unique<errors::SyntacticErrorBuilder<errors::SppSyntaxError>> _ErrorBuilder;
    Unique<utils::errors::ErrorFormatter> _ErrorFormatter;

    template <typename T>
    using parser_method_t = std::function<Unique<T>()>;

    template <typename T>
    using parser_method_alt_t = std::function<T()>;
};
