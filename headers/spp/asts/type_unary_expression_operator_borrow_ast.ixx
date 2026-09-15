module;
#include <spp/macros.hpp>

export module spp.asts.type_unary_expression_operator_borrow_ast;
import spp.asts.ast_kind;
import spp.asts.type_unary_expression_operator_ast;
import spp.utils.types;
import std;

SPP_AST_COMMON_FWD_DECL(TypeUnaryExpressionOperatorBorrowAst);
use(spp::asts, struct ConventionAst);
use(spp::asts, struct IdentifierAst);
use(spp::asts, struct TypeIdentifierAst);

SPP_EXP_CLS struct spp::asts::TypeUnaryExpressionOperatorBorrowAst final : TypeUnaryExpressionOperatorAst {
  SPP_GCC_VTABLE_FIX;
  SPP_AST_KEY_FUNCTIONS(TypeUnaryExpressionOperatorBorrowAst);

  /// The borrow convention: whether the type is borrowed
  /// immutably or mutably.
  Unique<ConventionAst> Conv;

  explicit TypeUnaryExpressionOperatorBorrowAst(
    decltype(Conv) &&conv);

  ~TypeUnaryExpressionOperatorBorrowAst() override;

  SPP_ATTR_NODISCARD auto EqualsOpBorrow(TypeUnaryExpressionOperatorBorrowAst const &) const -> Ordering override;

  SPP_ATTR_NODISCARD auto Equals(TypeUnaryExpressionOperatorAst const &) const -> Ordering override;

  SPP_ATTR_NODISCARD auto NsParts() const -> Vec<IdentifierAst const*> override;

  SPP_ATTR_NODISCARD auto NsParts() -> Vec<IdentifierAst*> override;

  SPP_ATTR_NODISCARD auto TypeParts() const -> Vec<TypeIdentifierAst const*> override;

  SPP_ATTR_NODISCARD auto TypeParts() -> Vec<TypeIdentifierAst*> override;
};

SPP_GCC_VTABLE_FIX_IMPL(spp::asts::TypeUnaryExpressionOperatorBorrowAst)
