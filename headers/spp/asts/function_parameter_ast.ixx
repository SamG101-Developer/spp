module;
#include <spp/macros.hpp>

export module spp.asts.function_parameter_ast;
import spp.asts.ast;
import spp.asts.mixins.orderable_ast;
import spp.asts.utils.orderable;
import spp.codegen.llvm_ctx;
import spp.utils.types;
import llvm;
import std;

SPP_AST_COMMON_FWD_DECL(FunctionParameterAst);
use(spp::asts, struct ExpressionAst);
use(spp::asts, struct IdentifierAst);
use(spp::asts, struct LocalVariableAst);
use(spp::asts, struct TokenAst);
use(spp::asts, struct TypeAst);

/// The common base of all parameter types in a function
/// prototype. It is inherited by the required, optional,
/// variadic and self parameters, and provides the functionality
/// common to all of them.
SPP_EXP_CLS struct spp::asts::FunctionParameterAst : Ast, mixins::OrderableAst {
  /// The local variable declaration for this parameter, which
  /// uses the same syntax as variables, such as destructuring.
  Unique<LocalVariableAst> Var;

  /// The ":" token, separating the parameter name from the type.
  Unique<TokenAst> TokColon;

  /// The type of the parameter, such as "I32" or "F64". It is
  /// required, as the type must be known at compile time.
  Shared<TypeAst> Type;

  struct {
    Shared<TypeAst> OriginalType;
    Unique<ExpressionAst> OriginalDefaultVal;
  } Source;

  FunctionParameterAst(
    decltype(Var) &&var,
    decltype(TokColon) &&tok_colon,
    decltype(Type) type,
    utils::OrderableTag order_tag);

  ~FunctionParameterAst() override;

  auto Stage7_AnalyseSemantics(ScopeManager *sm, CompilerMetaData *meta) -> void override;

  auto Stage8_CheckMemory(ScopeManager *sm, CompilerMetaData *meta) -> void override;

  auto Stage11_CodeGen(ScopeManager *sm, CompilerMetaData *meta, codegen::LlvmCtx *ctx) -> llvm::Value* override;

  SPP_ATTR_NODISCARD auto ExtractNames() const -> Vec<Shared<IdentifierAst>>;

  SPP_ATTR_NODISCARD auto ExtractName() const -> Shared<IdentifierAst>;
};
