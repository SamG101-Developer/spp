module;
#include <spp/macros.hpp>
#include <spp/analyse/macros.hpp>
#include <spp/codegen/macros.hpp>

module spp.asts.subroutine_prototype_ast;
import spp.analyse.errors.semantic_error;
import spp.analyse.errors.semantic_error_builder;
import spp.analyse.scopes.scope;
import spp.analyse.scopes.scope_manager;
import spp.analyse.scopes.symbols;
import spp.analyse.utils.annotation_utils;
import spp.analyse.utils.control_flow;
import spp.analyse.utils.expr_utils;
import spp.analyse.utils.type_compare;
import spp.analyse.utils.type_predicates;
import spp.asts.annotation_ast;
import spp.asts.function_implementation_ast;
import spp.asts.function_parameter_group_ast;
import spp.asts.generic_parameter_ast;
import spp.asts.generic_parameter_group_ast;
import spp.asts.generic_parameter_type_constraints_ast;
import spp.asts.identifier_ast;
import spp.asts.statement_ast;
import spp.asts.token_ast;
import spp.asts.generate.common_types_precompiled;
import spp.asts.meta.compiler_meta_data;
import spp.asts.utils.ast_utils;
import spp.lex.tokens;
import genex;

SPP_MOD_BEGIN
SubroutinePrototypeAst::SubroutinePrototypeAst(
  decltype(Annotations) &&annotations,
  decltype(TokCmp) &&tok_cmp,
  decltype(TokFun) &&tok_fun,
  decltype(Name) name,
  decltype(GnParamGroup) &&generic_param_group,
  decltype(FnParamGroup) &&param_group,
  decltype(TokArrow) &&tok_arrow,
  decltype(ReturnType) return_type,
  decltype(Impl) &&impl) :
  FunctionPrototypeAst(
    std::move(annotations), std::move(tok_cmp), std::move(tok_fun), std::move(name),
    std::move(generic_param_group), std::move(param_group), std::move(tok_arrow),
    std::move(return_type), std::move(impl)) {
  SPP_SET_AST_TO_DEFAULT_IF_NULLPTR(this->TokFun, lex::SppTokenType::KW_FUN, "fun");
}

SubroutinePrototypeAst::~SubroutinePrototypeAst() = default;

auto SubroutinePrototypeAst::Clone() const -> Unique<Ast> {
  IMPORT_UTILS;
  auto ast = MakeUnique<SubroutinePrototypeAst>(
    AstCloneVec(Annotations),
    AstClone(TokCmp),
    AstClone(TokFun),
    AstClone(Name),
    AstClone(GnParamGroup),
    AstClone(FnParamGroup),
    AstClone(TokArrow),
    AstClone(ReturnType),
    AstClone(Impl));
  _CloneStateInto(*ast);
  return ast;
}

auto SubroutinePrototypeAst::Stage7_AnalyseSemantics(
  ScopeManager *sm, CompilerMetaData *meta) -> void {
  //
  IMPORT_UTILS;
  using generate::common_types_precompiled::VOID;

  // Perform default function prototype semantic analysis
  FunctionPrototypeAst::Stage7_AnalyseSemantics(sm, meta);
  const auto ret_type_sym = sm->CurrentScope->FindTypeSymbol(ReturnType.get());

  // Update the meta information for enclosing function information.
  meta->Save();
  meta->EnclosingFnFlavour = this->TokFun.get();
  meta->EnclosingFnRetType.EmplaceBack(ret_type_sym->FqName());
  meta->EnclosingFnSourceRetType.EmplaceBack(ReturnType);
  meta->EnclosingFnScope = sm->CurrentScope;
  meta->EnclosingFnCmp = TokCmp.get();
  Impl->Stage7_AnalyseSemantics(sm, meta);

  // Check for a void return type.
  const auto is_void = type_compare::TypeEq(
    TypeRef::Of(*ReturnType, *sm->CurrentScope),
    TypeRef::Of(*VOID, *sm->CurrentScope),
    *sm->CurrentScope, *sm->CurrentScope);

  // Check there is a return statement at the end (for non-void
  // functions).
  const auto final_member = Impl->FinalMember();
  const auto annotation_blocks_ret = FfiAnnotation or BuiltinAnnotation or AbstractAnnotation;

  // A body that never reaches its end ("ret", "abort()", a loop
  // with no way out, a "case" every branch of which does one of
  // those) needs no value there.
  const auto body_diverges = control_flow::Diverges(*Impl, sm, meta);
  RaiseIf<SppFunctionSubroutineMissingReturnStatementError>(
    not is_void and not annotation_blocks_ret and not body_diverges,
    {sm->CurrentScope}, ERR_ARGS(*final_member, *ReturnType, *ReturnType));

  // Ffi functions cannot be generic, otherwise we get
  // multiple prototypes for the singular C function,
  // breaking C ABI compatibility.
  if (FfiAnnotation != nullptr) {
    const auto ffi_symbol = GetFfiSymbolName();
    for (auto const *gn_param : GnParamGroup->Params | genex::views::ptr) {
      Raise<SppFfiGenericParameterError>(
        {sm->CurrentScope}, ERR_ARGS(*FfiAnnotation, *gn_param, StrView(ffi_symbol)));
    }
  }

  sm->MoveOutOfCurrentScope();
  meta->Restore(true);
  meta->LoopReturnTypes->clear();
}

auto SubroutinePrototypeAst::Stage11_CodeGen(
  ScopeManager *sm, CompilerMetaData *meta, codegen::LlvmCtx *ctx) -> llvm::Value* {
  // Build the function body.
  // Todo: Move all to the subroutine prototype. Given coroutine
  //  ast overrides this.
  sm->MoveToNextScope();

  const auto llvm_func = GetLlvmFn();
  const auto llvm_func_target = llvm_func != nullptr ? llvm_func->Target : nullptr;

  // An "@ffi" function is a declaration and will receive its
  // implementation from the linker. Nothing needs to be emitted
  // into it.
  if (FfiAnnotation != nullptr) {
    ctx->Builder.ClearInsertionPoint();
    const auto ffi_final_scope = sm->CurrentScope->GetFinalChildScope();
    while (sm->CurrentScope != ffi_final_scope) { sm->MoveToNextScope(false); }
    sm->MoveOutOfCurrentScope();
    return nullptr;
  }

  // A template ("_IsPureGn" declined to declare it) or an
  // uninstantiable signature. There is no llvm function to emit
  // into, so nothing here applies to it: not an entry block
  // (which would be built parentless, and leak), not the enclosing
  // function meta data, and not the return type lookup, which a
  // template's return type need not even satisfy. Only its
  // instantiations have bodies, and those are emitted below.
  if (llvm_func_target == nullptr) {
    sm->ExhaustScope();
    sm->MoveOutOfCurrentScope();
    _CodeGenGnSubstitutions(sm, meta, ctx);
    return nullptr;
  }

  // Add the entry block to the function.
  const auto entry_bb = llvm::BasicBlock::Create(
    *ctx->Context, "entry", llvm_func_target);
  ctx->Builder.SetInsertPoint(entry_bb);

  // Generate the parameters as variables.
  FnParamGroup->Stage11_CodeGen(sm, meta, ctx);
  GnParamGroup->Stage11_CodeGen(sm, meta, ctx);

  const auto ret_type_sym = sm->CurrentScope->FindTypeSymbol(
    ReturnType.get());
  {
    const auto _meta_guard = MetaGuard(meta);
    meta->EnclosingFnFlavour = TokFun.get();
    meta->EnclosingFnRetType.EmplaceBack(ret_type_sym->FqName());
    meta->EnclosingFnSourceRetType.EmplaceBack(ReturnType);
    meta->EnclosingFnScope = sm->CurrentScope;

    // If there is an implementation, generate its code.
    if (BuiltinAnnotation or FfiAnnotation) {
      // Get manual IR from a codegen module.
      Impl->Stage11_CodeGen(sm, meta, ctx);
      if (entry_bb->empty()) {
        entry_bb->eraseFromParent();
        ctx->Builder.ClearInsertionPoint();
      }
    }
    else {
      // Generate the function implementation. For abstract method,
      // shift scopes, as there is still a body, it's just empty.
      Impl->Stage11_CodeGen(sm, meta, ctx);

      // Add a return instruction inside the function if there isn't
      // one (abstract methods will never be called due to previous
      // semantic analysis on abstracts, but to satisfy LLVM analysis).
      const auto insert_bb = ctx->Builder.GetInsertBlock();
      if (not insert_bb->hasTerminator()) {
        const auto ret_void = insert_bb->getParent()->getReturnType()->isVoidTy();
        if (ret_void) { ctx->Builder.CreateRetVoid(); }
        else { ctx->Builder.CreateUnreachable(); }
      }
    }
    VALIDATE_LLVM

  }
  sm->MoveOutOfCurrentScope();
  _CodeGenGnSubstitutions(sm, meta, ctx);
  return nullptr;
}

auto SubroutinePrototypeAst::IsCoroutine() const -> bool {
  return false;
}

SPP_MOD_END
