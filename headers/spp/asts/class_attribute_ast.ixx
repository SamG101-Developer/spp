module;
#include <spp/macros.hpp>

export module spp.asts.class_attribute_ast;
import spp.asts.ast;
import spp.asts.ast_kind;
import spp.asts.class_member_ast;
import spp.asts.mixins.visibility_enabled_ast;
import spp.utils.types;
import std;

SPP_AST_COMMON_FWD_DECL(ClassAttributeAst);
use(spp::asts, struct AnnotationAst);
use(spp::asts, struct ExpressionAst);
use(spp::asts, struct IdentifierAst);
use(spp::asts, struct TokenAst);
use(spp::asts, struct TypeAst);

/// An attribute of a class. It is defined on the class
/// prototype ast, and is used to add "state" to a type.
SPP_EXP_CLS struct spp::asts::ClassAttributeAst final : Ast, ClassMemberAst, mixins::VisibilityAst {
  SPP_AST_KEY_FUNCTIONS(ClassAttributeAst);

  /// The annotations applied to this attribute. Typically,
  /// access modifiers in this context.
  Vec<Unique<AnnotationAst>> Annotations;

  /// The name used to refer to the attribute on the class,
  /// which must be unique to the class.
  Shared<IdentifierAst> Name;

  /// The ":" token separating the name from the type.
  Unique<TokenAst> TokColon;

  /// The type the attribute holds, which must be specified.
  Shared<TypeAst> Type;

  /// An optional default value, assigned to the attribute if
  /// no value is provided when creating an instance of the
  /// class. Otherwise, standard "default initialization" is
  /// used, which is the default value of the type.
  Unique<ExpressionAst> DefaultVal;

  struct {
    Shared<TypeAst> OriginalType;
  } Source;

  ClassAttributeAst(
    decltype(Annotations) &&annotations,
    decltype(Name) &&name,
    decltype(TokColon) &&tok_colon,
    decltype(Type) &&type,
    decltype(DefaultVal) &&default_val);

  ~ClassAttributeAst() override;

  auto Stage1_PreProcess(Ast *ctx) -> void override;

  auto Stage2_GenTopLvlScopes(ScopeManager *sm, CompilerMetaData *meta) -> void override;

  auto Stage4_ResolveDeclarations(ScopeManager *sm, CompilerMetaData *meta) -> void override;

  auto Stage5_LoadSupScopes(ScopeManager *sm, CompilerMetaData *meta) -> void override;

  auto Stage7_AnalyseSemantics(ScopeManager *sm, CompilerMetaData *meta) -> void override;

  auto Stage8_CheckMemory(ScopeManager *sm, CompilerMetaData *meta) -> void override;

  auto Stage9_CompTimeResolve(ScopeManager *sm, CompilerMetaData *meta) -> void override;
};
