module;
#include <spp/macros.hpp>

export module spp.asts.statement_ast;
import spp.asts.ast;
import spp.asts.mixins.type_inferrable_ast;
import spp.utils.types;
import std;

SPP_AST_COMMON_FWD_DECL(StatementAst);
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
  auto InferType(ScopeManager *sm, CompilerMetaData *meta) -> Shared<TypeAst> override;

  /// Test if the statement always terminates control flow with
  /// the "ret" instruction. For blocks, the final member is
  /// always inspected, recursively.
  SPP_ATTR_NODISCARD virtual auto Terminates() const -> bool;
};
