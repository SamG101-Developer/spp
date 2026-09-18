module;
#include <spp/macros.hpp>

export module spp.asts.local_variable_destructure_tuple_ast;
import spp.asts.ast_kind;
import spp.asts.local_variable_ast;
import spp.codegen.llvm_ctx;
import spp.utils.types;
import llvm;
import std;

SPP_AST_COMMON_FWD_DECL(LocalVariableDestructureTupleAst);
use(spp::asts, struct CasePatternVariantDestructureTupleAst);
use(spp::asts, struct LetStatementInitializedAst);
use(spp::asts, struct IdentifierAst);
use(spp::asts, struct TokenAst);

SPP_EXP_CLS struct spp::asts::LocalVariableDestructureTupleAst final : LocalVariableAst {
  SPP_AST_KEY_FUNCTIONS(LocalVariableDestructureTupleAst);

  /// The "(" token starting the tuple destructuring pattern.
  Unique<TokenAst> TokL;

  /// The patterns destructured from the tuple. Each element
  /// can be a single identifier, a nested destructuring
  /// pattern, or a literal.
  Vec<Unique<LocalVariableAst>> Elems;

  /// The ")" token ending the tuple destructuring pattern.
  Unique<TokenAst> TokR;

  LocalVariableDestructureTupleAst(
    decltype(TokL) &&tok_l,
    decltype(Elems) &&elems,
    decltype(TokR) &&tok_r);

  ~LocalVariableDestructureTupleAst() override;

  SPP_ATTR_NODISCARD auto BindsByMove() const -> bool override;

  auto Stage7_AnalyseSemantics(ScopeManager *sm, CompilerMetaData *meta) -> void override;

  auto Stage8_CheckMemory(ScopeManager *sm, CompilerMetaData *meta) -> void override;

  auto Stage9_CompTimeResolve(ScopeManager *sm, CompilerMetaData *meta) -> void override;

  auto Stage11_CodeGen(ScopeManager *sm, CompilerMetaData *meta, codegen::LlvmCtx *ctx) -> llvm::Value* override;

  SPP_ATTR_NODISCARD auto ExtractNames() const -> Vec<Shared<IdentifierAst>> override;

  SPP_ATTR_NODISCARD auto ExtractName() const -> Shared<IdentifierAst> override;

private:
  Vec<Unique<LetStatementInitializedAst>> _NewAsts;
  Shared<IdentifierAst> _TmpName;
};
