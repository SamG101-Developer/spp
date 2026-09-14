module;
#include <spp/macros.hpp>

export module spp.asts.object_initializer_argument_shorthand_ast;
import spp.asts.ast_kind;
import spp.asts.object_initializer_argument_ast;
import spp.utils.types;
import std;

SPP_AST_COMMON_FWD_DECL(ObjectInitializerArgumentShorthandAst);
use(spp::asts, struct ExpressionAst);
use(spp::asts, struct TokenAst);

/// A shorthand argument in an object initializer. It forces
/// the argument to be matched by shorthand value rather than
/// a keyword.
SPP_EXP_CLS struct spp::asts::ObjectInitializerArgumentShorthandAst final : ObjectInitializerArgumentAst {
  SPP_AST_KEY_FUNCTIONS(ObjectInitializerArgumentShorthandAst);

  /// The optional ".." token indicating an "else" argument.
  /// This fills all the missing attributes in the object with
  /// the corresponding attributes from this argument.
  Unique<TokenAst> TokEllipsis;

  /// Create a shorthand argument with the provided expression,
  /// used for "autofill", ie the "..arg" argument: fill all
  /// missing fields from this object into the new initializer.
  static auto CreateAutoFillArg(Unique<ExpressionAst> &&val) -> Unique<ObjectInitializerArgumentShorthandAst>;

  explicit ObjectInitializerArgumentShorthandAst(
    decltype(TokEllipsis) &&tok_ellipsis,
    decltype(Val) &&val);

  ~ObjectInitializerArgumentShorthandAst() override;

  auto Stage7_AnalyseSemantics(ScopeManager *sm, CompilerMetaData *meta) -> void override;
};
