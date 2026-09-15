module;
#include <spp/macros.hpp>

export module spp.asts.generic_parameter_group_ast;
import spp.asts.ast;
import spp.asts.ast_kind;
import spp.codegen.llvm_ctx;
import spp.utils.types;
import llvm;
import std;

SPP_AST_COMMON_FWD_DECL(GenericParameterGroupAst);
use(spp::asts, struct GenericParameterAst);
use(spp::asts, struct GenericParameterCompAst);
use(spp::asts, struct GenericParameterTypeAst);
use(spp::asts, struct TokenAst);

SPP_EXP_CLS struct spp::asts::GenericParameterGroupAst final : Ast {
  SPP_AST_KEY_FUNCTIONS(GenericParameterGroupAst);

  /// The "[" token that opens the generic parameter group.
  Unique<TokenAst> TokL;

  /// The parameters in the group. This can contain both
  /// required and optional parameters.
  Vec<Unique<GenericParameterAst>> Params;

  /// The "]" token that closes the generic parameter group.
  Unique<TokenAst> TokR;

  static auto NewEmpty() -> Unique<GenericParameterGroupAst>;

  static auto NewEmptyShared() -> Shared<GenericParameterGroupAst>;

  GenericParameterGroupAst(
    decltype(TokL) &&tok_l,
    decltype(Params) &&params,
    decltype(TokR) &&tok_r);

  ~GenericParameterGroupAst() override;

  auto operator+(GenericParameterGroupAst const &other) const -> Unique<GenericParameterGroupAst>;

  auto operator+=(GenericParameterGroupAst const &other) -> GenericParameterGroupAst&;

  auto Stage2_GenTopLvlScopes(ScopeManager *sm, CompilerMetaData *meta) -> void override;

  auto Stage4_QualifyTypes(ScopeManager *sm, CompilerMetaData *meta) -> void override;

  auto Stage7_AnalyseSemantics(ScopeManager *sm, CompilerMetaData *meta) -> void override;

  auto Stage8_CheckMemory(ScopeManager *sm, CompilerMetaData *meta) -> void override;

  auto Stage9_CompTimeResolve(ScopeManager *sm, CompilerMetaData *meta) -> void override;

  auto Stage11_CodeGen(ScopeManager *sm, CompilerMetaData *meta, codegen::LlvmCtx *ctx) -> llvm::Value* override;

  auto MergeGenerics(decltype(Params) &&other_params) -> void;

  SPP_ATTR_NODISCARD auto GetRequiredParams() const -> Vec<GenericParameterAst*>;

  SPP_ATTR_NODISCARD auto GetOptionalParams() const -> Vec<GenericParameterAst*>;

  SPP_ATTR_NODISCARD auto GetVariadicParams() const -> GenericParameterAst*;

  SPP_ATTR_NODISCARD auto GetCompParams() const -> Vec<GenericParameterCompAst*>;

  SPP_ATTR_NODISCARD auto GetTypeParams() const -> Vec<GenericParameterTypeAst*>;

  SPP_ATTR_NODISCARD auto GetAllParams() const -> Vec<GenericParameterAst*>;

  SPP_ATTR_NODISCARD auto OptToReq() const -> Unique<GenericParameterGroupAst>;
};
