#pragma once

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
        if (temp_out_opt == nullptr) {     \
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
                out.emplace_back(std::move(ast)); \
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


// ensure the next token isn't of the given type
#define PARSE_NEGATE(invalid)                  \
    if (m_pos < m_tokens_len) {                \
        if (m_tokens[m_pos].type == invalid) { \
            return nullptr;                    \
        }                                      \
    }


#define PARSE_ALTERNATE_TRY(_1, base_type, method)                \
    {                                                             \
        const auto _alt_pos = m_pos;                              \
        auto _alt_tmp = method();                                 \
        if (_alt_tmp != nullptr) {                                \
            _pa = std::unique_ptr<base_type>(_alt_tmp.release()); \
            break;                                                \
        }                                                         \
        m_pos = _alt_pos;                                         \
    }


#define PARSE_ALTERNATE(out, base_type, ...)        \
    auto _pa = std::unique_ptr<base_type>(nullptr); \
    do {                                            \
        BOOST_PP_SEQ_FOR_EACH(                      \
            PARSE_ALTERNATE_TRY, base_type,         \
            BOOST_PP_VARIADIC_TO_SEQ(__VA_ARGS__))  \
        return nullptr;                             \
    } while (false);                                \
    auto out = std::move(_pa);


#define FORWARD_AST(ast) \
    ast


#define CREATE_AST(T, ...) \
    std::make_unique<T>(BOOST_PP_SEQ_ENUM(BOOST_PP_SEQ_TRANSFORM(MOVE_AST_FOR_CREATION, BOOST_PP_EMPTY(), BOOST_PP_VARIADIC_TO_SEQ(__VA_ARGS__))))


#define CREATE_AST_CUSTOM(T, static_func, ...) \
    T::static_func(BOOST_PP_SEQ_ENUM(BOOST_PP_SEQ_TRANSFORM(MOVE_AST_FOR_CREATION, BOOST_PP_EMPTY(), BOOST_PP_VARIADIC_TO_SEQ(__VA_ARGS__))))


#define MOVE_AST_FOR_CREATION(_1, _2, ast) \
    std::move((ast))


#define INJECT_CODE(code, method) \
    spp::parse::ParserSpp(spp::lex::Lexer((code)).lex()).method()
