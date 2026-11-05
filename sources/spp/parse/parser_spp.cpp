module;
#include <spp/macros.hpp>
#include <spp/parse/macros.hpp>

#define NO_ANNOTATIONS Vec<Unique<AnnotationAst>>()

#define NO_TOKENS Vec<Unique<TokenAst>>()

module spp.parse.parser_spp;
import spp.asts._all;
import spp.asts.generate.common_types;
import spp.asts.utils.ast_utils;
import spp.parse.errors.parser_error;
import spp.parse.errors.parser_error_builder;
import spp.utils.ptr;
import genex;
import std;

SPP_MOD_BEGIN
constexpr auto kBinChars = spp::StrView("01");
constexpr auto kOctChars = spp::StrView("01234567");
constexpr auto kHexChars = spp::StrView("0123456789abcdef");

auto ParserSpp::Parse() -> Unique<ModulePrototypeAst> {
  auto root = ParseRoot();
  if (root == nullptr) {
    _ErrorBuilder->Raise();
  }
  return root;
}

auto ParserSpp::ParseRoot() -> Unique<ModulePrototypeAst> {
  PARSE_ONCE(p1, ParseModulePrototype)
  PARSE_ONCE(_a, ParseEof);
  return FORWARD_AST(p1);
}

auto ParserSpp::ParseEof() -> Unique<TokenAst> {
  PARSE_ONCE(p1, [this] {
    return ParseTokenRaw(
      lex::RawTokenType::SP_EOF, lex::SppTokenType::SP_NO_TOK);
  });
  return FORWARD_AST(p1);
}

auto ParserSpp::ParseModulePrototype()
  -> Unique<ModulePrototypeAst> {
  PARSE_ONCE(p1, ParseModuleImplementation);
  return _CreateAst<ModulePrototypeAst>(p1);
}

auto ParserSpp::ParseModuleImplementation()
  -> Unique<ModuleImplementationAst> {
  PARSE_ZERO_OR_MORE(p1, ParseModuleMember, ParseNewline);
  return _CreateAst<ModuleImplementationAst>(p1);
}

auto ParserSpp::ParseModuleMember()
  -> Unique<Ast> {
  PARSE_ALTERNATE(
    p1, Ast, ParseFunctionPrototype, ParseClassPrototype,
    ParseSupPrototypeExtension, ParseSupPrototypeFunctions,
    ParseGlobalUseStatement, ParseGlobalUseVarStatement,
    ParseGlobalTypeStatement, ParseGlobalCmpStatement);
  return FORWARD_AST(p1);
}

auto ParserSpp::ParseClassPrototype()
  -> Unique<ClassPrototypeAst> {
  PARSE_ZERO_OR_MORE(p1, ParseAnnotation, ParseNothing);
  PARSE_ONCE(p2, ParseKeywordCls);
  PARSE_ONCE(p3, ParseUpperIdentifier);
  PARSE_OPTIONAL(p4, ParseGenericParameterGroup);
  PARSE_ONCE(p5, ParseClassImplementation);
  return _CreateAst<ClassPrototypeAst>(
    p1, p2, TypeIdentifierAst::FromIdentifier(*p3), p4, p5);
}

auto ParserSpp::ParseClassImplementation()
  -> Unique<ClassImplementationAst> {
  PARSE_ONCE(p1, ParseTokenLeftCurlyBrace);
  PARSE_ZERO_OR_MORE(p2, ParseClassMember, ParseNewline);
  PARSE_ONCE(p3, ParseTokenRightCurlyBrace);
  return _CreateAst<ClassImplementationAst>(p1, p2, p3);
}

auto ParserSpp::ParseClassMember()
  -> Unique<Ast> {
  PARSE_ALTERNATE(p1, Ast, ParseClassAttribute);
  return FORWARD_AST(p1);
}

auto ParserSpp::ParseClassAttribute()
  -> Unique<ClassAttributeAst> {
  PARSE_ZERO_OR_MORE(p1, ParseAnnotation, ParseNothing);
  PARSE_ONCE(p2, ParseIdentifier);
  PARSE_ONCE(p3, ParseTokenColon);
  PARSE_ONCE(p4, ParseTypeExpression);
  PARSE_OPTIONAL(p5, ParseClassAttributeDefaultValue);
  return _CreateAst<ClassAttributeAst>(p1, p2, p3, p4, p5);
}

auto ParserSpp::ParseClassAttributeDefaultValue()
  -> Unique<ExpressionAst> {
  PARSE_ONCE(p1, ParseTokenAssign);
  PARSE_ONCE(p2, ParseExpression);
  return FORWARD_AST(p2);
}

auto ParserSpp::ParseSupPrototypeFunctions()
  -> Unique<SupPrototypeFunctionsAst> {
  PARSE_ONCE(p1, ParseKeywordSup);
  PARSE_OPTIONAL(p2, ParseGenericParameterGroup);
  PARSE_ONCE(p3, ParseTypeExpression);
  PARSE_ONCE(p4, ParseSupImplementation);
  return _CreateAst<SupPrototypeFunctionsAst>(p1, p2, p3, p4);
}

auto ParserSpp::ParseSupPrototypeExtension()
  -> Unique<SupPrototypeExtensionAst> {
  PARSE_ONCE(p1, ParseKeywordSup);
  PARSE_OPTIONAL(p2, ParseGenericParameterGroup);
  PARSE_ONCE(p3, ParseTypeExpression);
  PARSE_ONCE(p4, ParseKeywordExt);
  PARSE_ONCE(p5, ParseTypeExpression);
  PARSE_ONCE(p6, ParseSupImplementation);
  return _CreateAst<SupPrototypeExtensionAst>(p1, p2, p3, p4, p5, p6);
}

auto ParserSpp::ParseSupImplementation()
  -> Unique<SupImplementationAst> {
  PARSE_ONCE(p1, ParseTokenLeftCurlyBrace);
  PARSE_ZERO_OR_MORE(p2, ParseSupMember, ParseNewline);
  PARSE_ONCE(p3, ParseTokenRightCurlyBrace);
  return _CreateAst<SupImplementationAst>(p1, p2, p3);
}

auto ParserSpp::ParseSupMember()
  -> Unique<Ast> {
  PARSE_ALTERNATE(
    p1, Ast, ParseFunctionPrototype, ParseSupTypeStatement,
    ParseSupCmpStatement);
  return FORWARD_AST(p1);
}

auto ParserSpp::ParseSupTypeStatement()
  -> Unique<TypeStatementAst> {
  PARSE_ZERO_OR_MORE(p1, ParseAnnotation, ParseNothing);
  PARSE_ONCE(p2, ParseTypeStatement);
  p2->Annotations = std::move(p1);
  return p2;
}

auto ParserSpp::ParseSupCmpStatement()
  -> Unique<CmpStatementAst> {
  PARSE_ZERO_OR_MORE(p1, ParseAnnotation, ParseNothing);
  PARSE_ONCE(p2, ParseCmpStatement);
  p2->Annotations = std::move(p1);
  return p2;
}

auto ParserSpp::ParseFunctionPrototype()
  -> Unique<FunctionPrototypeAst> {
  PARSE_ALTERNATE(
    p1, FunctionPrototypeAst, ParseSubroutinePrototype,
    ParseCoroutinePrototype);
  return FORWARD_AST(p1);
}

auto ParserSpp::ParseSubroutinePrototype()
  -> Unique<SubroutinePrototypeAst> {
  PARSE_ZERO_OR_MORE(p1, ParseAnnotation, ParseNothing);
  PARSE_OPTIONAL(p2, ParseKeywordCmp);
  PARSE_ONCE(p3, ParseKeywordFun);
  PARSE_ONCE(p4, ParseIdentifier);
  PARSE_OPTIONAL(p5, ParseGenericParameterGroup);
  PARSE_ONCE(p6, ParseFunctionParameterGroup);
  PARSE_ONCE(p7, ParseTokenArrowRight);
  PARSE_ONCE(p8, ParseTypeExpression);
  PARSE_ONCE(p9, ParseFunctionImplementation);
  return _CreateAst<SubroutinePrototypeAst>(
    p1, p2, p3, p4, p5, p6, p7, p8, p9);
}

auto ParserSpp::ParseCoroutinePrototype()
  -> Unique<CoroutinePrototypeAst> {
  PARSE_ZERO_OR_MORE(p1, ParseAnnotation, ParseNothing);
  PARSE_ONCE(p2, ParseKeywordCor);
  PARSE_ONCE(p3, ParseIdentifier);
  PARSE_OPTIONAL(p4, ParseGenericParameterGroup);
  PARSE_ONCE(p5, ParseFunctionParameterGroup);
  PARSE_ONCE(p6, ParseTokenArrowRight);
  PARSE_ONCE(p7, ParseTypeExpression);
  PARSE_ONCE(p8, ParseFunctionImplementation);
  return _CreateAst<CoroutinePrototypeAst>(
    p1, nullptr, p2, p3, p4, p5, p6, p7, p8);
}

auto ParserSpp::ParseFunctionImplementation()
  -> Unique<FunctionImplementationAst> {
  PARSE_ONCE(p1, ParseTokenLeftCurlyBrace);
  PARSE_ZERO_OR_MORE(p2, ParseFunctionMember, ParseNewline);
  PARSE_ONCE(p3, ParseTokenRightCurlyBrace);
  return _CreateAst<FunctionImplementationAst>(p1, p2, p3);
}

auto ParserSpp::ParseFunctionMember()
  -> Unique<StatementAst> {
  PARSE_ONCE(p1, ParseStatement);
  return FORWARD_AST(p1);
}

auto ParserSpp::ParseFunctionParameterGroup()
  -> Unique<FunctionParameterGroupAst> {
  PARSE_ONCE(p1, ParseTokenLeftParenthesis);
  PARSE_ZERO_OR_MORE(p2, ParseFunctionParameter, ParseTokenComma);
  PARSE_ONCE(p3, ParseTokenRightParenthesis);
  return _CreateAst<FunctionParameterGroupAst>(p1, p2, p3);
}

auto ParserSpp::ParseFunctionParameter()
  -> Unique<FunctionParameterAst> {
  PARSE_ALTERNATE(
    p1, FunctionParameterAst, ParseFunctionParameterVariadic,
    ParseFunctionParameterOptional, ParseFunctionParameterRequired,
    ParseFunctionParameterSelf);
  return FORWARD_AST(p1);
}

auto ParserSpp::ParseFunctionParameterSelf()
  -> Unique<FunctionParameterSelfAst> {
  PARSE_ALTERNATE(
    p1, FunctionParameterSelfAst,
    ParseFunctionParameterSelfWithoutConvention,
    ParseFunctionParameterSelfWithConvention);
  return FORWARD_AST(p1);
}

auto ParserSpp::ParseFunctionParameterSelfWithConvention()
  -> Unique<FunctionParameterSelfAst> {
  PARSE_ONCE(p1, ParseConvention);
  PARSE_ONCE(p2, ParseSelfIdentifier);
  return _CreateAst<FunctionParameterSelfAst>(
    p1, _CreateAst<LocalVariableSingleIdentifierAst>(
      nullptr, p2, nullptr));
}

auto ParserSpp::ParseFunctionParameterSelfWithoutConvention()
  -> Unique<FunctionParameterSelfAst> {
  PARSE_OPTIONAL(p1, ParseKeywordMut)
  PARSE_ONCE(p2, ParseSelfIdentifier);
  return _CreateAst<FunctionParameterSelfAst>(
    nullptr, _CreateAst<LocalVariableSingleIdentifierAst>(
      p1, p2, nullptr));
}

auto ParserSpp::ParseFunctionParameterRequired()
  -> Unique<FunctionParameterRequiredAst> {
  PARSE_ONCE(p1, ParseLocalVariable);
  PARSE_ONCE(p2, ParseTokenColon);
  PARSE_ONCE(p3, ParseTypeExpression);
  return _CreateAst<FunctionParameterRequiredAst>(p1, p2, p3);
}

auto ParserSpp::ParseFunctionParameterOptional()
  -> Unique<FunctionParameterOptionalAst> {
  PARSE_ONCE(p1, ParseLocalVariable);
  PARSE_ONCE(p2, ParseTokenColon);
  PARSE_ONCE(p3, ParseTypeExpression);
  PARSE_ONCE(p4, ParseTokenAssign);
  PARSE_ONCE(p5, ParseExpression);
  return _CreateAst<FunctionParameterOptionalAst>(p1, p2, p3, p4, p5);
}

auto ParserSpp::ParseFunctionParameterVariadic()
  -> Unique<FunctionParameterVariadicAst> {
  PARSE_ONCE(p1, ParseTokenDoubleDot);
  PARSE_ONCE(p2, ParseLocalVariable);
  PARSE_ONCE(p3, ParseTokenColon);
  PARSE_ONCE(p4, ParseTypeExpression);
  return _CreateAst<FunctionParameterVariadicAst>(p1, p2, p3, p4);
}

auto ParserSpp::ParseFunctionCallArgumentGroup()
  -> Unique<FunctionCallArgumentGroupAst> {
  PARSE_ONCE(p1, ParseTokenLeftParenthesis);
  PARSE_ZERO_OR_MORE(p2, ParseFunctionCallArgument, ParseTokenComma);
  PARSE_ONCE(p3, ParseTokenRightParenthesis);
  return _CreateAst<FunctionCallArgumentGroupAst>(p1, p2, p3);
}

auto ParserSpp::ParseFunctionCallArgument()
  -> Unique<FunctionCallArgumentAst> {
  PARSE_ALTERNATE(
    p1, FunctionCallArgumentAst, ParseFunctionCallArgumentKeyword,
    ParseFunctionCallArgumentPositional);
  return FORWARD_AST(p1);
}

auto ParserSpp::ParseFunctionCallArgumentPositional()
  -> Unique<FunctionCallArgumentPositionalAst> {
  PARSE_OPTIONAL(p1, ParseConvention);
  PARSE_OPTIONAL(p2, ParseTokenDoubleDot)
  PARSE_ONCE(p3, ParseExpression);
  return _CreateAst<FunctionCallArgumentPositionalAst>(p1, p2, p3);
}

auto ParserSpp::ParseFunctionCallArgumentKeyword()
  -> Unique<FunctionCallArgumentKeywordAst> {
  PARSE_ONCE(p1, ParseIdentifier);
  PARSE_ONCE(p2, ParseTokenAssign);
  PARSE_OPTIONAL(p3, ParseConvention);
  PARSE_ONCE(p4, ParseExpression);
  return _CreateAst<FunctionCallArgumentKeywordAst>(p1, p2, p3, p4);
}

auto ParserSpp::ParseGenericParameterGroup()
  -> Unique<GenericParameterGroupAst> {
  PARSE_ONCE(p1, ParseTokenLeftSquareBracket);
  PARSE_ONE_OR_MORE(p2, ParseGenericParameter, ParseTokenComma);
  PARSE_ONCE(p3, ParseTokenRightSquareBracket);
  return _CreateAst<GenericParameterGroupAst>(p1, p2, p3);
}

auto ParserSpp::ParseGenericParameter()
  -> Unique<GenericParameterAst> {
  PARSE_ALTERNATE(
    p1, GenericParameterAst, ParseGenericParameterType,
    ParseGenericParameterComp);
  return FORWARD_AST(p1);
}

auto ParserSpp::ParseGenericParameterComp()
  -> Unique<GenericParameterAst> {
  PARSE_ALTERNATE(
    p1, GenericParameterAst, ParseGenericParameterCompVariadic,
    ParseGenericParameterCompOptional, ParseGenericParameterCompRequired);
  return FORWARD_AST(p1);
}

auto ParserSpp::ParseGenericParameterCompRequired()
  -> Unique<GenericParameterAst> {
  PARSE_ONCE(p1, ParseKeywordCmp);
  PARSE_ONCE(p2, ParseIdentifier);
  PARSE_ONCE(p3, ParseTokenColon);
  PARSE_ONCE(p4, ParseTypeExpression);
  return _CreateAst<GenericParameterAst>(
    p1, nullptr, TypeIdentifierAst::FromIdentifier(*p2), nullptr, p3, p4,
    nullptr, nullptr, nullptr);
}

auto ParserSpp::ParseGenericParameterCompOptional()
  -> Unique<GenericParameterAst> {
  PARSE_ONCE(p1, ParseKeywordCmp);
  PARSE_ONCE(p2, ParseIdentifier);
  PARSE_ONCE(p3, ParseTokenColon);
  PARSE_ONCE(p4, ParseTypeExpression);
  PARSE_ONCE(p5, ParseTokenAssign);
  PARSE_ONCE(p6, ParseExpression);
  return _CreateAst<GenericParameterAst>(
    p1, nullptr, TypeIdentifierAst::FromIdentifier(*p2), nullptr, p3, p4,
    p5, nullptr, p6);
}

auto ParserSpp::ParseGenericParameterCompVariadic()
  -> Unique<GenericParameterAst> {
  PARSE_ONCE(p1, ParseKeywordCmp);
  PARSE_ONCE(p2, ParseTokenDoubleDot);
  PARSE_ONCE(p3, ParseIdentifier);
  PARSE_ONCE(p4, ParseTokenColon);
  PARSE_ONCE(p5, ParseTypeExpression);
  return _CreateAst<GenericParameterAst>(
    p1, p2, TypeIdentifierAst::FromIdentifier(*p3), nullptr, p4, p5,
    nullptr, nullptr, nullptr);
}

auto ParserSpp::ParseGenericParameterType()
  -> Unique<GenericParameterAst> {
  PARSE_ALTERNATE(
    p1, GenericParameterAst, ParseGenericParameterTypeVariadic,
    ParseGenericParameterTypeOptional, ParseGenericParameterTypeRequired);
  return FORWARD_AST(p1);
}

auto ParserSpp::ParseGenericParameterTypeRequired()
  -> Unique<GenericParameterAst> {
  PARSE_ONCE(p1, ParseTypeIdentifier);
  PARSE_OPTIONAL(p2, ParseGenericParameterTypeConstraints);
  return _CreateAst<GenericParameterAst>(
    nullptr, nullptr, p1, p2, nullptr, nullptr, nullptr, nullptr, nullptr);
}

auto ParserSpp::ParseGenericParameterTypeOptional()
  -> Unique<GenericParameterAst> {
  PARSE_ONCE(p1, ParseTypeIdentifier);
  PARSE_OPTIONAL(p2, ParseGenericParameterTypeConstraints);
  PARSE_ONCE(p3, ParseTokenAssign);
  PARSE_ONCE(p4, ParseTypeExpression);
  return _CreateAst<GenericParameterAst>(
    nullptr, nullptr, p1, p2, nullptr, nullptr, p3, p4, nullptr);
}

auto ParserSpp::ParseGenericParameterTypeVariadic()
  -> Unique<GenericParameterAst> {
  PARSE_ONCE(p1, ParseTokenDoubleDot);
  PARSE_ONCE(p2, ParseTypeIdentifier);
  PARSE_OPTIONAL(p3, ParseGenericParameterTypeConstraints);
  return _CreateAst<GenericParameterAst>(
    nullptr, p1, p2, p3, nullptr, nullptr, nullptr, nullptr, nullptr);
}

auto ParserSpp::ParseGenericParameterTypeConstraints()
  -> Unique<GenericParameterTypeConstraintsAst> {
  PARSE_ONCE(p1, ParseTokenColon);
  PARSE_ONE_OR_MORE(p2, ParseTypeExpression, ParseTokenBitAnd);
  return _CreateAst<GenericParameterTypeConstraintsAst>(p1, p2);
}

auto ParserSpp::ParseGenericArgumentGroup()
  -> Unique<GenericArgumentGroupAst> {
  PARSE_ONCE(p1, ParseTokenLeftSquareBracket);
  PARSE_ONE_OR_MORE(p2, ParseGenericArgument, ParseTokenComma);
  PARSE_ONCE(p3, ParseTokenRightSquareBracket);
  return _CreateAst<GenericArgumentGroupAst>(p1, p2, p3);
}

auto ParserSpp::ParseGenericArgument()
  -> Unique<GenericArgumentAst> {
  PARSE_ALTERNATE(
    p1, GenericArgumentAst, ParseGenericArgumentType,
    ParseGenericArgumentComp);
  return FORWARD_AST(p1);
}

auto ParserSpp::ParseGenericArgumentComp()
  -> Unique<GenericArgumentAst> {
  PARSE_ALTERNATE(
    p1, GenericArgumentAst, ParseGenericArgumentCompKeyword,
    ParseGenericArgumentCompPositional);
  return FORWARD_AST(p1);
}

auto ParserSpp::ParseGenericArgumentCompPositional()
  -> Unique<GenericArgumentAst> {
  PARSE_ONCE(p1, ParseExpression);
  return GenericArgumentAst::NewComp(nullptr, std::move(p1));
}

auto ParserSpp::ParseGenericArgumentCompKeyword()
  -> Unique<GenericArgumentAst> {
  PARSE_ONCE(p1, ParseIdentifier);
  PARSE_ONCE(p2, ParseTokenAssign);
  PARSE_ONCE(p3, ParseExpression);
  return _CreateAst<GenericArgumentAst>(
    TypeIdentifierAst::FromIdentifier(*p1), p2, nullptr, p3);
}

auto ParserSpp::ParseGenericArgumentType()
  -> Unique<GenericArgumentAst> {
  PARSE_ALTERNATE(
    p1, GenericArgumentAst, ParseGenericArgumentTypeKeyword,
    ParseGenericArgumentTypePositional);
  PARSE_NEGATE(lex::RawTokenType::TK_LEFT_PARENTHESIS)
  PARSE_NEGATE(lex::RawTokenType::TK_COLON)
  return FORWARD_AST(p1);
}

auto ParserSpp::ParseGenericArgumentTypePositional()
  -> Unique<GenericArgumentAst> {
  // 1964
  PARSE_ONCE(p1, ParseTypeExpression);
  return GenericArgumentAst::NewType(nullptr, AstClone(p1));
}

auto ParserSpp::ParseGenericArgumentTypeKeyword()
  -> Unique<GenericArgumentAst> {
  PARSE_ONCE(p1, ParseUpperIdentifier);
  PARSE_ONCE(p2, ParseTokenAssign);
  PARSE_ONCE(p3, ParseTypeExpression);
  return _CreateAst<GenericArgumentAst>(
    TypeIdentifierAst::FromIdentifier(*p1), p2, p3, nullptr);
}

auto ParserSpp::ParseAnnotation()
  -> Unique<AnnotationAst> {
  PARSE_ALTERNATE(p1, AnnotationAst, ParseAnnotationCall,
  ParseAnnotationNoCall);
  return FORWARD_AST(p1);
}

auto ParserSpp::ParseAnnotationCall()
  -> Unique<AnnotationAst> {
  PARSE_ONCE(p1, ParseTokenExclamationMark);
  PARSE_ONCE(p2, ParsePostfixExpressionStrictlyStaticAccessZero)
  PARSE_OPTIONAL(p3, ParseGenericArgumentGroup);
  PARSE_ONCE(p4, ParseFunctionCallArgumentGroup);
  return _CreateAst<AnnotationAst>(p1, p2, p3, p4);
}

auto ParserSpp::ParseAnnotationNoCall()
  -> Unique<AnnotationAst> {
  PARSE_ONCE(p1, ParseTokenExclamationMark);
  PARSE_ONCE(p2, ParsePostfixExpressionStrictlyStaticAccessZero);
  return _CreateAst<AnnotationAst>(p1, p2, nullptr, nullptr);
}

auto ParserSpp::ParseExpression()
  -> Unique<ExpressionAst> {
  PARSE_ONCE(p1, ParseBinaryExpression);
  return FORWARD_AST(p1);
}

auto ParserSpp::ParseBinaryExpression(const std::uint8_t min_prec)
  -> Unique<ExpressionAst> {
  using RT = lex::RawTokenType;

  struct BinOpInfo {
    Unique<TokenAst> Tok;
    std::uint8_t Prec;
    bool IsIs;
  };

  auto try_tok = [this](
    auto fn, const std::uint8_t prec, const bool is_is = false)
    -> Opt<BinOpInfo> {
    const auto pos = _Pos;
    if (auto tok = fn()) return BinOpInfo{std::move(tok), prec, is_is};
    _Pos = pos;
    return Nullopt;
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
      lhs = _CreateAst<IsExpressionAst>(lhs, bin_op.Tok, rhs);
    }
    else {
      auto rhs = ParseBinaryExpression(static_cast<std::uint8_t>(bin_op.Prec + 1));
      if (!rhs) {
        _Pos = saved;
        break;
      }
      lhs = _CreateAst<BinaryExpressionAst>(lhs, bin_op.Tok, rhs);
    }
  }

  return lhs;
}

auto ParserSpp::ParseUnaryExpression()
  -> Unique<ExpressionAst> {
  PARSE_ZERO_OR_MORE(p1, ParseUnaryExpressionOp, ParseNothing);
  PARSE_ONCE(p2, ParsePostfixExpression);
  return genex::fold_left(
    p1 | genex::views::move_reverse, std::move(p2),
    [](Unique<ExpressionAst> &&acc, Unique<UnaryExpressionOperatorAst> &&x) {
      return _CreateAst<UnaryExpressionAst>(std::move(x), std::move(acc));
    });
}

auto ParserSpp::ParseUnaryExpressionOp()
  -> Unique<UnaryExpressionOperatorAst> {
  PARSE_ALTERNATE(
    p1, UnaryExpressionOperatorAst, ParseUnaryExpressionOpAsync);
  return FORWARD_AST(p1);
}

auto ParserSpp::ParseUnaryExpressionOpAsync()
  -> Unique<UnaryExpressionOperatorAsyncAst> {
  PARSE_ONCE(p1, ParseKeywordAsync);
  return _CreateAst<UnaryExpressionOperatorAsyncAst>(p1);
}

auto ParserSpp::ParsePostfixExpression()
  -> Unique<ExpressionAst> {
  PARSE_ONCE(p1, ParsePrimaryExpression);
  PARSE_ZERO_OR_MORE(p2, ParsePostfixExpressionOp, ParseNothing);
  return genex::fold_left(
    p2 | genex::views::move, std::move(p1),
    [](Unique<ExpressionAst> &&acc, Unique<PostfixExpressionOperatorAst> &&x) {
      return _CreateAst<PostfixExpressionAst>(std::move(acc), std::move(x));
    });
}

auto ParserSpp::ParsePostfixExpressionOp()
  -> Unique<PostfixExpressionOperatorAst> {
  PARSE_ALTERNATE(
    p1, PostfixExpressionOperatorAst, ParsePostfixExpressionOpDeref,
    ParsePostfixExpressionOpEarlyReturn, ParsePostfixExpressionOpFunctionCall,
    ParsePostfixExpressionOpRuntimeMemberAccess,
    ParsePostfixExpressionOpStaticMemberAccess,
    ParsePostfixExpressionOpKeywordNot, ParsePostfixExpressionOpKeywordRes,
    ParsePostfixExpressionOpKeywordAwait, ParsePostfixExpressionOpSlice,
    ParsePostfixExpressionOpIndex);
  return FORWARD_AST(p1);
}

auto ParserSpp::ParsePostfixExpressionOpDeref()
  -> Unique<PostfixExpressionOperatorDerefAst> {
  PARSE_ONCE(p1, ParseTokenDeref);
  PARSE_NEGATE(lex::RawTokenType::LX_CHARACTER)
  return _CreateAst<PostfixExpressionOperatorDerefAst>(p1);
}

auto ParserSpp::ParsePostfixExpressionOpEarlyReturn()
  -> Unique<PostfixExpressionOperatorEarlyReturnAst> {
  PARSE_ONCE(p1, ParseTokenQuestionMark);
  return _CreateAst<PostfixExpressionOperatorEarlyReturnAst>(p1);
}

auto ParserSpp::ParsePostfixExpressionOpFunctionCall()
  -> Unique<PostfixExpressionOperatorFunctionCallAst> {
  if (_LineFeedAhead()) { return nullptr; }
  PARSE_OPTIONAL(p1, ParseGenericArgumentGroup);
  PARSE_ONCE(p2, ParseFunctionCallArgumentGroup);
  PARSE_OPTIONAL(p3, ParseFoldExpression);
  return _CreateAst<PostfixExpressionOperatorFunctionCallAst>(p1, p2, p3);
}

auto ParserSpp::ParsePostfixExpressionOpRuntimeMemberAccess()
  -> Unique<PostfixExpressionOperatorRuntimeMemberAccessAst> {
  PARSE_ONCE(p1, ParseTokenDot);
  PARSE_ALTERNATE(
    p2, IdentifierAst, ParseIdentifier, ParseNumericIdentifier);
  return _CreateAst<PostfixExpressionOperatorRuntimeMemberAccessAst>(
    p1, p2);
}

auto ParserSpp::ParsePostfixExpressionOpStaticMemberAccess()
  -> Unique<PostfixExpressionOperatorStaticMemberAccessAst> {
  PARSE_ONCE(p1, ParseTokenDoubleColon);
  PARSE_ONCE(p2, ParseIdentifier);
  return _CreateAst<PostfixExpressionOperatorStaticMemberAccessAst>(
    p1, p2);
}

auto ParserSpp::ParsePostfixExpressionOpKeywordNot()
  -> Unique<PostfixExpressionOperatorKeywordNotAst> {
  PARSE_ONCE(p1, ParseTokenDot);
  PARSE_ONCE(p2, ParseKeywordNot);
  return _CreateAst<PostfixExpressionOperatorKeywordNotAst>(p1, p2);
}

auto ParserSpp::ParsePostfixExpressionOpKeywordAwait()
  -> Unique<PostfixExpressionOperatorKeywordAwaitAst> {
  PARSE_ONCE(p1, ParseTokenDot);
  PARSE_ONCE(p2, ParseKeywordAwait);
  return _CreateAst<PostfixExpressionOperatorKeywordAwaitAst>(p1, p2);
}

auto ParserSpp::ParsePostfixExpressionOpKeywordRes()
  -> Unique<PostfixExpressionOperatorKeywordResAst> {
  PARSE_ONCE(p1, ParseTokenDot);
  PARSE_ONCE(p2, ParseKeywordRes);
  PARSE_ONCE(p3, ParseFunctionCallArgumentGroup)
  return _CreateAst<PostfixExpressionOperatorKeywordResAst>(p1, p2, p3);
}

auto ParserSpp::ParsePostfixExpressionOpIndex()
  -> Unique<PostfixExpressionOperatorIndexAst> {
  if (_LineFeedAhead()) { return nullptr; }
  PARSE_ONCE(p1, ParseTokenLeftSquareBracket);
  PARSE_OPTIONAL(p2, ParseKeywordMut);
  PARSE_ONCE(p3, ParseExpression);
  PARSE_ONCE(p4, ParseTokenRightSquareBracket);
  return _CreateAst<PostfixExpressionOperatorIndexAst>(p1, p2, p3, p4);
}

auto ParserSpp::ParsePostfixExpressionOpSlice()
  -> Unique<PostfixExpressionOperatorSliceAst> {
  if (_LineFeedAhead()) { return nullptr; }
  PARSE_ONCE(p1, ParseTokenLeftSquareBracket);
  PARSE_OPTIONAL(p2, ParseKeywordMut);
  PARSE_OPTIONAL(p3, ParseExpression);
  PARSE_ONCE(p4, ParseKeywordTo);
  PARSE_OPTIONAL(p5, ParseExpression);
  PARSE_ONCE(p6, ParseTokenRightSquareBracket);
  return _CreateAst<PostfixExpressionOperatorSliceAst>(
    p1, p2, p3, p4, p5, p6);
}

auto ParserSpp::ParsePostfixExpressionStrictlyStaticAccessZero()
  -> Unique<ExpressionAst> {
  PARSE_ONCE(p1, ParseIdentifierAsExpression);
  PARSE_ZERO_OR_MORE(
    p2, ParsePostfixExpressionOpStaticMemberAccess, ParseNothing);
  return genex::fold_left(
    p2 | genex::views::move, std::move(p1),
    [](Unique<ExpressionAst> &&acc, Unique<PostfixExpressionOperatorStaticMemberAccessAst> &&x) {
      return _CreateAst<PostfixExpressionAst>(std::move(acc), std::move(x));
    });
}

auto ParserSpp::ParsePostfixExpressionStrictlyStaticAccessOne()
  -> Unique<ExpressionAst> {
  PARSE_ONCE(p1, ParseIdentifierAsExpression);
  PARSE_ONE_OR_MORE(
    p2, ParsePostfixExpressionOpStaticMemberAccess, ParseNothing);
  return genex::fold_left(
    p2 | genex::views::move, std::move(p1),
    [](Unique<ExpressionAst> &&acc, Unique<PostfixExpressionOperatorStaticMemberAccessAst> &&x) {
      return _CreateAst<PostfixExpressionAst>(
        std::move(acc), std::move(x));
    });
}

auto ParserSpp::ParsePrimaryExpression()
  -> Unique<ExpressionAst> {
  PARSE_ALTERNATE(
    p1, PrimaryExpressionAst, ParseClosureExpression,
    ParseParenthesisedExpression, ParseLiteral, ParseObjectInitializer,
    ParseCaseOfExpression, ParseCaseExpression, ParseLoopExpression,
    ParseGenExpression, ParseTypeExpression, ParseIdentifier,
    ParseSelfIdentifier, [this] { return ParseInnerScopeExpression([this] { return ParseStatement(); }); },
    ParseFoldExpression);
  return FORWARD_AST(p1);
}

auto ParserSpp::ParseParenthesisedExpression()
  -> Unique<ParenthesisedExpressionAst> {
  PARSE_ONCE(p1, ParseTokenLeftParenthesis);
  PARSE_ONCE(p2, ParseExpression);
  PARSE_ONCE(p3, ParseTokenRightParenthesis);
  return _CreateAst<ParenthesisedExpressionAst>(p1, p2, p3);
}

auto ParserSpp::ParseFoldExpression()
  -> Unique<FoldExpressionAst> {
  PARSE_ONCE(p1, ParseTokenDoubleDot);
  return _CreateAst<FoldExpressionAst>(p1);
}

auto ParserSpp::ParseCaseExpression()
  -> Unique<CaseExpressionAst> {
  PARSE_ONCE(p1, ParseKeywordCase);
  PARSE_ONCE(p2, ParseExpression);
  PARSE_ONCE(p3, [this] { return ParseInnerScopeExpression([this] { return ParseStatement(); }); })
  PARSE_ZERO_OR_MORE(p4, ParseCaseExpressionBranch, ParseNothing);
  return CaseExpressionAst::NewNonPatternMatch(
    std::move(p1), std::move(p2), std::move(p3), std::move(p4));
}

auto ParserSpp::ParseCaseExpressionBranch()
  -> Unique<CaseExpressionBranchAst> {
  PARSE_ALTERNATE(
    p1, CaseExpressionBranchAst, ParseCaseExpressionBranchElseCase,
    ParseCaseExpressionBranchElse)
  return p1;
}

auto ParserSpp::ParseCaseExpressionBranchElse()
  -> Unique<CaseExpressionBranchAst> {
  PARSE_ONCE(p1, ParseCaseExpressionPatternVariantElse);
  PARSE_ONCE(p2, [this] { return ParseInnerScopeExpression([this] { return ParseStatement(); }); })

  auto temp = Vec<decltype(p1)>(1);
  temp[0] = std::move(p1);
  return _CreateAst<CaseExpressionBranchAst>(nullptr, std::move(temp), nullptr, p2);
}

auto ParserSpp::ParseCaseExpressionBranchElseCase()
  -> Unique<CaseExpressionBranchAst> {
  PARSE_ONCE(p1, ParseCaseExpressionPatternVariantElseCase);

  auto else_pattern = unique_cast<CasePatternVariantAst>(
    _CreateAst<CasePatternVariantElseAst>(nullptr));
  auto temp_1 = Vec<Unique<StatementAst>>();
  auto ptr = unique_cast<StatementAst>(
    std::move(unique_cast<CasePatternVariantElseCaseAst>(std::move(p1))->CaseExpr));
  temp_1.emplace_back(std::move(ptr));

  auto else_case_body = _CreateAst<InnerScopeExpressionAst>(nullptr, std::move(temp_1), nullptr);
  auto temp_2 = Vec<decltype(else_pattern)>();
  temp_2.emplace_back(std::move(else_pattern));
  return _CreateAst<CaseExpressionBranchAst>(nullptr, temp_2, nullptr, else_case_body);
}

auto ParserSpp::ParseCaseOfExpression()
  -> Unique<CaseExpressionAst> {
  PARSE_ONCE(p1, ParseKeywordCase);
  PARSE_ONCE(p2, ParseExpression);
  PARSE_ONCE(p3, ParseKeywordOf);
  PARSE_ONCE(p4, ParseTokenLeftCurlyBrace);
  PARSE_ONE_OR_MORE(p5, ParseCaseOfExpressionBranch, ParseNewline);
  PARSE_ONCE(p6, ParseTokenRightCurlyBrace);
  return _CreateAst<CaseExpressionAst>(p1, p2, p3, p5);
}

auto ParserSpp::ParseCaseOfExpressionBranch()
  -> Unique<CaseExpressionBranchAst> {
  PARSE_ALTERNATE(
    p1, CaseExpressionBranchAst, ParseCaseOfExpressionBranchDestructuring,
    ParseCaseOfExpressionBranchComparing, ParseCaseExpressionBranchElseCase,
    ParseCaseExpressionBranchElse);
  return FORWARD_AST(p1);
}

auto ParserSpp::ParseCaseOfExpressionBranchDestructuring()
  -> Unique<CaseExpressionBranchAst> {
  PARSE_ONCE(p1, ParseKeywordIs);
  PARSE_ONCE(p2, ParseCaseExpressionPatternVariantDestructure);
  PARSE_OPTIONAL(p3, ParsePatternGuard)
  PARSE_ONCE(p4, [this] { return ParseInnerScopeExpression([this] { return ParseStatement(); }); })
  auto temp = Vec<decltype(p2)>(1);
  temp[0] = std::move(p2);
  return _CreateAst<CaseExpressionBranchAst>(p1, temp, p3, p4);
}

auto ParserSpp::ParseCaseOfExpressionBranchComparing()
  -> Unique<CaseExpressionBranchAst> {
  PARSE_ONCE(p1, ParseBooleanComparisonOp)
  PARSE_ONE_OR_MORE(
    p2, ParseCaseExpressionPatternVariantExpression, ParseTokenComma);
  PARSE_ONCE(p3, [this] { return ParseInnerScopeExpression([this] { return ParseStatement(); }); })
  return _CreateAst<CaseExpressionBranchAst>(p1, p2, nullptr, p3);
}

auto ParserSpp::ParseCaseExpressionPatternVariantDestructure()
  -> Unique<CasePatternVariantAst> {
  PARSE_ALTERNATE(
    p1, CasePatternVariantAst,
    ParseCaseExpressionPatternVariantDestructureArray,
    ParseCaseExpressionPatternVariantDestructureObject,
    ParseCaseExpressionPatternVariantDestructureTuple);
  return FORWARD_AST(p1);
}

auto ParserSpp::ParseCaseExpressionPatternVariantDestructureArray()
  -> Unique<CasePatternVariantAst> {
  PARSE_ONCE(p1, ParseTokenLeftSquareBracket);
  PARSE_ZERO_OR_MORE(
    p2, ParseCaseExpressionPatternNestedForDestructureArray, ParseTokenComma);
  PARSE_ONCE(p3, ParseTokenRightSquareBracket);
  return _CreateAst<CasePatternVariantDestructureArrayAst>(
    p1, p2, p3);
}

auto ParserSpp::ParseCaseExpressionPatternVariantDestructureObject()
  -> Unique<CasePatternVariantAst> {
  PARSE_ONCE(p1, ParseTypeExpressionSimple)
  PARSE_ONCE(p2, ParseTokenLeftParenthesis);
  PARSE_ZERO_OR_MORE(
    p3, ParseCaseExpressionPatternNestedForDestructureObject, ParseTokenComma);
  PARSE_ONCE(p4, ParseTokenRightParenthesis);
  return _CreateAst<CasePatternVariantDestructureObjectAst>(
    p1, p2, p3, p4);
}

auto ParserSpp::ParseCaseExpressionPatternVariantDestructureTuple()
  -> Unique<CasePatternVariantAst> {
  PARSE_ONCE(p1, ParseTokenLeftParenthesis);
  PARSE_ZERO_OR_MORE(
    p2, ParseCaseExpressionPatternNestedForDestructureTuple, ParseTokenComma);
  PARSE_ONCE(p3, ParseTokenRightParenthesis);
  return _CreateAst<CasePatternVariantDestructureTupleAst>(
    p1, p2, p3);
}

auto ParserSpp::ParseCaseExpressionPatternVariantDestructureSkipSingleArgument()
  -> Unique<CasePatternVariantAst> {
  PARSE_ONCE(p1, ParseTokenUnderscore);
  return _CreateAst<CasePatternVariantDestructureSkipSingleArgumentAst>(
    p1);
}

auto ParserSpp::ParseCaseExpressionPatternVariantDestructureSkipMultipleArguments()
  -> Unique<CasePatternVariantAst> {
  PARSE_ONCE(p1, ParseTokenDoubleDot);
  PARSE_OPTIONAL(p2, ParseCaseExpressionPatternVariantSingleIdentifier);
  return _CreateAst<CasePatternVariantDestructureSkipMultipleArgumentsAst>(
    p1, p2);
}

auto ParserSpp::ParseCaseExpressionPatternVariantDestructureAttributeBinding()
  -> Unique<CasePatternVariantAst> {
  PARSE_ONCE(p1, ParseIdentifier);
  PARSE_ONCE(p2, ParseTokenAssign);
  PARSE_ONCE(p3, ParseCaseExpressionPatternNestedForDestructureAttributeBinding);
  return _CreateAst<CasePatternVariantDestructureAttributeBindingAst>(
    p1, p2, p3);
}

auto ParserSpp::ParseCaseExpressionPatternVariantSingleIdentifier()
  -> Unique<CasePatternVariantAst> {
  PARSE_ALTERNATE(
    p1, CasePatternVariantAst,
    ParseCaseExpressionPatternVariantSingleIdentifierWithConvention,
    ParseCaseExpressionPatternVariantSingleIdentifierWithoutConvention);
  return FORWARD_AST(p1);
}

auto ParserSpp::ParseCaseExpressionPatternVariantSingleIdentifierWithConvention()
  -> Unique<CasePatternVariantAst> {
  PARSE_ONCE(p1, ParseConvention);
  PARSE_ONCE(p2, ParseIdentifier);
  return _CreateAst<CasePatternVariantSingleIdentifierAst>(p1, nullptr, p2, nullptr);
}

auto ParserSpp::ParseCaseExpressionPatternVariantSingleIdentifierWithoutConvention()
  -> Unique<CasePatternVariantAst> {
  PARSE_OPTIONAL(p1, ParseKeywordMut);
  PARSE_ONCE(p2, ParseIdentifier);
  return _CreateAst<CasePatternVariantSingleIdentifierAst>(nullptr, p1, p2, nullptr);
}

auto ParserSpp::ParseCaseExpressionPatternVariantSingleIdentifierAliasable()
  -> Unique<CasePatternVariantAst> {
  PARSE_ONCE(p1, ParseCaseExpressionPatternVariantSingleIdentifier);
  PARSE_OPTIONAL(p2, ParseLocalVariableSingleIdentifierAlias);
  p1->To<CasePatternVariantSingleIdentifierAst>()->Alias = std::move(p2);
  return FORWARD_AST(p1);
}

auto ParserSpp::ParseCaseExpressionPatternVariantLiteral()
  -> Unique<CasePatternVariantAst> {
  PARSE_ALTERNATE(
    p1, LiteralAst, ParseLiteralFloat, ParseLiteralInteger, ParseLiteralChar,
    ParseLiteralString, ParseLiteralBoolean);
  return _CreateAst<CasePatternVariantLiteralAst>(p1);
}

auto ParserSpp::ParseCaseExpressionPatternVariantExpression()
  -> Unique<CasePatternVariantAst> {
  PARSE_ONCE(p1, ParseExpression);
  return _CreateAst<CasePatternVariantExpressionAst>(p1);
}

auto ParserSpp::ParseCaseExpressionPatternVariantElse()
  -> Unique<CasePatternVariantAst> {
  PARSE_ONCE(p1, ParseKeywordElse);
  return _CreateAst<CasePatternVariantElseAst>(p1);
}

auto ParserSpp::ParseCaseExpressionPatternVariantElseCase()
  -> Unique<CasePatternVariantAst> {
  PARSE_ONCE(p1, ParseKeywordElse);
  PARSE_ONCE(p2, ParseCaseExpression);
  return _CreateAst<CasePatternVariantElseCaseAst>(p1, p2);
}

auto ParserSpp::ParseCaseExpressionPatternNestedForDestructureArray()
  -> Unique<CasePatternVariantAst> {
  PARSE_ALTERNATE(
    p1, CasePatternVariantAst,
    ParseCaseExpressionPatternVariantDestructureSkipSingleArgument,
    ParseCaseExpressionPatternVariantDestructureSkipMultipleArguments,
    ParseCaseExpressionPatternVariantDestructureArray,
    ParseCaseExpressionPatternVariantDestructureTuple,
    ParseCaseExpressionPatternVariantDestructureObject,
    ParseCaseExpressionPatternVariantSingleIdentifier,
    ParseCaseExpressionPatternVariantLiteral);
  return FORWARD_AST(p1);
}

auto ParserSpp::ParseCaseExpressionPatternNestedForDestructureObject()
  -> Unique<CasePatternVariantAst> {
  PARSE_ALTERNATE(
    p1, CasePatternVariantAst,
    ParseCaseExpressionPatternVariantDestructureAttributeBinding,
    ParseCaseExpressionPatternVariantDestructureSkipMultipleArguments,
    ParseCaseExpressionPatternVariantSingleIdentifierAliasable);
  return FORWARD_AST(p1);
}

auto ParserSpp::ParseCaseExpressionPatternNestedForDestructureTuple()
  -> Unique<CasePatternVariantAst> {
  PARSE_ALTERNATE(
    p1, CasePatternVariantAst,
    ParseCaseExpressionPatternVariantDestructureSkipSingleArgument,
    ParseCaseExpressionPatternVariantDestructureSkipMultipleArguments,
    ParseCaseExpressionPatternVariantDestructureArray,
    ParseCaseExpressionPatternVariantDestructureTuple,
    ParseCaseExpressionPatternVariantDestructureObject,
    ParseCaseExpressionPatternVariantSingleIdentifier,
    ParseCaseExpressionPatternVariantLiteral);
  return FORWARD_AST(p1);
}

auto ParserSpp::ParseCaseExpressionPatternNestedForDestructureAttributeBinding()
  -> Unique<CasePatternVariantAst> {
  PARSE_ALTERNATE(
    p1, CasePatternVariantAst,
    ParseCaseExpressionPatternVariantDestructureArray,
    ParseCaseExpressionPatternVariantDestructureObject,
    ParseCaseExpressionPatternVariantDestructureTuple,
    ParseCaseExpressionPatternVariantLiteral);
  return FORWARD_AST(p1);
}

auto ParserSpp::ParsePatternGuard()
  -> Unique<PatternGuardAst> {
  PARSE_ONCE(p1, ParseKeywordAnd);
  PARSE_ONCE(p2, ParseExpression);
  return _CreateAst<PatternGuardAst>(p1, p2);
}

auto ParserSpp::ParseBooleanComparisonOp()
  -> Unique<TokenAst> {
  PARSE_ALTERNATE(
    p1, TokenAst, ParseTokenEquals, ParseTokenNotEquals,
    ParseTokenLessThanEquals, ParseTokenGreaterThanEquals, ParseTokenLessThan,
    ParseTokenGreaterThan);
  return FORWARD_AST(p1);
}

auto ParserSpp::ParseLoopExpression()
  -> Unique<LoopExpressionAst> {
  PARSE_ALTERNATE(
    p1, LoopExpressionAst, ParseLoopConditionalExpression,
    ParseLoopIterableExpression);
  return FORWARD_AST(p1);
}

auto ParserSpp::ParseLoopConditionalExpression()
  -> Unique<LoopConditionalExpressionAst> {
  PARSE_ONCE(p1, ParseKeywordLoop);
  PARSE_ONCE(p2, ParseExpression);
  PARSE_ONCE(p3, [this] { return ParseInnerScopeExpression([this] { return ParseStatement(); }); })
  PARSE_OPTIONAL(p4, ParseLoopElseStatement);
  return _CreateAst<LoopConditionalExpressionAst>(p1, p2, p3, p4);
}

auto ParserSpp::ParseLoopIterableExpression()
  -> Unique<LoopIterableExpressionAst> {
  PARSE_ONCE(p1, ParseKeywordLoop);
  PARSE_ONCE(p2, ParseLocalVariable);
  PARSE_ONCE(p3, ParseKeywordIn);
  PARSE_ONCE(p4, ParseExpression);
  PARSE_ONCE(p5, [this] { return ParseInnerScopeExpression([this] { return ParseStatement(); }); })
  PARSE_OPTIONAL(p6, ParseLoopElseStatement);
  return _CreateAst<LoopIterableExpressionAst>(p1, p2, p3, p4, p5, p6);
}

auto ParserSpp::ParseLoopElseStatement()
  -> Unique<LoopElseStatementAst> {
  PARSE_ONCE(p1, ParseKeywordElse);
  PARSE_ONCE(p2, [this] { return ParseInnerScopeExpression([this] { return ParseStatement(); }); })
  return _CreateAst<LoopElseStatementAst>(p1, p2);
}

auto ParserSpp::ParseGenExpression()
  -> Unique<GenExpressionAst> {
  PARSE_ALTERNATE(
    p1, GenExpressionAst, ParseGenExpressionWithExpression,
    ParseGenExpressionWithoutExpression);
  return FORWARD_AST(p1);
}

auto ParserSpp::ParseGenExpressionWithExpression()
  -> Unique<GenExpressionAst> {
  PARSE_ONCE(p1, ParseKeywordGen);
  PARSE_OPTIONAL(p2, ParseConvention);
  PARSE_ONCE(p3, ParseExpression);
  return _CreateAst<GenExpressionAst>(p1, p2, p3);
}

auto ParserSpp::ParseGenExpressionWithoutExpression()
  -> Unique<GenExpressionAst> {
  PARSE_ONCE(p1, ParseKeywordGen);
  return _CreateAst<GenExpressionAst>(p1, nullptr, nullptr);
}

auto ParserSpp::ParseGenUnrollExpression()
  -> Unique<GenWithExpressionAst> {
  PARSE_ONCE(p1, ParseKeywordGen);
  PARSE_ONCE(p2, ParseKeywordWith);
  PARSE_ONCE(p3, ParseExpression);
  return _CreateAst<GenWithExpressionAst>(p1, p2, p3);
}

auto ParserSpp::ParseInnerScopeExpression(auto &&parser)
  -> Unique<InnerScopeExpressionAst> {
  PARSE_ONCE(p1, ParseTokenLeftCurlyBrace);
  PARSE_ZERO_OR_MORE(p2, parser, ParseNewline);
  PARSE_ONCE(p3, ParseTokenRightCurlyBrace);
  return _CreateAst<InnerScopeExpressionAst>(p1, p2, p3);
}

auto ParserSpp::ParseStatement()
  -> Unique<StatementAst> {
  PARSE_ALTERNATE(
    p1, StatementAst, ParseTypeStatement, ParseLetStatement, ParseRetStatement,
    ParseExitStatement, ParseExitStatementWithValue, ParseSkipStatement,
    ParseDeferStatement, ParseAssignmentStatement, ParseGenUnrollExpression,
    ParseExpression);
  return FORWARD_AST(p1);
}

auto ParserSpp::ParseAssignmentStatement()
  -> Unique<AssignmentStatementAst> {
  PARSE_ONE_OR_MORE(p1, ParseAssignmentTarget, ParseTokenComma);
  PARSE_ONCE(p2, ParseTokenAssign);
  PARSE_ONE_OR_MORE(p3, ParseExpression, ParseTokenComma);
  return _CreateAst<AssignmentStatementAst>(p1, p2, p3);
}

auto ParserSpp::ParseAssignmentTarget()
  -> Unique<ExpressionAst> {
  // Skip binary expression on lhs of assignment.
  PARSE_ONCE(p1, ParseAssignmentTargetPostfixExpression);
  return FORWARD_AST(p1);
}

auto ParserSpp::ParseAssignmentTargetPostfixExpression()
  -> Unique<ExpressionAst> {
  PARSE_ONCE(p1, ParseAssignmentTargetPrimaryExpression);
  PARSE_ZERO_OR_MORE(
    p2, ParseAssignmentTargetPostfixExpressionOp, ParseNothing);
  return genex::fold_left(
    p2 | genex::views::move, std::move(p1),
    [](Unique<ExpressionAst> &&acc, Unique<PostfixExpressionOperatorAst> &&x) {
      return _CreateAst<PostfixExpressionAst>(std::move(acc), std::move(x));
    });
}

auto ParserSpp::ParseAssignmentTargetPostfixExpressionOp()
  -> Unique<PostfixExpressionOperatorAst> {
  PARSE_ALTERNATE(
    p1, PostfixExpressionOperatorAst,
    ParsePostfixExpressionOpDeref, ParsePostfixExpressionOpFunctionCall,
    ParsePostfixExpressionOpRuntimeMemberAccess,
    ParsePostfixExpressionOpStaticMemberAccess,
    ParsePostfixExpressionOpSlice, ParsePostfixExpressionOpIndex);
  return FORWARD_AST(p1);
}

auto ParserSpp::ParseAssignmentTargetPrimaryExpression()
  -> Unique<ExpressionAst> {
  PARSE_ALTERNATE(
    p1, PrimaryExpressionAst, ParseIdentifier, ParseSelfIdentifier);
  return FORWARD_AST(p1);
}

auto ParserSpp::ParseDeferStatement()
  -> Unique<DeferStatementAst> {
  PARSE_ONCE(p1, ParseKeywordDefer);
  PARSE_ONCE(p2, ParseExpression);
  return _CreateAst<DeferStatementAst>(p1, p2);
}

auto ParserSpp::ParseRetStatement()
  -> Unique<RetStatementAst> {
  PARSE_ONCE(p1, ParseKeywordRet);
  PARSE_OPTIONAL(p2, ParseExpression);
  return _CreateAst<RetStatementAst>(p1, p2);
}

auto ParserSpp::ParseExitStatement()
  -> Unique<LoopControlFlowStatementAst> {
  PARSE_ONE_OR_MORE(p1, ParseKeywordExit, ParseSpace);
  PARSE_ONCE(p2, ParseKeywordSkip);
  return _CreateAst<LoopControlFlowStatementAst>(p1, p2, nullptr);
}

auto ParserSpp::ParseExitStatementWithValue()
  -> Unique<LoopControlFlowStatementAst> {
  PARSE_ONE_OR_MORE(p1, ParseKeywordExit, ParseSpace);
  PARSE_OPTIONAL(p2, ParseExpression)
  return _CreateAst<LoopControlFlowStatementAst>(p1, nullptr, p2);
}

auto ParserSpp::ParseSkipStatement()
  -> Unique<LoopControlFlowStatementAst> {
  PARSE_ONCE(p1, ParseKeywordSkip);
  return _CreateAst<LoopControlFlowStatementAst>(NO_TOKENS, p1, nullptr);
}

auto ParserSpp::ParseUseStatement()
  -> Unique<UseStatementAst> {
  PARSE_ONCE(p1, ParseKeywordUse);
  PARSE_ONCE(p2, ParseTypeExpressionSimple)
  return _CreateAst<UseStatementAst>(NO_ANNOTATIONS, p1, p2);
}

auto ParserSpp::ParseUseVarStatement()
  -> Unique<UseStatementVariableAst> {
  PARSE_ONCE(p1, ParseKeywordUse);
  PARSE_ONCE(p2, ParsePostfixExpressionStrictlyStaticAccessOne);
  return _CreateAst<UseStatementVariableAst>(NO_ANNOTATIONS, p1, p2);
}

auto ParserSpp::ParseTypeStatement()
  -> Unique<TypeStatementAst> {
  PARSE_ONCE(p1, ParseKeywordType);
  PARSE_ONCE(p2, ParseUpperIdentifier);
  PARSE_OPTIONAL(p3, ParseGenericParameterGroup);
  PARSE_ONCE(p4, ParseTokenAssign);
  PARSE_ONCE(p5, ParseTypeExpression);
  return _CreateAst<TypeStatementAst>(
    NO_ANNOTATIONS, p1, TypeIdentifierAst::FromIdentifier(*p2), p3, p4, p5);
}

auto ParserSpp::ParseCmpStatement()
  -> Unique<CmpStatementAst> {
  PARSE_ONCE(p1, ParseKeywordCmp);
  PARSE_ONCE(p2, ParseIdentifier);
  PARSE_ONCE(p3, ParseTokenColon);
  PARSE_ONCE(p4, ParseTypeExpression);
  PARSE_ONCE(p5, ParseTokenAssign);
  PARSE_ONCE(p6, ParseExpression);
  return _CreateAst<CmpStatementAst>(NO_ANNOTATIONS, p1, p2, p3, p4, p5, p6);
}

auto ParserSpp::ParseLetStatement()
  -> Unique<LetStatementAst> {
  PARSE_ALTERNATE(p1, LetStatementAst, ParseLetStatementInitialized, ParseLetStatementUninitialized);
  return FORWARD_AST(p1);
}

auto ParserSpp::ParseLetStatementInitialized()
  -> Unique<LetStatementAst> {
  PARSE_ONCE(p1, ParseKeywordLet);
  PARSE_ONCE(p2, ParseLocalVariable);
  PARSE_OPTIONAL(p3, ParseLetStatementInitializedExplicitType);
  PARSE_ONCE(p4, ParseTokenAssign);
  PARSE_ONCE(p5, ParseExpression);
  return _CreateAst<LetStatementInitializedAst>(p1, p2, p3, p4, p5);
}

auto ParserSpp::ParseLetStatementInitializedExplicitType()
  -> Unique<TypeAst> {
  PARSE_ONCE(p1, ParseTokenColon);
  PARSE_ONCE(p2, ParseTypeExpression);
  return FORWARD_AST(p2);
}

auto ParserSpp::ParseLetStatementUninitialized()
  -> Unique<LetStatementAst> {
  PARSE_ONCE(p1, ParseKeywordLet);
  PARSE_ONCE(p2, ParseLocalVariable);
  PARSE_ONCE(p3, ParseTokenColon);
  PARSE_ONCE(p4, ParseTypeExpression);
  return _CreateAst<LetStatementUninitializedAst>(p1, p2, p3, p4);
}

auto ParserSpp::ParseGlobalUseStatement()
  -> Unique<UseStatementAst> {
  PARSE_ZERO_OR_MORE(p1, ParseAnnotation, ParseNothing);
  PARSE_ONCE(p2, ParseUseStatement);
  p2->Annotations = std::move(p1);
  return FORWARD_AST(p2);
}

auto ParserSpp::ParseGlobalUseVarStatement()
  -> Unique<UseStatementVariableAst> {
  PARSE_ZERO_OR_MORE(p1, ParseAnnotation, ParseNothing);
  PARSE_ONCE(p2, ParseUseVarStatement);
  p2->Annotations = std::move(p1);
  return FORWARD_AST(p2);
}

auto ParserSpp::ParseGlobalTypeStatement()
  -> Unique<TypeStatementAst> {
  PARSE_ZERO_OR_MORE(p1, ParseAnnotation, ParseNothing);
  PARSE_ONCE(p2, ParseTypeStatement);
  p2->Annotations = std::move(p1);
  return FORWARD_AST(p2);
}

auto ParserSpp::ParseGlobalCmpStatement()
  -> Unique<CmpStatementAst> {
  PARSE_ZERO_OR_MORE(p1, ParseAnnotation, ParseNothing);
  PARSE_ONCE(p2, ParseCmpStatement);
  p2->Annotations = std::move(p1);
  return FORWARD_AST(p2);
}

auto ParserSpp::ParseLocalVariable()
  -> Unique<LocalVariableAst> {
  PARSE_ALTERNATE(
    p1, LocalVariableAst, ParseLocalVariableDestructureArray,
    ParseLocalVariableDestructureTuple, ParseLocalVariableDestructureObject,
    ParseLocalVariableSingleIdentifier);
  return FORWARD_AST(p1);
}

auto ParserSpp::ParseLocalVariableDestructureArray()
  -> Unique<LocalVariableDestructureArrayAst> {
  PARSE_ONCE(p1, ParseTokenLeftSquareBracket);
  PARSE_ZERO_OR_MORE(p2, ParseLocalVariableNestedForDestructureArray, ParseTokenComma);
  PARSE_ONCE(p3, ParseTokenRightSquareBracket);
  return _CreateAst<LocalVariableDestructureArrayAst>(p1, p2, p3);
}

auto ParserSpp::ParseLocalVariableDestructureObject()
  -> Unique<LocalVariableDestructureObjectAst> {
  PARSE_ONCE(p1, ParseTypeExpressionSimple);
  PARSE_ONCE(p2, ParseTokenLeftParenthesis);
  PARSE_ZERO_OR_MORE(p3, ParseLocalVariableNestedForDestructureObject, ParseTokenComma);
  PARSE_ONCE(p4, ParseTokenRightParenthesis);
  return _CreateAst<LocalVariableDestructureObjectAst>(p1, p2, p3, p4);
}

auto ParserSpp::ParseLocalVariableDestructureTuple()
  -> Unique<LocalVariableDestructureTupleAst> {
  PARSE_ONCE(p1, ParseTokenLeftParenthesis);
  PARSE_ZERO_OR_MORE(p2, ParseLocalVariableNestedForDestructureTuple, ParseTokenComma);
  PARSE_ONCE(p3, ParseTokenRightParenthesis);
  return _CreateAst<LocalVariableDestructureTupleAst>(p1, p2, p3);
}

auto ParserSpp::ParseLocalVariableDestructureSkipSingleArgument()
  -> Unique<LocalVariableDestructureSkipSingleArgumentAst> {
  PARSE_ONCE(p1, ParseTokenUnderscore);
  return _CreateAst<LocalVariableDestructureSkipSingleArgumentAst>(p1);
}

auto ParserSpp::ParseLocalVariableDestructureSkipMultipleArguments()
  -> Unique<LocalVariableDestructureSkipMultipleArgumentsAst> {
  PARSE_ONCE(p1, ParseTokenDoubleDot);
  PARSE_OPTIONAL(p2, ParseLocalVariableSingleIdentifier);
  return _CreateAst<LocalVariableDestructureSkipMultipleArgumentsAst>(p1, p2);
}

auto ParserSpp::ParseLocalVariableDestructureAttributeBinding()
  -> Unique<LocalVariableDestructureAttributeBindingAst> {
  PARSE_ONCE(p1, ParseIdentifier);
  PARSE_ONCE(p2, ParseTokenAssign);
  PARSE_ONCE(p3, ParseLocalVariableNestedForDestructureAttributeBinding);
  return _CreateAst<LocalVariableDestructureAttributeBindingAst>(p1, p2, p3);
}

auto ParserSpp::ParseLocalVariableSingleIdentifier()
  -> Unique<LocalVariableSingleIdentifierAst> {
  PARSE_OPTIONAL(p1, ParseKeywordMut);
  PARSE_ONCE(p2, ParseIdentifier);
  return _CreateAst<LocalVariableSingleIdentifierAst>(p1, p2, nullptr);
}

auto ParserSpp::ParseLocalVariableSingleIdentifierAliasable()
  -> Unique<LocalVariableSingleIdentifierAst> {
  PARSE_ONCE(p1, ParseLocalVariableSingleIdentifier);
  PARSE_OPTIONAL(p2, ParseLocalVariableSingleIdentifierAlias);
  p1->Alias = std::move(p2);
  return FORWARD_AST(p1);
}

auto ParserSpp::ParseLocalVariableSingleIdentifierAlias()
  -> Unique<LocalVariableSingleIdentifierAliasAst> {
  PARSE_ONCE(p1, ParseKeywordAs);
  PARSE_ONCE(p2, ParseIdentifier);
  return _CreateAst<LocalVariableSingleIdentifierAliasAst>(p1, p2);
}

auto ParserSpp::ParseLocalVariableNestedForDestructureArray()
  -> Unique<LocalVariableAst> {
  PARSE_ALTERNATE(
    p1, LocalVariableAst, ParseLocalVariableDestructureSkipSingleArgument,
    ParseLocalVariableDestructureSkipMultipleArguments, ParseLocalVariableDestructureArray,
    ParseLocalVariableDestructureTuple, ParseLocalVariableDestructureObject,
    ParseLocalVariableSingleIdentifier);
  return FORWARD_AST(p1);
}

auto ParserSpp::ParseLocalVariableNestedForDestructureObject()
  -> Unique<LocalVariableAst> {
  PARSE_ALTERNATE(
    p1, LocalVariableAst, ParseLocalVariableDestructureSkipMultipleArguments,
    ParseLocalVariableDestructureAttributeBinding, ParseLocalVariableSingleIdentifierAliasable);
  return FORWARD_AST(p1);
}

auto ParserSpp::ParseLocalVariableNestedForDestructureTuple()
  -> Unique<LocalVariableAst> {
  PARSE_ALTERNATE(
    p1, LocalVariableAst, ParseLocalVariableDestructureSkipSingleArgument,
    ParseLocalVariableDestructureSkipMultipleArguments, ParseLocalVariableDestructureArray,
    ParseLocalVariableDestructureTuple, ParseLocalVariableDestructureObject,
    ParseLocalVariableSingleIdentifier);
  return FORWARD_AST(p1);
}

auto ParserSpp::ParseLocalVariableNestedForDestructureAttributeBinding()
  -> Unique<LocalVariableAst> {
  PARSE_ALTERNATE(
    p1, LocalVariableAst, ParseLocalVariableDestructureArray,
    ParseLocalVariableDestructureTuple, ParseLocalVariableDestructureObject);
  return FORWARD_AST(p1);
}

auto ParserSpp::ParseConvention()
  -> Unique<ConventionAst> {
  PARSE_ALTERNATE(p1, ConventionAst, ParseConventionMut, ParseConventionRef);
  return FORWARD_AST(p1);
}

auto ParserSpp::ParseConventionRef()
  -> Unique<ConventionRefAst> {
  PARSE_ONCE(p1, ParseTokenBorrow);
  return _CreateAst<ConventionRefAst>(p1);
}

auto ParserSpp::ParseConventionMut()
  -> Unique<ConventionMutAst> {
  PARSE_ONCE(p1, ParseTokenBorrow);
  PARSE_ONCE(p2, ParseKeywordMut);
  return _CreateAst<ConventionMutAst>(p1, p2);
}

auto ParserSpp::ParseObjectInitializer()
  -> Unique<ObjectInitializerAst> {
  PARSE_ONCE(p1, ParseTypeExpressionSimple);
  PARSE_ONCE(p2, ParseObjectInitializerArgumentGroup);
  auto ast = _CreateAst<ObjectInitializerAst>(p1, p2);
  ast->Source.IsWritten = true;
  return ast;
}

auto ParserSpp::ParseObjectInitializerArgumentGroup()
  -> Unique<ObjectInitializerArgumentGroupAst> {
  PARSE_ONCE(p1, ParseTokenLeftParenthesis);
  PARSE_ZERO_OR_MORE(p2, ParseObjectInitializerArgument, ParseTokenComma);
  PARSE_ONCE(p3, ParseTokenRightParenthesis);
  return _CreateAst<ObjectInitializerArgumentGroupAst>(p1, p2, p3);
}

auto ParserSpp::ParseObjectInitializerArgument()
  -> Unique<ObjectInitializerArgumentAst> {
  PARSE_ALTERNATE(
    p1, ObjectInitializerArgumentAst, ParseObjectInitializerArgumentKeyword,
    ParseObjectInitializerArgumentShorthand);
  return FORWARD_AST(p1);
}

auto ParserSpp::ParseObjectInitializerArgumentKeyword()
  -> Unique<ObjectInitializerArgumentKeywordAst> {
  PARSE_ONCE(p1, ParseIdentifier);
  PARSE_ONCE(p2, ParseTokenAssign);
  PARSE_ONCE(p3, ParseExpression);
  return _CreateAst<ObjectInitializerArgumentKeywordAst>(p1, p2, p3);
}

auto ParserSpp::ParseObjectInitializerArgumentShorthand()
  -> Unique<ObjectInitializerArgumentShorthandAst> {
  PARSE_OPTIONAL(p1, ParseTokenDoubleDot);
  PARSE_ONCE(p2, ParseExpression);
  return _CreateAst<ObjectInitializerArgumentShorthandAst>(p1, p2);
}

auto ParserSpp::ParseClosureExpression()
  -> Unique<ClosureExpressionAst> {
  PARSE_ALTERNATE(
    p1, ClosureExpressionAst, ParseClosureExpressionWithReturnType,
    ParseClosureExpressionWithoutReturnType);
  return FORWARD_AST(p1);
}

auto ParserSpp::ParseClosureExpressionWithReturnType()
  -> Unique<ClosureExpressionAst> {
  PARSE_OPTIONAL(p1, ParseKeywordCor);
  PARSE_ONCE(p2, ParseClosureExpressionParameterAndCaptureGroup);
  PARSE_ONCE(p3, ParseTokenArrowRight);
  PARSE_ONCE(p4, ParseTypeExpression);
  PARSE_ONCE(p5, [this] { return ParseInnerScopeExpression([this] { return ParseStatement(); }); });
  return _CreateAst<ClosureExpressionAst>(p1, p2, p3, p4, p5);
}

auto ParserSpp::ParseClosureExpressionWithoutReturnType()
  -> Unique<ClosureExpressionAst> {
  PARSE_OPTIONAL(p1, ParseKeywordCor);
  PARSE_ONCE(p2, ParseClosureExpressionParameterAndCaptureGroup);
  // Prevent "()" tuple on one line combining with stmt on next.
  if (_LineFeedAhead()) { return nullptr; }
  PARSE_ONCE(p3, ParseExpression);
  return _CreateAst<ClosureExpressionAst>(p1, p2, nullptr, nullptr, p3);
}

auto ParserSpp::ParseClosureExpressionCaptureGroup()
  -> Unique<ClosureExpressionCaptureGroupAst> {
  PARSE_ONCE(p1, ParseKeywordCaps);
  PARSE_ONE_OR_MORE(p2, ParseClosureExpressionCapture, ParseTokenComma);
  return _CreateAst<ClosureExpressionCaptureGroupAst>(p1, p2);
}

auto ParserSpp::ParseClosureExpressionCapture()
  -> Unique<ClosureExpressionCaptureAst> {
  PARSE_OPTIONAL(p1, ParseConvention);
  PARSE_ALTERNATE(p2, IdentifierAst, ParseIdentifier, ParseSelfIdentifier);
  return _CreateAst<ClosureExpressionCaptureAst>(p1, p2);
}

auto ParserSpp::ParseClosureExpressionParameterAndCaptureGroup()
  -> Unique<ClosureExpressionParameterAndCaptureGroupAst> {
  PARSE_ONCE(p1, ParseTokenLeftParenthesis);
  PARSE_ONCE(p2, ParseClosureExpressionParameterGroup);
  PARSE_OPTIONAL(p3, ParseClosureExpressionCaptureGroup);
  PARSE_ONCE(p4, ParseTokenRightParenthesis);
  return _CreateAst<ClosureExpressionParameterAndCaptureGroupAst>(p1, p2, p3, p4);
}

auto ParserSpp::ParseClosureExpressionParameterGroup()
  -> Unique<ClosureExpressionParameterGroupAst> {
  PARSE_ZERO_OR_MORE(p1, ParseClosureExpressionParameter, ParseTokenComma);
  return _CreateAst<ClosureExpressionParameterGroupAst>(nullptr, p1, nullptr);
}

auto ParserSpp::ParseClosureExpressionParameter()
  -> Unique<ClosureExpressionParameterAst> {
  PARSE_ALTERNATE(
    p1, ClosureExpressionParameterAst, ParseFunctionParameterVariadic,
    ParseFunctionParameterRequired);
  return FORWARD_AST(p1);
}

auto ParserSpp::ParseTypeExpression()
  -> Unique<TypeAst> {
  PARSE_ALTERNATE(
    p1, TypeAst, ParseTypeNever, ParseTypeParenthesisedExpression, ParseTypeArray, ParseTypeTuple,
    ParseBinaryTypeExpression);
  return FORWARD_AST(p1);
}

auto ParserSpp::ParseBinaryTypeExpression(const std::uint8_t min_prec)
  -> Unique<TypeAst> {
  using RT = lex::RawTokenType;

  struct BinOpInfo {
    Unique<TokenAst> Tok;
    std::uint8_t Prec;
  };

  auto try_tok = [this](auto fn, const std::uint8_t prec) -> Opt<BinOpInfo> {
    const auto pos = _Pos;
    if (auto tok = fn()) return BinOpInfo{std::move(tok), prec};
    _Pos = pos;
    return Nullopt;
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
        // "A and B" would be an intersection type, which the language
        // does not have: the type ends before the "and", which is
        // reported if nothing else can read it.
        _StoreSpecificError(peek, "Intersection types (\"A and B\") are not supported");
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
    lhs = _CreateAst<TypeBinaryExpressionAst>(lhs, op, rhs)->Convert();
    // A variant's members are a list of types, so a type pack may be one of
    // them ("Bool or Rest").
    // "A or B" is "Var[A, B]" ("or" is the only binary type operator).
    const auto pos = lhs->PosStart();
    auto members = Vec<Shared<TypeAst>>();
    members.emplace_back(shared_cast<TypeAst>(std::move(lhs)));
    members.emplace_back(shared_cast<TypeAst>(std::move(rhs)));
    lhs = AstClone(generate::common_types::VariantType(pos, std::move(members)));
  }

  return lhs;
}

auto ParserSpp::ParseUnaryTypeExpression()
  -> Unique<TypeAst> {
  PARSE_OPTIONAL(p1, ParseUnaryTypeExpressionOpBorrow)
  PARSE_ZERO_OR_MORE(p2, ParseUnaryTypeExpressionOp, ParseNothing);
  PARSE_ONCE(p3, [this] { return unique_cast<TypeAst>(ParseTypeIdentifier()); });
  if (p1 != nullptr) {
    p2.insert(p2.begin(), unique_cast<TypeUnaryExpressionOperatorAst>(std::move(p1)));
  }
  return genex::fold_left(
    p2 | genex::views::move_reverse, std::move(p3),
    [](Unique<TypeAst> &&acc, Unique<TypeUnaryExpressionOperatorAst> &&x) {
      return _CreateAst<TypeUnaryExpressionAst>(x, std::move(acc));
    });
}

auto ParserSpp::ParseUnaryTypeExpressionOp()
  -> Unique<TypeUnaryExpressionOperatorAst> {
  PARSE_ALTERNATE(
    p1, TypeUnaryExpressionOperatorAst, ParseUnaryTypeExpressionOpBorrow,
    ParseUnaryTypeExpressionOpNamespace);
  return FORWARD_AST(p1);
}

auto ParserSpp::ParseUnaryTypeExpressionOpBorrow()
  -> Unique<TypeUnaryExpressionOperatorBorrowAst> {
  PARSE_ONCE(p1, ParseConvention);
  return _CreateAst<TypeUnaryExpressionOperatorBorrowAst>(p1);
}

auto ParserSpp::ParseUnaryTypeExpressionOpNamespace()
  -> Unique<TypeUnaryExpressionOperatorNamespaceAst> {
  PARSE_ONCE(p1, ParseIdentifier);
  PARSE_ONCE(p2, ParseTokenDoubleColon);
  return _CreateAst<TypeUnaryExpressionOperatorNamespaceAst>(p1, p2);
}

auto ParserSpp::ParsePostfixTypeExpression()
  -> Unique<TypeAst> {
  PARSE_ONCE(p1, ParseUnaryTypeExpression);
  PARSE_ZERO_OR_MORE(p2, ParsePostfixTypeExpressionOp, ParseNothing);
  return genex::fold_left(
    p2 | genex::views::move, std::move(p1),
    [](Unique<TypeAst> &&acc, Unique<TypePostfixExpressionOperatorAst> &&x) {
      return _CreateAst<TypePostfixExpressionAst>(std::move(acc), x);
    });
}

auto ParserSpp::ParsePostfixTypeExpressionOp()
  -> Unique<TypePostfixExpressionOperatorAst> {
  PARSE_ALTERNATE(
    p1, TypePostfixExpressionOperatorAst,
    ParsePostfixTypeExpressionOpNested);
  return FORWARD_AST(p1);
}

auto ParserSpp::ParsePostfixTypeExpressionOpNested()
  -> Unique<TypePostfixExpressionOperatorNestedTypeAst> {
  PARSE_ONCE(p1, ParseTokenDoubleColon);
  PARSE_ONCE(p2, ParseTypeIdentifier);
  return _CreateAst<TypePostfixExpressionOperatorNestedTypeAst>(p1, p2);
}

auto ParserSpp::ParseTypeParenthesisedExpression()
  -> Unique<TypeAst> {
  PARSE_ONCE(p1, ParseTokenLeftParenthesis);
  PARSE_ONCE(p2, ParseTypeExpression);
  PARSE_ONCE(p3, ParseTokenRightParenthesis);
  return _CreateAst<TypeParenthesisedExpressionAst>(p1, p2, p3)->Convert();
}

auto ParserSpp::ParseTypeNever()
  -> Unique<TypeAst> {
  PARSE_ONCE(p1, ParseTokenExclamationMark);
  return AstClone(generate::common_types::NeverType(p1->PosStart()));
}

auto ParserSpp::ParseTypeExpressionSimple()
  -> Unique<TypeAst> {
  PARSE_ONCE(p1, ParsePostfixTypeExpressionSimple)
  return FORWARD_AST(p1);
}

auto ParserSpp::ParsePostfixTypeExpressionSimple()
  -> Unique<TypeAst> {
  PARSE_ONCE(p1, ParseUnaryTypeExpressionSimple);
  PARSE_ZERO_OR_MORE(p2, ParsePostfixTypeExpressionOp, ParseNothing);
  return genex::fold_left(
    p2 | genex::views::move, std::move(p1),
    [](Unique<TypeAst> &&acc, Unique<TypePostfixExpressionOperatorAst> &&x) {
      return _CreateAst<TypePostfixExpressionAst>(std::move(acc), x);
    });
}

auto ParserSpp::ParseUnaryTypeExpressionSimple()
  -> Unique<TypeAst> {
  PARSE_OPTIONAL(p1, ParseUnaryTypeExpressionOpBorrow)
  PARSE_ZERO_OR_MORE(p2, ParseUnaryTypeExpressionOp, ParseNothing);
  PARSE_ONCE(p3, [this] { return unique_cast<TypeAst>(ParseTypeIdentifier()); });
  if (p1 != nullptr) {
    p2.insert(p2.begin(), unique_cast<TypeUnaryExpressionOperatorAst>(std::move(p1)));
  }
  return genex::fold_left(
    p2 | genex::views::move_reverse, std::move(p3),
    [](Unique<TypeAst> &&acc, Unique<TypeUnaryExpressionOperatorAst> &&x) {
      return _CreateAst<TypeUnaryExpressionAst>(x, std::move(acc));
    });
}

auto ParserSpp::ParseTypeIdentifier()
  -> Unique<TypeIdentifierAst> {
  PARSE_ONCE(p1, ParseLexemeUpperIdentifier);
  PARSE_OPTIONAL(p2, ParseGenericArgumentGroup);
  auto t = _CreateAst<TypeIdentifierAst>(p1->PosStart(), p1->TokenData, p2);
  t->MarkSourceWritten();
  return t;
}

auto ParserSpp::ParseTypeArray()
  -> Unique<TypeAst> {
  PARSE_ONCE(p1, ParseTokenLeftSquareBracket);
  PARSE_ONCE(p2, ParseTypeExpression);
  PARSE_ONCE(p3, ParseTokenSemicolon);
  PARSE_ONCE(p4, ParseExpression);
  PARSE_ONCE(p5, ParseTokenRightSquareBracket);
  return AstClone(generate::common_types::ArrayType(
    p1->PosStart(), shared_cast<TypeAst>(std::move(p2)), std::move(p4)));
}

auto ParserSpp::ParseTypeTuple()
  -> Unique<TypeAst> {
  PARSE_ALTERNATE(
    p1, TypeAst, ParseTypeTuple0Types, ParseTypeTuple1Types, ParseTypeTupleNTypes);
  return FORWARD_AST(p1);
}

auto ParserSpp::ParseTypeTuple0Types()
  -> Unique<TypeAst> {
  PARSE_ONCE(p1, ParseTokenLeftParenthesis);
  auto p2 = Vec<Shared<TypeAst>>();
  PARSE_ONCE(p3, ParseTokenRightParenthesis);
  return AstClone(generate::common_types::TupleType(
    p1->PosStart(), std::move(p2)));
}

auto ParserSpp::ParseTypeTuple1Types()
  -> Unique<TypeAst> {
  PARSE_ONCE(p1, ParseTokenLeftParenthesis);
  PARSE_ONCE(p2, ParseTypeExpression);
  PARSE_ONCE(p3, ParseTokenComma);
  PARSE_ONCE(p4, ParseTokenRightParenthesis);

  auto temp = Vec<Shared<TypeAst>>();
  temp.emplace_back(shared_cast<TypeAst>(std::move(p2)));
  return AstClone(generate::common_types::TupleType(p1->PosStart(), std::move(temp)));
}

auto ParserSpp::ParseTypeTupleNTypes()
  -> Unique<TypeAst> {
  PARSE_ONCE(p1, ParseTokenLeftParenthesis);
  PARSE_TWO_OR_MORE(p2, ParseTypeExpression, ParseTokenComma);
  PARSE_ONCE(p3, ParseTokenRightParenthesis);

  auto temp = Vec<Shared<TypeAst>>();
  for (auto &&x : p2) {
    temp.emplace_back(shared_cast<TypeAst>(std::move(x)));
  }
  return AstClone(generate::common_types::TupleType(
    p1->PosStart(), std::move(temp)));
}

auto ParserSpp::ParseIdentifier()
  -> Unique<IdentifierAst> {
  PARSE_ONCE(p1, ParseLexemeIdentifier);
  return _CreateAst<IdentifierAst>(p1->PosStart(), p1->TokenData);
}

auto ParserSpp::ParseNumericIdentifier()
  -> Unique<IdentifierAst> {
  PARSE_ONCE(p1, ParseLexemeDecInteger);
  return _CreateAst<IdentifierAst>(p1->PosStart(), p1->TokenData);
}

auto ParserSpp::ParseSelfIdentifier()
  -> Unique<IdentifierAst> {
  PARSE_ONCE(p1, ParseKeywordSelf);
  return IdentifierAst::MappedFromTok(*p1, p1->TokenData);
}

auto ParserSpp::ParseUpperIdentifier()
  -> Unique<IdentifierAst> {
  PARSE_ONCE(p1, ParseLexemeUpperIdentifier);
  if (p1 == nullptr) { return nullptr; }
  return _CreateAst<IdentifierAst>(p1->PosStart(), p1->TokenData);
}

auto ParserSpp::ParseIdentifierAsExpression()
  -> Unique<ExpressionAst> {
  PARSE_ALTERNATE(p1, IdentifierAst, ParseIdentifier, ParseSelfIdentifier);
  return unique_cast<ExpressionAst>(std::move(p1));
}

auto ParserSpp::ParseLiteral()
  -> Unique<LiteralAst> {
  PARSE_ALTERNATE(
    p1, LiteralAst, ParseLiteralChar, ParseLiteralString, ParseLiteralFloat, ParseLiteralInteger,
    ParseLiteralBoolean,
    [this] { return ParseLiteralTuple([this] { return ParseExpression(); }); },
    [this] { return ParseLiteralArray([this] { return ParseExpression(); }); });
  return FORWARD_AST(p1);
}

auto ParserSpp::ParseLiteralChar()
  -> Unique<CharLiteralAst> {
  PARSE_OPTIONAL(p1, ParseBytePrefixType)
  PARSE_ONCE(p2, ParseLexemeSingleQuoteChar);
  return _CreateAst<CharLiteralAst>(p1, p2);
}

auto ParserSpp::ParseLiteralString()
  -> Unique<StringLiteralAst> {
  PARSE_OPTIONAL(p1, ParseBytePrefixType)
  PARSE_ONCE(p2, ParseLexemeDoubleQuoteString);
  return _CreateAst<StringLiteralAst>(p1, p2);
}

auto ParserSpp::ParseLiteralFloat()
  -> Unique<FloatLiteralAst> {
  PARSE_ONCE(p1, ParseLiteralFloatB10);
  return FORWARD_AST(p1);
}

auto ParserSpp::ParseLiteralInteger()
  -> Unique<IntegerLiteralAst> {
  PARSE_ALTERNATE(
    p1, IntegerLiteralAst, ParseLiteralIntegerB02, ParseLiteralIntegerB08, ParseLiteralIntegerB16,
    ParseLiteralIntegerB10);
  return FORWARD_AST(p1);
}

auto ParserSpp::ParseLiteralBoolean()
  -> Unique<BooleanLiteralAst> {
  PARSE_ALTERNATE(p1, TokenAst, ParseKeywordTrue, ParseKeywordFalse);
  return _CreateAst<BooleanLiteralAst>(p1);
}

auto ParserSpp::ParseLiteralTuple(
  Func<Unique<ExpressionAst>()> &&elem_parser)
  -> Unique<TupleLiteralAst> {
  auto parser_1 = [elem_parser, this] mutable { return ParseLiteralTuple1Element(std::move(elem_parser)); };
  auto parser_n = [elem_parser, this] mutable { return ParseLiteralTupleNElements(std::move(elem_parser)); };
  PARSE_ALTERNATE(p1, TupleLiteralAst, parser_1, parser_n);
  return FORWARD_AST(p1);
}

auto ParserSpp::ParseLiteralArray(
  Func<Unique<ExpressionAst>()> &&elem_parser)
  -> Unique<ArrayLiteralAst> {
  auto parser_e = [elem_parser, this] mutable { return ParseLiteralArrayExplicitElements(std::move(elem_parser)); };
  auto parser_r = [elem_parser, this] mutable { return ParseLiteralArrayRepeatedElement(std::move(elem_parser)); };
  PARSE_ALTERNATE(p1, ArrayLiteralAst, parser_e, parser_r);
  return FORWARD_AST(p1);
}

auto ParserSpp::ParseLiteralFloatB10()
  -> Unique<FloatLiteralAst> {
  PARSE_OPTIONAL(p1, ParseNumericPrefixOp);
  PARSE_ONCE(p2, ParseLexemeDecInteger);
  PARSE_ONCE(p3, ParseTokenDot);
  PARSE_ONCE(p4, ParseLexemeDecInteger);
  PARSE_OPTIONAL(p5, ParseFloatSuffixType);
  return _CreateAst<FloatLiteralAst>(p1, p2, p3, p4, p5 ? p5->TokenData : "");
}

auto ParserSpp::ParseLiteralIntegerB02()
  -> Unique<IntegerLiteralAst> {
  PARSE_OPTIONAL(p1, ParseNumericPrefixOp);
  PARSE_ONCE(p2, ParseLexemeBinInteger);
  PARSE_OPTIONAL(p3, ParseIntegerSuffixType);
  return _CreateAst<IntegerLiteralAst>(p1, p2, p3 ? p3->TokenData : "");
}

auto ParserSpp::ParseLiteralIntegerB08()
  -> Unique<IntegerLiteralAst> {
  PARSE_OPTIONAL(p1, ParseNumericPrefixOp);
  PARSE_ONCE(p2, ParseLexemeOctInteger);
  PARSE_OPTIONAL(p3, ParseIntegerSuffixType);
  return _CreateAst<IntegerLiteralAst>(p1, p2, p3 ? p3->TokenData : "");
}

auto ParserSpp::ParseLiteralIntegerB10()
  -> Unique<IntegerLiteralAst> {
  PARSE_OPTIONAL(p1, ParseNumericPrefixOp);
  PARSE_ONCE(p2, ParseLexemeDecInteger);
  PARSE_OPTIONAL(p3, ParseIntegerSuffixType);
  return _CreateAst<IntegerLiteralAst>(p1, p2, p3 ? p3->TokenData : "");
}

auto ParserSpp::ParseLiteralIntegerB16()
  -> Unique<IntegerLiteralAst> {
  PARSE_OPTIONAL(p1, ParseNumericPrefixOp);
  PARSE_ONCE(p2, ParseLexemeHexInteger);
  PARSE_OPTIONAL(p3, ParseIntegerSuffixType);
  return _CreateAst<IntegerLiteralAst>(p1, p2, p3 ? p3->TokenData : "");
}

auto ParserSpp::ParseNumericPrefixOp()
  -> Unique<TokenAst> {
  PARSE_ALTERNATE(p1, TokenAst, ParseTokenAdd, ParseTokenSub);
  return FORWARD_AST(p1);
}

auto ParserSpp::ParseFloatSuffixType()
  -> Unique<TokenAst> {
  PARSE_ONCE(p1, ParseTokenUnderscore)
  PARSE_ALTERNATE(
    p2, TokenAst,
    [this] { return ParseSpecificCharacters("f8"); },
    [this] { return ParseSpecificCharacters("f16"); },
    [this] { return ParseSpecificCharacters("f32"); },
    [this] { return ParseSpecificCharacters("f64"); },
    [this] { return ParseSpecificCharacters("f128"); },
    [this] { return ParseSpecificCharacters("f256"); });
  return FORWARD_AST(p2);
}

auto ParserSpp::ParseIntegerSuffixType()
  -> Unique<TokenAst> {
  PARSE_ONCE(p1, ParseTokenUnderscore);
  PARSE_ALTERNATE(
    p2, TokenAst,
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

auto ParserSpp::ParseBytePrefixType()
  -> Unique<TokenAst> {
  PARSE_ONCE(p1, [this] { return ParseSpecificCharacter('b'); });
  return FORWARD_AST(p1);
}

auto ParserSpp::ParseLiteralTuple1Element(
  Func<Unique<ExpressionAst>()> &&elem_parser)
  -> Unique<TupleLiteralAst> {
  PARSE_ONCE(p1, ParseTokenLeftParenthesis);
  PARSE_ONCE(p2, std::move(elem_parser));
  PARSE_ONCE(p3, ParseTokenComma);
  PARSE_ONCE(p4, ParseTokenRightParenthesis);

  auto temp = Vec<decltype(p2)>();
  temp.emplace_back(std::move(p2));
  return _CreateAst<TupleLiteralAst>(p1, temp, p4);
}

auto ParserSpp::ParseLiteralTupleNElements(
  Func<Unique<ExpressionAst>()> &&elem_parser)
  -> Unique<TupleLiteralAst> {
  PARSE_ONCE(p1, ParseTokenLeftParenthesis);
  PARSE_TWO_OR_MORE(p2, elem_parser, ParseTokenComma);
  PARSE_ONCE(p3, ParseTokenRightParenthesis);
  return _CreateAst<TupleLiteralAst>(p1, p2, p3);
}

auto ParserSpp::ParseLiteralArrayRepeatedElement(
  Func<Unique<ExpressionAst>()> &&elem_parser)
  -> Unique<ArrayLiteralRepeatedElementAst> {
  PARSE_ONCE(p1, ParseTokenLeftSquareBracket);
  PARSE_ONCE(p2, elem_parser);
  PARSE_ONCE(p3, ParseTokenSemicolon);
  PARSE_ONCE(p4, ParseExpression);
  PARSE_ONCE(p5, ParseTokenRightSquareBracket);
  return _CreateAst<ArrayLiteralRepeatedElementAst>(p1, p2, p3, p4, p5);
}

auto ParserSpp::ParseLiteralArrayExplicitElements(
  Func<Unique<ExpressionAst>()> &&elem_parser)
  -> Unique<ArrayLiteralExplicitElementsAst> {
  PARSE_ONCE(p1, ParseTokenLeftSquareBracket);
  PARSE_ONE_OR_MORE(p2, elem_parser, ParseTokenComma);
  PARSE_ONCE(p3, ParseTokenRightSquareBracket);
  return _CreateAst<ArrayLiteralExplicitElementsAst>(p1, p2, p3);
}

auto ParserSpp::ParseSpecificCharacters(Str &&s)
  -> Unique<TokenAst> {
  auto identifier = Str();
  PARSE_ONCE(_, ParseNothing);
  auto pos = _Pos;

  for (auto i = 0uz; i < s.length(); ++i) {
    PARSE_ONCE(p1, ParseLexemeCharacterOrDigit);
    identifier += p1->TokenData[0];
  }

  return identifier != s ? nullptr : _CreateAst<TokenAst>(
    _ErrorFormatter->PositionOf(pos),
    lex::SppTokenType::SP_NO_TOK,
    identifier);
}

auto ParserSpp::ParseSpecificCharacter(const char16_t c)
  -> Unique<TokenAst> {
  PARSE_ONCE(_, ParseNothing);
  PARSE_ONCE(p1, ParseLexemeCharacterOrDigit)
  if (p1->TokenData[0] != c) { return nullptr; }
  return FORWARD_AST(p1);
}

auto ParserSpp::ParseLexemeCharacter()
  -> Unique<TokenAst> {
  PARSE_ONCE(p1, [this] { return ParseTokenRaw(lex::RawTokenType::LX_CHARACTER, lex::SppTokenType::LX_CHARACTER); });
  return FORWARD_AST(p1);
}

auto ParserSpp::ParseLexemeDigit()
  -> Unique<TokenAst> {
  PARSE_ONCE(p1, [this] { return ParseTokenRaw(lex::RawTokenType::LX_DIGIT, lex::SppTokenType::LX_DIGIT); });
  return FORWARD_AST(p1);
}

auto ParserSpp::ParseLexemeCharacterOrDigit()
  -> Unique<TokenAst> {
  PARSE_ALTERNATE(p1, TokenAst, ParseLexemeCharacter, ParseLexemeDigit);
  return FORWARD_AST(p1);
}

auto ParserSpp::ParseLexemeCharacterOrDigitOrUnderscore()
  -> Unique<TokenAst> {
  PARSE_ALTERNATE(
    p1, TokenAst, ParseLexemeCharacter, ParseLexemeDigit, ParseTokenUnderscore)
  return FORWARD_AST(p1);
}

auto ParserSpp::ParseLexemeBinInteger()
  -> Unique<TokenAst> {
  PARSE_ONCE(_, ParseNothing);
  auto out = _CreateAst<TokenAst>(
    _ErrorFormatter->PositionOf(_Pos),
    lex::SppTokenType::LX_NUMBER, Str());

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

auto ParserSpp::ParseLexemeOctInteger()
  -> Unique<TokenAst> {
  PARSE_ONCE(_, ParseNothing);
  auto out = _CreateAst<TokenAst>(
    _ErrorFormatter->PositionOf(_Pos),
    lex::SppTokenType::LX_NUMBER, Str());

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

auto ParserSpp::ParseLexemeDecInteger()
  -> Unique<TokenAst> {
  PARSE_ONCE(_, ParseNothing);
  auto out = _CreateAst<TokenAst>(
    _ErrorFormatter->PositionOf(_Pos),
    lex::SppTokenType::LX_NUMBER, Str());

  PARSE_ONCE(p1, ParseLexemeDigit);
  out->TokenData += p1->TokenData;

  while (_Tokens[_Pos].Type == lex::RawTokenType::LX_DIGIT) {
    PARSE_ONCE(p2, ParseLexemeDigit);
    out->TokenData += p2->TokenData;
  }

  return out;
}

auto ParserSpp::ParseLexemeHexInteger()
  -> Unique<TokenAst> {
  PARSE_ONCE(_, ParseNothing);
  auto out = _CreateAst<TokenAst>(
    _ErrorFormatter->PositionOf(_Pos),
    lex::SppTokenType::LX_NUMBER, Str());

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

auto ParserSpp::ParseLexemeSingleQuoteChar()
  -> Unique<TokenAst> {
  PARSE_ONCE(_, ParseNothing);
  auto out = _CreateAst<TokenAst>(
    _ErrorFormatter->PositionOf(_Pos),
    lex::SppTokenType::LX_CHAR, Str());

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

auto ParserSpp::ParseLexemeDoubleQuoteString()
  -> Unique<TokenAst> {
  PARSE_ONCE(_, ParseNothing);
  auto out = _CreateAst<TokenAst>(
    _ErrorFormatter->PositionOf(_Pos),
    lex::SppTokenType::LX_STRING, Str());

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

auto ParserSpp::ParseLexemeIdentifier()
  -> Unique<TokenAst> {
  PARSE_ONCE(_, ParseNothing);
  auto out = _CreateAst<TokenAst>(
    _ErrorFormatter->PositionOf(_Pos),
    lex::SppTokenType::LX_IDENTIFIER, Str());

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

auto ParserSpp::ParseLexemeUpperIdentifier()
  -> Unique<TokenAst> {
  PARSE_ONCE(_, ParseNothing);
  auto out = _CreateAst<TokenAst>(
    _ErrorFormatter->PositionOf(_Pos),
    lex::SppTokenType::LX_IDENTIFIER, Str());

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

auto ParserSpp::ParseNothing()
  -> Unique<TokenAst> {
  PARSE_ONCE(p1, [this] { return ParseTokenRaw(lex::RawTokenType::SP_NO_TOK, lex::SppTokenType::SP_NO_TOK); });
  return FORWARD_AST(p1);
}

auto ParserSpp::ParseNewline()
  -> Unique<TokenAst> {
  PARSE_ONCE(p1, [this] { return ParseTokenRaw(lex::RawTokenType::TK_LINE_FEED, lex::SppTokenType::TK_NEWLINE); });
  return FORWARD_AST(p1);
}

auto ParserSpp::ParseSpace()
  -> Unique<TokenAst> {
  PARSE_ONCE(p1, [this] { return ParseTokenRaw(lex::RawTokenType::TK_SPACE, lex::SppTokenType::TK_SPACE); });
  return FORWARD_AST(p1);
}

auto ParserSpp::ParseTokenLeftCurlyBrace()
  -> Unique<TokenAst> {
  PARSE_ONCE(
    p1, [this] { return ParseTokenRaw(lex::RawTokenType::TK_LEFT_CURLY_BRACE, lex::SppTokenType::TK_LEFT_CURLY_BRACE);
    });
  return FORWARD_AST(p1);
}

auto ParserSpp::ParseTokenRightCurlyBrace()
  -> Unique<TokenAst> {
  PARSE_ONCE(
    p1, [this] { return ParseTokenRaw(lex::RawTokenType::TK_RIGHT_CURLY_BRACE, lex::SppTokenType::TK_RIGHT_CURLY_BRACE
    ); });
  return FORWARD_AST(p1);
}

auto ParserSpp::ParseTokenLeftSquareBracket()
  -> Unique<TokenAst> {
  PARSE_ONCE(
    p1, [this] { return ParseTokenRaw(lex::RawTokenType::TK_LEFT_SQUARE_BRACKET, lex::SppTokenType::
      TK_LEFT_SQUARE_BRACKET); });
  return FORWARD_AST(p1);
}

auto ParserSpp::ParseTokenLeftParenthesis()
  -> Unique<TokenAst> {
  PARSE_ONCE(
    p1, [this] { return ParseTokenRaw(lex::RawTokenType::TK_LEFT_PARENTHESIS, lex::SppTokenType::TK_LEFT_PARENTHESIS);
    });
  return FORWARD_AST(p1);
}

auto ParserSpp::ParseTokenRightParenthesis()
  -> Unique<TokenAst> {
  PARSE_ONCE(
    p1, [this] { return ParseTokenRaw(lex::RawTokenType::TK_RIGHT_PARENTHESIS, lex::SppTokenType::TK_RIGHT_PARENTHESIS
    ); });
  return FORWARD_AST(p1);
}

auto ParserSpp::ParseTokenRightSquareBracket()
  -> Unique<TokenAst> {
  PARSE_ONCE(
    p1, [this] { return ParseTokenRaw(lex::RawTokenType::TK_RIGHT_SQUARE_BRACKET, lex::SppTokenType::
      TK_RIGHT_SQUARE_BRACKET); });
  return FORWARD_AST(p1);
}

auto ParserSpp::ParseTokenColon()
  -> Unique<TokenAst> {
  PARSE_ONCE(p1, [this] { return ParseTokenRaw(lex::RawTokenType::TK_COLON, lex::SppTokenType::TK_COLON); });
  return FORWARD_AST(p1);
}

auto ParserSpp::ParseTokenComma()
  -> Unique<TokenAst> {
  PARSE_ONCE(p1, [this] { return ParseTokenRaw(lex::RawTokenType::TK_COMMA, lex::SppTokenType::TK_COMMA); });
  return FORWARD_AST(p1);
}

auto ParserSpp::ParseTokenAssign()
  -> Unique<TokenAst> {
  PARSE_ONCE(p1, [this] { return ParseTokenRaw(lex::RawTokenType::TK_EQUALS_TO, lex::SppTokenType::TK_ASSIGN); });
  return FORWARD_AST(p1);
}

auto ParserSpp::ParseTokenUnderscore()
  -> Unique<TokenAst> {
  PARSE_ONCE(
    p1, [this] { return ParseTokenRaw(lex::RawTokenType::TK_UNDERSCORE, lex::SppTokenType::TK_UNDERSCORE); });
  return FORWARD_AST(p1);
}

auto ParserSpp::ParseTokenLessThan()
  -> Unique<TokenAst> {
  PARSE_ONCE(p1, [this] { return ParseTokenRaw(lex::RawTokenType::TK_LESS_THAN, lex::SppTokenType::TK_LT); });
  return FORWARD_AST(p1);
}

auto ParserSpp::ParseTokenGreaterThan()
  -> Unique<TokenAst> {
  PARSE_ONCE(p1, [this] { return ParseTokenRaw(lex::RawTokenType::TK_GREATER_THAN, lex::SppTokenType::TK_GT); });
  return FORWARD_AST(p1);
}

auto ParserSpp::ParseTokenAdd()
  -> Unique<TokenAst> {
  PARSE_ONCE(p1, [this] { return ParseTokenRaw(lex::RawTokenType::TK_PLUS_SIGN, lex::SppTokenType::TK_ADD); });
  return FORWARD_AST(p1);
}

auto ParserSpp::ParseTokenSub()
  -> Unique<TokenAst> {
  PARSE_ONCE(p1, [this] { return ParseTokenRaw(lex::RawTokenType::TK_HYPHEN, lex::SppTokenType::TK_SUB); });
  return FORWARD_AST(p1);
}

auto ParserSpp::ParseTokenMul()
  -> Unique<TokenAst> {
  PARSE_ONCE(p1, [this] { return ParseTokenRaw(lex::RawTokenType::TK_ASTERISK, lex::SppTokenType::TK_MUL); });
  return FORWARD_AST(p1);
}

auto ParserSpp::ParseTokenDiv()
  -> Unique<TokenAst> {
  PARSE_ONCE(p1, [this] { return ParseTokenRaw(lex::RawTokenType::TK_SLASH, lex::SppTokenType::TK_DIV); });
  return FORWARD_AST(p1);
}

auto ParserSpp::ParseTokenRem()
  -> Unique<TokenAst> {
  PARSE_ONCE(p1, [this] { return ParseTokenRaw(lex::RawTokenType::TK_PERCENT_SIGN, lex::SppTokenType::TK_REM); });
  return FORWARD_AST(p1);
}

auto ParserSpp::ParseTokenBitIor()
  -> Unique<TokenAst> {
  PARSE_ONCE(p1, [this] { return ParseTokenRaw(lex::RawTokenType::TK_VERTICAL_BAR, lex::SppTokenType::TK_BIT_IOR); });
  return FORWARD_AST(p1);
}

auto ParserSpp::ParseTokenBitXor()
  -> Unique<TokenAst> {
  PARSE_ONCE(p1, [this] { return ParseTokenRaw(lex::RawTokenType::TK_CARET, lex::SppTokenType::TK_BIT_XOR); });
  return FORWARD_AST(p1);
}

auto ParserSpp::ParseTokenBitAnd()
  -> Unique<TokenAst> {
  PARSE_ONCE(p1, [this] { return ParseTokenRaw(lex::RawTokenType::TK_AMPERSAND, lex::SppTokenType::TK_BIT_AND); });
  return FORWARD_AST(p1);
}

auto ParserSpp::ParseTokenDot()
  -> Unique<TokenAst> {
  PARSE_ONCE(p1, [this] { return ParseTokenRaw(lex::RawTokenType::TK_PERIOD, lex::SppTokenType::TK_DOT); });
  return FORWARD_AST(p1);
}

auto ParserSpp::ParseTokenQuestionMark()
  -> Unique<TokenAst> {
  PARSE_ONCE(
    p1, [this] { return ParseTokenRaw(lex::RawTokenType::TK_QUESTION_MARK, lex::SppTokenType::TK_QUESTION_MARK); });
  return FORWARD_AST(p1);
}

auto ParserSpp::ParseTokenExclamationMark()
  -> Unique<TokenAst> {
  PARSE_ONCE(
    p1, [this] { return ParseTokenRaw(lex::RawTokenType::TK_EXCLAMATION_MARK, lex::SppTokenType::TK_EXCLAMATION_MARK);
    });
  return FORWARD_AST(p1);
}

auto ParserSpp::ParseTokenDeref()
  -> Unique<TokenAst> {
  PARSE_ONCE(p1, [this] { return ParseTokenRaw(lex::RawTokenType::TK_AT_SIGN, lex::SppTokenType::TK_DEREF); });
  return FORWARD_AST(p1);
}

auto ParserSpp::ParseTokenBorrow()
  -> Unique<TokenAst> {
  PARSE_ONCE(p1, [this] { return ParseTokenRaw(lex::RawTokenType::TK_AMPERSAND, lex::SppTokenType::TK_BORROW); });
  return FORWARD_AST(p1);
}

auto ParserSpp::ParseTokenSemicolon()
  -> Unique<TokenAst> {
  PARSE_ONCE(p1, [this] { return ParseTokenRaw(lex::RawTokenType::TK_SEMICOLON, lex::SppTokenType::TK_SEMICOLON); });
  return FORWARD_AST(p1);
}

auto ParserSpp::ParseTokenSingleQuote()
  -> Unique<TokenAst> {
  PARSE_ONCE(
    p1, [this] { return ParseTokenRaw(lex::RawTokenType::TK_APOSTROPHE, lex::SppTokenType::TK_APOSTROPHE); });
  return FORWARD_AST(p1);
}

auto ParserSpp::ParseTokenDoubleQuote()
  -> Unique<TokenAst> {
  PARSE_ONCE(
    p1, [this] { return ParseTokenRaw(lex::RawTokenType::TK_QUOTATION_MARK, lex::SppTokenType::TK_QUOTATION_MARK); });
  return FORWARD_AST(p1);
}

auto ParserSpp::ParseTokenArrowRight()
  -> Unique<TokenAst> {
  PARSE_ONCE(p1, [this] { return ParseTokenRaw(lex::RawTokenType::TK_HYPHEN, lex::SppTokenType::TK_ARROW_RIGHT); });
  PARSE_ONCE(
    p2, [this] { return ParseTokenRaw(lex::RawTokenType::TK_GREATER_THAN, lex::SppTokenType::TK_ARROW_RIGHT); });
  p1->TokenData = "->";
  return FORWARD_AST(p1);
}

auto ParserSpp::ParseTokenDoubleDot()
  -> Unique<TokenAst> {
  PARSE_ONCE(p1, [this] { return ParseTokenRaw(lex::RawTokenType::TK_PERIOD, lex::SppTokenType::TK_DOUBLE_DOT); });
  PARSE_ONCE(p2, [this] { return ParseTokenRaw(lex::RawTokenType::TK_PERIOD, lex::SppTokenType::TK_DOUBLE_DOT); });
  p1->TokenData = "..";
  return FORWARD_AST(p1);
}

auto ParserSpp::ParseTokenDoubleColon()
  -> Unique<TokenAst> {
  PARSE_ONCE(p1, [this] { return ParseTokenRaw(lex::RawTokenType::TK_COLON, lex::SppTokenType::TK_DOUBLE_COLON); });
  PARSE_ONCE(p2, [this] { return ParseTokenRaw(lex::RawTokenType::TK_COLON, lex::SppTokenType::TK_DOUBLE_COLON); });
  p1->TokenData = "::";
  return FORWARD_AST(p1);
}

auto ParserSpp::ParseTokenEquals()
  -> Unique<TokenAst> {
  PARSE_ONCE(p1, [this] { return ParseTokenRaw(lex::RawTokenType::TK_EQUALS_TO, lex::SppTokenType::TK_EQ); });
  PARSE_ONCE(p2, [this] { return ParseTokenRaw(lex::RawTokenType::TK_EQUALS_TO, lex::SppTokenType::TK_EQ); });
  p1->TokenData = "==";
  return FORWARD_AST(p1);
}

auto ParserSpp::ParseTokenNotEquals()
  -> Unique<TokenAst> {
  PARSE_ONCE(p1, [this] { return ParseTokenRaw(lex::RawTokenType::TK_EXCLAMATION_MARK, lex::SppTokenType::TK_NE); });
  PARSE_ONCE(p2, [this] { return ParseTokenRaw(lex::RawTokenType::TK_EQUALS_TO, lex::SppTokenType::TK_NE); });
  p1->TokenData = "!=";
  return FORWARD_AST(p1);
}

auto ParserSpp::ParseTokenLessThanEquals()
  -> Unique<TokenAst> {
  PARSE_ONCE(p1, [this] { return ParseTokenRaw(lex::RawTokenType::TK_LESS_THAN, lex::SppTokenType::TK_LE); });
  PARSE_ONCE(p2, [this] { return ParseTokenRaw(lex::RawTokenType::TK_EQUALS_TO, lex::SppTokenType::TK_LE); });
  p1->TokenData = "<=";
  return FORWARD_AST(p1);
}

auto ParserSpp::ParseTokenGreaterThanEquals()
  -> Unique<TokenAst> {
  PARSE_ONCE(p1, [this] { return ParseTokenRaw(lex::RawTokenType::TK_GREATER_THAN, lex::SppTokenType::TK_GE); });
  PARSE_ONCE(p2, [this] { return ParseTokenRaw(lex::RawTokenType::TK_EQUALS_TO, lex::SppTokenType::TK_GE); });
  p1->TokenData = ">=";
  return FORWARD_AST(p1);
}

auto ParserSpp::ParseTokenAddAssign()
  -> Unique<TokenAst> {
  PARSE_ONCE(p1, [this] { return ParseTokenRaw(lex::RawTokenType::TK_PLUS_SIGN, lex::SppTokenType::TK_ADD_ASSIGN); });
  PARSE_ONCE(p2, [this] { return ParseTokenRaw(lex::RawTokenType::TK_EQUALS_TO, lex::SppTokenType::TK_ADD_ASSIGN); });
  p1->TokenData = "+=";
  return FORWARD_AST(p1);
}

auto ParserSpp::ParseTokenSubAssign()
  -> Unique<TokenAst> {
  PARSE_ONCE(p1, [this] { return ParseTokenRaw(lex::RawTokenType::TK_HYPHEN, lex::SppTokenType::TK_SUB_ASSIGN); });
  PARSE_ONCE(p2, [this] { return ParseTokenRaw(lex::RawTokenType::TK_EQUALS_TO, lex::SppTokenType::TK_SUB_ASSIGN); });
  p1->TokenData = "-=";
  return FORWARD_AST(p1);
}

auto ParserSpp::ParseTokenMulAssign()
  -> Unique<TokenAst> {
  PARSE_ONCE(p1, [this] { return ParseTokenRaw(lex::RawTokenType::TK_ASTERISK, lex::SppTokenType::TK_MUL_ASSIGN); });
  PARSE_ONCE(p2, [this] { return ParseTokenRaw(lex::RawTokenType::TK_EQUALS_TO, lex::SppTokenType::TK_MUL_ASSIGN); });
  p1->TokenData = "*=";
  return FORWARD_AST(p1);
}

auto ParserSpp::ParseTokenDivAssign()
  -> Unique<TokenAst> {
  PARSE_ONCE(p1, [this] { return ParseTokenRaw(lex::RawTokenType::TK_SLASH, lex::SppTokenType::TK_DIV_ASSIGN); });
  PARSE_ONCE(p2, [this] { return ParseTokenRaw(lex::RawTokenType::TK_EQUALS_TO, lex::SppTokenType::TK_DIV_ASSIGN); });
  p1->TokenData = "/=";
  return FORWARD_AST(p1);
}

auto ParserSpp::ParseTokenRemAssign()
  -> Unique<TokenAst> {
  PARSE_ONCE(
    p1, [this] { return ParseTokenRaw(lex::RawTokenType::TK_PERCENT_SIGN, lex::SppTokenType::TK_REM_ASSIGN); });
  PARSE_ONCE(p2, [this] { return ParseTokenRaw(lex::RawTokenType::TK_EQUALS_TO, lex::SppTokenType::TK_REM_ASSIGN); });
  p1->TokenData = "%=";
  return FORWARD_AST(p1);
}

auto ParserSpp::ParseTokenPow()
  -> Unique<TokenAst> {
  PARSE_ONCE(p1, [this] { return ParseTokenRaw(lex::RawTokenType::TK_ASTERISK, lex::SppTokenType::TK_POW); });
  PARSE_ONCE(p2, [this] { return ParseTokenRaw(lex::RawTokenType::TK_ASTERISK, lex::SppTokenType::TK_POW); });
  p1->TokenData = "**";
  return FORWARD_AST(p1);
}

auto ParserSpp::ParseTokenBitShl()
  -> Unique<TokenAst> {
  PARSE_ONCE(p1, [this] { return ParseTokenRaw(lex::RawTokenType::TK_LESS_THAN, lex::SppTokenType::TK_BIT_SHL); });
  PARSE_ONCE(p2, [this] { return ParseTokenRaw(lex::RawTokenType::TK_LESS_THAN, lex::SppTokenType::TK_BIT_SHL); });
  p1->TokenData = "<<";
  return FORWARD_AST(p1);
}

auto ParserSpp::ParseTokenBitShr()
  -> Unique<TokenAst> {
  PARSE_ONCE(p1, [this] { return ParseTokenRaw(lex::RawTokenType::TK_GREATER_THAN, lex::SppTokenType::TK_BIT_SHR); });
  PARSE_ONCE(p2, [this] { return ParseTokenRaw(lex::RawTokenType::TK_GREATER_THAN, lex::SppTokenType::TK_BIT_SHR); });
  p1->TokenData = ">>";
  return FORWARD_AST(p1);
}

auto ParserSpp::ParseTokenBitIorAssign()
  -> Unique<TokenAst> {
  PARSE_ONCE(
    p1, [this] { return ParseTokenRaw(lex::RawTokenType::TK_VERTICAL_BAR, lex::SppTokenType::TK_BIT_IOR_ASSIGN); });
  PARSE_ONCE(
    p2, [this] { return ParseTokenRaw(lex::RawTokenType::TK_EQUALS_TO, lex::SppTokenType::TK_BIT_IOR_ASSIGN); });
  p1->TokenData = "|=";
  return FORWARD_AST(p1);
}

auto ParserSpp::ParseTokenBitXorAssign()
  -> Unique<TokenAst> {
  PARSE_ONCE(p1, [this] { return ParseTokenRaw(lex::RawTokenType::TK_CARET, lex::SppTokenType::TK_BIT_XOR_ASSIGN); });
  PARSE_ONCE(
    p2, [this] { return ParseTokenRaw(lex::RawTokenType::TK_EQUALS_TO, lex::SppTokenType::TK_BIT_XOR_ASSIGN); });
  p1->TokenData = "^=";
  return FORWARD_AST(p1);
}

auto ParserSpp::ParseTokenBitAndAssign()
  -> Unique<TokenAst> {
  PARSE_ONCE(
    p1, [this] { return ParseTokenRaw(lex::RawTokenType::TK_AMPERSAND, lex::SppTokenType::TK_BIT_AND_ASSIGN); });
  PARSE_ONCE(
    p2, [this] { return ParseTokenRaw(lex::RawTokenType::TK_EQUALS_TO, lex::SppTokenType::TK_BIT_AND_ASSIGN); });
  p1->TokenData = "&=";
  return FORWARD_AST(p1);
}

auto ParserSpp::ParseTokenPowAssign()
  -> Unique<TokenAst> {
  PARSE_ONCE(p1, [this] { return ParseTokenRaw(lex::RawTokenType::TK_ASTERISK, lex::SppTokenType::TK_POW_ASSIGN); });
  PARSE_ONCE(p2, [this] { return ParseTokenRaw(lex::RawTokenType::TK_ASTERISK, lex::SppTokenType::TK_POW_ASSIGN); });
  PARSE_ONCE(p3, [this] { return ParseTokenRaw(lex::RawTokenType::TK_EQUALS_TO, lex::SppTokenType::TK_POW_ASSIGN); });
  p1->TokenData = "**=";
  return FORWARD_AST(p1);
}

auto ParserSpp::ParseTokenBitShlAssign()
  -> Unique<TokenAst> {
  PARSE_ONCE(
    p1, [this] { return ParseTokenRaw(lex::RawTokenType::TK_LESS_THAN, lex::SppTokenType::TK_BIT_SHL_ASSIGN); });
  PARSE_ONCE(
    p2, [this] { return ParseTokenRaw(lex::RawTokenType::TK_LESS_THAN, lex::SppTokenType::TK_BIT_SHL_ASSIGN); });
  PARSE_ONCE(
    p3, [this] { return ParseTokenRaw(lex::RawTokenType::TK_EQUALS_TO, lex::SppTokenType::TK_BIT_SHL_ASSIGN); });
  p1->TokenData = "<<=";
  return FORWARD_AST(p1);
}

auto ParserSpp::ParseTokenBitShrAssign()
  -> Unique<TokenAst> {
  PARSE_ONCE(
    p1, [this] { return ParseTokenRaw(lex::RawTokenType::TK_GREATER_THAN, lex::SppTokenType::TK_BIT_SHR_ASSIGN); });
  PARSE_ONCE(
    p2, [this] { return ParseTokenRaw(lex::RawTokenType::TK_GREATER_THAN, lex::SppTokenType::TK_BIT_SHR_ASSIGN); });
  PARSE_ONCE(
    p3, [this] { return ParseTokenRaw(lex::RawTokenType::TK_EQUALS_TO, lex::SppTokenType::TK_BIT_SHR_ASSIGN); });
  p1->TokenData = ">>=";
  return FORWARD_AST(p1);
}

auto ParserSpp::ParseKeywordCls()
  -> Unique<TokenAst> {
  PARSE_ONCE(p1, [this] { return ParseTokenRaw(lex::RawTokenType::KW_CLS, lex::SppTokenType::KW_CLS); });
  return FORWARD_AST(p1);
}

auto ParserSpp::ParseKeywordFun()
  -> Unique<TokenAst> {
  PARSE_ONCE(p1, [this] { return ParseTokenRaw(lex::RawTokenType::KW_FUN, lex::SppTokenType::KW_FUN); });
  return FORWARD_AST(p1);
}

auto ParserSpp::ParseKeywordCor()
  -> Unique<TokenAst> {
  PARSE_ONCE(p1, [this] { return ParseTokenRaw(lex::RawTokenType::KW_COR, lex::SppTokenType::KW_COR); });
  return FORWARD_AST(p1);
}

auto ParserSpp::ParseKeywordSup()
  -> Unique<TokenAst> {
  PARSE_ONCE(p1, [this] { return ParseTokenRaw(lex::RawTokenType::KW_SUP, lex::SppTokenType::KW_SUP); });
  return FORWARD_AST(p1);
}

auto ParserSpp::ParseKeywordExt()
  -> Unique<TokenAst> {
  PARSE_ONCE(p1, [this] { return ParseTokenRaw(lex::RawTokenType::KW_EXT, lex::SppTokenType::KW_EXT); });
  return FORWARD_AST(p1);
}

auto ParserSpp::ParseKeywordMut()
  -> Unique<TokenAst> {
  PARSE_ONCE(p1, [this] { return ParseTokenRaw(lex::RawTokenType::KW_MUT, lex::SppTokenType::KW_MUT); });
  return FORWARD_AST(p1);
}

auto ParserSpp::ParseKeywordUse()
  -> Unique<TokenAst> {
  PARSE_ONCE(p1, [this] { return ParseTokenRaw(lex::RawTokenType::KW_USE, lex::SppTokenType::KW_USE); });
  return FORWARD_AST(p1);
}

auto ParserSpp::ParseKeywordCmp()
  -> Unique<TokenAst> {
  PARSE_ONCE(p1, [this] { return ParseTokenRaw(lex::RawTokenType::KW_CMP, lex::SppTokenType::KW_CMP); });
  return FORWARD_AST(p1);
}

auto ParserSpp::ParseKeywordLet()
  -> Unique<TokenAst> {
  PARSE_ONCE(p1, [this] { return ParseTokenRaw(lex::RawTokenType::KW_LET, lex::SppTokenType::KW_LET); });
  return FORWARD_AST(p1);
}

auto ParserSpp::ParseKeywordType()
  -> Unique<TokenAst> {
  PARSE_ONCE(p1, [this] { return ParseTokenRaw(lex::RawTokenType::KW_TYPE, lex::SppTokenType::KW_TYPE); });
  return FORWARD_AST(p1);
}

auto ParserSpp::ParseKeywordSelf()
  -> Unique<TokenAst> {
  PARSE_ONCE(p1, [this] { return ParseTokenRaw(lex::RawTokenType::KW_SELF, lex::SppTokenType::KW_SELF); });
  return FORWARD_AST(p1);
}

auto ParserSpp::ParseKeywordCase()
  -> Unique<TokenAst> {
  PARSE_ONCE(p1, [this] { return ParseTokenRaw(lex::RawTokenType::KW_CASE, lex::SppTokenType::KW_CASE); });
  return FORWARD_AST(p1);
}

auto ParserSpp::ParseKeywordOf()
  -> Unique<TokenAst> {
  PARSE_ONCE(p1, [this] { return ParseTokenRaw(lex::RawTokenType::KW_OF, lex::SppTokenType::KW_OF); });
  return FORWARD_AST(p1);
}

auto ParserSpp::ParseKeywordLoop()
  -> Unique<TokenAst> {
  PARSE_ONCE(p1, [this] { return ParseTokenRaw(lex::RawTokenType::KW_LOOP, lex::SppTokenType::KW_LOOP); });
  return FORWARD_AST(p1);
}

auto ParserSpp::ParseKeywordIn()
  -> Unique<TokenAst> {
  PARSE_ONCE(p1, [this] { return ParseTokenRaw(lex::RawTokenType::KW_IN, lex::SppTokenType::KW_IN); });
  return FORWARD_AST(p1);
}

auto ParserSpp::ParseKeywordTo()
  -> Unique<TokenAst> {
  PARSE_ONCE(p1, [this] { return ParseTokenRaw(lex::RawTokenType::KW_TO, lex::SppTokenType::KW_TO); });
  return FORWARD_AST(p1);
}

auto ParserSpp::ParseKeywordElse()
  -> Unique<TokenAst> {
  PARSE_ONCE(p1, [this] { return ParseTokenRaw(lex::RawTokenType::KW_ELSE, lex::SppTokenType::KW_ELSE); });
  return FORWARD_AST(p1);
}

auto ParserSpp::ParseKeywordGen()
  -> Unique<TokenAst> {
  PARSE_ONCE(p1, [this] { return ParseTokenRaw(lex::RawTokenType::KW_GEN, lex::SppTokenType::KW_GEN); });
  return FORWARD_AST(p1);
}

auto ParserSpp::ParseKeywordWith()
  -> Unique<TokenAst> {
  PARSE_ONCE(p1, [this] { return ParseTokenRaw(lex::RawTokenType::KW_WITH, lex::SppTokenType::KW_WITH); });
  return FORWARD_AST(p1);
}

auto ParserSpp::ParseKeywordDefer()
  -> Unique<TokenAst> {
  PARSE_ONCE(p1, [this] { return ParseTokenRaw(lex::RawTokenType::KW_DEFER, lex::SppTokenType::KW_DEFER); });
  return FORWARD_AST(p1);
}

auto ParserSpp::ParseKeywordRet()
  -> Unique<TokenAst> {
  PARSE_ONCE(p1, [this] { return ParseTokenRaw(lex::RawTokenType::KW_RET, lex::SppTokenType::KW_RET); });
  return FORWARD_AST(p1);
}

auto ParserSpp::ParseKeywordExit()
  -> Unique<TokenAst> {
  PARSE_ONCE(p1, [this] { return ParseTokenRaw(lex::RawTokenType::KW_EXIT, lex::SppTokenType::KW_EXIT); });
  return FORWARD_AST(p1);
}

auto ParserSpp::ParseKeywordSkip()
  -> Unique<TokenAst> {
  PARSE_ONCE(p1, [this] { return ParseTokenRaw(lex::RawTokenType::KW_SKIP, lex::SppTokenType::KW_SKIP); });
  return FORWARD_AST(p1);
}

auto ParserSpp::ParseKeywordIs()
  -> Unique<TokenAst> {
  PARSE_ONCE(p1, [this] { return ParseTokenRaw(lex::RawTokenType::KW_IS, lex::SppTokenType::KW_IS); });
  return FORWARD_AST(p1);
}

auto ParserSpp::ParseKeywordAs()
  -> Unique<TokenAst> {
  PARSE_ONCE(p1, [this] { return ParseTokenRaw(lex::RawTokenType::KW_AS, lex::SppTokenType::KW_AS); });
  return FORWARD_AST(p1);
}

auto ParserSpp::ParseKeywordOr()
  -> Unique<TokenAst> {
  PARSE_ONCE(p1, [this] { return ParseTokenRaw(lex::RawTokenType::KW_OR, lex::SppTokenType::KW_OR); });
  return FORWARD_AST(p1);
}

auto ParserSpp::ParseKeywordAnd()
  -> Unique<TokenAst> {
  PARSE_ONCE(p1, [this] { return ParseTokenRaw(lex::RawTokenType::KW_AND, lex::SppTokenType::KW_AND); });
  return FORWARD_AST(p1);
}

auto ParserSpp::ParseKeywordNot()
  -> Unique<TokenAst> {
  PARSE_ONCE(p1, [this] { return ParseTokenRaw(lex::RawTokenType::KW_NOT, lex::SppTokenType::KW_NOT); });
  return FORWARD_AST(p1);
}

auto ParserSpp::ParseKeywordAsync()
  -> Unique<TokenAst> {
  PARSE_ONCE(p1, [this] { return ParseTokenRaw(lex::RawTokenType::KW_ASYNC, lex::SppTokenType::KW_ASYNC); });
  return FORWARD_AST(p1);
}

auto ParserSpp::ParseKeywordTrue()
  -> Unique<TokenAst> {
  PARSE_ONCE(p1, [this] { return ParseTokenRaw(lex::RawTokenType::KW_TRUE, lex::SppTokenType::KW_TRUE); });
  return FORWARD_AST(p1);
}

auto ParserSpp::ParseKeywordFalse()
  -> Unique<TokenAst> {
  PARSE_ONCE(p1, [this] { return ParseTokenRaw(lex::RawTokenType::KW_FALSE, lex::SppTokenType::KW_FALSE); });
  return FORWARD_AST(p1);
}

auto ParserSpp::ParseKeywordAwait()
  -> Unique<TokenAst> {
  PARSE_ONCE(p1, [this] { return ParseTokenRaw(lex::RawTokenType::KW_AWAIT, lex::SppTokenType::KW_AWAIT); });
  return FORWARD_AST(p1);
}

auto ParserSpp::ParseKeywordRes()
  -> Unique<TokenAst> {
  PARSE_ONCE(p1, [this] { return ParseTokenRaw(lex::RawTokenType::KW_RES, lex::SppTokenType::KW_RES); });
  return FORWARD_AST(p1);
}

auto ParserSpp::ParseKeywordCaps()
  -> Unique<TokenAst> {
  PARSE_ONCE(p1, [this] { return ParseTokenRaw(lex::RawTokenType::KW_CAPS, lex::SppTokenType::KW_CAPS); });
  return FORWARD_AST(p1);
}

auto ParserSpp::ParseTokenRaw(const lex::RawTokenType tok, lex::SppTokenType mapped_tok)
  -> Unique<TokenAst> {
  if (_Pos > _Tokens.size()) {
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
    return _CreateAst<TokenAst>(
      _ErrorFormatter->PositionOf(_Pos),
      mapped_tok, Str());
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
    const auto got = _ErrorFormatter != nullptr and _ErrorFormatter->IsPastUserSource(
      _ErrorFormatter->PositionOf(_Pos))
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
  return _CreateAst<TokenAst>(
    _ErrorFormatter->PositionOf(pos),
    mapped_tok, _Tokens[_Pos - 1].Data.data());
}

auto ParserSpp::_StoreError(
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

auto ParserSpp::_StoreSpecificError(
  const std::size_t pos,
  Str &&err_str) const
  -> void {
  if (pos >= _ErrorBuilder->Pos) {
    _ErrorBuilder->WithArgs(std::move(err_str));
    _ErrorBuilder->Tokens.clear();
    _ErrorBuilder->Pos = pos;
  }
}

auto ParserSpp::_LineFeedAhead() const
  -> bool {
  auto pos = _Pos;
  while (pos < _TokensLen and _Tokens[pos].Type == lex::RawTokenType::TK_SPACE) {
    ++pos;
  }
  return pos < _TokensLen and _Tokens[pos].Type == lex::RawTokenType::TK_LINE_FEED;
}

SPP_MOD_END
