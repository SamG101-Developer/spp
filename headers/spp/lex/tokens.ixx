module;
#include <spp/macros.hpp>

export module spp.lex.tokens;
import std;


namespace spp::lex {
    SPP_EXP_CLS enum class RawTokenType : std::uint8_t;
    SPP_EXP_CLS enum class SppTokenType : std::uint8_t;
    SPP_EXP_CLS struct SppTokenSets;
    SPP_EXP_CLS class RawToken;
}

namespace spp::lex {
    SPP_EXP_FUN auto tok_to_string(SppTokenType token) noexcept -> std::string;
}


SPP_EXP_CLS enum class spp::lex::RawTokenType : std::uint8_t {
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
    TK_SEMICOLON,
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


SPP_EXP_CLS enum class spp::lex::SppTokenType : std::uint8_t {
    LX_CHARACTER,
    LX_DIGIT,
    LX_STRING,
    LX_NUMBER,
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
    TK_SEMICOLON,
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


SPP_EXP_CLS class spp::lex::RawToken {
public:
    RawTokenType type;
    std::string data;

    RawToken(RawTokenType type, std::string data);
};


SPP_EXP_FUN auto spp::lex::tok_to_string(const SppTokenType token) noexcept -> std::string {
    switch (token) {
    case SppTokenType::LX_CHARACTER: return "";
    case SppTokenType::LX_DIGIT: return "";
    case SppTokenType::LX_STRING: return "";
    case SppTokenType::LX_NUMBER: return "";
    case SppTokenType::LX_IDENTIFIER: return "";
    case SppTokenType::KW_CLS: return "cls";
    case SppTokenType::KW_FUN: return "fun";
    case SppTokenType::KW_COR: return "cor";
    case SppTokenType::KW_SUP: return "sup";
    case SppTokenType::KW_EXT: return "ext";
    case SppTokenType::KW_MUT: return "mut";
    case SppTokenType::KW_USE: return "use";
    case SppTokenType::KW_CMP: return "cmp";
    case SppTokenType::KW_LET: return "let";
    case SppTokenType::KW_TYPE: return "type";
    case SppTokenType::KW_SELF: return "self";
    case SppTokenType::KW_CASE: return "case";
    case SppTokenType::KW_ITER: return "iter";
    case SppTokenType::KW_OF: return "of";
    case SppTokenType::KW_LOOP: return "loop";
    case SppTokenType::KW_IN: return "in";
    case SppTokenType::KW_ELSE: return "else";
    case SppTokenType::KW_GEN: return "gen";
    case SppTokenType::KW_WITH: return "with";
    case SppTokenType::KW_RET: return "ret";
    case SppTokenType::KW_EXIT: return "exit";
    case SppTokenType::KW_SKIP: return "skip";
    case SppTokenType::KW_IS: return "is";
    case SppTokenType::KW_AS: return "as";
    case SppTokenType::KW_OR: return "or";
    case SppTokenType::KW_AND: return "and";
    case SppTokenType::KW_NOT: return "not";
    case SppTokenType::KW_ASYNC: return "async";
    case SppTokenType::KW_TRUE: return "true";
    case SppTokenType::KW_FALSE: return "false";
    case SppTokenType::KW_RES: return "res";
    case SppTokenType::KW_CAPS: return "caps";
    case SppTokenType::TK_EQ: return "==";
    case SppTokenType::TK_NE: return "!=";
    case SppTokenType::TK_GT: return ">";
    case SppTokenType::TK_GE: return ">=";
    case SppTokenType::TK_LT: return "<";
    case SppTokenType::TK_LE: return "<=";
    case SppTokenType::TK_ADD: return "+";
    case SppTokenType::TK_SUB: return "-";
    case SppTokenType::TK_MUL: return "*";
    case SppTokenType::TK_DIV: return "/";
    case SppTokenType::TK_REM: return "%";
    case SppTokenType::TK_POW: return "**";
    case SppTokenType::TK_BIT_IOR: return "|";
    case SppTokenType::TK_BIT_XOR: return "^";
    case SppTokenType::TK_BIT_AND: return "&";
    case SppTokenType::TK_BIT_SHL: return "<<";
    case SppTokenType::TK_BIT_SHR: return ">>";
    case SppTokenType::TK_ADD_ASSIGN: return "+=";
    case SppTokenType::TK_SUB_ASSIGN: return "-=";
    case SppTokenType::TK_MUL_ASSIGN: return "*=";
    case SppTokenType::TK_DIV_ASSIGN: return "/=";
    case SppTokenType::TK_REM_ASSIGN: return "%=";
    case SppTokenType::TK_POW_ASSIGN: return "**=";
    case SppTokenType::TK_BIT_IOR_ASSIGN: return "|=";
    case SppTokenType::TK_BIT_XOR_ASSIGN: return "^=";
    case SppTokenType::TK_BIT_AND_ASSIGN: return "&=";
    case SppTokenType::TK_BIT_SHL_ASSIGN: return "<<=";
    case SppTokenType::TK_BIT_SHR_ASSIGN: return ">>=";
    case SppTokenType::TK_LEFT_PARENTHESIS: return "(";
    case SppTokenType::TK_RIGHT_PARENTHESIS: return ")";
    case SppTokenType::TK_LEFT_SQUARE_BRACKET: return "[";
    case SppTokenType::TK_RIGHT_SQUARE_BRACKET: return "]";
    case SppTokenType::TK_LEFT_CURLY_BRACE: return "{";
    case SppTokenType::TK_RIGHT_CURLY_BRACE: return "}";
    case SppTokenType::TK_QUESTION_MARK: return "?";
    case SppTokenType::TK_DOT: return ".";
    case SppTokenType::TK_DOUBLE_DOT: return "..";
    case SppTokenType::TK_COLON: return ":";
    case SppTokenType::TK_DOUBLE_COLON: return "::";
    case SppTokenType::TK_DEREF: return "->";
    case SppTokenType::TK_BORROW: return "&";
    case SppTokenType::TK_VERTICAL_BAR: return "|";
    case SppTokenType::TK_COMMA: return ",";
    case SppTokenType::TK_SEMICOLON: return ";";
    case SppTokenType::TK_ASSIGN: return "=";
    case SppTokenType::TK_ARROW_RIGHT: return "->";
    case SppTokenType::TK_AT: return "@";
    case SppTokenType::TK_QUOTATION_MARK: return "\"";
    case SppTokenType::TK_UNDERSCORE: return "_";
    case SppTokenType::TK_EXCLAMATION_MARK: return "!";
    case SppTokenType::TK_DOUBLE_EXCLAMATION_MARK: return "!!";
    case SppTokenType::TK_DOLLAR: return "$";
    case SppTokenType::TK_SPACE: return " ";
    case SppTokenType::TK_NEWLINE: return "\n";
    case SppTokenType::SP_NO_TOK: return "";
    default: return "";
    }
}
