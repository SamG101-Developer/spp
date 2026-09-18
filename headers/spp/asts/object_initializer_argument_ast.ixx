module;
#include <spp/macros.hpp>

export module spp.asts.object_initializer_argument_ast;
import spp.asts.ast;
import spp.asts.mixins.type_inferrable_ast;
import spp.utils.types;
import std;

SPP_AST_COMMON_FWD_DECL(ObjectInitializerArgumentAst);
use(spp::asts, struct ExpressionAst);
use(spp::asts, struct IdentifierAst);
use(spp::analyse::scopes, struct TypeRef);
use(spp::asts, struct TypeAst);

/// The base class for an argument in an object
/// initialization, inherited into the "shorthand" and
/// "keyword" variants.
SPP_EXP_CLS struct spp::asts::ObjectInitializerArgumentAst : Ast, mixins::TypeInferrableAst {
  /// The name of the argument. For shorthand args, this is
  /// autofilled by cloning the value and casting it to an
  /// IdentifierAst. Otherwise, it is passed explicitly from
  /// the keyword arg parser.
  Shared<IdentifierAst> Name;

  /// The expression passed as the argument to the object
  /// initialization. Both shorthand and keyword arguments have
  /// a value.
  Unique<ExpressionAst> Val;

  bool IsCompilerGenerated = false;

  explicit ObjectInitializerArgumentAst(
    decltype(Name) name,
    decltype(Val) &&val);

  ~ObjectInitializerArgumentAst() override;

  auto Stage7_AnalyseSemantics(ScopeManager *sm, CompilerMetaData *meta) -> void override;

  auto Stage8_CheckMemory(ScopeManager *sm, CompilerMetaData *meta) -> void override;

  auto Stage9_CompTimeResolve(ScopeManager *sm, CompilerMetaData *meta) -> void override;

  auto InferType(ScopeManager *sm, CompilerMetaData *meta) -> Shared<TypeAst> override;

  auto InferTypeRef(ScopeManager *sm, CompilerMetaData *meta) -> TypeRef override;

  SPP_ATTR_NODISCARD auto IsAllowedInDefault() const -> bool override;
};
