module;
#include <spp/macros.hpp>

export module spp.asts.case_pattern_variant_ast;
import spp.asts.ast;
import spp.utils.types;
import std;

SPP_AST_COMMON_FWD_DECL(CasePatternVariantAst) {
  SPP_EXP_CLS struct LetStatementInitializedAst;
  SPP_EXP_CLS struct LocalVariableAst;
}

/**
 * Base class for all different case pattern variants. This is used to store different variants in the same list, and to
 * provide the conversion binding for creating variables defined in patterns.
 */
SPP_EXP_CLS struct spp::asts::CasePatternVariantAst : Ast {
  CasePatternVariantAst();

  /**
   * Handle pattern matching for case expressions (this will be reimplemented in all the other patterns, but due to a
   * GCC modules bug, it must be defined here too).
   * @param sm The scope manager to use for comptime generation.
   * @param meta Associated metadata.
   */
  auto Stage9_CompTimeResolve(
    analyse::scopes::ScopeManager *sm,
    meta::CompilerMetaData *meta)
    -> void override;

  /**
   * Case patterns can introduce variables via the bindings. To neatly introduce all required bindings into scope,
   * there is a conversion mechanism, which can also handle the nested bindings. This is overridden on the different
   * destructuring patterns.
   * @param meta Associated metadata.
   * @return The converted variable AST.
   */
  virtual auto ConvToVar(
    meta::CompilerMetaData *meta)
    -> Unique<LocalVariableAst>;

  /**
   * Whether this pattern takes a value out of what it is matched against, rather than only testing it. A name binds
   * what it stands for unless it asks for it through a borrow, which leaves the value where it was; a literal, an
   * expression, a skip and an "else" all only look. A destructure binds if any of its elements does, so one made only
   * of skips is a shape test and takes nothing.
   * @return Whether it takes anything.
   */
  SPP_ATTR_NODISCARD virtual auto BindsByMove() const
    -> bool;

protected:
  /**
   * The @c let statement that case-of-patterns are converted to, to introduce the variables created by the pattern.
   */
  Unique<LetStatementInitializedAst> _MappedLet;
};
