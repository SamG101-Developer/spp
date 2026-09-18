module;
#include <spp/macros.hpp>

export module spp.asts.array_literal_ast;
import spp.asts.literal_ast;
import std;

SPP_AST_COMMON_FWD_DECL(ArrayLiteralAst);

GCC_BUGZILLA_127346_FORWARD_DECL_GLOBAL_FRAGMENT
use(spp::asts, struct ArrayLiteralExplicitElementsAst);
use(spp::asts, struct ArrayLiteralRepeatedElementAst);
use(spp::asts, struct GenericArgumentAst);
use(spp::analyse::scopes, struct TypeRef);
use(spp::asts, struct TypeAst);

/// The base array class for the two array literals [0, 0, 0],
/// and [0; 3_uz]. Not strictly needed, but just for tidying
/// the hierarchy.
SPP_EXP_CLS struct spp::asts::ArrayLiteralAst : LiteralAst {
  ArrayLiteralAst();
  ~ArrayLiteralAst() override;
};
