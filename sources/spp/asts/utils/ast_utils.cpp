module spp.asts.utils.ast_utils;
import spp.asts.ast;
import spp.asts.class_implementation_ast;
import spp.asts.class_member_ast;
import spp.asts.class_prototype_ast;
import spp.asts.expression_ast;
import spp.asts.function_implementation_ast;
import spp.asts.function_prototype_ast;
import spp.asts.identifier_ast;
import spp.asts.let_statement_initialized_ast;
import spp.asts.local_variable_single_identifier_alias_ast;
import spp.asts.local_variable_single_identifier_ast;
import spp.asts.module_implementation_ast;
import spp.asts.module_prototype_ast;
import spp.asts.postfix_expression_ast;
import spp.asts.postfix_expression_operator_ast;
import spp.asts.postfix_expression_operator_runtime_member_access_ast;
import spp.asts.statement_ast;
import spp.asts.sup_implementation_ast;
import spp.asts.sup_prototype_extension_ast;
import spp.asts.sup_prototype_functions_ast;
import spp.asts.token_ast;
import spp.asts.type_ast;
import spp.utils.uid;
import genex;

auto spp::asts::AstNameOrNull(
  Ast *ast)
  -> Shared<TypeAst> {
  if (const auto cls = ast->To<ClassPrototypeAst>(); cls != nullptr) {
    return cls->Name;
  }
  if (const auto sup = ast->To<SupPrototypeFunctionsAst>(); sup != nullptr) {
    return sup->Name;
  }
  if (const auto ext = ast->To<SupPrototypeExtensionAst>(); ext != nullptr) {
    return ext->Name;
  }
  return nullptr;
}

auto spp::asts::AstName(
  Ast *ast)
  -> Shared<TypeAst> {
  if (auto name = AstNameOrNull(ast); name != nullptr) {
    return name;
  }
  throw std::runtime_error("ast_name: Unsupported AST type " + Str(typeid(*ast).name()));
}

auto spp::asts::AstBody(
  Ast *ast)
  -> Vec<Ast*> {
  if (const auto cls = ast->To<ClassPrototypeAst>(); cls != nullptr) {
    return cls->Impl->Members | genex::views::ptr | genex::views::cast_dynamic<Ast*>() | genex::to<Vec>();
  }
  if (const auto sup = ast->To<SupPrototypeFunctionsAst>(); sup != nullptr) {
    return sup->Impl->Members | genex::views::ptr | genex::views::cast_dynamic<Ast*>() | genex::to<Vec>();
  }
  if (const auto ext = ast->To<SupPrototypeExtensionAst>(); ext != nullptr) {
    return ext->Impl->Members | genex::views::ptr | genex::views::cast_dynamic<Ast*>() | genex::to<Vec>();
  }
  if (const auto fun = ast->To<FunctionPrototypeAst>(); fun != nullptr) {
    return fun->Impl->Members | genex::views::ptr | genex::views::cast_dynamic<Ast*>() | genex::to<Vec>();
  }
  if (const auto mod = ast->To<ModulePrototypeAst>(); mod != nullptr) {
    return mod->Impl->Members | genex::views::ptr | genex::views::cast_dynamic<Ast*>() | genex::to<Vec>();
  }

  // Special case for the top level scope for generic types (sup scopes are constraints).
  if (ast == nullptr) {
    return {};
  }

  throw std::runtime_error("ast_body: Unsupported AST type");
}

auto spp::asts::IsRuntimeMemberAccess(
  Ast const *ast)
  -> bool {
  // Check the ast is a postfix expression, whose operator is the runtime member access operator.
  const auto postfix = ast->To<PostfixExpressionAst>();
  return postfix != nullptr and postfix->Op->To<PostfixExpressionOperatorRuntimeMemberAccessAst>() != nullptr;
}

auto spp::asts::BindLocal(
  Unique<ExpressionAst> &slot,
  Vec<Unique<StatementAst>> &prelude,
  const std::size_t pos)
  -> Unique<IdentifierAst> {
  // Bind the expression to a fresh local, "let $uid = <expr>", and read the local where the expression was.
  const auto uid = spp::utils::Uid();
  auto var = MakeUnique<LocalVariableSingleIdentifierAst>(
    nullptr, MakeShared<IdentifierAst>(pos, Str(uid)), nullptr);
  prelude.EmplaceBack(MakeUnique<LetStatementInitializedAst>(
    nullptr, std::move(var), nullptr, nullptr, std::move(slot)));
  slot = MakeUnique<IdentifierAst>(pos, Str(uid));
  return MakeUnique<IdentifierAst>(pos, Str(uid));
}
