module;
#include <spp/macros.hpp>

export module spp.asts.object_initializer_ast;
import spp.asts.ast_kind;
import spp.asts.primary_expression_ast;
import spp.codegen.llvm_ctx;
import spp.utils.types;
import llvm;
import std;

SPP_AST_COMMON_FWD_DECL(ObjectInitializerAst) {
  SPP_EXP_CLS struct GenericArgumentAst;
  SPP_EXP_CLS struct ObjectInitializerArgumentGroupAst;
  SPP_EXP_CLS struct TypeAst;
}

SPP_EXP_CLS struct spp::asts::ObjectInitializerAst final : PrimaryExpressionAst {
  SPP_AST_KEY_FUNCTIONS(ObjectInitializerAst);

  /**
   * The type being initialized by the object initializer. This is the type of the object being created.
   */
  Shared<TypeAst> Type;

  /**
   * The object initializer argument group that contains the arguments for the object initializer. These arguments
   * will be passed into the attributes of the object being created.
   */
  Unique<ObjectInitializerArgumentGroupAst> ArgGroup;

  struct {
    Shared<TypeAst> OriginalType;
  } Source;

  /**
   * Construct the ObjectInitializerAst with the arguments matching the members.
   * @param type The type being initialized by the object initializer.
   * @param arg_group The object initializer argument group that contains the arguments for the object initializer.
   */
  ObjectInitializerAst(
    decltype(Type) type,
    decltype(ArgGroup) &&arg_group);

  ~ObjectInitializerAst() override;

  auto Stage7_AnalyseSemantics(
    analyse::scopes::ScopeManager *sm,
    meta::CompilerMetaData *meta)
    -> void override;

  auto Stage8_CheckMemory(
    analyse::scopes::ScopeManager *sm,
    meta::CompilerMetaData *meta)
    -> void override;

  auto Stage9_CompTimeResolve(
    analyse::scopes::ScopeManager *sm,
    meta::CompilerMetaData *meta)
    -> void override;

  auto Stage11_CodeGen(
    analyse::scopes::ScopeManager *sm,
    meta::CompilerMetaData *meta,
    codegen::LlvmCtx *ctx)
    -> llvm::Value* override;

  auto InferType(
    analyse::scopes::ScopeManager *sm,
    meta::CompilerMetaData *meta)
    -> Shared<TypeAst> override;

  auto InferTypeForDisplay(analyse::scopes::ScopeManager *sm, meta::CompilerMetaData *meta) -> Shared<TypeAst> override;

  SPP_ATTR_NODISCARD auto SubstituteGenericsExpr(
    Vec<GenericArgumentAst*> const &args) const
    -> Shared<ExpressionAst> override;

  SPP_ATTR_NODISCARD auto IsAllowedInDefault() const
    -> bool override;
};
