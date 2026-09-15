module;
#include <spp/macros.hpp>

export module spp.analyse.utils.bin_utils;
import spp.lex.tokens;
import spp.utils.types;
import std;

use(spp::asts, struct BinaryExpressionAst);
use(spp::asts, struct CaseExpressionAst);
use(spp::asts, struct IsExpressionAst);
use(spp::asts, struct LetStatementInitializedAst);
use(spp::asts, struct PostfixExpressionAst);
use(spp::asts::meta, struct CompilerMetaData);
use(spp::analyse::scopes, class ScopeManager);
use(spp::analyse::scopes, struct TypeSymbol);

namespace spp::analyse::utils::bin_utils {
  /// A mapping of the binary operators to the appropriate
  /// method names from the operator overloading classes.
  SPP_EXP_CLS const auto kBinMethods = Map<lex::SppTokenType, Str>{
    {lex::SppTokenType::TK_EQ, "eq"}, {lex::SppTokenType::TK_NE, "ne"},
    {lex::SppTokenType::TK_LT, "lt"}, {lex::SppTokenType::TK_LE, "le"},
    {lex::SppTokenType::TK_GT, "gt"}, {lex::SppTokenType::TK_GE, "ge"},
    {lex::SppTokenType::TK_BIT_IOR, "bit_ior"},
    {lex::SppTokenType::TK_BIT_XOR, "bit_xor"},
    {lex::SppTokenType::TK_BIT_AND, "bit_and"},
    {lex::SppTokenType::TK_BIT_SHL, "bit_shl"},
    {lex::SppTokenType::TK_BIT_SHR, "bit_shr"},
    {lex::SppTokenType::TK_ADD, "add"}, {lex::SppTokenType::TK_SUB, "sub"},
    {lex::SppTokenType::TK_MUL, "mul"}, {lex::SppTokenType::TK_DIV, "div"},
    {lex::SppTokenType::TK_REM, "rem"}, {lex::SppTokenType::TK_POW, "pow"},
    {lex::SppTokenType::TK_BIT_IOR_ASSIGN, "bit_ior_assign"},
    {lex::SppTokenType::TK_BIT_XOR_ASSIGN, "bit_xor_assign"},
    {lex::SppTokenType::TK_BIT_AND_ASSIGN, "bit_and_assign"},
    {lex::SppTokenType::TK_BIT_SHL_ASSIGN, "bit_shl_assign"},
    {lex::SppTokenType::TK_BIT_SHR_ASSIGN, "bit_shr_assign"},
    {lex::SppTokenType::TK_ADD_ASSIGN, "add_assign"}, {lex::SppTokenType::TK_SUB_ASSIGN, "sub_assign"},
    {lex::SppTokenType::TK_MUL_ASSIGN, "mul_assign"}, {lex::SppTokenType::TK_DIV_ASSIGN, "div_assign"},
    {lex::SppTokenType::TK_REM_ASSIGN, "rem_assign"}, {lex::SppTokenType::TK_POW_ASSIGN, "pow_assign"}
  };

  /// The list of binary comparison operators, used during
  /// comparison collapsing. Limited to the standard 6
  /// "<, >, <=, >=, ==, !=".
  SPP_EXP_CLS constexpr auto kBinComparisonOps = std::array{
    lex::SppTokenType::TK_EQ,
    lex::SppTokenType::TK_NE,
    lex::SppTokenType::TK_LT,
    lex::SppTokenType::TK_GT,
    lex::SppTokenType::TK_LE,
    lex::SppTokenType::TK_GE
  };

  /// Collapse a chain of comparisons into chained "and"
  /// binary expressions. For example, "a < b < c" becomes
  /// "(a < b) and (b < c)". Non-comparison chains are not
  /// changed.
  SPP_EXP_FUN auto CombineComparisonChain(
    BinaryExpressionAst &bin_expr,
    ScopeManager *sm,
    CompilerMetaData *meta,
    Vec<Unique<LetStatementInitializedAst>> &temps)
    -> Unique<BinaryExpressionAst>;

  /// Convert the binary expression to the equivalent
  /// function call, based on the above mapping. For example,
  /// "a + b" becomes "a.add(b)" which on analysis generates
  /// "S32::add(a, b)" or whatever the type is.
  SPP_EXP_FUN auto ConvertBinExprToFuncCall(
    BinaryExpressionAst &bin_expr,
    ScopeManager *sm,
    CompilerMetaData *meta)
    -> Unique<PostfixExpressionAst>;

  /// Convert the is expression to the equivalent function
  /// call, based on the above mapping. For example, "a is
  /// S32" becomes a case-pattern destructure check.
  /// Provides a uniform variant decomposition mechanism.
  SPP_EXP_FUN auto ConvertIsExprToFuncCall(
    IsExpressionAst &is_expr,
    ScopeManager *sm,
    CompilerMetaData *meta)
    -> Unique<CaseExpressionAst>;
}
