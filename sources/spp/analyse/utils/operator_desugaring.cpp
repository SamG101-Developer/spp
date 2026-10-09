module;
#include <spp/analyse/macros.hpp>

module spp.analyse.utils.operator_desugaring;
import spp.analyse.errors.semantic_error;
import spp.analyse.errors.semantic_error_builder;
import spp.analyse.scopes.scope;
import spp.analyse.scopes.scope_manager;
import spp.analyse.scopes.symbols;
import spp.asts.ast;
import spp.asts.binary_expression_ast;
import spp.asts.convention_ref_ast;
import spp.asts.expression_ast;
import spp.asts.fold_expression_ast;
import spp.asts.function_call_argument_ast;
import spp.asts.function_call_argument_group_ast;
import spp.asts.function_call_argument_positional_ast;
import spp.asts.generic_argument_group_ast;
import spp.asts.identifier_ast;
import spp.asts.let_statement_initialized_ast;
import spp.asts.local_variable_single_identifier_alias_ast;
import spp.asts.local_variable_single_identifier_ast;
import spp.asts.postfix_expression_ast;
import spp.asts.postfix_expression_operator_function_call_ast;
import spp.asts.postfix_expression_operator_runtime_member_access_ast;
import spp.asts.token_ast;
import spp.asts.type_ast;
import spp.asts.meta.compiler_meta_data;
import spp.asts.utils.ast_utils;
import spp.utils.uid;
import genex;

namespace spp::analyse::utils::operator_desugaring {
  namespace {
    auto CombineCompOpsImpl(
      BinaryExpressionAst &bin_expr, ScopeManager *sm, CompilerMetaData *meta,
      Vec<Unique<LetStatementInitializedAst>> *temps) -> Unique<BinaryExpressionAst> {
      // Check the left-hand-side is a binary expression with a
      // comparison operator. If there isn't a chaining combination
      // to do, hand the operands to a new bin expression.
      const auto bin_lhs = bin_expr.Lhs->To<BinaryExpressionAst>();
      if (
        bin_lhs == nullptr or
        not genex::contains(kBinComparisonOps, bin_expr.TokOp->TokenType) or
        not genex::contains(kBinComparisonOps, bin_lhs->TokOp->TokenType)) {
        return MakeUnique<BinaryExpressionAst>(
          std::move(bin_expr.Lhs),
          AstClone(bin_expr.TokOp),
          std::move(bin_expr.Rhs));
      }

      // Non-symbolic value being reused -> put it into a variable
      // first. Todo: Standardise materialization?
      if (sm->CurrentScope->FindVarSymbolOutermost(*bin_lhs->Rhs).first == nullptr) {
        const auto temp_var_name = [&] {
          const auto uid = spp::utils::Uid();
          return MakeShared<IdentifierAst>(
            bin_lhs->Rhs->PosStart(), uid);
        }();

        auto temp_let = [&] {
          auto var = MakeUnique<LocalVariableSingleIdentifierAst>(
            nullptr, temp_var_name, nullptr);
          return MakeUnique<LetStatementInitializedAst>(
            nullptr, std::move(var), nullptr, nullptr, std::move(bin_lhs->Rhs));
        }();

        temp_let->Stage7_AnalyseSemantics(sm, meta);
        bin_lhs->Rhs = AstClone(temp_var_name);

        // Kept by the caller: nothing else would give it storage.
        if (temps != nullptr) { temps->EmplaceBack(std::move(temp_let)); }
      }

      // Otherwise, re-arrange the ASTs, with an "and" combinator
      // binary expression.
      auto lhs = AstClone(bin_lhs->Rhs);
      auto rhs = std::move(bin_expr.Rhs);
      auto op_pos = bin_expr.TokOp->PosStart();
      bin_expr.Rhs = MakeUnique<BinaryExpressionAst>(
        std::move(lhs), std::move(bin_expr.TokOp), std::move(rhs));
      bin_expr.TokOp = MakeUnique<TokenAst>(
        op_pos, lex::SppTokenType::KW_AND, "and");

      return CombineCompOpsImpl(bin_expr, sm, meta, temps);
    }
  }
}

auto spp::analyse::utils::operator_desugaring::CombineComparisonChain(
  BinaryExpressionAst &bin_expr, ScopeManager *const sm,
  CompilerMetaData *const meta, Vec<Unique<LetStatementInitializedAst>> &temps)
  -> Unique<BinaryExpressionAst> {
  // Hook into the inner impl.
  return CombineCompOpsImpl(bin_expr, sm, meta, &temps);
}

auto spp::analyse::utils::operator_desugaring::ConvertBinExprToFnCall(
  BinaryExpressionAst &bin_expr, ScopeManager *sm,
  CompilerMetaData *meta) -> Unique<PostfixExpressionAst> {
  // Before converting into a function check if we can chain
  // comparison operators.
  const auto new_bin_expr = CombineCompOpsImpl(
    bin_expr, sm, meta, nullptr);

  // Get the method names based on the operator token. For
  // example, `1 + 2` is the same as `1.add(2)` (which after
  // further processing is `S32::add(1, 2)`).
  auto method_name = kBinMethods.at(new_bin_expr->TokOp->TokenType);
  auto method_name_wrapped = IdentifierAst::MappedFromTok(
    *new_bin_expr->TokOp, std::move(method_name));

  // Construct the field access ast using the previously
  // determined name, and then wrap the function call operator,
  // ready for argument injection from the operands.
  auto field = MakeUnique<PostfixExpressionOperatorRuntimeMemberAccessAst>(
    nullptr, std::move(method_name_wrapped));
  auto field_access = MakeUnique<PostfixExpressionAst>(
    std::move(new_bin_expr->Lhs), std::move(field));
  auto fn_call = MakeUnique<PostfixExpressionOperatorFunctionCallAst>(
    nullptr, nullptr, nullptr);

  // Inject the arguments for the function call, applying the
  // conventions in the standard way enforced by the operator
  // classes.
  auto conv = genex::contains(kBinComparisonOps, new_bin_expr->TokOp->TokenType)
    ? MakeUnique<ConventionRefAst>(nullptr)
    : nullptr;
  auto arg = MakeUnique<FunctionCallArgumentPositionalAst>(
    std::move(conv), nullptr, std::move(new_bin_expr->Rhs));
  fn_call->FnArgGroup->Args.EmplaceBack(std::move(arg));
  fn_call->Source.OriginalExpr = &bin_expr;

  // Finally combine the function call ast with the field access
  // ast into a postfix expression.
  auto new_ast = MakeUnique<PostfixExpressionAst>(
    std::move(field_access), std::move(fn_call));
  return new_ast;
}

auto spp::analyse::utils::operator_desugaring::CheckIndexable(
  Ast const &op,
  ScopeManager *sm,
  CompilerMetaData *meta)
  -> void {
  // Checked here, not left to the method lookup: the parse of "a[..]" is ambiguous with generic arguments.
  const auto lhs_type = meta->PostfixExpressionLhs->InferType(sm, meta);
  const auto type_sym = sm->CurrentScope->FindTypeSymbol(lhs_type.get());
  RaiseIf<errors::SppMemberAccessNonIndexableError>(
    type_sym == nullptr or type_sym->LinkedScope == nullptr or lhs_type->IsCompilerGeneratedType(),
    {sm->CurrentScope}, ERR_ARGS(*meta->PostfixExpressionLhs, *lhs_type, op));
}

auto spp::analyse::utils::operator_desugaring::MapToMethodCall(
  Ast &op,
  const std::size_t pos,
  const StrView method,
  Vec<Unique<ExpressionAst>> &&args,
  ScopeManager *sm,
  CompilerMetaData *meta)
  -> Shared<PostfixExpressionAst> {
  auto arg_group = MakeUnique<FunctionCallArgumentGroupAst>(nullptr, Vec<Unique<FunctionCallArgumentAst>>{}, nullptr);
  for (auto &&arg : args) {
    arg_group->Args.EmplaceBack(MakeUnique<FunctionCallArgumentPositionalAst>(nullptr, nullptr, std::move(arg)));
  }
  auto field_name = MakeUnique<IdentifierAst>(pos, Str(method));
  auto field = MakeUnique<PostfixExpressionOperatorRuntimeMemberAccessAst>(nullptr, std::move(field_name));
  auto member_access = MakeUnique<PostfixExpressionAst>(AstClone(meta->PostfixExpressionLhs), std::move(field));
  auto fn_call = MakeUnique<PostfixExpressionOperatorFunctionCallAst>(nullptr, std::move(arg_group), nullptr);
  fn_call->Source.OriginalExpr = &op;
  auto mapped = MakeShared<PostfixExpressionAst>(std::move(member_access), std::move(fn_call));
  mapped->Stage7_AnalyseSemantics(sm, meta);
  return mapped;
}
