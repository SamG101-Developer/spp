module;
#include <spp/macros.hpp>

export module spp.asts.type_unary_expression_ast;
import spp.asts.ast_kind;
import spp.asts.type_ast;
import spp.codegen.llvm_ctx;
import spp.utils.types;
import llvm;
import std;

SPP_AST_COMMON_FWD_DECL(TypeUnaryExpressionAst);
use(spp::asts, struct ConventionAst);
use(spp::asts, struct GenericArgumentAst);
use(spp::asts, struct GenericArgumentGroupAst);
use(spp::asts, struct GenericParameterAst);
use(spp::asts, struct IdentifierAst);
use(spp::asts, struct TypeAst);
use(spp::asts, struct TypeUnaryExpressionOperatorAst);
use(spp::asts, struct TypeIdentifierAst);

SPP_EXP_CLS struct spp::asts::TypeUnaryExpressionAst final : TypeAst {
  SPP_GCC_VTABLE_FIX;
  SPP_AST_KEY_FUNCTIONS(TypeUnaryExpressionAst);

  /// The unary operator applied to the type.
  Shared<TypeUnaryExpressionOperatorAst> Op;

  /// The type being operated on by the unary operator.
  Shared<TypeAst> Rhs;

  TypeUnaryExpressionAst(
    decltype(Op) op,
    decltype(Rhs) rhs);

  ~TypeUnaryExpressionAst() override;

  auto operator<=>(TypeUnaryExpressionAst const &other) const -> Ordering;
  auto operator==(TypeUnaryExpressionAst const &other) const -> bool;

  SPP_ATTR_NODISCARD auto EqualsTypeUnaryExpression(TypeUnaryExpressionAst const &other) const -> Ordering override;

  SPP_ATTR_NODISCARD auto Equals(ExpressionAst const &other) const -> Ordering override;

  auto Stage4_QualifyTypes(ScopeManager *sm, CompilerMetaData *meta) -> void override;

  auto Stage7_AnalyseSemantics(ScopeManager *sm, CompilerMetaData *meta) -> void override;

  auto Stage11_CodeGen(ScopeManager *sm, CompilerMetaData *meta, codegen::LlvmCtx *ctx) -> llvm::Value* override;

  auto InferType(ScopeManager *sm, CompilerMetaData *meta) -> Shared<TypeAst> override;

  SPP_ATTR_NODISCARD auto AnyPart(std::function<bool(TypeIdentifierAst const &)> const &pred) const -> bool override;

  SPP_ATTR_NODISCARD auto IsNeverType() const noexcept -> bool override;

  SPP_ATTR_NODISCARD auto IsSelfType() const noexcept -> bool override;

  auto NsPartsInto(Vec<IdentifierAst const*> &out) const -> void override;

  auto TypePartsInto(Vec<TypeIdentifierAst const*> &out) const -> void override;

  SPP_ATTR_NODISCARD auto NsParts() const -> Vec<IdentifierAst const*> override;

  SPP_ATTR_NODISCARD auto NsParts() -> Vec<IdentifierAst*> override;

  SPP_ATTR_NODISCARD auto TypeParts() const -> Vec<TypeIdentifierAst const*> override;

  SPP_ATTR_NODISCARD auto TypeParts() -> Vec<TypeIdentifierAst*> override;

  SPP_ATTR_NODISCARD auto LastTypePart() const -> TypeIdentifierAst const* override;

  SPP_ATTR_NODISCARD auto LastTypePart() -> TypeIdentifierAst* override;

  SPP_ATTR_NODISCARD auto WithoutConvention() const -> Shared<const TypeAst> override;

  SPP_ATTR_NODISCARD auto GetConvention() const -> ConventionAst* override;

  SPP_ATTR_NODISCARD auto WithConvention(Unique<ConventionAst> &&conv) const -> Shared<TypeAst> override;

  SPP_ATTR_NODISCARD auto WithoutGenerics() const -> Shared<TypeAst> override;

  SPP_ATTR_NODISCARD auto SubstituteGenerics(Vec<GenericArgumentAst*> const &args) const -> Shared<TypeAst> override;

  SPP_ATTR_NODISCARD auto ContainsGenerics(GenericParameterAst const &generic) const -> bool override;

  SPP_ATTR_NODISCARD auto WithGenerics(Unique<GenericArgumentGroupAst> &&arg_group) const -> Shared<TypeAst> override;

  SPP_ATTR_NODISCARD auto IsCompilerGeneratedType() const -> bool override;

  auto ResetCache() -> void override;
};

SPP_GCC_VTABLE_FIX_IMPL(spp::asts::TypeUnaryExpressionAst)
