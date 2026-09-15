module;
#include <spp/macros.hpp>

export module spp.asts.case_pattern_variant_ast;
import spp.asts.ast;
import spp.utils.types;
import std;

SPP_AST_COMMON_FWD_DECL(CasePatternVariantAst);
use(spp::asts, struct LetStatementInitializedAst);
use(spp::asts, struct LocalVariableAst);

/// Base class for all case pattern variants. Allows different
/// variants in the same list, and provides the conversion
/// binding for creating variables defined in patterns.
SPP_EXP_CLS struct spp::asts::CasePatternVariantAst : Ast {
  CasePatternVariantAst();

  /// Handle pattern matching for case expressions. This is
  /// reimplemented in all the other patterns, but due to a GCC
  /// modules bug, it must be defined here too.
  auto Stage9_CompTimeResolve(ScopeManager *sm, CompilerMetaData *meta) -> void override;

  /// Case patterns can introduce variables via bindings. This
  /// conversion introduces all required bindings into scope,
  /// including nested ones, and is overridden on the different
  /// destructuring patterns.
  virtual auto ConvToVar(CompilerMetaData *meta) -> Unique<LocalVariableAst>;

  /// Whether this pattern takes a value out of what it is
  /// matched against, rather than only testing it. A name binds
  /// what it stands for unless it asks for it through a borrow,
  /// which leaves the value where it was; a literal, an
  /// expression, a skip and an "else" all only look. A
  /// destructure binds if any of its elements does, so one made
  /// only of skips is a shape test and takes nothing.
  SPP_ATTR_NODISCARD virtual auto BindsByMove() const -> bool;

protected:
  /// The "let" statement that case-of-patterns are converted
  /// to, introducing the variables created by the pattern.
  Unique<LetStatementInitializedAst> _MappedLet;
};
