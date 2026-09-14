module;
#include <spp/macros.hpp>

export module spp.asts.object_initializer_ast;
import spp.asts.ast_kind;
import spp.asts.primary_expression_ast;
import spp.codegen.llvm_ctx;
import spp.utils.types;
import llvm;
import std;

SPP_AST_COMMON_FWD_DECL(ObjectInitializerAst);
use(spp::asts, struct GenericArgumentAst);
use(spp::asts, struct ObjectInitializerArgumentGroupAst);
use(spp::asts, struct TypeAst);

SPP_EXP_CLS struct spp::asts::ObjectInitializerAst final : PrimaryExpressionAst {
  SPP_AST_KEY_FUNCTIONS(ObjectInitializerAst);

  /// The type of the object being created.
  Shared<TypeAst> Type;

  /// The argument group, whose arguments are passed into the
  /// attributes of the object being created.
  Unique<ObjectInitializerArgumentGroupAst> ArgGroup;

  struct {
    Shared<TypeAst> OriginalType;
  } Source;

  ObjectInitializerAst(
    decltype(Type) type,
    decltype(ArgGroup) &&arg_group);

  ~ObjectInitializerAst() override;

  auto Stage7_AnalyseSemantics(ScopeManager *sm, CompilerMetaData *meta) -> void override;

  auto Stage8_CheckMemory(ScopeManager *sm, CompilerMetaData *meta) -> void override;

  auto Stage9_CompTimeResolve(ScopeManager *sm, CompilerMetaData *meta) -> void override;

  auto Stage11_CodeGen(ScopeManager *sm, CompilerMetaData *meta, codegen::LlvmCtx *ctx) -> llvm::Value* override;

  auto InferType(ScopeManager *sm, CompilerMetaData *meta) -> Shared<TypeAst> override;

  auto InferTypeForDisplay(ScopeManager *sm, CompilerMetaData *meta) -> Shared<TypeAst> override;

  SPP_ATTR_NODISCARD auto SubstituteGenericsExpr(
    Vec<GenericArgumentAst*> const &args) const
    -> Shared<ExpressionAst> override;

  SPP_ATTR_NODISCARD auto IsAllowedInDefault() const -> bool override;
};
