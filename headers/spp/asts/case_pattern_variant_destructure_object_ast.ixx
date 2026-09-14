module;
#include <spp/macros.hpp>

export module spp.asts.case_pattern_variant_destructure_object_ast;
import spp.asts.ast;
import spp.asts.ast_kind;
import spp.asts.case_pattern_variant_ast;
import spp.codegen.llvm_ctx;
import spp.utils.types;
import llvm;
import std;

SPP_AST_COMMON_FWD_DECL(CasePatternVariantDestructureObjectAst);
use(spp::asts, struct LocalVariableAst);
use(spp::asts, struct TokenAst);
use(spp::asts, struct TypeAst);
use(spp::analyse::scopes, struct VariableSymbol);

SPP_EXP_CLS struct spp::asts::CasePatternVariantDestructureObjectAst final : CasePatternVariantAst {
  SPP_AST_KEY_FUNCTIONS(CasePatternVariantDestructureObjectAst);

  /// The type of the object being destructured, used to
  /// determine the type of the destructured elements (by
  /// attribute type inference).
  Shared<TypeAst> Type;

  /// The "(" token starting the object destructuring pattern.
  Unique<TokenAst> TokL;

  /// The patterns destructured from the object. Each element
  /// can be a single identifier, a nested destructuring
  /// pattern, or a literal.
  Vec<Unique<CasePatternVariantAst>> Elems;

  /// The ")" token ending the object destructuring pattern.
  Unique<TokenAst> TokR;

  struct {
    Shared<TypeAst> OriginalType;
  } Source;

  CasePatternVariantDestructureObjectAst(
    decltype(Type) type,
    decltype(TokL) &&tok_l,
    decltype(Elems) &&elems,
    decltype(TokR) &&tok_r);

  ~CasePatternVariantDestructureObjectAst() override;

  static auto FromType(Shared<TypeAst> const &type) -> Unique<CasePatternVariantDestructureObjectAst>;

  SPP_ATTR_NODISCARD auto BindsByMove() const -> bool override;

  auto Stage7_AnalyseSemantics(ScopeManager *sm, CompilerMetaData *meta) -> void override;

  auto Stage8_CheckMemory(ScopeManager *sm, CompilerMetaData *meta) -> void override;

  auto Stage9_CompTimeResolve(ScopeManager *sm, CompilerMetaData *meta) -> void override;

  auto Stage11_CodeGen(ScopeManager *sm, CompilerMetaData *meta, codegen::LlvmCtx *ctx) -> llvm::Value* override;

  auto ConvToVar(CompilerMetaData *meta) -> Unique<LocalVariableAst> override;

private:
  Shared<VariableSymbol> _CondSym;
  Shared<VariableSymbol> _FlowSym;
};
