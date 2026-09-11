module;
#include <spp/macros.hpp>

export module spp.asts.generic_argument_group_ast;
import spp.analyse.utils.type_compare;
import spp.asts.ast;
import spp.asts.ast_kind;
import spp.utils.ptr;
import spp.utils.types;
import std;

SPP_AST_COMMON_FWD_DECL(GenericArgumentGroupAst) {
  SPP_EXP_CLS struct ExpressionAst;
  SPP_EXP_CLS struct GenericArgumentAst;
  SPP_EXP_CLS struct GenericArgumentCompAst;
  SPP_EXP_CLS struct GenericArgumentCompKeywordAst;
  SPP_EXP_CLS struct GenericArgumentTypeAst;
  SPP_EXP_CLS struct GenericArgumentTypeKeywordAst;
  SPP_EXP_CLS struct GenericParameterGroupAst;
  SPP_EXP_CLS struct TokenAst;
}

SPP_EXP_CLS struct spp::asts::GenericArgumentGroupAst final : Ast {
  SPP_AST_KEY_FUNCTIONS(GenericArgumentGroupAst);

  /**
   * The token that represents the left bracket @code [@endcode in the generic argument group. This introduces the
   * generic argument group.
   */
  Unique<TokenAst> TokL;

  /**
   * The list of arguments in the generic argument group. This can contain both positional and keyword arguments.
   */
  Vec<Unique<GenericArgumentAst>> Args;

  /**
   * The token that represents the right parenthesis @code ]@endcode in the generic call argument group. This closes
   * the generic argument group.
   */
  Unique<TokenAst> TokR;

  static auto NewEmpty()
    -> Unique<GenericArgumentGroupAst>;

  static auto FromParams(
    GenericParameterGroupAst const &generic_params)
    -> Unique<GenericArgumentGroupAst>;

  static auto FromMap(
    analyse::utils::type_compare::GenericInferenceMap const &map)
    -> Unique<GenericArgumentGroupAst>;

  /**
   * Construct the GenericArgumentGroupAst with the arguments matching the members.
   * @param tok_l The token that represents the left bracket @code [@endcode in the generic argument group.
   * @param args The list of arguments in the generic argument group.
   * @param tok_r The token that represents the right parenthesis @code ]@endcode in the generic call argument group.
   */
  GenericArgumentGroupAst(
    decltype(TokL) &&tok_l,
    decltype(Args) &&args,
    decltype(TokR) &&tok_r);

  ~GenericArgumentGroupAst() override;

  auto operator==(GenericArgumentGroupAst const &other) const -> bool;

  auto operator+=(const GenericArgumentGroupAst &other) -> GenericArgumentGroupAst&;

  auto operator+(const GenericArgumentGroupAst &other) const -> Unique<GenericArgumentGroupAst>;

  auto Stage4_QualifyTypes(
    analyse::scopes::ScopeManager *sm,
    meta::CompilerMetaData *meta)
    -> void override;

  auto Stage7_AnalyseSemantics(
    analyse::scopes::ScopeManager *sm,
    meta::CompilerMetaData *meta)
    -> void override;

  auto Stage8_CheckMemory(
    analyse::scopes::ScopeManager *sm,
    meta::CompilerMetaData *meta)
    -> void override;

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
