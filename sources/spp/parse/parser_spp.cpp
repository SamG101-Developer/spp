module;
#include <spp/macros.hpp>
#include <spp/parse/macros.hpp>

#define NO_ANNOTATIONS Vec<Unique<spp::asts::AnnotationAst>>()

#define NO_TOKENS Vec<Unique<spp::asts::TokenAst>>()

module spp.parse.parser_spp;
import spp.asts._all;
import spp.asts.generate.common_types;
import spp.asts.utils.ast_utils;
import spp.parse.errors.parser_error;
import spp.parse.errors.parser_error_builder;
import spp.utils.algorithms;
import spp.utils.ptr;
import genex;
import std;

SPP_MOD_BEGIN
constexpr auto kBinChars = spp::StrView("01");
constexpr auto kOctChars = spp::StrView("01234567");
constexpr auto kHexChars = spp::StrView("0123456789abcdef");

auto spp::parse::ParserSpp::Parse()
  -> Unique<asts::ModulePrototypeAst> {
  auto root = ParseRoot();
  if (root == nullptr) {
    _ErrorBuilder->Raise();
  }
  return root;
}

auto spp::parse::ParserSpp::ParseRoot()
  -> Unique<asts::ModulePrototypeAst> {
  PARSE_ONCE(p1, ParseModulePrototype)
  PARSE_ONCE(_a, ParseEof);
  return FORWARD_AST(p1);
}

auto spp::parse::ParserSpp::ParseEof()
  -> Unique<asts::TokenAst> {
  PARSE_ONCE(p1, [this] { return ParseTokenRaw(lex::RawTokenType::SP_EOF, lex::SppTokenType::SP_NO_TOK); });
  return FORWARD_AST(p1);
}

auto spp::parse::ParserSpp::ParseModulePrototype()
  -> Unique<asts::ModulePrototypeAst> {
  PARSE_ONCE(p1, ParseModuleImplementation);
  return CREATE_AST(asts::ModulePrototypeAst, p1);
}

auto spp::parse::ParserSpp::ParseModuleImplementation()
  -> Unique<asts::ModuleImplementationAst> {
  PARSE_ZERO_OR_MORE(p1, ParseModuleMember, ParseNewline);
  return CREATE_AST(asts::ModuleImplementationAst, p1);
}

auto spp::parse::ParserSpp::ParseModuleMember()
  -> Unique<asts::Ast> {
  PARSE_ALTERNATE(
    p1, asts::Ast, ParseFunctionPrototype, ParseClassPrototype, ParseSupPrototypeExtension,
    ParseSupPrototypeFunctions, ParseGlobalUseStatement, ParseGlobalUseVarStatement,
    ParseGlobalTypeStatement, ParseGlobalCmpStatement);
  return FORWARD_AST(p1);
}

auto spp::parse::ParserSpp::ParseClassPrototype()
  -> Unique<asts::ClassPrototypeAst> {
  PARSE_ZERO_OR_MORE(p1, ParseAnnotation, ParseNothing);
  PARSE_ONCE(p2, ParseKeywordCls);
  PARSE_ONCE(p3, ParseUpperIdentifier);
  PARSE_OPTIONAL(p4, ParseGenericParameterGroup);
  PARSE_ONCE(p5, ParseClassImplementation);
  return CREATE_AST(asts::ClassPrototypeAst, p1, p2, asts::TypeIdentifierAst::FromIdentifier(*p3), p4, p5);
}

auto spp::parse::ParserSpp::ParseClassImplementation()
  -> Unique<asts::ClassImplementationAst> {
  PARSE_ONCE(p1, ParseTokenLeftCurlyBrace);
  PARSE_ZERO_OR_MORE(p2, ParseClassMember, ParseNewline);
  PARSE_ONCE(p3, ParseTokenRightCurlyBrace);
  return CREATE_AST(asts::ClassImplementationAst, p1, p2, p3);
}

auto spp::parse::ParserSpp::ParseClassMember()
  -> Unique<asts::Ast> {
  PARSE_ALTERNATE(p1, asts::Ast, ParseClassAttribute);
  return FORWARD_AST(p1);
}

auto spp::parse::ParserSpp::ParseClassAttribute()
  -> Unique<asts::ClassAttributeAst> {
  PARSE_ZERO_OR_MORE(p1, ParseAnnotation, ParseNothing);
  PARSE_ONCE(p2, ParseIdentifier);
  PARSE_ONCE(p3, ParseTokenColon);
  PARSE_ONCE(p4, ParseTypeExpression);
  PARSE_OPTIONAL(p5, ParseClassAttributeDefaultValue);
  return CREATE_AST(asts::ClassAttributeAst, p1, p2, p3, p4, p5);
}

auto spp::parse::ParserSpp::ParseClassAttributeDefaultValue()
  -> Unique<asts::ExpressionAst> {
  PARSE_ONCE(p1, ParseTokenAssign);
  PARSE_ONCE(p2, ParseExpression); // TODO: Force this "cmp" in SA?
  return FORWARD_AST(p2);
}

auto spp::parse::ParserSpp::ParseSupPrototypeFunctions()
  -> Unique<asts::SupPrototypeFunctionsAst> {
  PARSE_ONCE(p1, ParseKeywordSup);
  PARSE_OPTIONAL(p2, ParseGenericParameterGroup);
  PARSE_ONCE(p3, ParseTypeExpression);
  PARSE_ONCE(p4, ParseSupImplementation);
  return CREATE_AST(asts::SupPrototypeFunctionsAst, p1, p2, p3, p4);
}

auto spp::parse::ParserSpp::ParseSupPrototypeExtension()
  -> Unique<asts::SupPrototypeExtensionAst> {
  PARSE_ONCE(p1, ParseKeywordSup);
  PARSE_OPTIONAL(p2, ParseGenericParameterGroup);
  PARSE_ONCE(p3, ParseTypeExpression);
  PARSE_ONCE(p4, ParseKeywordExt);
  PARSE_ONCE(p5, ParseTypeExpression);
  PARSE_ONCE(p6, ParseSupImplementation);
  return CREATE_AST(asts::SupPrototypeExtensionAst, p1, p2, p3, p4, p5, p6);
}

auto spp::parse::ParserSpp::ParseSupImplementation()
  -> Unique<asts::SupImplementationAst> {
  PARSE_ONCE(p1, ParseTokenLeftCurlyBrace);
  PARSE_ZERO_OR_MORE(p2, ParseSupMember, ParseNewline);
  PARSE_ONCE(p3, ParseTokenRightCurlyBrace);
  return CREATE_AST(asts::SupImplementationAst, p1, p2, p3);
}

auto spp::parse::ParserSpp::ParseSupMember()
  -> Unique<asts::Ast> {
  PARSE_ALTERNATE(
    p1, asts::Ast, ParseFunctionPrototype, ParseSupTypeStatement, ParseSupCmpStatement);
  return FORWARD_AST(p1);
}

auto spp::parse::ParserSpp::ParseSupTypeStatement()
  -> Unique<asts::TypeStatementAst> {
  PARSE_ZERO_OR_MORE(p1, ParseAnnotation, ParseNothing);
  PARSE_ONCE(p2, ParseTypeStatement);
  p2->Annotations = std::move(p1);
  return p2;
}

auto spp::parse::ParserSpp::ParseSupCmpStatement()
  -> Unique<asts::CmpStatementAst> {
  PARSE_ZERO_OR_MORE(p1, ParseAnnotation, ParseNothing);
  PARSE_ONCE(p2, ParseCmpStatement);
  p2->Annotations = std::move(p1);
  return p2;
}

auto spp::parse::ParserSpp::ParseFunctionPrototype()
  -> Unique<asts::FunctionPrototypeAst> {
  PARSE_ALTERNATE(p1, asts::FunctionPrototypeAst, ParseSubroutinePrototype, ParseCoroutinePrototype);
  return FORWARD_AST(p1);
}

auto spp::parse::ParserSpp::ParseSubroutinePrototype()
  -> Unique<asts::SubroutinePrototypeAst> {
  PARSE_ZERO_OR_MORE(p1, ParseAnnotation, ParseNothing);
  PARSE_OPTIONAL(p2, ParseKeywordCmp);
  PARSE_ONCE(p3, ParseKeywordFun);
  PARSE_ONCE(p4, ParseIdentifier);
  PARSE_OPTIONAL(p5, ParseGenericParameterGroup);
  PARSE_ONCE(p6, ParseFunctionParameterGroup);
  PARSE_ONCE(p7, ParseTokenArrowRight);
  PARSE_ONCE(p8, ParseTypeExpression);
  PARSE_ONCE(p9, ParseFunctionImplementation);
  return CREATE_AST(asts::SubroutinePrototypeAst, p1, p2, p3, p4, p5, p6, p7, p8, p9);
}

auto spp::parse::ParserSpp::ParseCoroutinePrototype()
  -> Unique<asts::CoroutinePrototypeAst> {
  PARSE_ZERO_OR_MORE(p1, ParseAnnotation, ParseNothing);
  PARSE_ONCE(p2, ParseKeywordCor);
  PARSE_ONCE(p3, ParseIdentifier);
  PARSE_OPTIONAL(p4, ParseGenericParameterGroup);
  PARSE_ONCE(p5, ParseFunctionParameterGroup);
  PARSE_ONCE(p6, ParseTokenArrowRight);
  PARSE_ONCE(p7, ParseTypeExpression);
  PARSE_ONCE(p8, ParseFunctionImplementation);
  return CREATE_AST(asts::CoroutinePrototypeAst, p1, nullptr, p2, p3, p4, p5, p6, p7, p8);
}

auto spp::parse::ParserSpp::ParseFunctionImplementation()
  -> Unique<asts::FunctionImplementationAst> {
  PARSE_ONCE(p1, ParseTokenLeftCurlyBrace);
  PARSE_ZERO_OR_MORE(p2, ParseFunctionMember, ParseNewline);
  PARSE_ONCE(p3, ParseTokenRightCurlyBrace);
  return CREATE_AST(asts::FunctionImplementationAst, p1, p2, p3);
}

auto spp::parse::ParserSpp::ParseFunctionMember()
  -> Unique<asts::StatementAst> {
  PARSE_ONCE(p1, ParseStatement);
  return FORWARD_AST(p1);
}

auto spp::parse::ParserSpp::ParseFunctionParameterGroup()
  -> Unique<asts::FunctionParameterGroupAst> {
  PARSE_ONCE(p1, ParseTokenLeftParenthesis);
  PARSE_ZERO_OR_MORE(p2, ParseFunctionParameter, ParseTokenComma);
  PARSE_ONCE(p3, ParseTokenRightParenthesis);
  return CREATE_AST(asts::FunctionParameterGroupAst, p1, p2, p3);
}

auto spp::parse::ParserSpp::ParseFunctionParameter()
  -> Unique<asts::FunctionParameterAst> {
  PARSE_ALTERNATE(
    p1, asts::FunctionParameterAst, ParseFunctionParameterVariadic, ParseFunctionParameterOptional,
    ParseFunctionParameterRequired, ParseFunctionParameterSelf);
  return FORWARD_AST(p1);
}

auto spp::parse::ParserSpp::ParseFunctionParameterSelf()
  -> Unique<asts::FunctionParameterSelfAst> {
  PARSE_ALTERNATE(
    p1, asts::FunctionParameterSelfAst, ParseFunctionParameterSelfWithoutConvention,
    ParseFunctionParameterSelfWithConvention);
  return FORWARD_AST(p1);
}

auto spp::parse::ParserSpp::ParseFunctionParameterSelfWithConvention()
  -> Unique<asts::FunctionParameterSelfAst> {
  PARSE_ONCE(p1, ParseConvention);
  PARSE_ONCE(p2, ParseSelfIdentifier);
  return CREATE_AST(asts::FunctionParameterSelfAst, p1,
                    CREATE_AST(asts::LocalVariableSingleIdentifierAst, nullptr, p2, nullptr));
}

auto spp::parse::ParserSpp::ParseFunctionParameterSelfWithoutConvention()
  -> Unique<asts::FunctionParameterSelfAst> {
  PARSE_OPTIONAL(p1, ParseKeywordMut)
  PARSE_ONCE(p2, ParseSelfIdentifier);
  return CREATE_AST(asts::FunctionParameterSelfAst, nullptr,
                    CREATE_AST(asts::LocalVariableSingleIdentifierAst, p1, p2, nullptr));
}

auto spp::parse::ParserSpp::ParseFunctionParameterRequired()
  -> Unique<asts::FunctionParameterRequiredAst> {
  PARSE_ONCE(p1, ParseLocalVariable);
  PARSE_ONCE(p2, ParseTokenColon);
  PARSE_ONCE(p3, ParseTypeExpression);
  return CREATE_AST(asts::FunctionParameterRequiredAst, p1, p2, p3);
}

auto spp::parse::ParserSpp::ParseFunctionParameterOptional()
  -> Unique<asts::FunctionParameterOptionalAst> {
  PARSE_ONCE(p1, ParseLocalVariable);
  PARSE_ONCE(p2, ParseTokenColon);
  PARSE_ONCE(p3, ParseTypeExpression);
  PARSE_ONCE(p4, ParseTokenAssign);
  PARSE_ONCE(p5, ParseExpression); // TODO: Force this "cmp" in SA?
  return CREATE_AST(asts::FunctionParameterOptionalAst, p1, p2, p3, p4, p5);
}

auto spp::parse::ParserSpp::ParseFunctionParameterVariadic()
  -> Unique<asts::FunctionParameterVariadicAst> {
  PARSE_ONCE(p1, ParseTokenDoubleDot);
  PARSE_ONCE(p2, ParseLocalVariable);
  PARSE_ONCE(p3, ParseTokenColon);
  PARSE_ONCE(p4, ParseTypeExpression);
  return CREATE_AST(asts::FunctionParameterVariadicAst, p1, p2, p3, p4);
}

auto spp::parse::ParserSpp::ParseFunctionCallArgumentGroup()
  -> Unique<asts::FunctionCallArgumentGroupAst> {
  PARSE_ONCE(p1, ParseTokenLeftParenthesis);
  PARSE_ZERO_OR_MORE(p2, ParseFunctionCallArgument, ParseTokenComma);
  PARSE_ONCE(p3, ParseTokenRightParenthesis);
  return CREATE_AST(asts::FunctionCallArgumentGroupAst, p1, p2, p3);
}

auto spp::parse::ParserSpp::ParseFunctionCallArgument()
  -> Unique<asts::FunctionCallArgumentAst> {
  PARSE_ALTERNATE(
    p1, asts::FunctionCallArgumentAst, ParseFunctionCallArgumentKeyword,
    ParseFunctionCallArgumentPositional);
  return FORWARD_AST(p1);
}

auto spp::parse::ParserSpp::ParseFunctionCallArgumentPositional()
  -> Unique<asts::FunctionCallArgumentPositionalAst> {
  PARSE_OPTIONAL(p1, ParseConvention);
  PARSE_OPTIONAL(p2, ParseTokenDoubleDot)
  PARSE_ONCE(p3, ParseExpression);
  return CREATE_AST(asts::FunctionCallArgumentPositionalAst, p1, p2, p3);
}

auto spp::parse::ParserSpp::ParseFunctionCallArgumentKeyword()
  -> Unique<asts::FunctionCallArgumentKeywordAst> {
  PARSE_ONCE(p1, ParseIdentifier);
  PARSE_ONCE(p2, ParseTokenAssign);
  PARSE_OPTIONAL(p3, ParseConvention);
  PARSE_ONCE(p4, ParseExpression);
  return CREATE_AST(asts::FunctionCallArgumentKeywordAst, p1, p2, p3, p4);
}

auto spp::parse::ParserSpp::ParseGenericParameterGroup()
  -> Unique<asts::GenericParameterGroupAst> {
  PARSE_ONCE(p1, ParseTokenLeftSquareBracket);
  PARSE_ZERO_OR_MORE(p2, ParseGenericParameter, ParseTokenComma);
  PARSE_ONCE(p3, ParseTokenRightSquareBracket);
  return CREATE_AST(asts::GenericParameterGroupAst, p1, p2, p3);
}

auto spp::parse::ParserSpp::ParseGenericParameter()
  -> Unique<asts::GenericParameterAst> {
  PARSE_ALTERNATE(
    p1, asts::GenericParameterAst, ParseGenericParameterType, ParseGenericParameterComp);
  return FORWARD_AST(p1);
}

auto spp::parse::ParserSpp::ParseGenericParameterComp()
  -> Unique<asts::GenericParameterAst> {
  PARSE_ALTERNATE(
    p1, asts::GenericParameterAst, ParseGenericParameterCompVariadic, ParseGenericParameterCompOptional,
    ParseGenericParameterCompRequired);
  return FORWARD_AST(p1);
}

auto spp::parse::ParserSpp::ParseGenericParameterCompRequired()
  -> Unique<asts::GenericParameterAst> {
  PARSE_ONCE(p1, ParseKeywordCmp);
  PARSE_ONCE(p2, ParseIdentifier);
  PARSE_ONCE(p3, ParseTokenColon);
  PARSE_ONCE(p4, ParseTypeExpression);
  return CREATE_AST(
    asts::GenericParameterAst, p1, nullptr, asts::TypeIdentifierAst::FromIdentifier(*p2), nullptr, p3, p4,
    nullptr, nullptr, nullptr);
}

auto spp::parse::ParserSpp::ParseGenericParameterCompOptional()
  -> Unique<asts::GenericParameterAst> {
  PARSE_ONCE(p1, ParseKeywordCmp);
  PARSE_ONCE(p2, ParseIdentifier);
  PARSE_ONCE(p3, ParseTokenColon);
  PARSE_ONCE(p4, ParseTypeExpression);
  PARSE_ONCE(p5, ParseTokenAssign);
  PARSE_ONCE(p6, ParseExpression); // TODO: Force this "cmp" in SA?
  return CREATE_AST(
    asts::GenericParameterAst, p1, nullptr, asts::TypeIdentifierAst::FromIdentifier(*p2), nullptr, p3, p4,
    p5, nullptr, p6);
}

auto spp::parse::ParserSpp::ParseGenericParameterCompVariadic()
  -> Unique<asts::GenericParameterAst> {
  PARSE_ONCE(p1, ParseKeywordCmp);
  PARSE_ONCE(p2, ParseTokenDoubleDot);
  PARSE_ONCE(p3, ParseIdentifier);
  PARSE_ONCE(p4, ParseTokenColon);
  PARSE_ONCE(p5, ParseTypeExpression);
  return CREATE_AST(
    asts::GenericParameterAst, p1, p2, asts::TypeIdentifierAst::FromIdentifier(*p3), nullptr, p4, p5,
    nullptr, nullptr, nullptr);
}

auto spp::parse::ParserSpp::ParseGenericParameterType()
  -> Unique<asts::GenericParameterAst> {
  PARSE_ALTERNATE(
    p1, asts::GenericParameterAst, ParseGenericParameterTypeVariadic, ParseGenericParameterTypeOptional,
    ParseGenericParameterTypeRequired);
  return FORWARD_AST(p1);
}

auto spp::parse::ParserSpp::ParseGenericParameterTypeRequired()
  -> Unique<asts::GenericParameterAst> {
  PARSE_ONCE(p1, ParseTypeIdentifier);
  PARSE_OPTIONAL(p2, ParseGenericParameterTypeConstraints);
  return CREATE_AST(asts::GenericParameterAst, nullptr, nullptr, p1, p2, nullptr, nullptr, nullptr, nullptr, nullptr);
}

auto spp::parse::ParserSpp::ParseGenericParameterTypeOptional()
  -> Unique<asts::GenericParameterAst> {
  PARSE_ONCE(p1, ParseTypeIdentifier);
  PARSE_OPTIONAL(p2, ParseGenericParameterTypeConstraints);
  PARSE_ONCE(p3, ParseTokenAssign);
  PARSE_ONCE(p4, ParseTypeExpression);
  return CREATE_AST(asts::GenericParameterAst, nullptr, nullptr, p1, p2, nullptr, nullptr, p3, p4, nullptr);
}

auto spp::parse::ParserSpp::ParseGenericParameterTypeVariadic()
  -> Unique<asts::GenericParameterAst> {
  PARSE_ONCE(p1, ParseTokenDoubleDot);
  PARSE_ONCE(p2, ParseTypeIdentifier);
  PARSE_OPTIONAL(p3, ParseGenericParameterTypeConstraints);
  return CREATE_AST(asts::GenericParameterAst, nullptr, p1, p2, p3, nullptr, nullptr, nullptr, nullptr, nullptr);
}

auto spp::parse::ParserSpp::ParseGenericParameterTypeConstraints()
  -> Unique<asts::GenericParameterTypeConstraintsAst> {
  PARSE_ONCE(p1, ParseTokenColon);
  PARSE_ONE_OR_MORE(p2, ParseTypeExpression, ParseTokenBitAnd);
  return CREATE_AST(asts::GenericParameterTypeConstraintsAst, p1, p2);
}

auto spp::parse::ParserSpp::ParseGenericArgumentGroup()
  -> Unique<asts::GenericArgumentGroupAst> {
  PARSE_ONCE(p1, ParseTokenLeftSquareBracket);
  PARSE_ZERO_OR_MORE(p2, ParseGenericArgument, ParseTokenComma);
  PARSE_ONCE(p3, ParseTokenRightSquareBracket);
  return CREATE_AST(asts::GenericArgumentGroupAst, p1, p2, p3);
}

auto spp::parse::ParserSpp::ParseGenericArgument()
  -> Unique<asts::GenericArgumentAst> {
  PARSE_ALTERNATE(
    p1, asts::GenericArgumentAst, ParseGenericArgumentType, ParseGenericArgumentComp);
  return FORWARD_AST(p1);
}

auto spp::parse::ParserSpp::ParseGenericArgumentComp()
  -> Unique<asts::GenericArgumentAst> {
  PARSE_ALTERNATE(
    p1, asts::GenericArgumentAst, ParseGenericArgumentCompKeyword, ParseGenericArgumentCompPositional);
  return FORWARD_AST(p1);
}

auto spp::parse::ParserSpp::ParseGenericArgumentCompPositional()
  -> Unique<asts::GenericArgumentAst> {
  PARSE_ONCE(p1, ParseExpression); // TODO: Force this "cmp" in SA?
  return asts::GenericArgumentAst::NewComp(nullptr, std::move(p1));
}

auto spp::parse::ParserSpp::ParseGenericArgumentCompKeyword()
  -> Unique<asts::GenericArgumentAst> {
  PARSE_ONCE(p1, ParseIdentifier);
  PARSE_ONCE(p2, ParseTokenAssign);
  PARSE_ONCE(p3, ParseExpression); // TODO: Force this "cmp" in SA?
  return CREATE_AST(asts::GenericArgumentAst, asts::TypeIdentifierAst::FromIdentifier(*p1), p2, nullptr, p3);
}

auto spp::parse::ParserSpp::ParseGenericArgumentType()
  -> Unique<asts::GenericArgumentAst> {
  PARSE_ALTERNATE(
    p1, asts::GenericArgumentAst, ParseGenericArgumentTypeKeyword, ParseGenericArgumentTypePositional);
  PARSE_NEGATE(lex::RawTokenType::TK_LEFT_PARENTHESIS)
  PARSE_NEGATE(lex::RawTokenType::TK_COLON)
  return FORWARD_AST(p1);
}

auto spp::parse::ParserSpp::ParseGenericArgumentTypePositional()
  -> Unique<asts::GenericArgumentAst> {
  // 1964
  PARSE_ONCE(p1, ParseTypeExpression);
  return asts::GenericArgumentAst::NewType(nullptr, asts::AstClone(p1));
}

auto spp::parse::ParserSpp::ParseGenericArgumentTypeKeyword()
  -> Unique<asts::GenericArgumentAst> {
  PARSE_ONCE(p1, ParseUpperIdentifier);
  PARSE_ONCE(p2, ParseTokenAssign);
  PARSE_ONCE(p3, ParseTypeExpression);
  return CREATE_AST(asts::GenericArgumentAst, asts::TypeIdentifierAst::FromIdentifier(*p1), p2, p3, nullptr);
}

auto spp::parse::ParserSpp::ParseAnnotation()
  -> Unique<asts::AnnotationAst> {
  PARSE_ALTERNATE(p1, asts::AnnotationAst, ParseAnnotationCall, ParseAnnotationNoCall);
  return FORWARD_AST(p1);
}

auto spp::parse::ParserSpp::ParseAnnotationCall()
  -> Unique<asts::AnnotationAst> {
  PARSE_ONCE(p1, ParseTokenExclamationMark);
  PARSE_ONCE(p2, ParsePostfixExpressionStrictlyStaticAccessZero)
  PARSE_OPTIONAL(p3, ParseGenericArgumentGroup);
  PARSE_ONCE(p4, ParseFunctionCallArgumentGroup);
  return CREATE_AST(asts::AnnotationAst, p1, p2, p3, p4);
}

auto spp::parse::ParserSpp::ParseAnnotationNoCall()
  -> Unique<asts::AnnotationAst> {
  PARSE_ONCE(p1, ParseTokenExclamationMark);
  PARSE_ONCE(p2, ParsePostfixExpressionStrictlyStaticAccessZero);
  return CREATE_AST(asts::AnnotationAst, p1, p2, nullptr, nullptr);
}

auto spp::parse::ParserSpp::ParseExpression()
  -> Unique<asts::ExpressionAst> {
  PARSE_ONCE(p1, ParseBinaryExpression);
  return FORWARD_AST(p1);
}

auto spp::parse::ParserSpp::ParseBinaryExpression(const std::uint8_t min_prec)
  -> Unique<asts::ExpressionAst> {
  using RT = lex::RawTokenType;

  struct BinOpInfo {
    Unique<asts::TokenAst> Tok;
    std::uint8_t Prec;
    bool IsIs;
  };

  auto try_tok = [this](auto fn, const std::uint8_t prec, const bool is_is = false) -> std::optional<BinOpInfo> {
    const auto pos = _Pos;
    if (auto tok = fn()) return BinOpInfo{std::move(tok), prec, is_is};
    _Pos = pos;
    return std::nullopt;
  };

  auto try_bin_op = [&]() -> BinOpInfo {
    auto peek = _Pos;
    while (peek < _TokensLen && (_Tokens[peek].Type == RT::TK_LINE_FEED || _Tokens[peek].Type == RT::TK_SPACE))
      peek++;
    if (peek >= _TokensLen) return {};

    switch (_Tokens[peek].Type) {
      case RT::TK_ASTERISK:
        if (auto r = try_tok([this] { return ParseTokenPowAssign(); }, 1)) return std::move(*r);
        if (auto r = try_tok([this] { return ParseTokenPow(); }, 11)) return std::move(*r);
        if (auto r = try_tok([this] { return ParseTokenMulAssign(); }, 1)) return std::move(*r);
        if (auto r = try_tok([this] { return ParseTokenMul(); }, 11)) return std::move(*r);
        break;
      case RT::TK_PLUS_SIGN:
        if (auto r = try_tok([this] { return ParseTokenAddAssign(); }, 1)) return std::move(*r);
        if (auto r = try_tok([this] { return ParseTokenAdd(); }, 10)) return std::move(*r);
        break;
      case RT::TK_HYPHEN:
        if (auto r = try_tok([this] { return ParseTokenSubAssign(); }, 1)) return std::move(*r);
        if (auto r = try_tok([this] { return ParseTokenSub(); }, 10)) return std::move(*r);
        break;
      case RT::TK_SLASH:
        if (auto r = try_tok([this] { return ParseTokenDivAssign(); }, 1)) return std::move(*r);
        if (auto r = try_tok([this] { return ParseTokenDiv(); }, 11)) return std::move(*r);
        break;
      case RT::TK_PERCENT_SIGN:
        if (auto r = try_tok([this] { return ParseTokenRemAssign(); }, 1)) return std::move(*r);
        if (auto r = try_tok([this] { return ParseTokenRem(); }, 11)) return std::move(*r);
        break;
      case RT::TK_VERTICAL_BAR:
        if (auto r = try_tok([this] { return ParseTokenBitIorAssign(); }, 1)) return std::move(*r);
        if (auto r = try_tok([this] { return ParseTokenBitIor(); }, 6)) return std::move(*r);
        break;
      case RT::TK_CARET:
        if (auto r = try_tok([this] { return ParseTokenBitXorAssign(); }, 1)) return std::move(*r);
        if (auto r = try_tok([this] { return ParseTokenBitXor(); }, 7)) return std::move(*r);
        break;
      case RT::TK_AMPERSAND:
        if (auto r = try_tok([this] { return ParseTokenBitAndAssign(); }, 1)) return std::move(*r);
        if (auto r = try_tok([this] { return ParseTokenBitAnd(); }, 8)) return std::move(*r);
        break;
      case RT::TK_LESS_THAN:
        if (auto r = try_tok([this] { return ParseTokenBitShlAssign(); }, 1)) return std::move(*r);
        if (auto r = try_tok([this] { return ParseTokenBitShl(); }, 9)) return std::move(*r);
        if (auto r = try_tok([this] { return ParseTokenLessThanEquals(); }, 5)) return std::move(*r);
        if (auto r = try_tok([this] { return ParseTokenLessThan(); }, 5)) return std::move(*r);
        break;
      case RT::TK_GREATER_THAN:
        if (auto r = try_tok([this] { return ParseTokenBitShrAssign(); }, 1)) return std::move(*r);
        if (auto r = try_tok([this] { return ParseTokenBitShr(); }, 9)) return std::move(*r);
        if (auto r = try_tok([this] { return ParseTokenGreaterThanEquals(); }, 5)) return std::move(*r);
        if (auto r = try_tok([this] { return ParseTokenGreaterThan(); }, 5)) return std::move(*r);
        break;
      case RT::TK_EQUALS_TO:
        if (auto r = try_tok([this] { return ParseTokenEquals(); }, 5)) return std::move(*r);
        break;
      case RT::TK_EXCLAMATION_MARK:
        if (auto r = try_tok([this] { return ParseTokenNotEquals(); }, 5)) return std::move(*r);
        break;
      case RT::KW_OR:
        if (auto r = try_tok([this] { return ParseKeywordOr(); }, 2)) return std::move(*r);
        break;
      case RT::KW_AND:
        if (auto r = try_tok([this] { return ParseKeywordAnd(); }, 3)) return std::move(*r);
        break;
      case RT::KW_IS:
        if (auto r = try_tok([this] { return ParseKeywordIs(); }, 4, true)) return std::move(*r);
        break;
      default: break;
    }
    return {};
  };

  PARSE_ONCE(lhs, ParseUnaryExpression);

  while (true) {
    const auto saved = _Pos;
    auto bin_op = try_bin_op();
    if (!bin_op.Tok || bin_op.Prec < min_prec) {
      _Pos = saved;
      break;
    }

    if (bin_op.IsIs) {
      PARSE_ONCE(rhs, ParseCaseExpressionPatternVariantDestructure);
      lhs = CREATE_AST(asts::IsExpressionAst, lhs, bin_op.Tok, rhs);
    }
    else {
      auto rhs = ParseBinaryExpression(static_cast<std::uint8_t>(bin_op.Prec + 1));
      if (!rhs) {
        _Pos = saved;
        break;
      }
      lhs = CREATE_AST(asts::BinaryExpressionAst, lhs, bin_op.Tok, rhs);
    }
  }

  return lhs;
}

auto spp::parse::ParserSpp::ParseUnaryExpression()
  -> Unique<asts::ExpressionAst> {
  PARSE_ZERO_OR_MORE(p1, ParseUnaryExpressionOp, ParseNothing);
  PARSE_ONCE(p2, ParsePostfixExpression);
  return utils::algorithms::move_accumulate(
    p1.rbegin(), p1.rend(), std::move(p2),
    [](Unique<asts::ExpressionAst> &&acc, Unique<asts::UnaryExpressionOperatorAst> &&x) {
      return CREATE_AST(asts::UnaryExpressionAst, std::move(x), std::move(acc));
    });
}

auto spp::parse::ParserSpp::ParseUnaryExpressionOp()
  -> Unique<asts::UnaryExpressionOperatorAst> {
  PARSE_ALTERNATE(
    p1, asts::UnaryExpressionOperatorAst, ParseUnaryExpressionOpAsync);
  return FORWARD_AST(p1);
}

auto spp::parse::ParserSpp::ParseUnaryExpressionOpAsync()
  -> Unique<asts::UnaryExpressionOperatorAsyncAst> {
  PARSE_ONCE(p1, ParseKeywordAsync);
  return CREATE_AST(asts::UnaryExpressionOperatorAsyncAst, p1);
}

auto spp::parse::ParserSpp::ParsePostfixExpression()
  -> Unique<asts::ExpressionAst> {
  PARSE_ONCE(p1, ParsePrimaryExpression);
  PARSE_ZERO_OR_MORE(p2, ParsePostfixExpressionOp, ParseNothing);
  return utils::algorithms::move_accumulate(
    p2.begin(), p2.end(), std::move(p1),
    [](Unique<asts::ExpressionAst> &&acc, Unique<asts::PostfixExpressionOperatorAst> &&x) {
      return CREATE_AST(asts::PostfixExpressionAst, std::move(acc), std::move(x));
    });
}

auto spp::parse::ParserSpp::ParsePostfixExpressionOp()
  -> Unique<asts::PostfixExpressionOperatorAst> {
  PARSE_ALTERNATE(
    p1, asts::PostfixExpressionOperatorAst, ParsePostfixExpressionOpDeref,
    ParsePostfixExpressionOpEarlyReturn, ParsePostfixExpressionOpFunctionCall,
    ParsePostfixExpressionOpRuntimeMemberAccess, ParsePostfixExpressionOpStaticMemberAccess,
    ParsePostfixExpressionOpKeywordNot, ParsePostfixExpressionOpKeywordRes,
    ParsePostfixExpressionOpKeywordAwait,
    ParsePostfixExpressionOpSlice, ParsePostfixExpressionOpIndex);
  return FORWARD_AST(p1);
}

auto spp::parse::ParserSpp::ParsePostfixExpressionOpDeref()
  -> Unique<asts::PostfixExpressionOperatorDerefAst> {
  PARSE_ONCE(p1, ParseTokenDeref);
  PARSE_NEGATE(lex::RawTokenType::LX_CHARACTER)
  return CREATE_AST(asts::PostfixExpressionOperatorDerefAst, p1);
}

auto spp::parse::ParserSpp::ParsePostfixExpressionOpEarlyReturn()
  -> Unique<asts::PostfixExpressionOperatorEarlyReturnAst> {
  PARSE_ONCE(p1, ParseTokenQuestionMark);
  return CREATE_AST(asts::PostfixExpressionOperatorEarlyReturnAst, p1);
}

auto spp::parse::ParserSpp::ParsePostfixExpressionOpFunctionCall()
  -> Unique<asts::PostfixExpressionOperatorFunctionCallAst> {
  if (_LineFeedAhead()) { return nullptr; }
  PARSE_OPTIONAL(p1, ParseGenericArgumentGroup);
  PARSE_ONCE(p2, ParseFunctionCallArgumentGroup);
  PARSE_OPTIONAL(p3, ParseFoldExpression);
  return CREATE_AST(asts::PostfixExpressionOperatorFunctionCallAst, p1, p2, p3);
}

auto spp::parse::ParserSpp::ParsePostfixExpressionOpRuntimeMemberAccess()
  -> Unique<asts::PostfixExpressionOperatorRuntimeMemberAccessAst> {
  PARSE_ONCE(p1, ParseTokenDot);
  PARSE_ALTERNATE(p2, asts::IdentifierAst, ParseIdentifier, ParseNumericIdentifier);
  return CREATE_AST(asts::PostfixExpressionOperatorRuntimeMemberAccessAst, p1, p2);
}

auto spp::parse::ParserSpp::ParsePostfixExpressionOpStaticMemberAccess()
  -> Unique<asts::PostfixExpressionOperatorStaticMemberAccessAst> {
  PARSE_ONCE(p1, ParseTokenDoubleColon);
  PARSE_ONCE(p2, ParseIdentifier);
  return CREATE_AST(asts::PostfixExpressionOperatorStaticMemberAccessAst, p1, p2);
}

auto spp::parse::ParserSpp::ParsePostfixExpressionOpKeywordNot()
  -> Unique<asts::PostfixExpressionOperatorKeywordNotAst> {
  PARSE_ONCE(p1, ParseTokenDot);
  PARSE_ONCE(p2, ParseKeywordNot);
  return CREATE_AST(asts::PostfixExpressionOperatorKeywordNotAst, p1, p2);
}

auto spp::parse::ParserSpp::ParsePostfixExpressionOpKeywordAwait()
  -> Unique<asts::PostfixExpressionOperatorKeywordAwaitAst> {
  PARSE_ONCE(p1, ParseTokenDot);
  PARSE_ONCE(p2, ParseKeywordAwait);
  return CREATE_AST(asts::PostfixExpressionOperatorKeywordAwaitAst, p1, p2);
}

auto spp::parse::ParserSpp::ParsePostfixExpressionOpKeywordRes()
  -> Unique<asts::PostfixExpressionOperatorKeywordResAst> {
  PARSE_ONCE(p1, ParseTokenDot);
  PARSE_ONCE(p2, ParseKeywordRes);
  PARSE_ONCE(p3, ParseFunctionCallArgumentGroup)
  return CREATE_AST(asts::PostfixExpressionOperatorKeywordResAst, p1, p2, p3);
}

auto spp::parse::ParserSpp::ParsePostfixExpressionOpIndex()
  -> Unique<asts::PostfixExpressionOperatorIndexAst> {
  if (_LineFeedAhead()) { return nullptr; }
  PARSE_ONCE(p1, ParseTokenLeftSquareBracket);
  PARSE_OPTIONAL(p2, ParseKeywordMut);
  PARSE_ONCE(p3, ParseExpression);
  PARSE_ONCE(p4, ParseTokenRightSquareBracket);
  return CREATE_AST(asts::PostfixExpressionOperatorIndexAst, p1, p2, p3, p4);
}

auto spp::parse::ParserSpp::ParsePostfixExpressionOpSlice()
  -> Unique<asts::PostfixExpressionOperatorSliceAst> {
  if (_LineFeedAhead()) { return nullptr; }
  PARSE_ONCE(p1, ParseTokenLeftSquareBracket);
  PARSE_OPTIONAL(p2, ParseKeywordMut);
  PARSE_OPTIONAL(p3, ParseExpression);
  PARSE_ONCE(p4, ParseKeywordTo);
  PARSE_OPTIONAL(p5, ParseExpression);
  PARSE_ONCE(p6, ParseTokenRightSquareBracket);
  return CREATE_AST(asts::PostfixExpressionOperatorSliceAst, p1, p2, p3, p4, p5, p6);
}

auto spp::parse::ParserSpp::ParsePostfixExpressionStrictlyStaticAccessZero()
  -> Unique<asts::ExpressionAst> {
  PARSE_ONCE(p1, ParseIdentifierAsExpression);
  PARSE_ZERO_OR_MORE(p2, ParsePostfixExpressionOpStaticMemberAccess, ParseNothing);
  return utils::algorithms::move_accumulate(
    p2.begin(), p2.end(), std::move(p1),
    [](Unique<asts::ExpressionAst> &&acc, Unique<asts::PostfixExpressionOperatorStaticMemberAccessAst> &&x) {
      return CREATE_AST(asts::PostfixExpressionAst, std::move(acc), std::move(x));
    });
}

auto spp::parse::ParserSpp::ParsePostfixExpressionStrictlyStaticAccessOne()
  -> Unique<asts::ExpressionAst> {
  PARSE_ONCE(p1, ParseIdentifierAsExpression);
  PARSE_ONE_OR_MORE(p2, ParsePostfixExpressionOpStaticMemberAccess, ParseNothing);
  return utils::algorithms::move_accumulate(
    p2.begin(), p2.end(), std::move(p1),
    [](Unique<asts::ExpressionAst> &&acc, Unique<asts::PostfixExpressionOperatorStaticMemberAccessAst> &&x) {
      return CREATE_AST(asts::PostfixExpressionAst, std::move(acc), std::move(x));
    });
}

auto spp::parse::ParserSpp::ParsePrimaryExpression()
  -> Unique<asts::ExpressionAst> {
  PARSE_ALTERNATE(
    p1, asts::PrimaryExpressionAst, ParseClosureExpression, ParseParenthesisedExpression, ParseLiteral,
    ParseObjectInitializer, ParseCaseOfExpression, ParseCaseExpression, ParseLoopExpression,
    ParseGenExpression, ParseTypeExpression, ParseIdentifier,
    ParseSelfIdentifier, [this] { return ParseInnerScopeExpression([this] { return ParseStatement(); }); },
    ParseFoldExpression);
  return FORWARD_AST(p1);
}

auto spp::parse::ParserSpp::ParseParenthesisedExpression()
  -> Unique<asts::ParenthesisedExpressionAst> {
  PARSE_ONCE(p1, ParseTokenLeftParenthesis);
  PARSE_ONCE(p2, ParseExpression);
  PARSE_ONCE(p3, ParseTokenRightParenthesis);
  return CREATE_AST(asts::ParenthesisedExpressionAst, p1, p2, p3);
}

auto spp::parse::ParserSpp::ParseFoldExpression()
  -> Unique<asts::FoldExpressionAst> {
  PARSE_ONCE(p1, ParseTokenDoubleDot);
  return CREATE_AST(asts::FoldExpressionAst, p1);
}

auto spp::parse::ParserSpp::ParseCaseExpression()
  -> Unique<asts::CaseExpressionAst> {
  PARSE_ONCE(p1, ParseKeywordCase);
  PARSE_ONCE(p2, ParseExpression);
  PARSE_ONCE(p3, [this] { return ParseInnerScopeExpression([this] { return ParseStatement(); }); })
  PARSE_ZERO_OR_MORE(p4, ParseCaseExpressionBranch, ParseNothing);
  return CREATE_AST_CUSTOM(asts::CaseExpressionAst, NewNonPatternMatch, p1, p2, p3, p4);
}

auto spp::parse::ParserSpp::ParseCaseExpressionBranch()
  -> Unique<asts::CaseExpressionBranchAst> {
  PARSE_ALTERNATE(
    p1, asts::CaseExpressionBranchAst, ParseCaseExpressionBranchElseCase, ParseCaseExpressionBranchElse)
  return p1;
}

auto spp::parse::ParserSpp::ParseCaseExpressionBranchElse()
  -> Unique<asts::CaseExpressionBranchAst> {
  PARSE_ONCE(p1, ParseCaseExpressionPatternVariantElse);
  PARSE_ONCE(p2, [this] { return ParseInnerScopeExpression([this] { return ParseStatement(); }); })

  auto temp = Vec<decltype(p1)>(1);
  temp[0] = std::move(p1);
  return CREATE_AST(asts::CaseExpressionBranchAst, nullptr, std::move(temp), nullptr, p2);
}

auto spp::parse::ParserSpp::ParseCaseExpressionBranchElseCase()
  -> Unique<asts::CaseExpressionBranchAst> {
  PARSE_ONCE(p1, ParseCaseExpressionPatternVariantElseCase);

  auto else_pattern = dynamic_unique_cast<asts::CasePatternVariantAst>(
    CREATE_AST(asts::CasePatternVariantElseAst, nullptr));
  auto temp_1 = Vec<Unique<asts::StatementAst>>();
  auto ptr = dynamic_unique_cast<asts::StatementAst>(
    std::move(dynamic_unique_cast<asts::CasePatternVariantElseCaseAst>(std::move(p1))->CaseExpr));
  temp_1.EmplaceBack(std::move(ptr));

  auto else_case_body = CREATE_AST(asts::InnerScopeExpressionAst, nullptr, std::move(temp_1), nullptr);
  auto temp_2 = Vec<decltype(else_pattern)>();
  temp_2.EmplaceBack(std::move(else_pattern));
  return CREATE_AST(asts::CaseExpressionBranchAst, nullptr, temp_2, nullptr, else_case_body);
}

auto spp::parse::ParserSpp::ParseCaseOfExpression()
  -> Unique<asts::CaseExpressionAst> {
  PARSE_ONCE(p1, ParseKeywordCase);
  PARSE_ONCE(p2, ParseExpression);
  PARSE_ONCE(p3, ParseKeywordOf);
  PARSE_ONCE(p4, ParseTokenLeftCurlyBrace);
  PARSE_ONE_OR_MORE(p5, ParseCaseOfExpressionBranch, ParseNewline);
  PARSE_ONCE(p6, ParseTokenRightCurlyBrace);
  return CREATE_AST(asts::CaseExpressionAst, p1, p2, p3, p5);
}

auto spp::parse::ParserSpp::ParseCaseOfExpressionBranch()
  -> Unique<asts::CaseExpressionBranchAst> {
  PARSE_ALTERNATE(
    p1, asts::CaseExpressionBranchAst, ParseCaseOfExpressionBranchDestructuring,
    ParseCaseOfExpressionBranchComparing, ParseCaseExpressionBranchElseCase,
    ParseCaseExpressionBranchElse);
  return FORWARD_AST(p1);
}

auto spp::parse::ParserSpp::ParseCaseOfExpressionBranchDestructuring()
  -> Unique<asts::CaseExpressionBranchAst> {
  PARSE_ONCE(p1, ParseKeywordIs);
  PARSE_ONCE(p2, ParseCaseExpressionPatternVariantDestructure);
  PARSE_OPTIONAL(p3, ParsePatternGuard)
  PARSE_ONCE(p4, [this] { return ParseInnerScopeExpression([this] { return ParseStatement(); }); })
  auto temp = Vec<decltype(p2)>(1);
  temp[0] = std::move(p2);
  return CREATE_AST(asts::CaseExpressionBranchAst, p1, temp, p3, p4);
}

auto spp::parse::ParserSpp::ParseCaseOfExpressionBranchComparing()
  -> Unique<asts::CaseExpressionBranchAst> {
  PARSE_ONCE(p1, ParseBooleanComparisonOp)
  PARSE_ONE_OR_MORE(p2, ParseCaseExpressionPatternVariantExpression, ParseTokenComma);
  PARSE_ONCE(p3, [this] { return ParseInnerScopeExpression([this] { return ParseStatement(); }); })
  return CREATE_AST(asts::CaseExpressionBranchAst, p1, p2, nullptr, p3);
}

auto spp::parse::ParserSpp::ParseCaseExpressionPatternVariantDestructure()
  -> Unique<asts::CasePatternVariantAst> {
  PARSE_ALTERNATE(
    p1, asts::CasePatternVariantAst, ParseCaseExpressionPatternVariantDestructureArray,
    ParseCaseExpressionPatternVariantDestructureObject,
    ParseCaseExpressionPatternVariantDestructureTuple);
  return FORWARD_AST(p1);
}

auto spp::parse::ParserSpp::ParseCaseExpressionPatternVariantDestructureArray()
  -> Unique<asts::CasePatternVariantAst> {
  PARSE_ONCE(p1, ParseTokenLeftSquareBracket);
  PARSE_ZERO_OR_MORE(p2, ParseCaseExpressionPatternNestedForDestructureArray, ParseTokenComma);
  PARSE_ONCE(p3, ParseTokenRightSquareBracket);
  return CREATE_AST(asts::CasePatternVariantDestructureArrayAst, p1, p2, p3);
}

auto spp::parse::ParserSpp::ParseCaseExpressionPatternVariantDestructureObject()
  -> Unique<asts::CasePatternVariantAst> {
  PARSE_ONCE(p1, ParseTypeExpressionSimple)
  PARSE_ONCE(p2, ParseTokenLeftParenthesis);
  PARSE_ZERO_OR_MORE(p3, ParseCaseExpressionPatternNestedForDestructureObject, ParseTokenComma);
  PARSE_ONCE(p4, ParseTokenRightParenthesis);
  return CREATE_AST(asts::CasePatternVariantDestructureObjectAst, p1, p2, p3, p4);
}

auto spp::parse::ParserSpp::ParseCaseExpressionPatternVariantDestructureTuple()
  -> Unique<asts::CasePatternVariantAst> {
  PARSE_ONCE(p1, ParseTokenLeftParenthesis);
  PARSE_ZERO_OR_MORE(p2, ParseCaseExpressionPatternNestedForDestructureTuple, ParseTokenComma);
  PARSE_ONCE(p3, ParseTokenRightParenthesis);
  return CREATE_AST(asts::CasePatternVariantDestructureTupleAst, p1, p2, p3);
}

auto spp::parse::ParserSpp::ParseCaseExpressionPatternVariantDestructureSkipSingleArgument()
  -> Unique<asts::CasePatternVariantAst> {
  PARSE_ONCE(p1, ParseTokenUnderscore);
  return CREATE_AST(asts::CasePatternVariantDestructureSkipSingleArgumentAst, p1);
}

auto spp::parse::ParserSpp::ParseCaseExpressionPatternVariantDestructureSkipMultipleArguments()
  -> Unique<asts::CasePatternVariantAst> {
  PARSE_ONCE(p1, ParseTokenDoubleDot);
  PARSE_OPTIONAL(p2, ParseCaseExpressionPatternVariantSingleIdentifier);
  return CREATE_AST(asts::CasePatternVariantDestructureSkipMultipleArgumentsAst, p1, p2);
}

auto spp::parse::ParserSpp::ParseCaseExpressionPatternVariantDestructureAttributeBinding()
  -> Unique<asts::CasePatternVariantAst> {
  PARSE_ONCE(p1, ParseIdentifier);
  PARSE_ONCE(p2, ParseTokenAssign);
  PARSE_ONCE(p3, ParseCaseExpressionPatternNestedForDestructureAttributeBinding);
  return CREATE_AST(asts::CasePatternVariantDestructureAttributeBindingAst, p1, p2, p3);
}

auto spp::parse::ParserSpp::ParseCaseExpressionPatternVariantSingleIdentifier()
  -> Unique<asts::CasePatternVariantAst> {
  PARSE_ALTERNATE(
    p1, asts::CasePatternVariantAst, ParseCaseExpressionPatternVariantSingleIdentifierWithConvention,
    ParseCaseExpressionPatternVariantSingleIdentifierWithoutConvention);
  return FORWARD_AST(p1);
}

auto spp::parse::ParserSpp::ParseCaseExpressionPatternVariantSingleIdentifierWithConvention()
  -> Unique<asts::CasePatternVariantAst> {
  PARSE_ONCE(p1, ParseConvention);
  PARSE_ONCE(p2, ParseIdentifier);
  return CREATE_AST(asts::CasePatternVariantSingleIdentifierAst, p1, nullptr, p2, nullptr);
}

auto spp::parse::ParserSpp::ParseCaseExpressionPatternVariantSingleIdentifierWithoutConvention()
  -> Unique<asts::CasePatternVariantAst> {
  PARSE_OPTIONAL(p1, ParseKeywordMut);
  PARSE_ONCE(p2, ParseIdentifier);
  return CREATE_AST(asts::CasePatternVariantSingleIdentifierAst, nullptr, p1, p2, nullptr);
}

auto spp::parse::ParserSpp::ParseCaseExpressionPatternVariantSingleIdentifierAliasable()
  -> Unique<asts::CasePatternVariantAst> {
  PARSE_ONCE(p1, ParseCaseExpressionPatternVariantSingleIdentifier);
  PARSE_OPTIONAL(p2, ParseLocalVariableSingleIdentifierAlias);
  p1->To<asts::CasePatternVariantSingleIdentifierAst>()->Alias = std::move(p2);
  return FORWARD_AST(p1);
}

auto spp::parse::ParserSpp::ParseCaseExpressionPatternVariantLiteral()
  -> Unique<asts::CasePatternVariantAst> {
  PARSE_ALTERNATE(
    p1, asts::LiteralAst, ParseLiteralFloat, ParseLiteralInteger, ParseLiteralChar, ParseLiteralString,
    ParseLiteralBoolean);
  return CREATE_AST(asts::CasePatternVariantLiteralAst, p1);
}

auto spp::parse::ParserSpp::ParseCaseExpressionPatternVariantExpression()
  -> Unique<asts::CasePatternVariantAst> {
  PARSE_ONCE(p1, ParseExpression);
  return CREATE_AST(asts::CasePatternVariantExpressionAst, p1);
}

auto spp::parse::ParserSpp::ParseCaseExpressionPatternVariantElse()
  -> Unique<asts::CasePatternVariantAst> {
  PARSE_ONCE(p1, ParseKeywordElse);
  return CREATE_AST(asts::CasePatternVariantElseAst, p1);
}

auto spp::parse::ParserSpp::ParseCaseExpressionPatternVariantElseCase()
  -> Unique<asts::CasePatternVariantAst> {
  PARSE_ONCE(p1, ParseKeywordElse);
  PARSE_ONCE(p2, ParseCaseExpression);
  return CREATE_AST(asts::CasePatternVariantElseCaseAst, p1, p2);
}

auto spp::parse::ParserSpp::ParseCaseExpressionPatternNestedForDestructureArray()
  -> Unique<asts::CasePatternVariantAst> {
  PARSE_ALTERNATE(
    p1, asts::CasePatternVariantAst, ParseCaseExpressionPatternVariantDestructureSkipSingleArgument,
    ParseCaseExpressionPatternVariantDestructureSkipMultipleArguments,
    ParseCaseExpressionPatternVariantDestructureArray,
    ParseCaseExpressionPatternVariantDestructureTuple,
    ParseCaseExpressionPatternVariantDestructureObject,
    ParseCaseExpressionPatternVariantSingleIdentifier,
    ParseCaseExpressionPatternVariantLiteral);
  return FORWARD_AST(p1);
}

auto spp::parse::ParserSpp::ParseCaseExpressionPatternNestedForDestructureObject()
  -> Unique<asts::CasePatternVariantAst> {
  PARSE_ALTERNATE(
    p1, asts::CasePatternVariantAst, ParseCaseExpressionPatternVariantDestructureAttributeBinding,
    ParseCaseExpressionPatternVariantDestructureSkipMultipleArguments,
    ParseCaseExpressionPatternVariantSingleIdentifierAliasable);
  return FORWARD_AST(p1);
}

auto spp::parse::ParserSpp::ParseCaseExpressionPatternNestedForDestructureTuple()
  -> Unique<asts::CasePatternVariantAst> {
  PARSE_ALTERNATE(
    p1, asts::CasePatternVariantAst, ParseCaseExpressionPatternVariantDestructureSkipSingleArgument,
    ParseCaseExpressionPatternVariantDestructureSkipMultipleArguments,
    ParseCaseExpressionPatternVariantDestructureArray,
    ParseCaseExpressionPatternVariantDestructureTuple,
    ParseCaseExpressionPatternVariantDestructureObject,
    ParseCaseExpressionPatternVariantSingleIdentifier,
    ParseCaseExpressionPatternVariantLiteral);
  return FORWARD_AST(p1);
}

auto spp::parse::ParserSpp::ParseCaseExpressionPatternNestedForDestructureAttributeBinding()
  -> Unique<asts::CasePatternVariantAst> {
  PARSE_ALTERNATE(
    p1, asts::CasePatternVariantAst, ParseCaseExpressionPatternVariantDestructureArray,
    ParseCaseExpressionPatternVariantDestructureObject,
    ParseCaseExpressionPatternVariantDestructureTuple,
    ParseCaseExpressionPatternVariantLiteral);
  return FORWARD_AST(p1);
}

auto spp::parse::ParserSpp::ParsePatternGuard()
  -> Unique<asts::PatternGuardAst> {
  PARSE_ONCE(p1, ParseKeywordAnd);
  PARSE_ONCE(p2, ParseExpression);
  return CREATE_AST(asts::PatternGuardAst, p1, p2);
}

auto spp::parse::ParserSpp::ParseBooleanComparisonOp()
  -> Unique<asts::TokenAst> {
  PARSE_ALTERNATE(
    p1, asts::TokenAst, ParseTokenEquals, ParseTokenNotEquals, ParseTokenLessThanEquals,
    ParseTokenGreaterThanEquals, ParseTokenLessThan, ParseTokenGreaterThan);
  return FORWARD_AST(p1);
}

auto spp::parse::ParserSpp::ParseLoopExpression()
  -> Unique<asts::LoopExpressionAst> {
  PARSE_ALTERNATE(
    p1, asts::LoopExpressionAst, ParseLoopConditionalExpression, ParseLoopIterableExpression);
  return FORWARD_AST(p1);
}

auto spp::parse::ParserSpp::ParseLoopConditionalExpression()
  -> Unique<asts::LoopConditionalExpressionAst> {
  PARSE_ONCE(p1, ParseKeywordLoop);
  PARSE_ONCE(p2, ParseExpression);
  PARSE_ONCE(p3, [this] { return ParseInnerScopeExpression([this] { return ParseStatement(); }); })
  PARSE_OPTIONAL(p4, ParseLoopElseStatement);
  return CREATE_AST(asts::LoopConditionalExpressionAst, p1, p2, p3, p4);
}

auto spp::parse::ParserSpp::ParseLoopIterableExpression()
  -> Unique<asts::LoopIterableExpressionAst> {
  PARSE_ONCE(p1, ParseKeywordLoop);
  PARSE_ONCE(p2, ParseLocalVariable);
  PARSE_ONCE(p3, ParseKeywordIn);
  PARSE_ONCE(p4, ParseExpression);
  PARSE_ONCE(p5, [this] { return ParseInnerScopeExpression([this] { return ParseStatement(); }); })
  PARSE_OPTIONAL(p6, ParseLoopElseStatement);
  return CREATE_AST(asts::LoopIterableExpressionAst, p1, p2, p3, p4, p5, p6);
}

auto spp::parse::ParserSpp::ParseLoopElseStatement()
  -> Unique<asts::LoopElseStatementAst> {
  PARSE_ONCE(p1, ParseKeywordElse);
  PARSE_ONCE(p2, [this] { return ParseInnerScopeExpression([this] { return ParseStatement(); }); })
  return CREATE_AST(asts::LoopElseStatementAst, p1, p2);
}

auto spp::parse::ParserSpp::ParseGenExpression()
  -> Unique<asts::GenExpressionAst> {
  PARSE_ALTERNATE(
    p1, asts::GenExpressionAst, ParseGenExpressionWithExpression, ParseGenExpressionWithoutExpression);
  return FORWARD_AST(p1);
}

auto spp::parse::ParserSpp::ParseGenExpressionWithExpression()
  -> Unique<asts::GenExpressionAst> {
  PARSE_ONCE(p1, ParseKeywordGen);
  PARSE_OPTIONAL(p2, ParseConvention);
  PARSE_ONCE(p3, ParseExpression);
  return CREATE_AST(asts::GenExpressionAst, p1, p2, p3);
}

auto spp::parse::ParserSpp::ParseGenExpressionWithoutExpression()
  -> Unique<asts::GenExpressionAst> {
  PARSE_ONCE(p1, ParseKeywordGen);
  return CREATE_AST(asts::GenExpressionAst, p1, nullptr, nullptr);
}

auto spp::parse::ParserSpp::ParseGenUnrollExpression()
  -> Unique<asts::GenWithExpressionAst> {
  PARSE_ONCE(p1, ParseKeywordGen);
  PARSE_ONCE(p2, ParseKeywordWith);
  PARSE_ONCE(p3, ParseExpression);
  return CREATE_AST(asts::GenWithExpressionAst, p1, p2, p3);
}

auto spp::parse::ParserSpp::ParseInnerScope(auto &&parser)
  -> Unique<asts::InnerScopeAst<decltype(parser())>> {
  PARSE_ONCE(p1, ParseTokenLeftCurlyBrace);
  PARSE_ZERO_OR_MORE(p2, parser, ParseNewline);
  PARSE_ONCE(p3, ParseTokenRightCurlyBrace);
  return CREATE_AST(asts::InnerScopeAst<decltype(parser())>, p1, p2, p3);
}

auto spp::parse::ParserSpp::ParseInnerScopeExpression(auto &&parser)
  -> Unique<asts::InnerScopeExpressionAst> {
  PARSE_ONCE(p1, ParseTokenLeftCurlyBrace);
  PARSE_ZERO_OR_MORE(p2, parser, ParseNewline);
  PARSE_ONCE(p3, ParseTokenRightCurlyBrace);
  return CREATE_AST(asts::InnerScopeExpressionAst, p1, p2, p3);
}

auto spp::parse::ParserSpp::ParseStatement()
  -> Unique<asts::StatementAst> {
  PARSE_ALTERNATE(
    p1, asts::StatementAst, ParseTypeStatement, ParseLetStatement,
    ParseRetStatement, ParseExitStatement, ParseExitStatementWithValue, ParseSkipStatement,
    ParseDeferStatement, ParseAssignmentStatement, ParseGenUnrollExpression, ParseExpression);
  return FORWARD_AST(p1);
}

auto spp::parse::ParserSpp::ParseAssignmentStatement()
  -> Unique<asts::AssignmentStatementAst> {
  PARSE_ONE_OR_MORE(p1, ParseAssignmentTarget, ParseTokenComma);
  PARSE_ONCE(p2, ParseTokenAssign);
  PARSE_ONE_OR_MORE(p3, ParseExpression, ParseTokenComma);
  return CREATE_AST(asts::AssignmentStatementAst, p1, p2, p3);
}

auto spp::parse::ParserSpp::ParseAssignmentTarget()
  -> Unique<asts::ExpressionAst> {
  // Skip binary expression on lhs of assignment.
  PARSE_ONCE(p1, ParseAssignmentTargetPostfixExpression);
  return FORWARD_AST(p1);
}

auto spp::parse::ParserSpp::ParseAssignmentTargetPostfixExpression()
  -> Unique<asts::ExpressionAst> {
  PARSE_ONCE(p1, ParseAssignmentTargetPrimaryExpression);
  PARSE_ZERO_OR_MORE(p2, ParseAssignmentTargetPostfixExpressionOp, ParseNothing);
  return utils::algorithms::move_accumulate(
    p2.begin(), p2.end(), std::move(p1),
    [](Unique<asts::ExpressionAst> &&acc, Unique<asts::PostfixExpressionOperatorAst> &&x) {
      return CREATE_AST(asts::PostfixExpressionAst, std::move(acc), std::move(x));
    });
}

auto spp::parse::ParserSpp::ParseAssignmentTargetPostfixExpressionOp()
  -> Unique<asts::PostfixExpressionOperatorAst> {
  PARSE_ALTERNATE(
    p1, asts::PostfixExpressionOperatorAst, ParsePostfixExpressionOpDeref,
    ParsePostfixExpressionOpFunctionCall, ParsePostfixExpressionOpRuntimeMemberAccess,
    ParsePostfixExpressionOpStaticMemberAccess, ParsePostfixExpressionOpSlice,
    ParsePostfixExpressionOpIndex);
  return FORWARD_AST(p1);
}

auto spp::parse::ParserSpp::ParseAssignmentTargetPrimaryExpression()
  -> Unique<asts::ExpressionAst> {
  PARSE_ALTERNATE(
    p1, asts::PrimaryExpressionAst, ParseIdentifier, ParseSelfIdentifier);
  return FORWARD_AST(p1);
}

auto spp::parse::ParserSpp::ParseDeferStatement()
  -> Unique<asts::DeferStatementAst> {
  PARSE_ONCE(p1, ParseKeywordDefer);
  PARSE_ONCE(p2, ParseExpression);
  return CREATE_AST(asts::DeferStatementAst, p1, p2);
}

auto spp::parse::ParserSpp::ParseRetStatement()
  -> Unique<asts::RetStatementAst> {
  PARSE_ONCE(p1, ParseKeywordRet);
  PARSE_OPTIONAL(p2, ParseExpression);
  return CREATE_AST(asts::RetStatementAst, p1, p2);
}

auto spp::parse::ParserSpp::ParseExitStatement()
  -> Unique<asts::LoopControlFlowStatementAst> {
  PARSE_ONE_OR_MORE(p1, ParseKeywordExit, ParseSpace);
  PARSE_ONCE(p2, ParseKeywordSkip);
  return CREATE_AST(asts::LoopControlFlowStatementAst, p1, p2, nullptr);
}

auto spp::parse::ParserSpp::ParseExitStatementWithValue()
  -> Unique<asts::LoopControlFlowStatementAst> {
  PARSE_ONE_OR_MORE(p1, ParseKeywordExit, ParseSpace);
  PARSE_OPTIONAL(p2, ParseExpression)
  return CREATE_AST(asts::LoopControlFlowStatementAst, p1, nullptr, p2);
}

auto spp::parse::ParserSpp::ParseSkipStatement()
  -> Unique<asts::LoopControlFlowStatementAst> {
  PARSE_ONCE(p1, ParseKeywordSkip);
  return CREATE_AST(asts::LoopControlFlowStatementAst, NO_TOKENS, p1, nullptr);
}

auto spp::parse::ParserSpp::ParseUseStatement()
  -> Unique<asts::UseStatementAst> {
  PARSE_ONCE(p1, ParseKeywordUse);
  PARSE_ONCE(p2, ParseTypeExpressionSimple)
  return CREATE_AST(asts::UseStatementAst, NO_ANNOTATIONS, p1, p2);
}

auto spp::parse::ParserSpp::ParseUseVarStatement()
  -> Unique<asts::UseStatementVariableAst> {
  PARSE_ONCE(p1, ParseKeywordUse);
  PARSE_ONCE(p2, ParsePostfixExpressionStrictlyStaticAccessOne);
  return CREATE_AST(asts::UseStatementVariableAst, NO_ANNOTATIONS, p1, p2);
}

auto spp::parse::ParserSpp::ParseTypeStatement()
  -> Unique<asts::TypeStatementAst> {
  PARSE_ONCE(p1, ParseKeywordType);
  PARSE_ONCE(p2, ParseUpperIdentifier);
  PARSE_OPTIONAL(p3, ParseGenericParameterGroup);
  PARSE_ONCE(p4, ParseTokenAssign);
  PARSE_ONCE(p5, ParseTypeExpression);
  return CREATE_AST(asts::TypeStatementAst, NO_ANNOTATIONS, p1, asts::TypeIdentifierAst::FromIdentifier(*p2), p3, p4,
                    p5);
}

auto spp::parse::ParserSpp::ParseCmpStatement()
  -> Unique<asts::CmpStatementAst> {
  PARSE_ONCE(p1, ParseKeywordCmp);
  PARSE_ONCE(p2, ParseIdentifier);
  PARSE_ONCE(p3, ParseTokenColon);
  PARSE_ONCE(p4, ParseTypeExpression);
  PARSE_ONCE(p5, ParseTokenAssign);
  PARSE_ONCE(p6, ParseExpression); // TODO: Force this "cmp" in SA?
  return CREATE_AST(asts::CmpStatementAst, NO_ANNOTATIONS, p1, p2, p3, p4, p5, p6);
}

auto spp::parse::ParserSpp::ParseLetStatement()
  -> Unique<asts::LetStatementAst> {
  PARSE_ALTERNATE(p1, asts::LetStatementAst, ParseLetStatementInitialized, ParseLetStatementUninitialized);
  return FORWARD_AST(p1);
}

auto spp::parse::ParserSpp::ParseLetStatementInitialized()
  -> Unique<asts::LetStatementAst> {
  PARSE_ONCE(p1, ParseKeywordLet);
  PARSE_ONCE(p2, ParseLocalVariable);
  PARSE_OPTIONAL(p3, ParseLetStatementInitializedExplicitType);
  PARSE_ONCE(p4, ParseTokenAssign);
  PARSE_ONCE(p5, ParseExpression);
  return CREATE_AST(asts::LetStatementInitializedAst, p1, p2, p3, p4, p5);
}

auto spp::parse::ParserSpp::ParseLetStatementInitializedExplicitType()
  -> Unique<asts::TypeAst> {
  PARSE_ONCE(p1, ParseTokenColon);
  PARSE_ONCE(p2, ParseTypeExpression);
  return FORWARD_AST(p2);
}

auto spp::parse::ParserSpp::ParseLetStatementUninitialized()
  -> Unique<asts::LetStatementAst> {
  PARSE_ONCE(p1, ParseKeywordLet);
  PARSE_ONCE(p2, ParseLocalVariable);
  PARSE_ONCE(p3, ParseTokenColon);
  PARSE_ONCE(p4, ParseTypeExpression);
  return CREATE_AST(asts::LetStatementUninitializedAst, p1, p2, p3, p4);
}

auto spp::parse::ParserSpp::ParseGlobalUseStatement()
  -> Unique<asts::UseStatementAst> {
  PARSE_ZERO_OR_MORE(p1, ParseAnnotation, ParseNothing);
  PARSE_ONCE(p2, ParseUseStatement);
  p2->Annotations = std::move(p1);
  return FORWARD_AST(p2);
}

auto spp::parse::ParserSpp::ParseGlobalUseVarStatement()
  -> Unique<asts::UseStatementVariableAst> {
  PARSE_ZERO_OR_MORE(p1, ParseAnnotation, ParseNothing);
  PARSE_ONCE(p2, ParseUseVarStatement);
  p2->Annotations = std::move(p1);
  return FORWARD_AST(p2);
}

auto spp::parse::ParserSpp::ParseGlobalTypeStatement()
  -> Unique<asts::TypeStatementAst> {
  PARSE_ZERO_OR_MORE(p1, ParseAnnotation, ParseNothing);
  PARSE_ONCE(p2, ParseTypeStatement);
  p2->Annotations = std::move(p1);
  return FORWARD_AST(p2);
}

auto spp::parse::ParserSpp::ParseGlobalCmpStatement()
  -> Unique<asts::CmpStatementAst> {
  PARSE_ZERO_OR_MORE(p1, ParseAnnotation, ParseNothing);
  PARSE_ONCE(p2, ParseCmpStatement);
  p2->Annotations = std::move(p1);
  return FORWARD_AST(p2);
}

auto spp::parse::ParserSpp::ParseLocalVariable()
  -> Unique<asts::LocalVariableAst> {
  PARSE_ALTERNATE(
    p1, asts::LocalVariableAst, ParseLocalVariableDestructureArray, ParseLocalVariableDestructureTuple,
    ParseLocalVariableDestructureObject, ParseLocalVariableSingleIdentifier);
  return FORWARD_AST(p1);
}

auto spp::parse::ParserSpp::ParseLocalVariableDestructureArray()
  -> Unique<asts::LocalVariableDestructureArrayAst> {
  PARSE_ONCE(p1, ParseTokenLeftSquareBracket);
  PARSE_ZERO_OR_MORE(p2, ParseLocalVariableNestedForDestructureArray, ParseTokenComma);
  PARSE_ONCE(p3, ParseTokenRightSquareBracket);
  return CREATE_AST(asts::LocalVariableDestructureArrayAst, p1, p2, p3);
}

auto spp::parse::ParserSpp::ParseLocalVariableDestructureObject()
  -> Unique<asts::LocalVariableDestructureObjectAst> {
  PARSE_ONCE(p1, ParseTypeExpressionSimple);
  PARSE_ONCE(p2, ParseTokenLeftParenthesis);
  PARSE_ZERO_OR_MORE(p3, ParseLocalVariableNestedForDestructureObject, ParseTokenComma);
  PARSE_ONCE(p4, ParseTokenRightParenthesis);
  return CREATE_AST(asts::LocalVariableDestructureObjectAst, p1, p2, p3, p4);
}

auto spp::parse::ParserSpp::ParseLocalVariableDestructureTuple()
  -> Unique<asts::LocalVariableDestructureTupleAst> {
  PARSE_ONCE(p1, ParseTokenLeftParenthesis);
  PARSE_ZERO_OR_MORE(p2, ParseLocalVariableNestedForDestructureTuple, ParseTokenComma);
  PARSE_ONCE(p3, ParseTokenRightParenthesis);
  return CREATE_AST(asts::LocalVariableDestructureTupleAst, p1, p2, p3);
}

auto spp::parse::ParserSpp::ParseLocalVariableDestructureSkipSingleArgument()
  -> Unique<asts::LocalVariableDestructureSkipSingleArgumentAst> {
  PARSE_ONCE(p1, ParseTokenUnderscore);
  return CREATE_AST(asts::LocalVariableDestructureSkipSingleArgumentAst, p1);
}

auto spp::parse::ParserSpp::ParseLocalVariableDestructureSkipMultipleArguments()
  -> Unique<asts::LocalVariableDestructureSkipMultipleArgumentsAst> {
  PARSE_ONCE(p1, ParseTokenDoubleDot);
  PARSE_OPTIONAL(p2, ParseLocalVariableSingleIdentifier);
  return CREATE_AST(asts::LocalVariableDestructureSkipMultipleArgumentsAst, p1, p2);
}

auto spp::parse::ParserSpp::ParseLocalVariableDestructureAttributeBinding()
  -> Unique<asts::LocalVariableDestructureAttributeBindingAst> {
  PARSE_ONCE(p1, ParseIdentifier);
  PARSE_ONCE(p2, ParseTokenAssign);
  PARSE_ONCE(p3, ParseLocalVariableNestedForDestructureAttributeBinding);
  return CREATE_AST(asts::LocalVariableDestructureAttributeBindingAst, p1, p2, p3);
}

auto spp::parse::ParserSpp::ParseLocalVariableSingleIdentifier()
  -> Unique<asts::LocalVariableSingleIdentifierAst> {
  PARSE_OPTIONAL(p1, ParseKeywordMut);
  PARSE_ONCE(p2, ParseIdentifier);
  return CREATE_AST(asts::LocalVariableSingleIdentifierAst, p1, p2, nullptr);
}

auto spp::parse::ParserSpp::ParseLocalVariableSingleIdentifierAliasable()
  -> Unique<asts::LocalVariableSingleIdentifierAst> {
  PARSE_ONCE(p1, ParseLocalVariableSingleIdentifier);
  PARSE_OPTIONAL(p2, ParseLocalVariableSingleIdentifierAlias);
  p1->Alias = std::move(p2);
  return FORWARD_AST(p1);
}

auto spp::parse::ParserSpp::ParseLocalVariableSingleIdentifierAlias()
  -> Unique<asts::LocalVariableSingleIdentifierAliasAst> {
  PARSE_ONCE(p1, ParseKeywordAs);
  PARSE_ONCE(p2, ParseIdentifier);
  return CREATE_AST(asts::LocalVariableSingleIdentifierAliasAst, p1, p2);
}

auto spp::parse::ParserSpp::ParseLocalVariableNestedForDestructureArray()
  -> Unique<asts::LocalVariableAst> {
  PARSE_ALTERNATE(
    p1, asts::LocalVariableAst, ParseLocalVariableDestructureSkipSingleArgument,
    ParseLocalVariableDestructureSkipMultipleArguments, ParseLocalVariableDestructureArray,
    ParseLocalVariableDestructureTuple, ParseLocalVariableDestructureObject,
    ParseLocalVariableSingleIdentifier);
  return FORWARD_AST(p1);
}

auto spp::parse::ParserSpp::ParseLocalVariableNestedForDestructureObject()
  -> Unique<asts::LocalVariableAst> {
  PARSE_ALTERNATE(
    p1, asts::LocalVariableAst, ParseLocalVariableDestructureSkipMultipleArguments,
    ParseLocalVariableDestructureAttributeBinding, ParseLocalVariableSingleIdentifierAliasable);
  return FORWARD_AST(p1);
}

auto spp::parse::ParserSpp::ParseLocalVariableNestedForDestructureTuple()
  -> Unique<asts::LocalVariableAst> {
  PARSE_ALTERNATE(
    p1, asts::LocalVariableAst, ParseLocalVariableDestructureSkipSingleArgument,
    ParseLocalVariableDestructureSkipMultipleArguments, ParseLocalVariableDestructureArray,
    ParseLocalVariableDestructureTuple, ParseLocalVariableDestructureObject,
    ParseLocalVariableSingleIdentifier);
  return FORWARD_AST(p1);
}

auto spp::parse::ParserSpp::ParseLocalVariableNestedForDestructureAttributeBinding()
  -> Unique<asts::LocalVariableAst> {
  PARSE_ALTERNATE(
    p1, asts::LocalVariableAst, ParseLocalVariableDestructureArray,
    ParseLocalVariableDestructureTuple, ParseLocalVariableDestructureObject);
  return FORWARD_AST(p1);
}

auto spp::parse::ParserSpp::ParseConvention()
  -> Unique<asts::ConventionAst> {
  PARSE_ALTERNATE(p1, asts::ConventionAst, ParseConventionMut, ParseConventionRef);
  return FORWARD_AST(p1);
}

auto spp::parse::ParserSpp::ParseConventionRef()
  -> Unique<asts::ConventionRefAst> {
  PARSE_ONCE(p1, ParseTokenBorrow);
  return CREATE_AST(asts::ConventionRefAst, p1);
}

auto spp::parse::ParserSpp::ParseConventionMut()
  -> Unique<asts::ConventionMutAst> {
  PARSE_ONCE(p1, ParseTokenBorrow);
  PARSE_ONCE(p2, ParseKeywordMut);
  return CREATE_AST(asts::ConventionMutAst, p1, p2);
}

auto spp::parse::ParserSpp::ParseObjectInitializer()
  -> Unique<asts::ObjectInitializerAst> {
  PARSE_ONCE(p1, ParseTypeExpressionSimple);
  PARSE_ONCE(p2, ParseObjectInitializerArgumentGroup);
  return CREATE_AST(asts::ObjectInitializerAst, p1, p2);
}

auto spp::parse::ParserSpp::ParseObjectInitializerArgumentGroup()
  -> Unique<asts::ObjectInitializerArgumentGroupAst> {
  PARSE_ONCE(p1, ParseTokenLeftParenthesis);
  PARSE_ZERO_OR_MORE(p2, ParseObjectInitializerArgument, ParseTokenComma);
  PARSE_ONCE(p3, ParseTokenRightParenthesis);
  return CREATE_AST(asts::ObjectInitializerArgumentGroupAst, p1, p2, p3);
}

auto spp::parse::ParserSpp::ParseObjectInitializerArgument()
  -> Unique<asts::ObjectInitializerArgumentAst> {
  PARSE_ALTERNATE(
    p1, asts::ObjectInitializerArgumentAst, ParseObjectInitializerArgumentKeyword,
    ParseObjectInitializerArgumentShorthand);
  return FORWARD_AST(p1);
}

auto spp::parse::ParserSpp::ParseObjectInitializerArgumentKeyword()
  -> Unique<asts::ObjectInitializerArgumentKeywordAst> {
  PARSE_ONCE(p1, ParseIdentifier);
  PARSE_ONCE(p2, ParseTokenAssign);
  PARSE_ONCE(p3, ParseExpression);
  return CREATE_AST(asts::ObjectInitializerArgumentKeywordAst, p1, p2, p3);
}

auto spp::parse::ParserSpp::ParseObjectInitializerArgumentShorthand()
  -> Unique<asts::ObjectInitializerArgumentShorthandAst> {
  PARSE_OPTIONAL(p1, ParseTokenDoubleDot);
  PARSE_ONCE(p2, ParseExpression);
  return CREATE_AST(asts::ObjectInitializerArgumentShorthandAst, p1, p2);
}

auto spp::parse::ParserSpp::ParseClosureExpression()
  -> Unique<asts::ClosureExpressionAst> {
  PARSE_ALTERNATE(
    p1, asts::ClosureExpressionAst, ParseClosureExpressionWithReturnType,
    ParseClosureExpressionWithoutReturnType);
  return FORWARD_AST(p1);
}

auto spp::parse::ParserSpp::ParseClosureExpressionWithReturnType()
  -> Unique<asts::ClosureExpressionAst> {
  PARSE_OPTIONAL(p1, ParseKeywordCor);
  PARSE_ONCE(p2, ParseClosureExpressionParameterAndCaptureGroup);
  PARSE_ONCE(p3, ParseTokenArrowRight);
  PARSE_ONCE(p4, ParseTypeExpression);
  PARSE_ONCE(p5, [this] { return ParseInnerScopeExpression([this] { return ParseStatement(); }); });
  return CREATE_AST(asts::ClosureExpressionAst, p1, p2, p3, p4, p5);
}

auto spp::parse::ParserSpp::ParseClosureExpressionWithoutReturnType()
  -> Unique<asts::ClosureExpressionAst> {
  PARSE_OPTIONAL(p1, ParseKeywordCor);
  PARSE_ONCE(p2, ParseClosureExpressionParameterAndCaptureGroup);
  // Prevent "()" tuple on one line combining with stmt on next.
  if (_LineFeedAhead()) { return nullptr; }
  PARSE_ONCE(p3, ParseExpression);
  return CREATE_AST(asts::ClosureExpressionAst, p1, p2, nullptr, nullptr, p3);
}

auto spp::parse::ParserSpp::ParseClosureExpressionCaptureGroup()
  -> Unique<asts::ClosureExpressionCaptureGroupAst> {
  PARSE_ONCE(p1, ParseKeywordCaps);
  PARSE_ONE_OR_MORE(p2, ParseClosureExpressionCapture, ParseTokenComma);
  return CREATE_AST(asts::ClosureExpressionCaptureGroupAst, p1, p2);
}

auto spp::parse::ParserSpp::ParseClosureExpressionCapture()
  -> Unique<asts::ClosureExpressionCaptureAst> {
  PARSE_OPTIONAL(p1, ParseConvention);
  PARSE_ALTERNATE(p2, asts::IdentifierAst, ParseIdentifier, ParseSelfIdentifier);
  return CREATE_AST(asts::ClosureExpressionCaptureAst, p1, p2);
}

auto spp::parse::ParserSpp::ParseClosureExpressionParameterAndCaptureGroup()
  -> Unique<asts::ClosureExpressionParameterAndCaptureGroupAst> {
  PARSE_ONCE(p1, ParseTokenLeftParenthesis);
  PARSE_ONCE(p2, ParseClosureExpressionParameterGroup);
  PARSE_OPTIONAL(p3, ParseClosureExpressionCaptureGroup);
  PARSE_ONCE(p4, ParseTokenRightParenthesis);
  return CREATE_AST(asts::ClosureExpressionParameterAndCaptureGroupAst, p1, p2, p3, p4);
}

auto spp::parse::ParserSpp::ParseClosureExpressionParameterGroup()
  -> Unique<asts::ClosureExpressionParameterGroupAst> {
  PARSE_ZERO_OR_MORE(p1, ParseClosureExpressionParameter, ParseTokenComma);
  return CREATE_AST(asts::ClosureExpressionParameterGroupAst, nullptr, p1, nullptr);
}

auto spp::parse::ParserSpp::ParseClosureExpressionParameter()
  -> Unique<asts::ClosureExpressionParameterAst> {
  PARSE_ALTERNATE(
    p1, asts::ClosureExpressionParameterAst, ParseFunctionParameterVariadic,
    ParseFunctionParameterRequired);
  return FORWARD_AST(p1);
}

auto spp::parse::ParserSpp::ParseTypeExpression()
  -> Unique<asts::TypeAst> {
  PARSE_ALTERNATE(
    p1, asts::TypeAst, ParseTypeNever, ParseTypeParenthesisedExpression, ParseTypeArray, ParseTypeTuple,
    ParseBinaryTypeExpression);
  return FORWARD_AST(p1);
}

auto spp::parse::ParserSpp::ParseBinaryTypeExpression(const std::uint8_t min_prec)
  -> Unique<asts::TypeAst> {
  using RT = lex::RawTokenType;

  struct BinOpInfo {
    Unique<asts::TokenAst> Tok;
    std::uint8_t Prec;
  };

  auto try_tok = [this](auto fn, const std::uint8_t prec) -> std::optional<BinOpInfo> {
    const auto pos = _Pos;
    if (auto tok = fn()) return BinOpInfo{std::move(tok), prec};
    _Pos = pos;
    return std::nullopt;
  };

  auto try_bin_op = [&]() -> BinOpInfo {
    auto peek = _Pos;
    while (peek < _TokensLen && (_Tokens[peek].Type == RT::TK_LINE_FEED || _Tokens[peek].Type == RT::TK_SPACE))
      peek++;
    if (peek >= _TokensLen) return {};

    switch (_Tokens[peek].Type) {
      case RT::KW_OR:
        if (auto r = try_tok([this] { return ParseKeywordOr(); }, 2)) return std::move(*r);
        break;
      case RT::KW_AND:
        if (auto r = try_tok([this] { return ParseKeywordAnd(); }, 1)) return std::move(*r);
        break;
      default: break;
    }
    return {};
  };

  PARSE_ONCE(lhs, ParsePostfixTypeExpression);

  while (true) {
    const auto saved = _Pos;
    auto [op, prec] = try_bin_op();

    if (!op || prec < min_prec) {
      _Pos = saved;
      break;
    }

    auto rhs = ParseBinaryTypeExpression(static_cast<std::uint8_t>(prec + 1));
    if (!rhs) {
      _Pos = saved;
      break;
    }
    lhs = CREATE_AST(asts::TypeBinaryExpressionAst, lhs, op, rhs)->Convert();
  }

  return lhs;
}

auto spp::parse::ParserSpp::ParseUnaryTypeExpression()
  -> Unique<asts::TypeAst> {
  PARSE_OPTIONAL(p1, ParseUnaryTypeExpressionOpBorrow)
  PARSE_ZERO_OR_MORE(p2, ParseUnaryTypeExpressionOp, ParseNothing);
  PARSE_ONCE(p3, [this] { return dynamic_unique_cast<asts::TypeAst>(ParseTypeIdentifier()); });
  if (p1 != nullptr) {
    p2.Insert(p2.begin(), dynamic_unique_cast<asts::TypeUnaryExpressionOperatorAst>(std::move(p1)));
  }
  return utils::algorithms::move_accumulate(
    p2.rbegin(), p2.rend(), std::move(p3),
    [](Unique<asts::TypeAst> &&acc, Unique<asts::TypeUnaryExpressionOperatorAst> &&x) {
      return CREATE_AST(asts::TypeUnaryExpressionAst, x, std::move(acc));
    });
}

auto spp::parse::ParserSpp::ParseUnaryTypeExpressionOp()
  -> Unique<asts::TypeUnaryExpressionOperatorAst> {
  PARSE_ALTERNATE(
    p1, asts::TypeUnaryExpressionOperatorAst, ParseUnaryTypeExpressionOpBorrow,
    ParseUnaryTypeExpressionOpNamespace);
  return FORWARD_AST(p1);
}

auto spp::parse::ParserSpp::ParseUnaryTypeExpressionOpBorrow()
  -> Unique<asts::TypeUnaryExpressionOperatorBorrowAst> {
  PARSE_ONCE(p1, ParseConvention);
  return CREATE_AST(asts::TypeUnaryExpressionOperatorBorrowAst, p1);
}

auto spp::parse::ParserSpp::ParseUnaryTypeExpressionOpNamespace()
  -> Unique<asts::TypeUnaryExpressionOperatorNamespaceAst> {
  PARSE_ONCE(p1, ParseIdentifier);
  PARSE_ONCE(p2, ParseTokenDoubleColon);
  return CREATE_AST(asts::TypeUnaryExpressionOperatorNamespaceAst, p1, p2);
}

auto spp::parse::ParserSpp::ParsePostfixTypeExpression()
  -> Unique<asts::TypeAst> {
  PARSE_ONCE(p1, ParseUnaryTypeExpression);
  PARSE_ZERO_OR_MORE(p2, ParsePostfixTypeExpressionOp, ParseNothing);
  return utils::algorithms::move_accumulate(
    p2.begin(), p2.end(), std::move(p1),
    [](Unique<asts::TypeAst> &&acc, Unique<asts::TypePostfixExpressionOperatorAst> &&x) {
      return CREATE_AST(asts::TypePostfixExpressionAst, std::move(acc), x);
    });
}

auto spp::parse::ParserSpp::ParsePostfixTypeExpressionOp()
  -> Unique<asts::TypePostfixExpressionOperatorAst> {
  PARSE_ALTERNATE(
    p1, asts::TypePostfixExpressionOperatorAst,
    ParsePostfixTypeExpressionOpNested);
  return FORWARD_AST(p1);
}

auto spp::parse::ParserSpp::ParsePostfixTypeExpressionOpNested()
  -> Unique<asts::TypePostfixExpressionOperatorNestedTypeAst> {
  PARSE_ONCE(p1, ParseTokenDoubleColon);
  PARSE_ONCE(p2, ParseTypeIdentifier);
  return CREATE_AST(asts::TypePostfixExpressionOperatorNestedTypeAst, p1, p2);
}

auto spp::parse::ParserSpp::ParseTypeParenthesisedExpression()
  -> Unique<asts::TypeAst> {
  PARSE_ONCE(p1, ParseTokenLeftParenthesis);
  PARSE_ONCE(p2, ParseTypeExpression);
  PARSE_ONCE(p3, ParseTokenRightParenthesis);
  return CREATE_AST(asts::TypeParenthesisedExpressionAst, p1, p2, p3)->Convert();
}

auto spp::parse::ParserSpp::ParseTypeNever()
  -> Unique<asts::TypeAst> {
  PARSE_ONCE(p1, ParseTokenExclamationMark);
  return asts::AstClone(asts::generate::common_types::NeverType(p1->PosStart()));
}

auto spp::parse::ParserSpp::ParseTypeExpressionSimple()
  -> Unique<asts::TypeAst> {
  PARSE_ONCE(p1, ParsePostfixTypeExpressionSimple)
  return FORWARD_AST(p1);
}

auto spp::parse::ParserSpp::ParsePostfixTypeExpressionSimple()
  -> Unique<asts::TypeAst> {
  PARSE_ONCE(p1, ParseUnaryTypeExpressionSimple);
  PARSE_ZERO_OR_MORE(p2, ParsePostfixTypeExpressionOp, ParseNothing);
  return utils::algorithms::move_accumulate(
    p2.begin(), p2.end(), std::move(p1),
    [](Unique<asts::TypeAst> &&acc, Unique<asts::TypePostfixExpressionOperatorAst> &&x) {
      return CREATE_AST(asts::TypePostfixExpressionAst, std::move(acc), x);
    });
}

auto spp::parse::ParserSpp::ParseUnaryTypeExpressionSimple()
  -> Unique<asts::TypeAst> {
  PARSE_OPTIONAL(p1, ParseUnaryTypeExpressionOpBorrow)
  PARSE_ZERO_OR_MORE(p2, ParseUnaryTypeExpressionOp, ParseNothing);
  PARSE_ONCE(p3, [this] { return dynamic_unique_cast<asts::TypeAst>(ParseTypeIdentifier()); });
  if (p1 != nullptr) {
    p2.Insert(p2.begin(), dynamic_unique_cast<asts::TypeUnaryExpressionOperatorAst>(std::move(p1)));
  }
  return utils::algorithms::move_accumulate(
    p2.rbegin(), p2.rend(), std::move(p3),
    [](Unique<asts::TypeAst> &&acc, Unique<asts::TypeUnaryExpressionOperatorAst> &&x) {
      return CREATE_AST(asts::TypeUnaryExpressionAst, x, std::move(acc));
    });
}

auto spp::parse::ParserSpp::ParseTypeIdentifier()
  -> Unique<asts::TypeIdentifierAst> {
  PARSE_ONCE(p1, ParseLexemeUpperIdentifier);
  PARSE_OPTIONAL(p2, ParseGenericArgumentGroup);
  auto t = CREATE_AST(asts::TypeIdentifierAst, p1->PosStart(), p1->TokenData, p2);
  t->MarkSourceWritten();
  return t;
}

auto spp::parse::ParserSpp::ParseTypeArray()
  -> Unique<asts::TypeAst> {
  PARSE_ONCE(p1, ParseTokenLeftSquareBracket);
  PARSE_ONCE(p2, ParseTypeExpression);
  PARSE_ONCE(p3, ParseTokenSemicolon);
  PARSE_ONCE(p4, ParseExpression); // TODO: Force this "cmp" in SA?
  PARSE_ONCE(p5, ParseTokenRightSquareBracket);
  return CREATE_AST(asts::TypeArrayShorthandAst, p1, p2, p3, p4, p5)->Convert();
}

auto spp::parse::ParserSpp::ParseTypeTuple()
  -> Unique<asts::TypeAst> {
  PARSE_ALTERNATE(
    p1, asts::TypeAst, ParseTypeTuple0Types, ParseTypeTuple1Types, ParseTypeTupleNTypes);
  return FORWARD_AST(p1);
}

auto spp::parse::ParserSpp::ParseTypeTuple0Types()
  -> Unique<asts::TypeAst> {
  PARSE_ONCE(p1, ParseTokenLeftParenthesis);
  auto p2 = Vec<Shared<asts::TypeAst>>();
  PARSE_ONCE(p3, ParseTokenRightParenthesis);
  return CREATE_AST(asts::TypeTupleShorthandAst, p1, p2, p3)->Convert();
}

auto spp::parse::ParserSpp::ParseTypeTuple1Types()
  -> Unique<asts::TypeAst> {
  PARSE_ONCE(p1, ParseTokenLeftParenthesis);
  PARSE_ONCE(p2, ParseTypeExpression);
  PARSE_ONCE(p3, ParseTokenComma);
  PARSE_ONCE(p4, ParseTokenRightParenthesis);

  auto temp = Vec<Shared<asts::TypeAst>>();
  temp.EmplaceBack(dynamic_shared_cast<asts::TypeAst>(std::move(p2)));
  return CREATE_AST(asts::TypeTupleShorthandAst, p1, temp, p4)->Convert();
}

auto spp::parse::ParserSpp::ParseTypeTupleNTypes()
  -> Unique<asts::TypeAst> {
  PARSE_ONCE(p1, ParseTokenLeftParenthesis);
  PARSE_TWO_OR_MORE(p2, ParseTypeExpression, ParseTokenComma);
  PARSE_ONCE(p3, ParseTokenRightParenthesis);

  auto temp = Vec<Shared<asts::TypeAst>>();
  for (auto &&x : p2) { temp.EmplaceBack(dynamic_shared_cast<asts::TypeAst>(std::move(x))); }
  return CREATE_AST(asts::TypeTupleShorthandAst, p1, temp, p3)->Convert();
}

auto spp::parse::ParserSpp::ParseIdentifier()
  -> Unique<asts::IdentifierAst> {
  PARSE_ONCE(p1, ParseLexemeIdentifier);
  return CREATE_AST(asts::IdentifierAst, p1->PosStart(), p1->TokenData);
}

auto spp::parse::ParserSpp::ParseNumericIdentifier()
  -> Unique<asts::IdentifierAst> {
  PARSE_ONCE(p1, ParseLexemeDecInteger);
  return CREATE_AST(asts::IdentifierAst, p1->PosStart(), p1->TokenData);
}

auto spp::parse::ParserSpp::ParseSelfIdentifier()
  -> Unique<asts::IdentifierAst> {
  PARSE_ONCE(p1, ParseKeywordSelf);
  return asts::IdentifierAst::MappedFromTok(*p1, p1->TokenData);
}

auto spp::parse::ParserSpp::ParseUpperIdentifier()
  -> Unique<asts::IdentifierAst> {
  PARSE_ONCE(p1, ParseLexemeUpperIdentifier);
  if (p1 == nullptr) { return nullptr; }
  return CREATE_AST(asts::IdentifierAst, p1->PosStart(), p1->TokenData);
}

auto spp::parse::ParserSpp::ParseIdentifierAsExpression()
  -> Unique<asts::ExpressionAst> {
  PARSE_ALTERNATE(p1, asts::IdentifierAst, ParseIdentifier, ParseSelfIdentifier);
  return dynamic_unique_cast<asts::ExpressionAst>(std::move(p1));
}

auto spp::parse::ParserSpp::ParseLiteral()
  -> Unique<asts::LiteralAst> {
  PARSE_ALTERNATE(
    p1, asts::LiteralAst, ParseLiteralChar, ParseLiteralString, ParseLiteralFloat, ParseLiteralInteger,
    ParseLiteralBoolean,
    [this] { return ParseLiteralTuple([this] { return ParseExpression(); }); },
    [this] { return ParseLiteralArray([this] { return ParseExpression(); }); });
  return FORWARD_AST(p1);
}

auto spp::parse::ParserSpp::ParseLiteralChar()
  -> Unique<asts::CharLiteralAst> {
  PARSE_OPTIONAL(p1, ParseBytePrefixType)
  PARSE_ONCE(p2, ParseLexemeSingleQuoteChar);
  return CREATE_AST(asts::CharLiteralAst, p1, p2);
}

auto spp::parse::ParserSpp::ParseLiteralString()
  -> Unique<asts::StringLiteralAst> {
  PARSE_OPTIONAL(p1, ParseBytePrefixType)
  PARSE_ONCE(p2, ParseLexemeDoubleQuoteString);
  return CREATE_AST(asts::StringLiteralAst, p1, p2);
}

auto spp::parse::ParserSpp::ParseLiteralFloat()
  -> Unique<asts::FloatLiteralAst> {
  PARSE_ONCE(p1, ParseLiteralFloatB10);
  return FORWARD_AST(p1);
}

auto spp::parse::ParserSpp::ParseLiteralInteger()
  -> Unique<asts::IntegerLiteralAst> {
  PARSE_ALTERNATE(
    p1, asts::IntegerLiteralAst, ParseLiteralIntegerB02, ParseLiteralIntegerB08, ParseLiteralIntegerB16,
    ParseLiteralIntegerB10);
  return FORWARD_AST(p1);
}

auto spp::parse::ParserSpp::ParseLiteralBoolean()
  -> Unique<asts::BooleanLiteralAst> {
  PARSE_ALTERNATE(p1, asts::TokenAst, ParseKeywordTrue, ParseKeywordFalse);
  return CREATE_AST(asts::BooleanLiteralAst, p1);
}

auto spp::parse::ParserSpp::ParseLiteralTuple(
  std::function<Unique<asts::ExpressionAst>()> &&elem_parser)
  -> Unique<asts::TupleLiteralAst> {
  auto parser_1 = [elem_parser, this] mutable { return ParseLiteralTuple1Element(std::move(elem_parser)); };
  auto parser_n = [elem_parser, this] mutable { return ParseLiteralTupleNElements(std::move(elem_parser)); };
  PARSE_ALTERNATE(p1, asts::TupleLiteralAst, parser_1, parser_n);
  return FORWARD_AST(p1);
}

auto spp::parse::ParserSpp::ParseLiteralArray(
  std::function<Unique<asts::ExpressionAst>()> &&elem_parser)
  -> Unique<asts::ArrayLiteralAst> {
  auto parser_e = [elem_parser, this] mutable { return ParseLiteralArrayExplicitElements(std::move(elem_parser)); };
  auto parser_r = [elem_parser, this] mutable { return ParseLiteralArrayRepeatedElement(std::move(elem_parser)); };
  PARSE_ALTERNATE(p1, asts::ArrayLiteralAst, parser_e, parser_r);
  return FORWARD_AST(p1);
}

auto spp::parse::ParserSpp::ParseLiteralFloatB10()
  -> Unique<asts::FloatLiteralAst> {
  PARSE_OPTIONAL(p1, ParseNumericPrefixOp);
  PARSE_ONCE(p2, ParseLexemeDecInteger);
  PARSE_ONCE(p3, ParseTokenDot);
  PARSE_ONCE(p4, ParseLexemeDecInteger);
  PARSE_OPTIONAL(p5, ParseFloatSuffixType);
  return CREATE_AST(asts::FloatLiteralAst, p1, p2, p3, p4, p5 ? p5->TokenData : "");
}

auto spp::parse::ParserSpp::ParseLiteralIntegerB02()
  -> Unique<asts::IntegerLiteralAst> {
  PARSE_OPTIONAL(p1, ParseNumericPrefixOp);
  PARSE_ONCE(p2, ParseLexemeBinInteger);
  PARSE_OPTIONAL(p3, ParseIntegerSuffixType);
  return CREATE_AST(asts::IntegerLiteralAst, p1, p2, p3 ? p3->TokenData : "");
}

auto spp::parse::ParserSpp::ParseLiteralIntegerB08()
  -> Unique<asts::IntegerLiteralAst> {
  PARSE_OPTIONAL(p1, ParseNumericPrefixOp);
  PARSE_ONCE(p2, ParseLexemeOctInteger);
  PARSE_OPTIONAL(p3, ParseIntegerSuffixType);
  return CREATE_AST(asts::IntegerLiteralAst, p1, p2, p3 ? p3->TokenData : "");
}

auto spp::parse::ParserSpp::ParseLiteralIntegerB10()
  -> Unique<asts::IntegerLiteralAst> {
  PARSE_OPTIONAL(p1, ParseNumericPrefixOp);
  PARSE_ONCE(p2, ParseLexemeDecInteger);
  PARSE_OPTIONAL(p3, ParseIntegerSuffixType);
  return CREATE_AST(asts::IntegerLiteralAst, p1, p2, p3 ? p3->TokenData : "");
}

auto spp::parse::ParserSpp::ParseLiteralIntegerB16()
  -> Unique<asts::IntegerLiteralAst> {
  PARSE_OPTIONAL(p1, ParseNumericPrefixOp);
  PARSE_ONCE(p2, ParseLexemeHexInteger);
  PARSE_OPTIONAL(p3, ParseIntegerSuffixType);
  return CREATE_AST(asts::IntegerLiteralAst, p1, p2, p3 ? p3->TokenData : "");
}

auto spp::parse::ParserSpp::ParseNumericPrefixOp()
  -> Unique<asts::TokenAst> {
  PARSE_ALTERNATE(p1, asts::TokenAst, ParseTokenAdd, ParseTokenSub);
  return FORWARD_AST(p1);
}

auto spp::parse::ParserSpp::ParseFloatSuffixType()
  -> Unique<asts::TokenAst> {
  PARSE_ONCE(p1, ParseTokenUnderscore)
  PARSE_ALTERNATE(
    p2, asts::TokenAst,
    [this] { return ParseSpecificCharacters("f8"); },
    [this] { return ParseSpecificCharacters("f16"); },
    [this] { return ParseSpecificCharacters("f32"); },
    [this] { return ParseSpecificCharacters("f64"); },
    [this] { return ParseSpecificCharacters("f128"); },
    [this] { return ParseSpecificCharacters("f256"); });
  return FORWARD_AST(p2);
}

auto spp::parse::ParserSpp::ParseIntegerSuffixType()
  -> Unique<asts::TokenAst> {
  PARSE_ONCE(p1, ParseTokenUnderscore);
  PARSE_ALTERNATE(
    p2, asts::TokenAst,
    [this] { return ParseSpecificCharacters("s8"); },
    [this] { return ParseSpecificCharacters("s16"); },
    [this] { return ParseSpecificCharacters("s32"); },
    [this] { return ParseSpecificCharacters("s64"); },
    [this] { return ParseSpecificCharacters("s128"); },
    [this] { return ParseSpecificCharacters("s256"); },
    [this] { return ParseSpecificCharacters("sz"); },
    [this] { return ParseSpecificCharacters("u8"); },
    [this] { return ParseSpecificCharacters("u16"); },
    [this] { return ParseSpecificCharacters("u32"); },
    [this] { return ParseSpecificCharacters("u64"); },
    [this] { return ParseSpecificCharacters("u128"); },
    [this] { return ParseSpecificCharacters("u256"); },
    [this] { return ParseSpecificCharacters("uz"); });
  return FORWARD_AST(p2);
}

auto spp::parse::ParserSpp::ParseBytePrefixType()
  -> Unique<asts::TokenAst> {
  PARSE_ONCE(p1, [this] { return ParseSpecificCharacter('b'); });
  return FORWARD_AST(p1);
}

auto spp::parse::ParserSpp::ParseLiteralTuple1Element(
  std::function<Unique<asts::ExpressionAst>()> &&elem_parser)
  -> Unique<asts::TupleLiteralAst> {
  PARSE_ONCE(p1, ParseTokenLeftParenthesis);
  PARSE_ONCE(p2, std::move(elem_parser));
  PARSE_ONCE(p3, ParseTokenComma);
  PARSE_ONCE(p4, ParseTokenRightParenthesis);

  auto temp = Vec<decltype(p2)>();
  temp.EmplaceBack(std::move(p2));
  return CREATE_AST(asts::TupleLiteralAst, p1, temp, p3);
}

auto spp::parse::ParserSpp::ParseLiteralTupleNElements(
  std::function<Unique<asts::ExpressionAst>()> &&elem_parser)
  -> Unique<asts::TupleLiteralAst> {
  PARSE_ONCE(p1, ParseTokenLeftParenthesis);
  PARSE_TWO_OR_MORE(p2, elem_parser, ParseTokenComma);
  PARSE_ONCE(p3, ParseTokenRightParenthesis);
  return CREATE_AST(asts::TupleLiteralAst, p1, p2, p3);
}

auto spp::parse::ParserSpp::ParseLiteralArrayRepeatedElement(
  std::function<Unique<asts::ExpressionAst>()> &&elem_parser)
  -> Unique<asts::ArrayLiteralRepeatedElementAst> {
  PARSE_ONCE(p1, ParseTokenLeftSquareBracket);
  PARSE_ONCE(p2, elem_parser);
  PARSE_ONCE(p3, ParseTokenSemicolon);
  PARSE_ONCE(p4, ParseExpression); // TODO: Force this "cmp" in SA?
  PARSE_ONCE(p5, ParseTokenRightSquareBracket);
  return CREATE_AST(asts::ArrayLiteralRepeatedElementAst, p1, p2, p3, p4, p5);
}

auto spp::parse::ParserSpp::ParseLiteralArrayExplicitElements(
  std::function<Unique<asts::ExpressionAst>()> &&elem_parser)
  -> Unique<asts::ArrayLiteralExplicitElementsAst> {
  PARSE_ONCE(p1, ParseTokenLeftSquareBracket);
  PARSE_ONE_OR_MORE(p2, elem_parser, ParseTokenComma);
  PARSE_ONCE(p3, ParseTokenRightSquareBracket);
  return CREATE_AST(asts::ArrayLiteralExplicitElementsAst, p1, p2, p3);
}

auto spp::parse::ParserSpp::ParseSpecificCharacters(Str &&s)
  -> Unique<asts::TokenAst> {
  auto identifier = Str();
  PARSE_ONCE(_, ParseNothing);
  auto pos = _Pos;

  for (auto i = 0uz; i < s.length(); ++i) {
    PARSE_ONCE(p1, ParseLexemeCharacterOrDigit);
    identifier += p1->TokenData[0];
  }

  return identifier != s ? nullptr : CREATE_AST(asts::TokenAst, pos, lex::SppTokenType::SP_NO_TOK, identifier);
}

auto spp::parse::ParserSpp::ParseSpecificCharacter(const char16_t c)
  -> Unique<asts::TokenAst> {
  PARSE_ONCE(_, ParseNothing);
  PARSE_ONCE(p1, ParseLexemeCharacterOrDigit)
  if (p1->TokenData[0] != c) { return nullptr; }
  return FORWARD_AST(p1);
}

auto spp::parse::ParserSpp::ParseLexemeCharacter()
  -> Unique<asts::TokenAst> {
  PARSE_ONCE(p1, [this] { return ParseTokenRaw(lex::RawTokenType::LX_CHARACTER, lex::SppTokenType::LX_CHARACTER); });
  return FORWARD_AST(p1);
}

auto spp::parse::ParserSpp::ParseLexemeDigit()
  -> Unique<asts::TokenAst> {
  PARSE_ONCE(p1, [this] { return ParseTokenRaw(lex::RawTokenType::LX_DIGIT, lex::SppTokenType::LX_DIGIT); });
  return FORWARD_AST(p1);
}

auto spp::parse::ParserSpp::ParseLexemeCharacterOrDigit()
  -> Unique<asts::TokenAst> {
  PARSE_ALTERNATE(p1, asts::TokenAst, ParseLexemeCharacter, ParseLexemeDigit);
  return FORWARD_AST(p1);
}

auto spp::parse::ParserSpp::ParseLexemeCharacterOrDigitOrUnderscore()
  -> Unique<asts::TokenAst> {
  PARSE_ALTERNATE(p1, asts::TokenAst, ParseLexemeCharacter, ParseLexemeDigit, ParseTokenUnderscore)
  return FORWARD_AST(p1);
}

auto spp::parse::ParserSpp::ParseLexemeBinInteger()
  -> Unique<asts::TokenAst> {
  PARSE_ONCE(_, ParseNothing);
  auto out = CREATE_AST(asts::TokenAst, _Pos, lex::SppTokenType::LX_NUMBER, Str());

  PARSE_ONCE(p1, [this] { return ParseSpecificCharacter('0'); });
  out->TokenData += p1->TokenData;

  PARSE_ONCE(p2, [this] { return ParseSpecificCharacter('b'); });
  out->TokenData += p2->TokenData;

  PARSE_ONCE(p3, ParseLexemeDigit);
  if (p3->TokenData[0] != '0' and p3->TokenData[0] != '1') { return nullptr; }
  out->TokenData += p3->TokenData;

  while (_Tokens[_Pos].Type == lex::RawTokenType::LX_DIGIT) {
    PARSE_ONCE(p4, ParseLexemeDigit);
    if (p4->TokenData[0] != '0' and p4->TokenData[0] != '1') { return nullptr; }
    out->TokenData += p4->TokenData;
  }

  return out;
}

auto spp::parse::ParserSpp::ParseLexemeOctInteger()
  -> Unique<asts::TokenAst> {
  PARSE_ONCE(_, ParseNothing);
  auto out = CREATE_AST(asts::TokenAst, _Pos, lex::SppTokenType::LX_NUMBER, Str());

  PARSE_ONCE(p1, [this] { return ParseSpecificCharacter('0'); });
  out->TokenData += p1->TokenData;

  PARSE_ONCE(p2, [this] { return ParseSpecificCharacter('o'); });
  out->TokenData += p2->TokenData;

  PARSE_ONCE(p3, ParseLexemeDigit);
  if (p3->TokenData[0] < '0' or p3->TokenData[0] > '7') { return nullptr; }
  out->TokenData += p3->TokenData;

  while (_Tokens[_Pos].Type == lex::RawTokenType::LX_DIGIT) {
    PARSE_ONCE(p4, ParseLexemeDigit);
    if (p4->TokenData[0] < '0' or p4->TokenData[0] > '7') { return nullptr; }
    out->TokenData += p4->TokenData;
  }

  return out;
}

auto spp::parse::ParserSpp::ParseLexemeDecInteger()
  -> Unique<asts::TokenAst> {
  PARSE_ONCE(_, ParseNothing);
  auto out = CREATE_AST(asts::TokenAst, _Pos, lex::SppTokenType::LX_NUMBER, Str());

  PARSE_ONCE(p1, ParseLexemeDigit);
  out->TokenData += p1->TokenData;

  while (_Tokens[_Pos].Type == lex::RawTokenType::LX_DIGIT) {
    PARSE_ONCE(p2, ParseLexemeDigit);
    out->TokenData += p2->TokenData;
  }

  return out;
}

auto spp::parse::ParserSpp::ParseLexemeHexInteger()
  -> Unique<asts::TokenAst> {
  PARSE_ONCE(_, ParseNothing);
  auto out = CREATE_AST(asts::TokenAst, _Pos, lex::SppTokenType::LX_NUMBER, Str());

  PARSE_ONCE(p1, [this] { return ParseSpecificCharacter('0'); });
  out->TokenData += p1->TokenData;

  PARSE_ONCE(p2, [this] { return ParseSpecificCharacter('x'); });
  out->TokenData += p2->TokenData;

  PARSE_ONCE(p3, ParseLexemeCharacterOrDigit);
  if (kHexChars.find(p3->TokenData[0]) == Str::npos) { return nullptr; }
  out->TokenData += p3->TokenData;

  while (_Tokens[_Pos].Type == lex::RawTokenType::LX_CHARACTER or _Tokens[_Pos].Type == lex::RawTokenType::LX_DIGIT) {
    PARSE_ONCE(p4, ParseLexemeCharacterOrDigit);
    if (kHexChars.find(p4->TokenData[0]) == Str::npos) { return nullptr; }
    out->TokenData += p4->TokenData;
  }

  return out;
}

auto spp::parse::ParserSpp::ParseLexemeSingleQuoteChar()
  -> Unique<asts::TokenAst> {
  PARSE_ONCE(_, ParseNothing);
  auto out = CREATE_AST(asts::TokenAst, _Pos, lex::SppTokenType::LX_CHAR, Str());

  PARSE_ONCE(p1, ParseTokenSingleQuote);
  out->TokenData += p1->TokenData;

  while (_Tokens[_Pos].Type == lex::RawTokenType::LX_CHARACTER) {
    PARSE_ONCE(p2, ParseLexemeCharacter);
    out->TokenData += p2->TokenData;
  }

  PARSE_ONCE(p3, ParseTokenSingleQuote);
  out->TokenData += p3->TokenData;

  return out;
}

auto spp::parse::ParserSpp::ParseLexemeDoubleQuoteString()
  -> Unique<asts::TokenAst> {
  PARSE_ONCE(_, ParseNothing);
  auto out = CREATE_AST(asts::TokenAst, _Pos, lex::SppTokenType::LX_STRING, Str());

  PARSE_ONCE(p1, ParseTokenDoubleQuote);
  out->TokenData += p1->TokenData;

  while (_Tokens[_Pos].Type == lex::RawTokenType::LX_CHARACTER) {
    PARSE_ONCE(p2, ParseLexemeCharacter);
    out->TokenData += p2->TokenData;
  }

  PARSE_ONCE(p3, ParseTokenDoubleQuote);
  out->TokenData += p3->TokenData;

  return out;
}

auto spp::parse::ParserSpp::ParseLexemeIdentifier()
  -> Unique<asts::TokenAst> {
  PARSE_ONCE(_, ParseNothing);
  auto out = CREATE_AST(asts::TokenAst, _Pos, lex::SppTokenType::LX_IDENTIFIER, Str());

  PARSE_OPTIONAL(p1, ParseTokenDollar);
  if (p1 != nullptr) { out->TokenData += p1->TokenData; }

  PARSE_ONCE(p2, ParseLexemeCharacter);
  if (std::isupper(p2->TokenData[0])) { return nullptr; }
  out->TokenData += p2->TokenData;

  auto t = _Tokens[_Pos].Type;
  while (t == lex::RawTokenType::LX_CHARACTER or t == lex::RawTokenType::LX_DIGIT or t ==
    lex::RawTokenType::TK_UNDERSCORE) {
    PARSE_ONCE(p3, ParseLexemeCharacterOrDigitOrUnderscore);
    out->TokenData += p3->TokenData;
    t = _Tokens[_Pos].Type;
  }

  return out;
}

auto spp::parse::ParserSpp::ParseLexemeUpperIdentifier()
  -> Unique<asts::TokenAst> {
  PARSE_ONCE(_, ParseNothing);
  auto out = CREATE_AST(asts::TokenAst, _Pos, lex::SppTokenType::LX_IDENTIFIER, Str());

  PARSE_OPTIONAL(p1, ParseTokenDollar);
  if (p1 != nullptr) { out->TokenData += p1->TokenData; }

  PARSE_ONCE(p2, ParseLexemeCharacter);
  if (std::islower(p2->TokenData[0])) { return nullptr; }
  out->TokenData += p2->TokenData;

  auto t = _Tokens[_Pos].Type;
  while (t == lex::RawTokenType::LX_CHARACTER or t == lex::RawTokenType::LX_DIGIT) {
    PARSE_ONCE(p3, ParseLexemeCharacterOrDigit);
    out->TokenData += p3->TokenData;
    t = _Tokens[_Pos].Type;
  }

  return out;
}

auto spp::parse::ParserSpp::ParseNothing()
  -> Unique<asts::TokenAst> {
  PARSE_ONCE(p1, [this] { return ParseTokenRaw(lex::RawTokenType::SP_NO_TOK, lex::SppTokenType::SP_NO_TOK); });
  return FORWARD_AST(p1);
}

auto spp::parse::ParserSpp::ParseNewline()
  -> Unique<asts::TokenAst> {
  PARSE_ONCE(p1, [this] { return ParseTokenRaw(lex::RawTokenType::TK_LINE_FEED, lex::SppTokenType::TK_NEWLINE); });
  return FORWARD_AST(p1);
}

auto spp::parse::ParserSpp::ParseSpace()
  -> Unique<asts::TokenAst> {
  PARSE_ONCE(p1, [this] { return ParseTokenRaw(lex::RawTokenType::TK_SPACE, lex::SppTokenType::TK_SPACE); });
  return FORWARD_AST(p1);
}

auto spp::parse::ParserSpp::ParseTokenLeftCurlyBrace()
  -> Unique<asts::TokenAst> {
  PARSE_ONCE(
    p1, [this] { return ParseTokenRaw(lex::RawTokenType::TK_LEFT_CURLY_BRACE, lex::SppTokenType::TK_LEFT_CURLY_BRACE);
    });
  return FORWARD_AST(p1);
}

auto spp::parse::ParserSpp::ParseTokenRightCurlyBrace()
  -> Unique<asts::TokenAst> {
  PARSE_ONCE(
    p1, [this] { return ParseTokenRaw(lex::RawTokenType::TK_RIGHT_CURLY_BRACE, lex::SppTokenType::TK_RIGHT_CURLY_BRACE
    ); });
  return FORWARD_AST(p1);
}

auto spp::parse::ParserSpp::ParseTokenLeftSquareBracket()
  -> Unique<asts::TokenAst> {
  PARSE_ONCE(
    p1, [this] { return ParseTokenRaw(lex::RawTokenType::TK_LEFT_SQUARE_BRACKET, lex::SppTokenType::
      TK_LEFT_SQUARE_BRACKET); });
  return FORWARD_AST(p1);
}

auto spp::parse::ParserSpp::ParseTokenLeftParenthesis()
  -> Unique<asts::TokenAst> {
  PARSE_ONCE(
    p1, [this] { return ParseTokenRaw(lex::RawTokenType::TK_LEFT_PARENTHESIS, lex::SppTokenType::TK_LEFT_PARENTHESIS);
    });
  return FORWARD_AST(p1);
}

auto spp::parse::ParserSpp::ParseTokenRightParenthesis()
  -> Unique<asts::TokenAst> {
  PARSE_ONCE(
    p1, [this] { return ParseTokenRaw(lex::RawTokenType::TK_RIGHT_PARENTHESIS, lex::SppTokenType::TK_RIGHT_PARENTHESIS
    ); });
  return FORWARD_AST(p1);
}

auto spp::parse::ParserSpp::ParseTokenRightSquareBracket()
  -> Unique<asts::TokenAst> {
  PARSE_ONCE(
    p1, [this] { return ParseTokenRaw(lex::RawTokenType::TK_RIGHT_SQUARE_BRACKET, lex::SppTokenType::
      TK_RIGHT_SQUARE_BRACKET); });
  return FORWARD_AST(p1);
}

auto spp::parse::ParserSpp::ParseTokenColon()
  -> Unique<asts::TokenAst> {
  PARSE_ONCE(p1, [this] { return ParseTokenRaw(lex::RawTokenType::TK_COLON, lex::SppTokenType::TK_COLON); });
  return FORWARD_AST(p1);
}

auto spp::parse::ParserSpp::ParseTokenComma()
  -> Unique<asts::TokenAst> {
  PARSE_ONCE(p1, [this] { return ParseTokenRaw(lex::RawTokenType::TK_COMMA, lex::SppTokenType::TK_COMMA); });
  return FORWARD_AST(p1);
}

auto spp::parse::ParserSpp::ParseTokenAssign()
  -> Unique<asts::TokenAst> {
  PARSE_ONCE(p1, [this] { return ParseTokenRaw(lex::RawTokenType::TK_EQUALS_TO, lex::SppTokenType::TK_ASSIGN); });
  return FORWARD_AST(p1);
}

auto spp::parse::ParserSpp::ParseTokenUnderscore()
  -> Unique<asts::TokenAst> {
  PARSE_ONCE(
    p1, [this] { return ParseTokenRaw(lex::RawTokenType::TK_UNDERSCORE, lex::SppTokenType::TK_UNDERSCORE); });
  return FORWARD_AST(p1);
}

auto spp::parse::ParserSpp::ParseTokenLessThan()
  -> Unique<asts::TokenAst> {
  PARSE_ONCE(p1, [this] { return ParseTokenRaw(lex::RawTokenType::TK_LESS_THAN, lex::SppTokenType::TK_LT); });
  return FORWARD_AST(p1);
}

auto spp::parse::ParserSpp::ParseTokenGreaterThan()
  -> Unique<asts::TokenAst> {
  PARSE_ONCE(p1, [this] { return ParseTokenRaw(lex::RawTokenType::TK_GREATER_THAN, lex::SppTokenType::TK_GT); });
  return FORWARD_AST(p1);
}

auto spp::parse::ParserSpp::ParseTokenAdd()
  -> Unique<asts::TokenAst> {
  PARSE_ONCE(p1, [this] { return ParseTokenRaw(lex::RawTokenType::TK_PLUS_SIGN, lex::SppTokenType::TK_ADD); });
  return FORWARD_AST(p1);
}

auto spp::parse::ParserSpp::ParseTokenSub()
  -> Unique<asts::TokenAst> {
  PARSE_ONCE(p1, [this] { return ParseTokenRaw(lex::RawTokenType::TK_HYPHEN, lex::SppTokenType::TK_SUB); });
  return FORWARD_AST(p1);
}

auto spp::parse::ParserSpp::ParseTokenMul()
  -> Unique<asts::TokenAst> {
  PARSE_ONCE(p1, [this] { return ParseTokenRaw(lex::RawTokenType::TK_ASTERISK, lex::SppTokenType::TK_MUL); });
  return FORWARD_AST(p1);
}

auto spp::parse::ParserSpp::ParseTokenDiv()
  -> Unique<asts::TokenAst> {
  PARSE_ONCE(p1, [this] { return ParseTokenRaw(lex::RawTokenType::TK_SLASH, lex::SppTokenType::TK_DIV); });
  return FORWARD_AST(p1);
}

auto spp::parse::ParserSpp::ParseTokenRem()
  -> Unique<asts::TokenAst> {
  PARSE_ONCE(p1, [this] { return ParseTokenRaw(lex::RawTokenType::TK_PERCENT_SIGN, lex::SppTokenType::TK_REM); });
  return FORWARD_AST(p1);
}

auto spp::parse::ParserSpp::ParseTokenBitIor()
  -> Unique<asts::TokenAst> {
  PARSE_ONCE(p1, [this] { return ParseTokenRaw(lex::RawTokenType::TK_VERTICAL_BAR, lex::SppTokenType::TK_BIT_IOR); });
  return FORWARD_AST(p1);
}

auto spp::parse::ParserSpp::ParseTokenBitXor()
  -> Unique<asts::TokenAst> {
  PARSE_ONCE(p1, [this] { return ParseTokenRaw(lex::RawTokenType::TK_CARET, lex::SppTokenType::TK_BIT_XOR); });
  return FORWARD_AST(p1);
}

auto spp::parse::ParserSpp::ParseTokenBitAnd()
  -> Unique<asts::TokenAst> {
  PARSE_ONCE(p1, [this] { return ParseTokenRaw(lex::RawTokenType::TK_AMPERSAND, lex::SppTokenType::TK_BIT_AND); });
  return FORWARD_AST(p1);
}

auto spp::parse::ParserSpp::ParseTokenDot()
  -> Unique<asts::TokenAst> {
  PARSE_ONCE(p1, [this] { return ParseTokenRaw(lex::RawTokenType::TK_PERIOD, lex::SppTokenType::TK_DOT); });
  return FORWARD_AST(p1);
}

auto spp::parse::ParserSpp::ParseTokenQuestionMark()
  -> Unique<asts::TokenAst> {
  PARSE_ONCE(
    p1, [this] { return ParseTokenRaw(lex::RawTokenType::TK_QUESTION_MARK, lex::SppTokenType::TK_QUESTION_MARK); });
  return FORWARD_AST(p1);
}

auto spp::parse::ParserSpp::ParseTokenExclamationMark()
  -> Unique<asts::TokenAst> {
  PARSE_ONCE(
    p1, [this] { return ParseTokenRaw(lex::RawTokenType::TK_EXCLAMATION_MARK, lex::SppTokenType::TK_EXCLAMATION_MARK);
    });
  return FORWARD_AST(p1);
}

auto spp::parse::ParserSpp::ParseTokenDeref()
  -> Unique<asts::TokenAst> {
  PARSE_ONCE(p1, [this] { return ParseTokenRaw(lex::RawTokenType::TK_AT_SIGN, lex::SppTokenType::TK_DEREF); });
  return FORWARD_AST(p1);
}

auto spp::parse::ParserSpp::ParseTokenBorrow()
  -> Unique<asts::TokenAst> {
  PARSE_ONCE(p1, [this] { return ParseTokenRaw(lex::RawTokenType::TK_AMPERSAND, lex::SppTokenType::TK_BORROW); });
  return FORWARD_AST(p1);
}

auto spp::parse::ParserSpp::ParseTokenSemicolon()
  -> Unique<asts::TokenAst> {
  PARSE_ONCE(p1, [this] { return ParseTokenRaw(lex::RawTokenType::TK_SEMICOLON, lex::SppTokenType::TK_SEMICOLON); });
  return FORWARD_AST(p1);
}

auto spp::parse::ParserSpp::ParseTokenSingleQuote()
  -> Unique<asts::TokenAst> {
  PARSE_ONCE(
    p1, [this] { return ParseTokenRaw(lex::RawTokenType::TK_APOSTROPHE, lex::SppTokenType::TK_APOSTROPHE); });
  return FORWARD_AST(p1);
}

auto spp::parse::ParserSpp::ParseTokenDoubleQuote()
  -> Unique<asts::TokenAst> {
  PARSE_ONCE(
    p1, [this] { return ParseTokenRaw(lex::RawTokenType::TK_QUOTATION_MARK, lex::SppTokenType::TK_QUOTATION_MARK); });
  return FORWARD_AST(p1);
}

auto spp::parse::ParserSpp::ParseTokenDollar()
  -> Unique<asts::TokenAst> {
  PARSE_ONCE(p1, [this] { return ParseTokenRaw(lex::RawTokenType::TK_DOLLAR_SIGN, lex::SppTokenType::TK_DOLLAR); });
  return FORWARD_AST(p1);
}

auto spp::parse::ParserSpp::ParseTokenArrowRight()
  -> Unique<asts::TokenAst> {
  PARSE_ONCE(p1, [this] { return ParseTokenRaw(lex::RawTokenType::TK_HYPHEN, lex::SppTokenType::TK_ARROW_RIGHT); });
  PARSE_ONCE(
    p2, [this] { return ParseTokenRaw(lex::RawTokenType::TK_GREATER_THAN, lex::SppTokenType::TK_ARROW_RIGHT); });
  p1->TokenData = "->";
  return FORWARD_AST(p1);
}

auto spp::parse::ParserSpp::ParseTokenDoubleDot()
  -> Unique<asts::TokenAst> {
  PARSE_ONCE(p1, [this] { return ParseTokenRaw(lex::RawTokenType::TK_PERIOD, lex::SppTokenType::TK_DOUBLE_DOT); });
  PARSE_ONCE(p2, [this] { return ParseTokenRaw(lex::RawTokenType::TK_PERIOD, lex::SppTokenType::TK_DOUBLE_DOT); });
  p1->TokenData = "..";
  return FORWARD_AST(p1);
}

auto spp::parse::ParserSpp::ParseTokenDoubleColon()
  -> Unique<asts::TokenAst> {
  PARSE_ONCE(p1, [this] { return ParseTokenRaw(lex::RawTokenType::TK_COLON, lex::SppTokenType::TK_DOUBLE_COLON); });
  PARSE_ONCE(p2, [this] { return ParseTokenRaw(lex::RawTokenType::TK_COLON, lex::SppTokenType::TK_DOUBLE_COLON); });
  p1->TokenData = "::";
  return FORWARD_AST(p1);
}

auto spp::parse::ParserSpp::ParseTokenEquals()
  -> Unique<asts::TokenAst> {
  PARSE_ONCE(p1, [this] { return ParseTokenRaw(lex::RawTokenType::TK_EQUALS_TO, lex::SppTokenType::TK_EQ); });
  PARSE_ONCE(p2, [this] { return ParseTokenRaw(lex::RawTokenType::TK_EQUALS_TO, lex::SppTokenType::TK_EQ); });
  p1->TokenData = "==";
  return FORWARD_AST(p1);
}

auto spp::parse::ParserSpp::ParseTokenNotEquals()
  -> Unique<asts::TokenAst> {
  PARSE_ONCE(p1, [this] { return ParseTokenRaw(lex::RawTokenType::TK_EXCLAMATION_MARK, lex::SppTokenType::TK_NE); });
  PARSE_ONCE(p2, [this] { return ParseTokenRaw(lex::RawTokenType::TK_EQUALS_TO, lex::SppTokenType::TK_NE); });
  p1->TokenData = "!=";
  return FORWARD_AST(p1);
}

auto spp::parse::ParserSpp::ParseTokenLessThanEquals()
  -> Unique<asts::TokenAst> {
  PARSE_ONCE(p1, [this] { return ParseTokenRaw(lex::RawTokenType::TK_LESS_THAN, lex::SppTokenType::TK_LE); });
  PARSE_ONCE(p2, [this] { return ParseTokenRaw(lex::RawTokenType::TK_EQUALS_TO, lex::SppTokenType::TK_LE); });
  p1->TokenData = "<=";
  return FORWARD_AST(p1);
}

auto spp::parse::ParserSpp::ParseTokenGreaterThanEquals()
  -> Unique<asts::TokenAst> {
  PARSE_ONCE(p1, [this] { return ParseTokenRaw(lex::RawTokenType::TK_GREATER_THAN, lex::SppTokenType::TK_GE); });
  PARSE_ONCE(p2, [this] { return ParseTokenRaw(lex::RawTokenType::TK_EQUALS_TO, lex::SppTokenType::TK_GE); });
  p1->TokenData = ">=";
  return FORWARD_AST(p1);
}

auto spp::parse::ParserSpp::ParseTokenAddAssign()
  -> Unique<asts::TokenAst> {
  PARSE_ONCE(p1, [this] { return ParseTokenRaw(lex::RawTokenType::TK_PLUS_SIGN, lex::SppTokenType::TK_ADD_ASSIGN); });
  PARSE_ONCE(p2, [this] { return ParseTokenRaw(lex::RawTokenType::TK_EQUALS_TO, lex::SppTokenType::TK_ADD_ASSIGN); });
  p1->TokenData = "+=";
  return FORWARD_AST(p1);
}

auto spp::parse::ParserSpp::ParseTokenSubAssign()
  -> Unique<asts::TokenAst> {
  PARSE_ONCE(p1, [this] { return ParseTokenRaw(lex::RawTokenType::TK_HYPHEN, lex::SppTokenType::TK_SUB_ASSIGN); });
  PARSE_ONCE(p2, [this] { return ParseTokenRaw(lex::RawTokenType::TK_EQUALS_TO, lex::SppTokenType::TK_SUB_ASSIGN); });
  p1->TokenData = "-=";
  return FORWARD_AST(p1);
}

auto spp::parse::ParserSpp::ParseTokenMulAssign()
  -> Unique<asts::TokenAst> {
  PARSE_ONCE(p1, [this] { return ParseTokenRaw(lex::RawTokenType::TK_ASTERISK, lex::SppTokenType::TK_MUL_ASSIGN); });
  PARSE_ONCE(p2, [this] { return ParseTokenRaw(lex::RawTokenType::TK_EQUALS_TO, lex::SppTokenType::TK_MUL_ASSIGN); });
  p1->TokenData = "*=";
  return FORWARD_AST(p1);
}

auto spp::parse::ParserSpp::ParseTokenDivAssign()
  -> Unique<asts::TokenAst> {
  PARSE_ONCE(p1, [this] { return ParseTokenRaw(lex::RawTokenType::TK_SLASH, lex::SppTokenType::TK_DIV_ASSIGN); });
  PARSE_ONCE(p2, [this] { return ParseTokenRaw(lex::RawTokenType::TK_EQUALS_TO, lex::SppTokenType::TK_DIV_ASSIGN); });
  p1->TokenData = "/=";
  return FORWARD_AST(p1);
}

auto spp::parse::ParserSpp::ParseTokenRemAssign()
  -> Unique<asts::TokenAst> {
  PARSE_ONCE(
    p1, [this] { return ParseTokenRaw(lex::RawTokenType::TK_PERCENT_SIGN, lex::SppTokenType::TK_REM_ASSIGN); });
  PARSE_ONCE(p2, [this] { return ParseTokenRaw(lex::RawTokenType::TK_EQUALS_TO, lex::SppTokenType::TK_REM_ASSIGN); });
  p1->TokenData = "%=";
  return FORWARD_AST(p1);
}

auto spp::parse::ParserSpp::ParseTokenPow()
  -> Unique<asts::TokenAst> {
  PARSE_ONCE(p1, [this] { return ParseTokenRaw(lex::RawTokenType::TK_ASTERISK, lex::SppTokenType::TK_POW); });
  PARSE_ONCE(p2, [this] { return ParseTokenRaw(lex::RawTokenType::TK_ASTERISK, lex::SppTokenType::TK_POW); });
  p1->TokenData = "**";
  return FORWARD_AST(p1);
}

auto spp::parse::ParserSpp::ParseTokenBitShl()
  -> Unique<asts::TokenAst> {
  PARSE_ONCE(p1, [this] { return ParseTokenRaw(lex::RawTokenType::TK_LESS_THAN, lex::SppTokenType::TK_BIT_SHL); });
  PARSE_ONCE(p2, [this] { return ParseTokenRaw(lex::RawTokenType::TK_LESS_THAN, lex::SppTokenType::TK_BIT_SHL); });
  p1->TokenData = "<<";
  return FORWARD_AST(p1);
}

auto spp::parse::ParserSpp::ParseTokenBitShr()
  -> Unique<asts::TokenAst> {
  PARSE_ONCE(p1, [this] { return ParseTokenRaw(lex::RawTokenType::TK_GREATER_THAN, lex::SppTokenType::TK_BIT_SHR); });
  PARSE_ONCE(p2, [this] { return ParseTokenRaw(lex::RawTokenType::TK_GREATER_THAN, lex::SppTokenType::TK_BIT_SHR); });
  p1->TokenData = ">>";
  return FORWARD_AST(p1);
}

auto spp::parse::ParserSpp::ParseTokenBitIorAssign()
  -> Unique<asts::TokenAst> {
  PARSE_ONCE(
    p1, [this] { return ParseTokenRaw(lex::RawTokenType::TK_VERTICAL_BAR, lex::SppTokenType::TK_BIT_IOR_ASSIGN); });
  PARSE_ONCE(
    p2, [this] { return ParseTokenRaw(lex::RawTokenType::TK_EQUALS_TO, lex::SppTokenType::TK_BIT_IOR_ASSIGN); });
  p1->TokenData = "|=";
  return FORWARD_AST(p1);
}

auto spp::parse::ParserSpp::ParseTokenBitXorAssign()
  -> Unique<asts::TokenAst> {
  PARSE_ONCE(p1, [this] { return ParseTokenRaw(lex::RawTokenType::TK_CARET, lex::SppTokenType::TK_BIT_XOR_ASSIGN); });
  PARSE_ONCE(
    p2, [this] { return ParseTokenRaw(lex::RawTokenType::TK_EQUALS_TO, lex::SppTokenType::TK_BIT_XOR_ASSIGN); });
  p1->TokenData = "^=";
  return FORWARD_AST(p1);
}

auto spp::parse::ParserSpp::ParseTokenBitAndAssign()
  -> Unique<asts::TokenAst> {
  PARSE_ONCE(
    p1, [this] { return ParseTokenRaw(lex::RawTokenType::TK_AMPERSAND, lex::SppTokenType::TK_BIT_AND_ASSIGN); });
  PARSE_ONCE(
    p2, [this] { return ParseTokenRaw(lex::RawTokenType::TK_EQUALS_TO, lex::SppTokenType::TK_BIT_AND_ASSIGN); });
  p1->TokenData = "&=";
  return FORWARD_AST(p1);
}

auto spp::parse::ParserSpp::ParseTokenPowAssign()
  -> Unique<asts::TokenAst> {
  PARSE_ONCE(p1, [this] { return ParseTokenRaw(lex::RawTokenType::TK_ASTERISK, lex::SppTokenType::TK_POW_ASSIGN); });
  PARSE_ONCE(p2, [this] { return ParseTokenRaw(lex::RawTokenType::TK_ASTERISK, lex::SppTokenType::TK_POW_ASSIGN); });
  PARSE_ONCE(p3, [this] { return ParseTokenRaw(lex::RawTokenType::TK_EQUALS_TO, lex::SppTokenType::TK_POW_ASSIGN); });
  p1->TokenData = "**=";
  return FORWARD_AST(p1);
}

auto spp::parse::ParserSpp::ParseTokenBitShlAssign()
  -> Unique<asts::TokenAst> {
  PARSE_ONCE(
    p1, [this] { return ParseTokenRaw(lex::RawTokenType::TK_LESS_THAN, lex::SppTokenType::TK_BIT_SHL_ASSIGN); });
  PARSE_ONCE(
    p2, [this] { return ParseTokenRaw(lex::RawTokenType::TK_LESS_THAN, lex::SppTokenType::TK_BIT_SHL_ASSIGN); });
  PARSE_ONCE(
    p3, [this] { return ParseTokenRaw(lex::RawTokenType::TK_EQUALS_TO, lex::SppTokenType::TK_BIT_SHL_ASSIGN); });
  p1->TokenData = "<<=";
  return FORWARD_AST(p1);
}

auto spp::parse::ParserSpp::ParseTokenBitShrAssign()
  -> Unique<asts::TokenAst> {
  PARSE_ONCE(
    p1, [this] { return ParseTokenRaw(lex::RawTokenType::TK_GREATER_THAN, lex::SppTokenType::TK_BIT_SHR_ASSIGN); });
  PARSE_ONCE(
    p2, [this] { return ParseTokenRaw(lex::RawTokenType::TK_GREATER_THAN, lex::SppTokenType::TK_BIT_SHR_ASSIGN); });
  PARSE_ONCE(
    p3, [this] { return ParseTokenRaw(lex::RawTokenType::TK_EQUALS_TO, lex::SppTokenType::TK_BIT_SHR_ASSIGN); });
  p1->TokenData = ">>=";
  return FORWARD_AST(p1);
}

auto spp::parse::ParserSpp::ParseKeywordCls()
  -> Unique<asts::TokenAst> {
  PARSE_ONCE(p1, [this] { return ParseTokenRaw(lex::RawTokenType::KW_CLS, lex::SppTokenType::KW_CLS); });
  return FORWARD_AST(p1);
}

auto spp::parse::ParserSpp::ParseKeywordFun()
  -> Unique<asts::TokenAst> {
  PARSE_ONCE(p1, [this] { return ParseTokenRaw(lex::RawTokenType::KW_FUN, lex::SppTokenType::KW_FUN); });
  return FORWARD_AST(p1);
}

auto spp::parse::ParserSpp::ParseKeywordCor()
  -> Unique<asts::TokenAst> {
  PARSE_ONCE(p1, [this] { return ParseTokenRaw(lex::RawTokenType::KW_COR, lex::SppTokenType::KW_COR); });
  return FORWARD_AST(p1);
}

auto spp::parse::ParserSpp::ParseKeywordSup()
  -> Unique<asts::TokenAst> {
  PARSE_ONCE(p1, [this] { return ParseTokenRaw(lex::RawTokenType::KW_SUP, lex::SppTokenType::KW_SUP); });
  return FORWARD_AST(p1);
}

auto spp::parse::ParserSpp::ParseKeywordExt()
  -> Unique<asts::TokenAst> {
  PARSE_ONCE(p1, [this] { return ParseTokenRaw(lex::RawTokenType::KW_EXT, lex::SppTokenType::KW_EXT); });
  return FORWARD_AST(p1);
}

auto spp::parse::ParserSpp::ParseKeywordMut()
  -> Unique<asts::TokenAst> {
  PARSE_ONCE(p1, [this] { return ParseTokenRaw(lex::RawTokenType::KW_MUT, lex::SppTokenType::KW_MUT); });
  return FORWARD_AST(p1);
}

auto spp::parse::ParserSpp::ParseKeywordUse()
  -> Unique<asts::TokenAst> {
  PARSE_ONCE(p1, [this] { return ParseTokenRaw(lex::RawTokenType::KW_USE, lex::SppTokenType::KW_USE); });
  return FORWARD_AST(p1);
}

auto spp::parse::ParserSpp::ParseKeywordCmp()
  -> Unique<asts::TokenAst> {
  PARSE_ONCE(p1, [this] { return ParseTokenRaw(lex::RawTokenType::KW_CMP, lex::SppTokenType::KW_CMP); });
  return FORWARD_AST(p1);
}

auto spp::parse::ParserSpp::ParseKeywordLet()
  -> Unique<asts::TokenAst> {
  PARSE_ONCE(p1, [this] { return ParseTokenRaw(lex::RawTokenType::KW_LET, lex::SppTokenType::KW_LET); });
  return FORWARD_AST(p1);
}

auto spp::parse::ParserSpp::ParseKeywordType()
  -> Unique<asts::TokenAst> {
  PARSE_ONCE(p1, [this] { return ParseTokenRaw(lex::RawTokenType::KW_TYPE, lex::SppTokenType::KW_TYPE); });
  return FORWARD_AST(p1);
}

auto spp::parse::ParserSpp::ParseKeywordSelf()
  -> Unique<asts::TokenAst> {
  PARSE_ONCE(p1, [this] { return ParseTokenRaw(lex::RawTokenType::KW_SELF, lex::SppTokenType::KW_SELF); });
  return FORWARD_AST(p1);
}

auto spp::parse::ParserSpp::ParseKeywordCase()
  -> Unique<asts::TokenAst> {
  PARSE_ONCE(p1, [this] { return ParseTokenRaw(lex::RawTokenType::KW_CASE, lex::SppTokenType::KW_CASE); });
  return FORWARD_AST(p1);
}

auto spp::parse::ParserSpp::ParseKeywordOf()
  -> Unique<asts::TokenAst> {
  PARSE_ONCE(p1, [this] { return ParseTokenRaw(lex::RawTokenType::KW_OF, lex::SppTokenType::KW_OF); });
  return FORWARD_AST(p1);
}

auto spp::parse::ParserSpp::ParseKeywordLoop()
  -> Unique<asts::TokenAst> {
  PARSE_ONCE(p1, [this] { return ParseTokenRaw(lex::RawTokenType::KW_LOOP, lex::SppTokenType::KW_LOOP); });
  return FORWARD_AST(p1);
}

auto spp::parse::ParserSpp::ParseKeywordIn()
  -> Unique<asts::TokenAst> {
  PARSE_ONCE(p1, [this] { return ParseTokenRaw(lex::RawTokenType::KW_IN, lex::SppTokenType::KW_IN); });
  return FORWARD_AST(p1);
}

auto spp::parse::ParserSpp::ParseKeywordTo()
  -> Unique<asts::TokenAst> {
  PARSE_ONCE(p1, [this] { return ParseTokenRaw(lex::RawTokenType::KW_TO, lex::SppTokenType::KW_TO); });
  return FORWARD_AST(p1);
}

auto spp::parse::ParserSpp::ParseKeywordElse()
  -> Unique<asts::TokenAst> {
  PARSE_ONCE(p1, [this] { return ParseTokenRaw(lex::RawTokenType::KW_ELSE, lex::SppTokenType::KW_ELSE); });
  return FORWARD_AST(p1);
}

auto spp::parse::ParserSpp::ParseKeywordGen()
  -> Unique<asts::TokenAst> {
  PARSE_ONCE(p1, [this] { return ParseTokenRaw(lex::RawTokenType::KW_GEN, lex::SppTokenType::KW_GEN); });
  return FORWARD_AST(p1);
}

auto spp::parse::ParserSpp::ParseKeywordWith()
  -> Unique<asts::TokenAst> {
  PARSE_ONCE(p1, [this] { return ParseTokenRaw(lex::RawTokenType::KW_WITH, lex::SppTokenType::KW_WITH); });
  return FORWARD_AST(p1);
}

auto spp::parse::ParserSpp::ParseKeywordDefer()
  -> Unique<asts::TokenAst> {
  PARSE_ONCE(p1, [this] { return ParseTokenRaw(lex::RawTokenType::KW_DEFER, lex::SppTokenType::KW_DEFER); });
  return FORWARD_AST(p1);
}

auto spp::parse::ParserSpp::ParseKeywordRet()
  -> Unique<asts::TokenAst> {
  PARSE_ONCE(p1, [this] { return ParseTokenRaw(lex::RawTokenType::KW_RET, lex::SppTokenType::KW_RET); });
  return FORWARD_AST(p1);
}

auto spp::parse::ParserSpp::ParseKeywordExit()
  -> Unique<asts::TokenAst> {
  PARSE_ONCE(p1, [this] { return ParseTokenRaw(lex::RawTokenType::KW_EXIT, lex::SppTokenType::KW_EXIT); });
  return FORWARD_AST(p1);
}

auto spp::parse::ParserSpp::ParseKeywordSkip()
  -> Unique<asts::TokenAst> {
  PARSE_ONCE(p1, [this] { return ParseTokenRaw(lex::RawTokenType::KW_SKIP, lex::SppTokenType::KW_SKIP); });
  return FORWARD_AST(p1);
}

auto spp::parse::ParserSpp::ParseKeywordIs()
  -> Unique<asts::TokenAst> {
  PARSE_ONCE(p1, [this] { return ParseTokenRaw(lex::RawTokenType::KW_IS, lex::SppTokenType::KW_IS); });
  return FORWARD_AST(p1);
}

auto spp::parse::ParserSpp::ParseKeywordAs()
  -> Unique<asts::TokenAst> {
  PARSE_ONCE(p1, [this] { return ParseTokenRaw(lex::RawTokenType::KW_AS, lex::SppTokenType::KW_AS); });
  return FORWARD_AST(p1);
}

auto spp::parse::ParserSpp::ParseKeywordOr()
  -> Unique<asts::TokenAst> {
  PARSE_ONCE(p1, [this] { return ParseTokenRaw(lex::RawTokenType::KW_OR, lex::SppTokenType::KW_OR); });
  return FORWARD_AST(p1);
}

auto spp::parse::ParserSpp::ParseKeywordAnd()
  -> Unique<asts::TokenAst> {
  PARSE_ONCE(p1, [this] { return ParseTokenRaw(lex::RawTokenType::KW_AND, lex::SppTokenType::KW_AND); });
  return FORWARD_AST(p1);
}

auto spp::parse::ParserSpp::ParseKeywordNot()
  -> Unique<asts::TokenAst> {
  PARSE_ONCE(p1, [this] { return ParseTokenRaw(lex::RawTokenType::KW_NOT, lex::SppTokenType::KW_NOT); });
  return FORWARD_AST(p1);
}

auto spp::parse::ParserSpp::ParseKeywordAsync()
  -> Unique<asts::TokenAst> {
  PARSE_ONCE(p1, [this] { return ParseTokenRaw(lex::RawTokenType::KW_ASYNC, lex::SppTokenType::KW_ASYNC); });
  return FORWARD_AST(p1);
}

auto spp::parse::ParserSpp::ParseKeywordTrue()
  -> Unique<asts::TokenAst> {
  PARSE_ONCE(p1, [this] { return ParseTokenRaw(lex::RawTokenType::KW_TRUE, lex::SppTokenType::KW_TRUE); });
  return FORWARD_AST(p1);
}

auto spp::parse::ParserSpp::ParseKeywordFalse()
  -> Unique<asts::TokenAst> {
  PARSE_ONCE(p1, [this] { return ParseTokenRaw(lex::RawTokenType::KW_FALSE, lex::SppTokenType::KW_FALSE); });
  return FORWARD_AST(p1);
}

auto spp::parse::ParserSpp::ParseKeywordAwait()
  -> Unique<asts::TokenAst> {
  PARSE_ONCE(p1, [this] { return ParseTokenRaw(lex::RawTokenType::KW_AWAIT, lex::SppTokenType::KW_AWAIT); });
  return FORWARD_AST(p1);
}

auto spp::parse::ParserSpp::ParseKeywordRes()
  -> Unique<asts::TokenAst> {
  PARSE_ONCE(p1, [this] { return ParseTokenRaw(lex::RawTokenType::KW_RES, lex::SppTokenType::KW_RES); });
  return FORWARD_AST(p1);
}

auto spp::parse::ParserSpp::ParseKeywordCaps()
  -> Unique<asts::TokenAst> {
  PARSE_ONCE(p1, [this] { return ParseTokenRaw(lex::RawTokenType::KW_CAPS, lex::SppTokenType::KW_CAPS); });
  return FORWARD_AST(p1);
}

auto spp::parse::ParserSpp::ParseTokenRaw(const lex::RawTokenType tok, lex::SppTokenType mapped_tok)
  -> Unique<asts::TokenAst> {
  if (_Pos > _Tokens.Len()) {
    return nullptr;
  }

  if (tok != lex::RawTokenType::TK_LINE_FEED and tok != lex::RawTokenType::TK_SPACE) {
    while (_Tokens[_Pos].Type == lex::RawTokenType::TK_LINE_FEED or _Tokens[_Pos].Type == lex::RawTokenType::TK_SPACE) {
      _Pos++;
    }
  }

  else if (tok == lex::RawTokenType::TK_LINE_FEED) {
    while (_Tokens[_Pos].Type == lex::RawTokenType::TK_SPACE) {
      _Pos++;
    }
  }

  else if (tok == lex::RawTokenType::TK_SPACE) {
    while (_Tokens[_Pos].Type == lex::RawTokenType::TK_LINE_FEED) {
      _Pos++;
    }
  }

  if (tok == lex::RawTokenType::SP_NO_TOK) {
    return CREATE_AST(asts::TokenAst, _Pos, mapped_tok, Str());
  }

  if (_Tokens[_Pos].Type != tok) {
    using namespace std::string_literals;
    if (_ErrorBuilder->Pos == _Pos) {
      _ErrorBuilder->Tokens.insert(mapped_tok);
      return nullptr;
    }

    // Naming the token only when it is one the author wrote. Past
    // the end of their file sits the appended prelude, and reporting
    // "got 'use'" against a "use" the compiler put there reads as a
    // complaint about their code.
    const auto got = _ErrorFormatter != nullptr and _ErrorFormatter->IsPastUserSource(_Pos)
      ? "the end of the file"s
      : "'"s + _Tokens[_Pos].Data + "'";

    if (_StoreError(_Pos, "Expected £, got "s + got)) {
      _ErrorBuilder->Tokens.insert(mapped_tok);
      return nullptr;
    }

    return nullptr;
  }

  const auto pos = _Pos;
  ++_Pos;
  return CREATE_AST(asts::TokenAst, pos, mapped_tok, _Tokens[_Pos - 1].Data.data());
}

auto spp::parse::ParserSpp::_StoreError(
  const std::size_t pos,
  Str &&err_str) const
  -> bool {
  if (pos > _ErrorBuilder->Pos) {
    _ErrorBuilder->WithArgs(std::move(err_str));
    _ErrorBuilder->Tokens.clear();
    _ErrorBuilder->Pos = pos;
    return true;
  }
  return false;
}

auto spp::parse::ParserSpp::_LineFeedAhead() const
  -> bool {
  auto pos = _Pos;
  while (pos < _TokensLen and _Tokens[pos].Type == lex::RawTokenType::TK_SPACE) {
    ++pos;
  }
  return pos < _TokensLen and _Tokens[pos].Type == lex::RawTokenType::TK_LINE_FEED;
}

SPP_MOD_END
