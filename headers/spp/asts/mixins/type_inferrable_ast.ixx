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

  /// The core type inference function: what the expression's
  /// type resolves to where it is inferred, its symbol and how
  /// it is held. Every analysis and codegen reader uses this.
  virtual auto InferTypeRef(
    ScopeManager *sm, CompilerMetaData *meta) -> TypeRef = 0;

  /// The inferred type as syntax, for messages and for building
  /// other syntax. By default the resolved type's name, placed at
  /// this expression ("TypeRef::AstIn"); an expression overrides
  /// it where the written form says more (an alias's name, a
  /// declared type), or where its type is built as syntax.
  virtual auto InferType(
    ScopeManager *sm, CompilerMetaData *meta) -> Shared<TypeAst>;
};
