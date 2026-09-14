module;
#include <spp/macros.hpp>

export module spp.asts.mixins.temp_type_ast;
import spp.utils.types;
import std;

use(spp::asts, struct TypeAst);
use(spp::asts::mixins, struct TempTypeAst);

/// A temporary type ast is a shorthand type ast that gets
/// mapped into an expanded type, like "(S32, Str)" becomes
/// Tup[S32, Str]". The conversion method gets overridden to
/// produce the true type ast.
SPP_EXP_CLS struct spp::asts::mixins::TempTypeAst {
  TempTypeAst();

  virtual ~TempTypeAst();

  /// Run the conversion steps, on a per-asst implementation
  /// basis, to get the true type.
  virtual auto Convert() -> Unique<TypeAst> = 0;
};
