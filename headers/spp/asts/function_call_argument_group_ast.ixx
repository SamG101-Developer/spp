module;
#include <spp/macros.hpp>

export module spp.asts.function_call_argument_group_ast;
import spp.asts.ast;
import spp.asts.ast_kind;
import spp.utils.types;
import std;

SPP_AST_COMMON_FWD_DECL(FunctionCallArgumentGroupAst);
use(spp::asts, struct FunctionCallArgumentAst);
use(spp::asts, struct FunctionCallArgumentKeywordAst);
use(spp::asts, struct FunctionCallArgumentPositionalAst);
use(spp::asts, struct TokenAst);

/// A group of function call arguments, grouping multiple
/// positional or keyword arguments together in a function call.
SPP_EXP_CLS struct spp::asts::FunctionCallArgumentGroupAst final : Ast {
  SPP_AST_KEY_FUNCTIONS(FunctionCallArgumentGroupAst);

  /// The "(" token that opens the argument group.
  Unique<TokenAst> TokL;

  /// The arguments in the group, which can be both positional
  /// and keyword arguments.
  Vec<Unique<FunctionCallArgumentAst>> Args;

  /// The ")" token that closes the argument group.
  Unique<TokenAst> TokR;

  static auto NewEmpty() -> Unique<FunctionCallArgumentGroupAst>;

  FunctionCallArgumentGroupAst(
    decltype(TokL) &&tok_l,
    decltype(Args) &&args,
    decltype(TokR) &&tok_r);

  ~FunctionCallArgumentGroupAst() override;

  SPP_ATTR_NODISCARD auto GetAllArgs() const -> Vec<FunctionCallArgumentAst*>;

  SPP_ATTR_NODISCARD auto GetKeywordArgs() const -> Vec<FunctionCallArgumentKeywordAst*>;

  SPP_ATTR_NODISCARD auto GetPositionalArgs() const -> Vec<FunctionCallArgumentPositionalAst*>;

  auto Stage7_AnalyseSemantics(ScopeManager *sm, CompilerMetaData *meta) -> void override;

  auto Stage8_CheckMemory(ScopeManager *sm, CompilerMetaData *meta) -> void override;

  auto At(const char *key) const -> FunctionCallArgumentAst const*;

  SPP_ATTR_NODISCARD auto ConvertToPositional() const -> Unique<FunctionCallArgumentGroupAst>;

  SPP_ATTR_NODISCARD auto IsAllowedInDefault() const -> bool override;
};
