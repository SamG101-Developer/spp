module;
#include <spp/macros.hpp>

export module spp.asts.statement_ast;
import spp.asts.ast;
import spp.asts.mixins.type_inferrable_ast;
import spp.utils.types;
import std;

SPP_AST_COMMON_FWD_DECL(StatementAst);
use(spp::analyse::scopes, struct TypeRef);
use(spp::asts, struct TypeAst);

/// The base class for all statements. It represents asts that
/// do not return a value, such as variable declarations and
/// control flow statements.
SPP_EXP_CLS struct spp::asts::StatementAst : Ast, mixins::TypeInferrableAst {
  StatementAst();

  ~StatementAst() override;

  /// All statements are inferred as the Void type, so the
  /// method is implemented here, rather than on every
  /// statement ast.
  auto InferTypeRef(
    ScopeManager *sm, CompilerMetaData *meta) -> TypeRef override;

  /// Whether control never continues past this statement: it is
  /// written to leave ("ret", "exit", "skip"), it is a block
  /// whose last statement diverges, a "case" nothing falls
  /// through and every reachable branch of which diverges, a
  /// "let" whose value diverges, or an expression typed "!" (a
  /// call to "abort", a loop with no way out). The one
  /// definition: everything asking this reads it. The statement
  /// must be analysed. Any other statement continues.
  SPP_ATTR_NODISCARD virtual auto Diverges(ScopeManager *sm, CompilerMetaData *meta) -> bool;
};
