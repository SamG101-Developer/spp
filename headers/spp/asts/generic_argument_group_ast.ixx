module;
#include <spp/macros.hpp>

export module spp.asts.generic_argument_group_ast;
import spp.analyse.utils.type_compare;
import spp.asts.ast;
import spp.asts.ast_kind;
import spp.utils.ptr;
import spp.utils.types;
import std;

SPP_AST_COMMON_FWD_DECL(GenericArgumentGroupAst);
use(spp::asts, struct ExpressionAst);
use(spp::asts, struct GenericArgumentAst);
use(spp::asts, struct GenericArgumentCompAst);
use(spp::asts, struct GenericArgumentCompKeywordAst);
use(spp::asts, struct GenericArgumentTypeAst);
use(spp::asts, struct GenericArgumentTypeKeywordAst);
use(spp::asts, struct GenericParameterGroupAst);
use(spp::asts, struct TokenAst);

SPP_EXP_CLS struct spp::asts::GenericArgumentGroupAst final : Ast {
  SPP_AST_KEY_FUNCTIONS(GenericArgumentGroupAst);

  /// The "[" token that opens the generic argument group.
  Unique<TokenAst> TokL;

  /// The arguments in the group. This can contain both
  /// positional and keyword arguments.
  Vec<Unique<GenericArgumentAst>> Args;

  /// The "]" token that closes the generic argument group.
  Unique<TokenAst> TokR;

  static auto NewEmpty() -> Unique<GenericArgumentGroupAst>;

  static auto FromParams(GenericParameterGroupAst const &generic_params) -> Unique<GenericArgumentGroupAst>;

  static auto FromMap(analyse::utils::type_compare::GenericInferenceMap const &map) -> Unique<GenericArgumentGroupAst>;

  GenericArgumentGroupAst(
    decltype(TokL) &&tok_l,
    decltype(Args) &&args,
    decltype(TokR) &&tok_r);

  ~GenericArgumentGroupAst() override;

  auto operator==(GenericArgumentGroupAst const &other) const -> bool;

  auto operator+=(const GenericArgumentGroupAst &other) -> GenericArgumentGroupAst&;

  auto operator+(const GenericArgumentGroupAst &other) const -> Unique<GenericArgumentGroupAst>;

  auto Stage4_QualifyTypes(ScopeManager *sm, CompilerMetaData *meta) -> void override;

  auto Stage7_AnalyseSemantics(ScopeManager *sm, CompilerMetaData *meta) -> void override;

  auto Stage8_CheckMemory(ScopeManager *sm, CompilerMetaData *meta) -> void override;

  auto TypeAt(const char *key) const -> GenericArgumentTypeAst const*;

  auto CompAt(const char *key) const -> GenericArgumentCompAst const*;

  auto MergeGenerics(decltype(Args) &&other_args) -> void;

  SPP_ATTR_NODISCARD auto GetTypeArgs() const -> Vec<GenericArgumentTypeAst*>;

  SPP_ATTR_NODISCARD auto GetCompArgs() const -> Vec<GenericArgumentCompAst*>;

  SPP_ATTR_NODISCARD auto GetKeywordArgs() const -> Vec<GenericArgumentAst*>;

  SPP_ATTR_NODISCARD auto GetPositionalArgs() const -> Vec<GenericArgumentAst*>;

  SPP_ATTR_NODISCARD auto GetTypeKeywordArgs() const -> Vec<GenericArgumentTypeKeywordAst*>;

  SPP_ATTR_NODISCARD auto GetCompKeywordArgs() const -> Vec<GenericArgumentCompKeywordAst*>;

  SPP_ATTR_NODISCARD auto GetAllArgs() const -> Vec<GenericArgumentAst*>;
};
