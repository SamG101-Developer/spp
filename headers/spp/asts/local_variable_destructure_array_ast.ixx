module;
#include <spp/macros.hpp>

export module spp.asts.local_variable_destructure_array_ast;
import spp.asts.ast_kind;
import spp.asts.local_variable_ast;
import spp.codegen.llvm_ctx;
import spp.utils.types;
import llvm;
import std;

SPP_AST_COMMON_FWD_DECL(LocalVariableDestructureArrayAst);
use(spp::asts, struct CasePatternVariantDestructureArrayAst);
use(spp::asts, struct LetStatementInitializedAst);
use(spp::asts, struct TokenAst);
use(spp::asts, struct IdentifierAst);

SPP_EXP_CLS struct spp::asts::LocalVariableDestructureArrayAst final : LocalVariableAst {
  SPP_AST_KEY_FUNCTIONS(LocalVariableDestructureArrayAst);

  /// The "[" token that starts the array destructure.
  Unique<TokenAst> TokL;

  /// The patterns destructured from the array. Each element
  /// can be a single identifier, a nested destructure, or a
  /// literal.
  Vec<Unique<LocalVariableAst>> Elems;

  /// The "]" token that ends the array destructure.
  Unique<TokenAst> TokR;

  LocalVariableDestructureArrayAst(
    decltype(TokL) &&tok_l,
    decltype(Elems) &&elems,
    decltype(TokR) &&tok_r);

  ~LocalVariableDestructureArrayAst() override;

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
