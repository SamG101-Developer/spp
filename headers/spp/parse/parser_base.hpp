#ifndef PARSER_BASE_HPP
#define PARSER_BASE_HPP

#include <cstddef>
#include <functional>
#include <memory>
#include <utility>
#include <vector>

#include <spp/lex/tokens.hpp>

#include <boost/preprocessor.hpp>
#include <boost/preprocessor/enum.hpp>


#define PARSE_ONCE(out, f) \
    auto out = f();        \
    if (out == nullptr) { return nullptr; }


#define PARSE_OPTIONAL(out, f)             \
    decltype(f()) out;                     \
    {                                      \
        const auto temp_opt_index = m_pos; \
        auto temp_out_opt = f();           \
        if (out == nullptr) {              \
            m_pos = temp_opt_index;        \
            out = nullptr;                 \
        }                                  \
        else {                             \
            out = std::move(temp_out_opt); \
        }                                  \
    }


#define PARSE_ZERO_OR_MORE(out, f, s)          \
    auto out = std::vector<decltype(f())>();   \
    {                                          \
        auto done_1_parse = false;             \
        auto temp_index = m_pos;               \
        while (true) {                         \
            if (done_1_parse) {                \
                PARSE_OPTIONAL(sep, s);        \
                if (sep == nullptr) { break; } \
            }                                  \
            auto ast = f();                    \
            if (ast == nullptr) {              \
                m_pos = temp_index;            \
                break;                         \
            }                                  \
            else {                             \
                out.push_back(std::move(ast)); \
                done_1_parse = true;           \
                temp_index = m_pos;            \
            }                                  \
        }                                      \
    }


#define PARSE_ONE_OR_MORE(out, f, s) \
    PARSE_ZERO_OR_MORE(out, f, s)    \
    if (out.empty()) {               \
        return nullptr;              \
    }


#define PARSE_TWO_OR_MORE(out, f, s) \
    PARSE_ONE_OR_MORE(out, f, s)     \
    if (out.size() < 2) {            \
        return nullptr;              \
    }


#define MAKE_METHOD_VECTOR(...) std::vector{                                                    \
    BOOST_PP_SEQ_FOR_EACH(WRAP_METHOD, BOOST_PP_EMPTY(), BOOST_PP_VARIADIC_TO_SEQ(__VA_ARGS__)) \
}


#define WRAP_METHOD(_1, _2, m) parser_method_t<alt_t>([=, this] mutable { \
    return std::unique_ptr<alt_t>(m().release());                   \
}),


#define PARSE_ALTERNATE(out, base_type, ...)        \
    using alt_t = base_type;                        \
    auto out = std::unique_ptr<base_type>(nullptr); \
    auto wrapped = MAKE_METHOD_VECTOR(__VA_ARGS__); \
    for (auto &&f : wrapped) {                      \
        PARSE_OPTIONAL(temp_out, f);                \
        if (temp_out != nullptr) {                  \
            out = std::move(temp_out);              \
            break;                                  \
        }                                           \
    }

#define FORWARD_AST(ast) \
    ast


#define CREATE_AST(T, ...) \
    std::make_unique<T>(BOOST_PP_SEQ_ENUM(BOOST_PP_SEQ_TRANSFORM(MOVE_AST_FOR_CREATION, BOOST_PP_EMPTY(), BOOST_PP_VARIADIC_TO_SEQ(__VA_ARGS__))))


#define CREATE_AST_CUSTOM(T, static_func, ...) \
    T::static_func(BOOST_PP_SEQ_ENUM(BOOST_PP_SEQ_TRANSFORM(MOVE_AST_FOR_CREATION, BOOST_PP_EMPTY(), BOOST_PP_VARIADIC_TO_SEQ(__VA_ARGS__))))


#define MOVE_AST_FOR_CREATION(_1, _2, ast) std::move(ast)


#define CREATE_AST_WITH_BASE(out, T, base) \
    auto out = T::new_empty();\
    *static_cast<decltype(base)::pointer>(out.get()) = std::move(*base.release());


namespace spp::utils::errors {
    class ErrorFormatter;
    struct SyntacticError;
}


namespace spp::parse {
    class ParserBase;
}


class spp::parse::ParserBase {
public:
    explicit ParserBase(std::vector<lex::RawToken> tokens, std::string_view file_name = "", std::unique_ptr<utils::errors::ErrorFormatter> error_formatter = nullptr);
    virtual ~ParserBase() = default;

public:
    auto get_error() const -> std::string;

protected:
    std::size_t m_pos = 0;
    std::vector<lex::RawToken> m_tokens = {};
    std::size_t m_tokens_len = 0;
    std::unique_ptr<utils::errors::SyntacticError> m_error;
    std::unique_ptr<utils::errors::ErrorFormatter> m_error_formatter;

    template <typename T>
    using parser_method_t = std::function<std::unique_ptr<T>()>;

    template <typename T>
    using parser_method_alt_t = std::function<T()>;
};


#endif //PARSER_BASE_HPP
