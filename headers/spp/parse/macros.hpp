#pragma once

#define PARSE_ONCE(out, f) \
    auto out = f();        \
    if (out == nullptr) { return nullptr; }

#define PARSE_OPTIONAL(out, f)             \
    decltype(f()) out;                     \
    {                                      \
        const auto temp_opt_index = _Pos;  \
        auto temp_out_opt = f();           \
        if (temp_out_opt == nullptr) {     \
            _Pos = temp_opt_index;         \
            out = nullptr;                 \
        }                                  \
        else {                             \
            out = std::move(temp_out_opt); \
        }                                  \
    }

#define PARSE_ZERO_OR_MORE(out, f, s)          \
    auto out = Vec<decltype(f())>();           \
    {                                          \
        auto done_1_parse = false;             \
        auto temp_index = _Pos;                \
        while (true) {                         \
            if (done_1_parse) {                \
                PARSE_OPTIONAL(sep, s);        \
                if (sep == nullptr) { break; } \
            }                                  \
            auto ast = f();                    \
            if (ast == nullptr) {              \
                _Pos = temp_index;             \
                break;                         \
            }                                  \
            else {                             \
                out.emplace_back(std::move(ast)); \
                done_1_parse = true;           \
                temp_index = _Pos;             \
            }                                  \
        }                                      \
    }

#define PARSE_ONE_OR_MORE(out, f, s) \
    PARSE_ZERO_OR_MORE(out, f, s)    \
    if (out.empty()) {             \
        return nullptr;              \
    }

#define PARSE_TWO_OR_MORE(out, f, s) \
    PARSE_ONE_OR_MORE(out, f, s)     \
    if (out.size() < 2) {             \
        return nullptr;              \
    }

#define PARSE_NEGATE(invalid)                \
    if (_Pos < _TokensLen) {                 \
        if (_Tokens[_Pos].Type == invalid) { \
            return nullptr;                  \
        }                                    \
    }

// Apply m(d, x) to each x in the variadic list; 256 rescans, so up to 256 elements.
#define SPP_PP_PARENS ()
#define SPP_PP_EXPAND(...) SPP_PP_EXPAND3(SPP_PP_EXPAND3(SPP_PP_EXPAND3(SPP_PP_EXPAND3(__VA_ARGS__))))
#define SPP_PP_EXPAND3(...) SPP_PP_EXPAND2(SPP_PP_EXPAND2(SPP_PP_EXPAND2(SPP_PP_EXPAND2(__VA_ARGS__))))
#define SPP_PP_EXPAND2(...) SPP_PP_EXPAND1(SPP_PP_EXPAND1(SPP_PP_EXPAND1(SPP_PP_EXPAND1(__VA_ARGS__))))
#define SPP_PP_EXPAND1(...) __VA_ARGS__
#define SPP_PP_FOR_EACH(m, d, ...) __VA_OPT__(SPP_PP_EXPAND(SPP_PP_FOR_EACH_HELPER(m, d, __VA_ARGS__)))
#define SPP_PP_FOR_EACH_HELPER(m, d, a, ...) m(d, a) __VA_OPT__(SPP_PP_FOR_EACH_AGAIN SPP_PP_PARENS(m, d, __VA_ARGS__))
#define SPP_PP_FOR_EACH_AGAIN() SPP_PP_FOR_EACH_HELPER

#define PARSE_ALTERNATE_TRY(base_type, method)           \
    {                                                    \
        const auto _alt_pos = _Pos;                      \
        auto _alt_tmp = method();                        \
        if (_alt_tmp != nullptr) {                       \
            _pa = Unique<base_type>(_alt_tmp.release()); \
            break;                                       \
        }                                                \
        _Pos = _alt_pos;                                 \
    }

#define PARSE_ALTERNATE(out, base_type, ...)       \
    auto _pa = Unique<base_type>(nullptr);         \
    do {                                           \
        SPP_PP_FOR_EACH(                           \
            PARSE_ALTERNATE_TRY, base_type,        \
            __VA_ARGS__)                           \
        return nullptr;                            \
    } while (false);                               \
    auto out = std::move(_pa);

#define FORWARD_AST(ast) \
    ast

#define INJECT_CODE(code, method) \
    spp::parse::ParserSpp(spp::lex::Lexer((code)).Lex()).method()
