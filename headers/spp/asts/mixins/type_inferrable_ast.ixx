module;
#include <spp/macros.hpp>

export module spp.asts.mixins.type_inferrable_ast;
import spp.utils.types;
import std;

use(spp::analyse::scopes, class ScopeManager);
use(spp::analyse::scopes, struct TypeRef);
use(spp::asts, struct TypeAst);
use(spp::asts::meta, struct CompilerMetaData);
use(spp::asts::mixins, struct TypeInferrableAst);

/// The marker that allows for ast to be type-inferrable. All
/// statements default to returning the "Void" type, and then
/// the majority or expressions override the default method
/// to get the actual type. Postfix and unary expressions do
/// work on this too.
SPP_EXP_CLS struct spp::asts::mixins::TypeInferrableAst {
  TypeInferrableAst();

  virtual ~TypeInferrableAst();

  /// The core type inference function, which recursively
  /// uses asts and the fields to derive the type that the
  /// expression produces.
  virtual auto InferType(ScopeManager *sm, CompilerMetaData *meta) -> Shared<TypeAst> = 0;

  /// The inferred type as what it resolves to where it is inferred: its symbol and how it is held. By default this is
  /// "InferType" resolved in the current scope; an expression that knows its symbol already answers with it directly.
  virtual auto InferTypeRef(ScopeManager *sm, CompilerMetaData *meta) -> TypeRef;

  /// The source agnostic version, that uses a "Source"
  /// struct's original type, for error reporting purposes.
  /// Less used, potentially removable.
  virtual auto InferTypeForDisplay(ScopeManager *sm, CompilerMetaData *meta) -> Shared<TypeAst>;
};
