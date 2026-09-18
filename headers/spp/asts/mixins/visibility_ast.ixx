module;
#include <spp/macros.hpp>

export module spp.asts.mixins.visibility_enabled_ast;
import spp.asts.utils.visibility;
import spp.utils.types;
import std;

use(spp::asts, struct AnnotationAst);
use(spp::asts, struct TypeStatementAst);
use(spp::asts::mixins, struct VisibilityAst);

namespace spp::asts::mixins {
  SPP_EXP_CLS using VisibilityPair = Pair<utils::Visibility, AnnotationAst*>;
}

SPP_EXP_CLS struct spp::asts::mixins::VisibilityAst {
  /// A visibility tag and the annotation ast that was used to
  /// dictate this tag.
  VisibilityPair Visibility;

  /// Default to the private visibility with a nullptr ast.
  VisibilityAst();

  virtual ~VisibilityAst();
};
