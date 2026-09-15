module;
#include <spp/macros.hpp>

export module spp.asts.annotation_ast;
import spp.asts.ast;
import spp.asts.ast_kind;
import spp.utils.types;
import std;

SPP_AST_COMMON_FWD_DECL(AnnotationAst);
use(spp::asts, struct ExpressionAst);
use(spp::asts, struct GenericArgumentGroupAst);
use(spp::asts, struct FunctionCallArgumentGroupAst);
use(spp::asts, struct FunctionPrototypeAst);
use(spp::asts, struct TokenAst);

/// An annotation is used to represent a behavioural transformation
/// inside an ast, such as marking a method as virtual or
/// a type as private, etc.
SPP_EXP_CLS struct spp::asts::AnnotationAst final : Ast {
  SPP_GCC_VTABLE_FIX;
  SPP_AST_KEY_FUNCTIONS(AnnotationAst);

  /// The ! token starting this annotation ast.
  Unique<TokenAst> TokExclamationMark;

  /// The name of the annotation (could be postfix static).
  Unique<ExpressionAst> Name;

  /// The generic arguments into the annotation: !extend[Copy]()
  Unique<GenericArgumentGroupAst> GnArgGroup;

  /// The function arguments into annotation: !ffi(symbol="...")
  Unique<FunctionCallArgumentGroupAst> FnArgGroup;

  AnnotationAst(
    decltype(TokExclamationMark) &&tok_exclamation_mark,
    decltype(Name) &&name,
    decltype(GnArgGroup) &&gn_arg_group,
    decltype(FnArgGroup) &&fn_arg_group);

  ~AnnotationAst() override;

  /// Do an expression ast equality against another annotations
  /// name, which will move through postfix identifiers and
  /// regular identifiers.
  auto operator==(AnnotationAst const &that) const -> bool;

  /// Run the default stage 1 steps to store the enclosing context
  /// on this ast.
  auto Stage1_PreProcess(Ast *ctx) -> void override;

  /// Run the default stage 2 steps to store the enclosing scope
  /// on this ast.
  auto Stage2_GenTopLvlScopes(ScopeManager *sm, CompilerMetaData *meta) -> void override;

  /// Ensure the target annotation definition is actually: a
  /// function, a "cmp" function, and !annotation bound. This
  /// is done is stage 4 because we rely on builtin annotations
  /// legitimately existing in stage 5.
  auto Stage4_QualifyTypes(ScopeManager *sm, CompilerMetaData *meta) -> void override;

  /// For builtin annotations, set fields on context asts
  /// based on the annotations, like virtual/abstract, the
  /// visibility etc, which are all checked in stage6+,
  /// maintaining the order agnostic behaviour.
  auto Stage5_LoadSupScopes(ScopeManager *sm, CompilerMetaData *meta) -> void override;

  /// Analyse the generic arguments and function arguments
  /// (if provided), and do overload resolution on the target,
  /// checking it exists (custom annotations).
  auto Stage7_AnalyseSemantics(ScopeManager *sm, CompilerMetaData *meta) -> void override;

  /// Do the final annotation compile time resolution -
  /// evaluate the target and check it is correct. Safe to do
  /// it like this, because properties for builtins silently
  /// fail (fine) for non-matched asts. Todo: For custom
  /// annotations, this will need changing.
  auto Stage9_CompTimeResolve(ScopeManager *sm, CompilerMetaData *meta) -> void override;

private:
  /// The target that this annotation "is" ie the function
  /// prototype defining it.
  FunctionPrototypeAst *_Target;
};

SPP_GCC_VTABLE_FIX_IMPL(spp::asts::AnnotationAst)
