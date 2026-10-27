module;
#include <spp/macros.hpp>

export module spp.asts.function_parameter_group_ast;
import spp.asts.ast;
import spp.asts.ast_kind;
import spp.codegen.llvm_ctx;
import spp.utils.types;
import llvm;
import std;

SPP_AST_COMMON_FWD_DECL(FunctionParameterGroupAst);
use(spp::analyse::scopes, class Scope);
use(spp::asts, struct FunctionParameterAst);
use(spp::asts, struct FunctionParameterOptionalAst);
use(spp::asts, struct FunctionParameterRequiredAst);
use(spp::asts, struct FunctionParameterSelfAst);
use(spp::asts, struct FunctionParameterVariadicAst);
use(spp::asts, struct TokenAst);

/// A group of function parameters in a function prototype.
SPP_EXP_CLS struct spp::asts::FunctionParameterGroupAst final : Ast {
  SPP_AST_KEY_FUNCTIONS(FunctionParameterGroupAst);

  /// The "(" token that opens the parameter group.
  Unique<TokenAst> TokL;

  /// The parameters in the group. This can contain self,
  /// required, optional and variadic parameters.
  Vec<Unique<FunctionParameterAst>> Params;

  /// The ")" token that closes the parameter group.
  Unique<TokenAst> TokR;

  FunctionParameterGroupAst(
    decltype(TokL) &&tok_l,
    decltype(Params) &&params,
    decltype(TokR) &&tok_r);

  ~FunctionParameterGroupAst() override;

  auto Stage7_AnalyseSemantics(ScopeManager *sm, CompilerMetaData *meta) -> void override;

  auto Stage8_CheckMemory(ScopeManager *sm, CompilerMetaData *meta) -> void override;

  auto Stage11_CodeGen(ScopeManager *sm, CompilerMetaData *meta, codegen::LlvmCtx *ctx) -> llvm::Value* override;

  SPP_ATTR_NODISCARD auto GetAllParams() const -> Vec<FunctionParameterAst*>;

  SPP_ATTR_NODISCARD auto GetSelfParam() const -> FunctionParameterSelfAst*;

  SPP_ATTR_NODISCARD auto GetOptionalParams() const -> Vec<FunctionParameterOptionalAst*>;

  SPP_ATTR_NODISCARD auto GetVariadicParam() const -> FunctionParameterVariadicAst*;

  SPP_ATTR_NODISCARD auto GetNonSelfParams() const -> Vec<FunctionParameterAst*>;

  /// Whether "param"'s type is "Void" read in "scope": a value of
  /// type "Void" is no value, so a parameter of it is no parameter.
  SPP_ATTR_NODISCARD static auto IsVoidParam(FunctionParameterAst const &param, Scope const &scope) -> bool;

  /// Remove the parameters whose type is "Void" in "scope". Done by
  /// the prototype that owns this group, once its parameter types
  /// are known.
  auto RemoveVoidParams(Scope const &scope) -> void;
};
