#ifndef TOKENS_HPP
#define TOKENS_HPP

#include <cstdint>

#include <unicode/unistr.h>


namespace spp::lex {
    enum class RawTokenType : std::uint8_t;
    enum class SppTokenType : std::uint8_t;
    class RawToken;
}


enum class spp::lex::RawTokenType : std::uint8_t {
    LX_CHARACTER,
    LX_DIGIT,

    TK_EQUALS_TO,
    TK_GREATER_THAN,
    TK_LESS_THAN,
    TK_PLUS_SIGN,
    TK_HYPHEN,
    TK_ASTERISK,
    TK_SLASH,
    TK_PERCENT_SIGN,
    TK_LEFT_PARENTHESIS,
    TK_RIGHT_PARENTHESIS,
    TK_LEFT_SQUARE_BRACKET,
    TK_RIGHT_SQUARE_BRACKET,
    TK_LEFT_CURLY_BRACE,
    TK_RIGHT_CURLY_BRACE,
    TK_QUESTION_MARK,
    TK_COLON,
    TK_AMPERSAND,
    TK_VERTICAL_BAR,
    TK_CARET,
    TK_PERIOD,
    TK_COMMA,
    TK_AT_SIGN,
    TK_UNDERSCORE,
    TK_QUOTATION_MARK,
    TK_EXCLAMATION_MARK,
    TK_DOLLAR_SIGN,
    TK_SPACE,
    TK_LINE_FEED,

    SP_UNKNOWN,
    SP_NO_TOK,
    SP_EOF,

    KW_CLS,
    KW_FUN,
    KW_COR,
    KW_SUP,
    KW_EXT,
    KW_MUT,
    KW_USE,
    KW_CMP,
    KW_LET,
    KW_TYPE,
    KW_SELF,
    KW_CASE,
    KW_ITER,
    KW_OF,
    KW_LOOP,
    KW_IN,
    KW_ELSE,
    KW_GEN,
    KW_WITH,
    KW_RET,
    KW_EXIT,
    KW_SKIP,
    KW_IS,
    KW_AS,
    KW_OR,
    KW_AND,
    KW_NOT,
    KW_ASYNC,
    KW_TRUE,
    KW_FALSE,
    KW_RES,
    KW_CAPS,
};


enum class spp::lex::SppTokenType : std::uint8_t {
    LX_CHARACTER,
    LX_DIGIT,
    LX_STRING,
    LX_IDENTIFIER,

    KW_CLS,
    KW_FUN,
    KW_COR,
    KW_SUP,
    KW_EXT,
    KW_MUT,
    KW_USE,
    KW_CMP,
    KW_LET,
    KW_TYPE,
    KW_SELF,
    KW_CASE,
    KW_ITER,
    KW_OF,
    KW_LOOP,
    KW_IN,
    KW_ELSE,
    KW_GEN,
    KW_WITH,
    KW_RET,
    KW_EXIT,
    KW_SKIP,
    KW_IS,
    KW_AS,
    KW_OR,
    KW_AND,
    KW_NOT,
    KW_ASYNC,
    KW_TRUE,
    KW_FALSE,
    KW_RES,
    KW_CAPS,

    TK_EQ,
    TK_NE,
    TK_GT,
    TK_GE,
    TK_LT,
    TK_LE,
    TK_ADD,
    TK_SUB,
    TK_MUL,
    TK_DIV,
    TK_REM,
    TK_POW,
    TK_BIT_IOR,
    TK_BIT_XOR,
    TK_BIT_AND,
    TK_BIT_SHL,
    TK_BIT_SHR,
    TK_ADD_ASSIGN,
    TK_SUB_ASSIGN,
    TK_MUL_ASSIGN,
    TK_DIV_ASSIGN,
    TK_REM_ASSIGN,
    TK_POW_ASSIGN,
    TK_BIT_IOR_ASSIGN,
    TK_BIT_XOR_ASSIGN,
    TK_BIT_AND_ASSIGN,
    TK_BIT_SHL_ASSIGN,
    TK_BIT_SHR_ASSIGN,
    TK_LEFT_PARENTHESIS,
    TK_RIGHT_PARENTHESIS,
    TK_LEFT_SQUARE_BRACKET,
    TK_RIGHT_SQUARE_BRACKET,
    TK_LEFT_CURLY_BRACE,
    TK_RIGHT_CURLY_BRACE,
    TK_QUESTION_MARK,
    TK_DOT,
    TK_DOUBLE_DOT,
    TK_COLON,
    TK_DOUBLE_COLON,
    TK_DEREF,
    TK_BORROW,
    TK_VERTICAL_BAR,
    TK_COMMA,
    TK_ASSIGN,
    TK_ARROW_RIGHT,
    TK_AT,
    TK_QUOTATION_MARK,
    TK_UNDERSCORE,
    TK_EXCLAMATION_MARK,
    TK_DOUBLE_EXCLAMATION_MARK,
    TK_DOLLAR,
    TK_SPACE,
    TK_NEWLINE,

    SP_NO_TOK
};


class spp::lex::RawToken {
public:
    RawToken(RawTokenType type, icu::UnicodeString &&data);

public:
    RawTokenType type;
    icu::UnicodeString data;
};


#endif //TOKENS_HPP
