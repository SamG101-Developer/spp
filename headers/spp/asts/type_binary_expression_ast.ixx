module;
#include <spp/macros.hpp>

export module spp.asts.type_binary_expression_ast;
import spp.asts.ast;
import spp.asts.ast_kind;
import spp.asts.mixins.temp_type_ast;
import spp.utils.types;
import std;

SPP_AST_COMMON_FWD_DECL(TypeBinaryExpressionAst);
use(spp::asts, struct TokenAst);
use(spp::asts, struct TypeAst);

SPP_EXP_CLS struct spp::asts::TypeBinaryExpressionAst final : Ast, mixins::TempTypeAst {
  SPP_AST_KEY_FUNCTIONS(TypeBinaryExpressionAst);

  /// The left-hand-side type (first operand).
  Shared<TypeAst> Lhs;

  /// The type binary operator token: either "or" (union) or
  /// "and" (intersection).
  Unique<TokenAst> TokOp;

  /// The right-hand-side type (second operand).
  Shared<TypeAst> Rhs;

  TypeBinaryExpressionAst(
    decltype(Lhs) &&lhs,
    decltype(TokOp) &&tok_op,
    decltype(Rhs) &&rhs);

  ~TypeBinaryExpressionAst() override;

  auto Convert() -> Unique<TypeAst> override;
};
