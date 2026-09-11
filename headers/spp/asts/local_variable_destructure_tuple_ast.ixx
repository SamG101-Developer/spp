module;
#include <spp/macros.hpp>

export module spp.asts.local_variable_destructure_tuple_ast;
import spp.asts.ast_kind;
import spp.asts.local_variable_ast;
import spp.codegen.llvm_ctx;
import spp.utils.types;
import llvm;
import std;

SPP_AST_COMMON_FWD_DECL(LocalVariableDestructureTupleAst) {
  SPP_EXP_CLS struct CasePatternVariantDestructureTupleAst;
  SPP_EXP_CLS struct LetStatementInitializedAst;
  SPP_EXP_CLS struct IdentifierAst;
  SPP_EXP_CLS struct TokenAst;
}

SPP_EXP_CLS struct spp::asts::LocalVariableDestructureTupleAst final : LocalVariableAst {
  SPP_AST_KEY_FUNCTIONS(LocalVariableDestructureTupleAst);

  /**
   * The @code (@endcode token that indicates the start of a tuple destructuring pattern.
   */
  Unique<TokenAst> TokL;

  /**
   * The elements of the tuple destructuring pattern. This is a list of patterns that will be destructured from the
   * tuple. Each element can be a single identifier, a nested destructuring pattern, or a literal.
   */
  Vec<Unique<LocalVariableAst>> Elems;

  /**
   * The @code )@endcode token that indicates the end of an tuple destructuring pattern.
   */
  Unique<TokenAst> TokR;

  /**
   * Construct the LocalVariableDestructureTupleAst with the arguments matching the members.
   * @param[in] tok_l The @code (@endcode token that indicates the start of a tuple destructuring pattern.
   * @param[in] elems The elements of the tuple destructuring pattern.
   * @param[in] tok_r The @code )@endcode token that indicates the end of a tuple destructuring pattern.
   */
  LocalVariableDestructureTupleAst(
    decltype(TokL) &&tok_l,
    decltype(Elems) &&elems,
    decltype(TokR) &&tok_r);

  ~LocalVariableDestructureTupleAst() override;

  SPP_ATTR_NODISCARD auto BindsByMove() const -> bool override;

  auto Stage7_AnalyseSemantics(
    analyse::scopes::ScopeManager *sm,
    meta::CompilerMetaData *meta)
    -> void override;

  auto Stage8_CheckMemory(
    analyse::scopes::ScopeManager *sm,
    meta::CompilerMetaData *meta)
    -> void override;

  auto Stage9_CompTimeResolve(
    analyse::scopes::ScopeManager *sm,
    meta::CompilerMetaData *meta)
    -> void override;

  auto Stage11_CodeGen(
    analyse::scopes::ScopeManager *sm,
    meta::CompilerMetaData *meta,
    codegen::LlvmCtx *ctx)
    -> llvm::Value* override;

  SPP_ATTR_NODISCARD auto ExtractNames() const -> Vec<Shared<IdentifierAst>> override;

  SPP_ATTR_NODISCARD auto ExtractName() const -> Shared<IdentifierAst> override;

private:
  Vec<Unique<LetStatementInitializedAst>> _NewAsts;
  Shared<IdentifierAst> _TmpName;
};
