module;
#include <spp/macros.hpp>

export module spp.asts.type_tuple_shorthand_ast;
import spp.asts.ast;
import spp.asts.ast_kind;
import spp.asts.mixins.temp_type_ast;
import spp.utils.types;
import std;

SPP_AST_COMMON_FWD_DECL(TypeTupleShorthandAst);
use(spp::asts, struct TokenAst);
use(spp::asts, struct TypeAst);

SPP_EXP_CLS struct spp::asts::TypeTupleShorthandAst final : Ast, mixins::TempTypeAst {
  SPP_AST_KEY_FUNCTIONS(TypeTupleShorthandAst);

  /// The "(" token that starts the tuple type.
  Unique<TokenAst> TokL;

  /// The types of the elements in the tuple.
  Vec<Shared<TypeAst>> ElemTypes;

  /// The ")" token that ends the tuple type.
  Unique<TokenAst> TokR;

  TypeTupleShorthandAst(
    decltype(TokL) &&tok_l,
    decltype(ElemTypes) &&element_types,
    decltype(TokR) &&tok_r);

  ~TypeTupleShorthandAst() override;

  auto Convert() -> Unique<TypeAst> override;
};
