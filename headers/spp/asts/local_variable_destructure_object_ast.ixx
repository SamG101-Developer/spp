module;
#include <spp/macros.hpp>

export module spp.asts.local_variable_destructure_object_ast;
import spp.asts.ast_kind;
import spp.asts.local_variable_ast;
import spp.codegen.llvm_ctx;
import spp.utils.types;
import llvm;
import std;

SPP_AST_COMMON_FWD_DECL(LocalVariableDestructureObjectAst);
use(spp::analyse::scopes, struct VariableSymbol);
use(spp::asts, struct CasePatternVariantDestructureObjectAst);
use(spp::asts, struct IdentifierAst);
use(spp::asts, struct LetStatementInitializedAst);
use(spp::asts, struct TokenAst);
use(spp::asts, struct TypeAst);

SPP_EXP_CLS struct spp::asts::LocalVariableDestructureObjectAst final : LocalVariableAst {
  SPP_AST_KEY_FUNCTIONS(LocalVariableDestructureObjectAst);

  /// The type of the object being destructured, used to
  /// infer the types of the destructured elements from the
  /// attribute types.
  Shared<TypeAst> Type;

  /// The "(" token that starts the object destructure.
  Unique<TokenAst> TokL;

  /// The patterns destructured from the object. Each element
  /// can be a single identifier, a nested destructure, or a
  /// literal.
  Vec<Unique<LocalVariableAst>> Elems;

  /// The ")" token that ends the object destructure.
  Unique<TokenAst> TokR;

  LocalVariableDestructureObjectAst(
    decltype(Type) &&type,
    decltype(TokL) &&tok_l,
    decltype(Elems) &&elems,
    decltype(TokR) &&tok_r);

  ~LocalVariableDestructureObjectAst() override;

  SPP_ATTR_NODISCARD auto BindsByMove() const -> bool override;

  auto Stage7_AnalyseSemantics(ScopeManager *sm, CompilerMetaData *meta) -> void override;

  auto Stage8_CheckMemory(ScopeManager *sm, CompilerMetaData *meta) -> void override;

  auto Stage9_CompTimeResolve(ScopeManager *sm, CompilerMetaData *meta) -> void override;

  auto Stage11_CodeGen(ScopeManager *sm, CompilerMetaData *meta, codegen::LlvmCtx *ctx) -> llvm::Value* override;

  SPP_ATTR_NODISCARD auto ExtractNames() const -> Vec<Shared<IdentifierAst>> override;

  SPP_ATTR_NODISCARD auto ExtractName() const -> Shared<IdentifierAst> override;

private:
  Vec<Unique<LetStatementInitializedAst>> _NewAsts;
  Shared<VariableSymbol> _CondSym;
  Shared<VariableSymbol> _FlowSym;
  Unique<LetStatementInitializedAst> _CondLet;
  Shared<IdentifierAst> _TmpName;
};
