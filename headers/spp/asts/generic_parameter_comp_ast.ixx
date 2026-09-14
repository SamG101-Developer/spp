module;
#include <spp/macros.hpp>

export module spp.asts.generic_parameter_comp_ast;
import spp.asts.generic_parameter_ast;
import spp.asts.utils.orderable;
import spp.codegen.llvm_ctx;
import spp.utils.types;
import llvm;
import std;

SPP_AST_COMMON_FWD_DECL(GenericParameterCompAst);
use(spp::asts, struct ExpressionAst);
use(spp::asts, struct TokenAst);
use(spp::asts, struct TypeAst);

namespace spp::asts::detail {
  template <>
  struct make_required_param<GenericParameterCompAst> {
    using type = GenericParameterCompAst;
  };

  template <>
  struct generic_param_value_type<GenericParameterCompAst> {
    using type = ExpressionAst const*;
  };
}

SPP_EXP_CLS struct spp::asts::GenericParameterCompAst : GenericParameterAst {
  /// The "cmp" token, marking this as a comp generic rather
  /// than a type generic.
  Unique<TokenAst> TokCmp;

  /// The ":" token separating the parameter name from its type.
  Unique<TokenAst> TokColon;

  /// The type of the parameter, such as "I32" or "F64". This is
  /// required, as the type must be known at compile time.
  Shared<TypeAst> Type;

  struct {
    Shared<TypeAst> OriginalType;
  } Source;

  GenericParameterCompAst(
    decltype(TokCmp) &&tok_cmp,
    decltype(Name) name,
    decltype(TokColon) &&tok_colon,
    decltype(Type) type,
    utils::OrderableTag order_tag);

  ~GenericParameterCompAst() override;

  auto Stage2_GenTopLvlScopes(ScopeManager *sm, CompilerMetaData *) -> void override;

  auto Stage4_QualifyTypes(ScopeManager *sm, CompilerMetaData *meta) -> void override;

  auto Stage7_AnalyseSemantics(ScopeManager *sm, CompilerMetaData *meta) -> void override;

  auto Stage9_CompTimeResolve(ScopeManager *sm, CompilerMetaData *meta) -> void override;

  auto Stage11_CodeGen(ScopeManager *sm, CompilerMetaData *meta, codegen::LlvmCtx *ctx) -> llvm::Value* override;
};
