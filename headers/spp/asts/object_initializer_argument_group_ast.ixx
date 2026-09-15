module;
#include <spp/macros.hpp>

export module spp.asts.object_initializer_argument_group_ast;
import spp.asts.ast;
import spp.asts.ast_kind;
import spp.utils.types;
import std;

SPP_AST_COMMON_FWD_DECL(ObjectInitializerArgumentGroupAst);
use(spp::asts, struct ObjectInitializerArgumentAst);
use(spp::asts, struct ObjectInitializerArgumentKeywordAst);
use(spp::asts, struct ObjectInitializerArgumentShorthandAst);
use(spp::asts, struct TokenAst);

/// A group of shorthand or keyword arguments in an object
/// initializer.
SPP_EXP_CLS struct spp::asts::ObjectInitializerArgumentGroupAst final : Ast {
  SPP_AST_KEY_FUNCTIONS(ObjectInitializerArgumentGroupAst);

  /// The "(" token opening the argument group.
  Unique<TokenAst> TokL;

  /// The arguments in the group, which can be both shorthand
  /// and keyword arguments.
  Vec<Unique<ObjectInitializerArgumentAst>> Args;

  /// The ")" token closing the argument group.
  Unique<TokenAst> TokR;

  static auto NewEmpty() -> Unique<ObjectInitializerArgumentGroupAst>;

  ObjectInitializerArgumentGroupAst(
    decltype(TokL) &&tok_l,
    decltype(Args) &&args,
    decltype(TokR) &&tok_r);

  ~ObjectInitializerArgumentGroupAst() override;

  auto Stage6_PreAnalyseSemantics(ScopeManager *sm, CompilerMetaData *meta) -> void override;

  auto Stage7_AnalyseSemantics(ScopeManager *sm, CompilerMetaData *meta) -> void override;

  auto Stage8_CheckMemory(ScopeManager *sm, CompilerMetaData *meta) -> void override;

  auto GetAllArgs() -> Vec<ObjectInitializerArgumentAst*>;

  auto GetAutoFillArg() -> ObjectInitializerArgumentShorthandAst*;

  auto GetNonAutoFillArgs() -> Vec<ObjectInitializerArgumentAst*>;

  auto GetShorthandArgs() -> Vec<ObjectInitializerArgumentShorthandAst*>;

  auto GetKeywordArgs() -> Vec<ObjectInitializerArgumentKeywordAst*>;

  SPP_ATTR_NODISCARD auto IsAllowedInDefault() const -> bool override;
};
