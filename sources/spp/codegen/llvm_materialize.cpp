module;
#include <spp/macros.hpp>

module spp.codegen.llvm_materialize;
import spp.analyse.scopes.scope;
import spp.analyse.scopes.scope_manager;
import spp.analyse.scopes.symbols;
import spp.analyse.utils.assignment_utils;
import spp.asts.expression_ast;
import spp.asts.identifier_ast;
import spp.asts.let_statement_initialized_ast;
import spp.asts.local_variable_single_identifier_alias_ast;
import spp.asts.local_variable_single_identifier_ast;
import spp.asts.postfix_expression_ast;
import spp.asts.token_ast;
import spp.asts.type_ast;
import spp.asts.meta.compiler_meta_data;
import spp.asts.utils.ast_utils;
import spp.codegen.llvm_func;
import spp.utils.types;
import spp.utils.uid;
import llvm;
import std;

auto spp::codegen::llvm_materialize(
  asts::ExpressionAst &ast,
  analyse::scopes::ScopeManager *sm,
  asts::meta::CompilerMetaData *meta,
  LlvmCtx *ctx)
  -> asts::IdentifierAst* {
  // Materialise an expression by assigning it to a temporary
  // variable.
  const auto uid = "." + spp::utils::Uid(&ast);
  auto var_name = MakeShared<asts::IdentifierAst>(ast.PosStart(), "$temp" + uid);
  const auto var = MakeUnique<asts::LocalVariableSingleIdentifierAst>(nullptr, std::move(var_name), nullptr);

  // Analyse semantics and generate code for the let statement.
  {
    const auto _meta_guard = asts::meta::MetaGuard(meta);
    meta->LetStatementExplicitType = ast.InferType(sm, meta);
    meta->LetStatementFromUninitialized = true;
    meta->LetStatementValue = nullptr;
    var->Stage7_AnalyseSemantics(sm, meta);

    // Set the lhs to the variable name.
    meta->LetStatementFromUninitialized = false; // Need to generate the expression now.
    meta->LetStatementValue = &ast;
    var->Stage11_CodeGen(sm, meta, ctx);
  }
  const auto materialized_val = var->To<asts::LocalVariableSingleIdentifierAst>()->Name.get();
  return materialized_val;
}

auto spp::codegen::llvm_addr_of(
  asts::ExpressionAst &ast,
  analyse::scopes::ScopeManager *sm,
  asts::meta::CompilerMetaData *meta,
  LlvmCtx *ctx)
  -> llvm::Value* {
  //
  using analyse::utils::assignment_utils::IsDeref;

  // An expression that is already a borrow evaluates to the address of what it borrows, so it is its own address:
  // this covers re-borrowing a borrowed variable, and the forwarding calls ("x.fwd_ref()") that yield one. Note: we
  // don't enforce the borrow on the llvm type, because Gen[&XXX] is valid, but not a borrow.
  if (const auto type = ast.InferType(sm, meta); type != nullptr and type->GetConvention() != nullptr) {
    const auto borrow_val = ast.Stage11_CodeGen(sm, meta, ctx);
    return borrow_val;
  }

  // "x@" names the value the borrow points at, so its address
  // is the pointer that "x" holds - not the address of "x"
  // itself, (the borrow)
  if (IsDeref(&ast)) {
    return ast.To<asts::PostfixExpressionAst>()->Lhs->Stage11_CodeGen(sm, meta, ctx);
  }

  // A member access generates the address of its own field, which is the object a borrow of it points at. The symbol
  // lookup below cannot be used for one, because it resolves to the head of the chain ("a" in "a.b").
  if (asts::IsRuntimeMemberAccess(&ast)) {
    const auto field_ptr = [&] {
      const auto _meta_guard = asts::meta::MetaGuard(meta);
      meta->LlvmWantAddress = true;
      return ast.Stage11_CodeGen(sm, meta, ctx);
    }();
    SPP_ASSERT(field_ptr->getType()->isPointerTy());
    return field_ptr;
  }

  // A symbolic expression (a variable, or a static member of a type or namespace) is already allocated somewhere.
  if (const auto sym = sm->CurrentScope->GetVarSymbolOutermost(ast).first; sym != nullptr) {
    const auto llvm_alloca = sym->LlvmInfo->Alloca;
    SPP_ASSERT(llvm_alloca != nullptr and llvm_alloca->getType()->isPointerTy());

    // A "cmp" constant is a global, and a global belongs to the one module that defines it - borrowing one from
    // another module has to go through that module's own declaration of the symbol, the same way a load of one does.
    if (const auto global_var = llvm::dyn_cast<llvm::GlobalVariable>(llvm_alloca); global_var != nullptr) {
      return GetOrAddGlobalIntoCurrentModule(*global_var, *GetEmissionModule(*ctx));
    }
    return llvm_alloca;
  }

  // Anything else has no storage of its own, so give it some by binding it to a temporary.
  const auto materialized_val = llvm_materialize(ast, sm, meta, ctx);
  const auto materialized_sym = sm->CurrentScope->GetVarSymbol(materialized_val);
  SPP_ASSERT(materialized_sym->LlvmInfo->Alloca->getType()->isPointerTy());
  return materialized_sym->LlvmInfo->Alloca;
}
