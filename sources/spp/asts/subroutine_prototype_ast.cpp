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
import spp.analyse.utils.type_compare;
import spp.analyse.utils.type_predicates;
import spp.asts.annotation_ast;
import spp.asts.function_implementation_ast;
import spp.asts.function_parameter_group_ast;
import spp.asts.generic_parameter_ast;
import spp.asts.generic_parameter_group_ast;
import spp.asts.generic_parameter_type_ast;
import spp.asts.generic_parameter_type_inline_constraints_ast;
import spp.asts.identifier_ast;
import spp.asts.ret_statement_ast;
import spp.asts.statement_ast;
import spp.asts.token_ast;
import spp.asts.generate.common_types_precompiled;
import spp.asts.meta.compiler_meta_data;
import spp.asts.utils.ast_utils;
import spp.lex.tokens;
import genex;

SPP_MOD_BEGIN
spp::asts::SubroutinePrototypeAst::SubroutinePrototypeAst(
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

spp::asts::SubroutinePrototypeAst::~SubroutinePrototypeAst() = default;

auto spp::asts::SubroutinePrototypeAst::Clone() const
  -> Unique<Ast> {
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
  ast->_AnnotationInfo = _AnnotationInfo
    ? MakeUnique<analyse::utils::annotation_utils::AnnotationInfo>(*_AnnotationInfo)
    : nullptr;
  ast->Source.OriginalImpl = AstClone(Source.OriginalImpl);
  ast->Source.OriginalReturnType = AstClone(Source.OriginalReturnType);
  ast->_Ctx = _Ctx;
  ast->_Scope = _Scope;
  ast->AbstractAnnotation = AbstractAnnotation;
  ast->VirtualAnnotation = VirtualAnnotation;
  ast->TemperatureAnnotation = TemperatureAnnotation;
  ast->FfiAnnotation = FfiAnnotation;
  ast->BuiltinAnnotation = BuiltinAnnotation;
  ast->TestAnnotation = TestAnnotation;
  ast->InlineAnnotation = InlineAnnotation;
  ast->Visibility = Visibility;
  ast->_LlvmFunc = _LlvmFunc;
  ast->VariadicPackType = VariadicPackType;
  for (auto const &a : ast->Annotations) { a->SetAstCtx(ast.get()); }
  return ast;
}

auto spp::asts::SubroutinePrototypeAst::Stage7_AnalyseSemantics(
  analyse::scopes::ScopeManager *sm,
  meta::CompilerMetaData *meta)
  -> void {
  //
  using analyse::utils::type_compare::TypeEq;
  using generate::common_types_precompiled::VOID;
  using generate::common_types_precompiled::NEVER;

  // Perform default function prototype semantic analysis
  FunctionPrototypeAst::Stage7_AnalyseSemantics(sm, meta);
  const auto ret_type_sym = sm->CurrentScope->GetTypeSymbol(ReturnType.get());

  // Update the meta information for enclosing function information.
  meta->Save();
  meta->EnclosingFunctionFlavour = this->TokFun.get();
  meta->EnclosingFunctionRetType.EmplaceBack(ret_type_sym->FqName());
  meta->EnclosingFunctionSourceRetType.EmplaceBack(ReturnType);
  meta->EnclosingFunctionScope = sm->CurrentScope;
  meta->EnclosingFunctionCmp = TokCmp.get();
  Impl->Stage7_AnalyseSemantics(sm, meta);

  // Handle the "!" never type.
  auto tm = analyse::scopes::ScopeManager(
    sm->GlobalScope, sm->CurrentScope->Children[0].get());
  const auto is_never = [&] {
    const auto _meta_guard = meta::MetaGuard(meta);
    meta->IgnoreMissingElseBranchForInference = true;
    return not Impl->Members.IsEmpty() and TypeEq(
      *Impl->FinalMember()->To<StatementAst>()->InferType(&tm, meta), *NEVER,
      *tm.CurrentScope, *sm->CurrentScope);
  }();

  // Check for a void return type.
  const auto is_void = TypeEq(
    *ReturnType, *VOID, *sm->CurrentScope, *sm->CurrentScope);

  // Check there is a return statement at the end (for non-void functions).
  const auto final_member = Impl->FinalMember();
  const auto annotation_blocks_ret = FfiAnnotation or BuiltinAnnotation or AbstractAnnotation;
  const auto final_member_check = (not Impl->Members.IsEmpty() and Impl->Members.Back()->To<RetStatementAst>());
  RaiseUnless<analyse::errors::SppFunctionSubroutineMissingReturnStatementError>(
    is_void or is_never or annotation_blocks_ret or final_member_check,
    {sm->CurrentScope}, ERR_ARGS(*final_member, *Source.OriginalReturnType, *ReturnType));

  // Ffi functions cannot be generic, otherwise we get
  // multiple prototypes for the singular C function,
  // breaking C ABI compatibility.
  if (FfiAnnotation != nullptr) {
    const auto ffi_symbol = GetFfiSymbolName();
    for (auto const *gn_param : GnParamGroup->Params | genex::views::ptr) {
      Raise<analyse::errors::SppFfiGenericParameterError>(
        {sm->CurrentScope}, ERR_ARGS(*FfiAnnotation, *gn_param, StrView(ffi_symbol)));
    }
  }

  sm->MoveOutOfCurrentScope();
  meta->Restore(true);
  meta->LoopReturnTypes->clear();
}

auto spp::asts::SubroutinePrototypeAst::Stage11_CodeGen(
  analyse::scopes::ScopeManager *sm,
  meta::CompilerMetaData *meta,
  codegen::LlvmCtx *ctx)
  -> llvm::Value* {
  // Build the function body.
  // Todo: Move all to the subroutine prototype. Given coroutine
  //  ast overrides this.
  sm->MoveToNextScope();

  const auto llvm_func = GetLlvmFunc();
  const auto llvm_func_target = llvm_func != nullptr ? llvm_func->Target : nullptr;

  // An "@ffi" function is a declaration and will receive its
  // implementation from the linker. Nothing needs to be emitted
  // into it.
  if (FfiAnnotation != nullptr) {
    ctx->Builder.ClearInsertionPoint();
    const auto ffi_final_scope = sm->CurrentScope->FinalChildScope();
    while (sm->CurrentScope != ffi_final_scope) { sm->MoveToNextScope(false); }
    sm->MoveOutOfCurrentScope();
    return nullptr;
  }

  // A template ("_IsPureGeneric" declined to declare it) or an
  // uninstantiable signature. There is no llvm function to emit
  // into, so nothing here applies to it: not an entry block
  // (which would be built parentless, and leak), not the enclosing
  // function meta data, and not the return type lookup, which a
  // template's return type need not even satisfy. Only its
  // instantiations have bodies, and those are emitted below.
  if (llvm_func_target == nullptr) {
    sm->ExhaustScope();
    sm->MoveOutOfCurrentScope();
    _CodeGenGenericSubstitutions(sm, meta, ctx);
    return nullptr;
  }

  // Add the entry block to the function.
  const auto entry_bb = llvm::BasicBlock::Create(
    *ctx->Context, "entry", llvm_func_target);
  ctx->Builder.SetInsertPoint(entry_bb);

  // Generate the parameters as variables.
  FnParamGroup->Stage11_CodeGen(sm, meta, ctx);
  GnParamGroup->Stage11_CodeGen(sm, meta, ctx);

  const auto ret_type_sym = sm->CurrentScope->GetTypeSymbol(
    ReturnType.get());
  {
    const auto _meta_guard = meta::MetaGuard(meta);
    meta->EnclosingFunctionFlavour = TokFun.get();
    meta->EnclosingFunctionRetType.EmplaceBack(ret_type_sym->FqName());
    meta->EnclosingFunctionSourceRetType.EmplaceBack(ReturnType);
    meta->EnclosingFunctionScope = sm->CurrentScope;

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
  _CodeGenGenericSubstitutions(sm, meta, ctx);
  return nullptr;
}

auto spp::asts::SubroutinePrototypeAst::IsCoroutine() const
  -> bool {
  return false;
}

SPP_MOD_END
