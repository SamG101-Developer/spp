module;
#include <spp/macros.hpp>

export module spp.asts.type_array_shorthand_ast;
import spp.asts.ast;
import spp.asts.ast_kind;
import spp.asts.mixins.temp_type_ast;
import spp.utils.types;
import std;

SPP_AST_COMMON_FWD_DECL(TypeArrayShorthandAst);
use(spp::asts, struct ExpressionAst);
use(spp::asts, struct TokenAst);
use(spp::asts, struct TypeAst);

SPP_EXP_CLS struct spp::asts::TypeArrayShorthandAst final : Ast, mixins::TempTypeAst {
  SPP_AST_KEY_FUNCTIONS(TypeArrayShorthandAst);

  /// The "[" token that starts the array type.
  Unique<TokenAst> TokL;

  /// The type of the elements in the array.
  Shared<TypeAst> ElemType;

  /// The ";" token that separates the element type from the
  /// size.
  Unique<TokenAst> TokSemiColon;

  /// The size of the array, either a literal or an expression.
  Unique<ExpressionAst> Size;

  /// The "]" token that ends the array type.
  Unique<TokenAst> TokR;

  TypeArrayShorthandAst(
    decltype(TokL) &&tok_l,
    decltype(ElemType) &&element_type,
    decltype(TokSemiColon) &&tok_semicolon,
    decltype(Size) &&size,
    decltype(TokR) &&tok_r);

  ~TypeArrayShorthandAst() override;

  auto Convert() -> Unique<TypeAst> override;
};
