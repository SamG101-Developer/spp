module;
#include <spp/macros.hpp>

export module spp.asts.case_pattern_variant_single_identifier_ast;
import spp.asts.ast_kind;
import spp.asts.case_pattern_variant_ast;
import spp.codegen.llvm_ctx;
import spp.utils.types;
import llvm;
import std;

SPP_AST_COMMON_FWD_DECL(CasePatternVariantSingleIdentifierAst);
use(spp::asts, struct ConventionAst);
use(spp::asts, struct IdentifierAst);
use(spp::asts, struct LocalVariableSingleIdentifierAliasAst);
use(spp::asts, struct LocalVariableAst);
use(spp::asts, struct TokenAst);

SPP_EXP_CLS struct spp::asts::CasePatternVariantSingleIdentifierAst final : CasePatternVariantAst {
  SPP_AST_KEY_FUNCTIONS(CasePatternVariantSingleIdentifierAst);

  /// The optional convention, indicating how the introduced
  /// variable is treated, such as by reference or by mutable
  /// reference. Mutually exclusive with the "mut" token (but
  /// both can be absent).
  Unique<ConventionAst> Conv;

  /// The optional "mut" token, making the introduced variable
  /// mutable. Without it, the variable is not mutable.
  Unique<TokenAst> TokMut;

  /// The identifier used to refer to the variable introduced
  /// by the pattern.
  Shared<IdentifierAst> Name;

  /// The optional alias. Matching happens against "name", but
  /// the introduced variable is named by the alias.
  Unique<LocalVariableSingleIdentifierAliasAst> Alias;

  CasePatternVariantSingleIdentifierAst(
    decltype(Conv) &&conv,
    decltype(TokMut) &&tok_mut,
    decltype(Name) &&name,
    decltype(Alias) &&alias);

  ~CasePatternVariantSingleIdentifierAst() override;

  SPP_ATTR_NODISCARD auto BindsByMove() const -> bool override;

  auto Stage7_AnalyseSemantics(ScopeManager *sm, CompilerMetaData *meta) -> void override;

  auto Stage8_CheckMemory(ScopeManager *sm, CompilerMetaData *meta) -> void override;

  auto Stage11_CodeGen(ScopeManager *sm, CompilerMetaData *meta, codegen::LlvmCtx *ctx) -> llvm::Value* override;

  auto ConvToVar(CompilerMetaData *meta) -> Unique<LocalVariableAst> override;
};
