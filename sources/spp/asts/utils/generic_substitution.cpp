module spp.asts.utils.generic_substitution;
import spp.asts.expression_ast;
import spp.asts.function_call_argument_ast;
import spp.asts.function_call_argument_group_ast;
import spp.asts.generic_argument_ast;
import spp.asts.generic_argument_group_ast;
import spp.asts.generic_argument_type_ast;
import spp.asts.object_initializer_argument_ast;
import spp.asts.object_initializer_argument_group_ast;
import spp.asts.object_initializer_ast;
import spp.asts.postfix_expression_ast;
import spp.asts.postfix_expression_operator_ast;
import spp.asts.postfix_expression_operator_function_call_ast;
import spp.asts.type_ast;
import spp.asts.utils.ast_utils;

auto spp::asts::utils::generic_substitution::SubstituteGenericsInExpression(
  Unique<ExpressionAst> &expr,
  Vec<GenericArgumentAst*> const &generic_args)
  -> void {
  if (expr == nullptr or generic_args.IsEmpty()) { return; }

  // A type in expression position: the "A" of "A::new()".
  if (const auto type = expr->To<TypeAst>(); type != nullptr) {
    expr = AstClone(type->SubstituteGenerics(generic_args));
    return;
  }

  // An object initializer names its type outright, and each of its arguments is an expression in its own right.
  if (const auto obj_init = expr->To<ObjectInitializerAst>(); obj_init != nullptr) {
    obj_init->Type = obj_init->Type->SubstituteGenerics(generic_args);
    for (auto const &arg : obj_init->ArgGroup->Args) {
      SubstituteGenericsInExpression(arg->Val, generic_args);
    }
    return;
  }

  // A postfix expression carries types on the left ("A::new()") and inside a call's generic and function arguments.
  if (const auto postfix = expr->To<PostfixExpressionAst>(); postfix != nullptr) {
    SubstituteGenericsInExpression(postfix->Lhs, generic_args);
    if (const auto call = postfix->Op->To<PostfixExpressionOperatorFunctionCallAst>(); call != nullptr) {
      for (auto const &gn_arg : call->GnArgGroup->Args) {
        if (const auto type_arg = gn_arg->To<GenericArgumentTypeAst>(); type_arg != nullptr) {
          type_arg->Val = type_arg->Val->SubstituteGenerics(generic_args);
        }
      }
      for (auto const &fn_arg : call->FnArgGroup->Args) {
        SubstituteGenericsInExpression(fn_arg->Val, generic_args);
      }
    }
  }
}
