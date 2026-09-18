module;
#include <spp/macros.hpp>
#include <spp/analyse/macros.hpp>
#include <spp/parse/macros.hpp>

module spp.asts.annotation_ast;
import spp.analyse.errors.semantic_error;
import spp.analyse.errors.semantic_error_builder;
import spp.analyse.scopes.scope;
import spp.analyse.scopes.scope_manager;
import spp.analyse.scopes.symbols;
import spp.analyse.utils.annotation_utils;
import spp.asts.class_prototype_ast;
import spp.asts.cmp_statement_ast;
import spp.asts.fold_expression_ast;
import spp.asts.function_call_argument_ast;
import spp.asts.function_call_argument_group_ast;
import spp.asts.function_prototype_ast;
import spp.asts.generic_argument_group_ast;
import spp.asts.identifier_ast;
import spp.asts.integer_literal_ast;
import spp.asts.module_prototype_ast;
import spp.asts.postfix_expression_ast;
import spp.asts.postfix_expression_operator_ast;
import spp.asts.postfix_expression_operator_function_call_ast;
import spp.asts.sup_prototype_extension_ast;
import spp.asts.sup_prototype_functions_ast;
import spp.asts.token_ast;
import spp.asts.type_identifier_ast;
import spp.asts.type_statement_ast;
import spp.asts.meta.compiler_meta_data;
import spp.asts.mixins.visibility_enabled_ast;
import spp.asts.utils.ast_utils;
import spp.asts.utils.visibility;
import spp.lex.lexer;
import spp.parse.parser_spp;

namespace spp::asts {
  namespace {
    using namespace spp::asts::utils;

    /// Simple utility method to convert a visibility-oriented
    /// annotation tag into a genuine visibility tag. Returns
    /// std::nullopt if the annotation is not a visibility tag.
    auto VisibilityOf(Str const &fq_name) -> std::optional<Visibility> {
      // Declare enums to cast between.
      using A = analyse::utils::annotation_utils::BuiltinAnnotations;
      using V = Visibility;

      // Simple comparison and return the actual visibility, or
      // nullopt.
      if (fq_name == A::kPublic) { return V::kPublic; }
      if (fq_name == A::kPackage) { return V::kPackage; }
      if (fq_name == A::kProtected) { return V::kProtected; }
      if (fq_name == A::kPrivate) { return V::kPrivate; }
      return std::nullopt;
    }
  }
}

SPP_MOD_BEGIN
AnnotationAst::AnnotationAst(
  decltype(TokExclamationMark) &&tok_exclamation_mark,
  decltype(Name) &&name,
  decltype(GnArgGroup) &&gn_arg_group,
  decltype(FnArgGroup) &&fn_arg_group) :
  TokExclamationMark(std::move(tok_exclamation_mark)),
  Name(std::move(name)),
  GnArgGroup(std::move(gn_arg_group)),
  FnArgGroup(std::move(fn_arg_group)),
  _Target(nullptr) {
  // Default the two optional argument groups.
  SPP_SET_AST_TO_DEFAULT_IF_NULLPTR(this->GnArgGroup);
  SPP_SET_AST_TO_DEFAULT_IF_NULLPTR(this->FnArgGroup);
}

AnnotationAst::~AnnotationAst() = default;

auto AnnotationAst::PosStart() const -> std::size_t {
  // Use the "!" token.
  return TokExclamationMark->PosStart();
}

auto AnnotationAst::PosEnd() const -> std::size_t {
  // Use the "name" token.
  return Name->PosEnd();
}

auto AnnotationAst::Clone() const -> Unique<Ast> {
  // Clone all the members of the ast.
  auto ast = MakeUnique<AnnotationAst>(
    AstClone(TokExclamationMark),
    AstClone(Name),
    AstClone(GnArgGroup),
    AstClone(FnArgGroup));
  ast->_Ctx = _Ctx;
  ast->_Scope = _Scope;
  ast->_Target = _Target;
  return ast;
}

auto AnnotationAst::ToString() const -> Str {
  SPP_STRING_START;
  SPP_STRING_APPEND(TokExclamationMark);
  SPP_STRING_APPEND(Name);
  SPP_STRING_APPEND(GnArgGroup);
  SPP_STRING_APPEND(FnArgGroup);
  SPP_STRING_END;
}

auto AnnotationAst::operator==(
  AnnotationAst const &that) const -> bool {
  // Annotation equality is based on the name.
  return *Name == *that.Name;
}

auto AnnotationAst::Stage1_PreProcess(
  Ast *ctx) -> void {
  // Default AST processing (sets context).
  Ast::Stage1_PreProcess(ctx);
}

auto AnnotationAst::Stage2_GenTopLvlScopes(
  ScopeManager *sm, CompilerMetaData *meta) -> void {
  // Default AST processing (sets scope).
  Ast::Stage2_GenTopLvlScopes(sm, meta);
}

auto AnnotationAst::Stage4_ResolveDeclarations(
  ScopeManager *sm, CompilerMetaData *) -> void {
  // Get the fully qualified name of the annotation, to bypass
  // "use"-imports annotations. Needed to check if we are
  // currently analysing a "!annotation" annotation.
  const auto sym = sm->CurrentScope->GetVarSymbolOutermost(*Name).first;
  if (sym == nullptr) { return; } // Todo: Remove?
  const auto fq_name = sym->FqName()->ToString();
  const auto func_ctx = _Ctx->To<FunctionPrototypeAst>();

  // Early-apply the visibility annotations as some stage 5
  // mechanics need it in asts that can have visibility.
  if (const auto vis = VisibilityOf(fq_name); vis.has_value()) {
    if (const auto vis_ctx = _Ctx->To<mixins::VisibilityAst>()) {
      vis_ctx->Visibility = {*vis, this};
    }
  }

  // If this is a "!annotation" annotation, check that it is
  // a valid target (unique for this ast), as a cmp function,
  // so future steps can read off it properly. Root of all
  // annotations.
  if (fq_name == "std::annotations::annotation") {
    RaiseIf<analyse::errors::SppAnnotationTargetNotACmpFunctionError>(
      not(func_ctx and func_ctx->TokCmp), {_Scope},
      ERR_ARGS(*this, *_Ctx));
    func_ctx->MarkAsAnnotation();
    func_ctx->GetAnnotationInfo()->Definition = this;
  }
}

auto AnnotationAst::Stage5_LoadSupScopes(
  ScopeManager *sm, CompilerMetaData *meta) -> void {
  // Handle builtin annotations.
  using A = analyse::utils::annotation_utils::BuiltinAnnotations;

  // Analyse the target to ensure that it is valid. This needs
  // to *not* include the "()" call on it, just the actual
  // identifier. As "()" are optional, we need to branch on that.
  {
    const auto _meta_guard = MetaGuard(meta);
    meta->IgnoreAccessModifierViolations = true;
    const auto pf = Name->To<PostfixExpressionAst>();
    pf != nullptr and pf->Op->To<PostfixExpressionOperatorFunctionCallAst>()
      ? pf->Lhs->Stage7_AnalyseSemantics(sm, meta)
      : Name->Stage7_AnalyseSemantics(sm, meta);
  }

  // Get the fully qualified name like in stage 4 - todo: can we
  // stamp this symbol into the ast? saves on one lookup per ast.
  const auto sym = sm->CurrentScope->GetVarSymbolOutermost(*Name).first;
  const auto fq_name = sym->FqName()->ToString();

  // For the known builtin annotations, they will attempt to
  // modify their contextual objects if possible, for required
  // behaviour like virtual and abstract tags on function
  // prototypes. If the context is incorrect, nothing happens,
  // and a later stage will pickup the error, as there is a
  // unified target-checking mechanism (custom and builtin).

  // An intrinsic function is a "builtin", and the compiler will
  // generate raw LLVM IR for it. This typically leverages LLVM
  // intrinsics, but sometimes involves larger functions that
  // cannot be expression in S++ (pointer maths for example).
  if (fq_name == A::kIntrinsic) {
    const auto func_ctx = _Ctx->To<FunctionPrototypeAst>();
    if (func_ctx) { func_ctx->BuiltinAnnotation = this; }
  }

  // Mark a visibility-enabled ast with the visibility it names.
  // Needed in stage 4 and 5 for some analysis fixes (just leave
  // it for now).
  else if (const auto vis = VisibilityOf(fq_name); vis.has_value()) {
    const auto vis_ctx = _Ctx->To<mixins::VisibilityAst>();
    if (vis_ctx) { vis_ctx->Visibility = {*vis, this}; }
  }

  // Mark a method ast as being "virtual", enabling overriding.
  else if (fq_name == A::kVirtualMethod) {
    const auto fun_ctx = _Ctx->To<FunctionPrototypeAst>();
    if (fun_ctx) { fun_ctx->VirtualAnnotation = this; }
  }

  // Mark a method ast as being "abstract", requiring overriding.
  else if (fq_name == A::kAbstractMethod) {
    const auto fun_ctx = _Ctx->To<FunctionPrototypeAst>();
    if (fun_ctx) { fun_ctx->AbstractAnnotation = this; }
  }

  // Mark a function ast as being "ffi", additionally providing a
  // linkage name. Makes a function public (stub.spp file).
  else if (fq_name == A::kFfi) {
    const auto fun_ctx = _Ctx->To<FunctionPrototypeAst>();
    if (fun_ctx) { fun_ctx->FfiAnnotation = this; }
    if (fun_ctx) { fun_ctx->Visibility = {Visibility::kPublic, this}; }
  }

  // Mark a type symbol as being "zero type". This can be applied
  // to a class or a type alias. Todo: does this have an attr count
  // check?
  else if (fq_name == A::kZeroType and _Ctx->To<ClassPrototypeAst>()) {
    const auto cls_ctx = _Ctx->To<ClassPrototypeAst>();
    sm->CurrentScope->GetTypeSymbol(cls_ctx->Name->WithoutGenerics().get())->IsDirectlyZeroType = true;
    if (cls_ctx) { cls_ctx->ZeroTypeAnnotation = this; }
  }

  else if (fq_name == A::kZeroType and _Ctx->To<TypeStatementAst>()) {
    const auto cls_ctx = _Ctx->To<TypeStatementAst>();
    sm->CurrentScope->GetTypeSymbol(cls_ctx->NewType->WithoutGenerics().get())->IsDirectlyZeroType = true;
    sm->CurrentScope->GetTypeSymbol(cls_ctx->OldType.get())->IsDirectlyZeroType = true;
  }

  // Mark a type symbol as a thread hazard, so neither it nor
  // anything holding one may cross a thread boundary.
  else if (fq_name == A::kThreadHazard and _Ctx->To<ClassPrototypeAst>()) {
    const auto cls_ctx = _Ctx->To<ClassPrototypeAst>();
    sm->CurrentScope->GetTypeSymbol(cls_ctx->Name->WithoutGenerics().get())->IsDirectlyThreadHazard = true;
  }

  else if (fq_name == A::kThreadHazard and _Ctx->To<TypeStatementAst>()) {
    const auto cls_ctx = _Ctx->To<TypeStatementAst>();
    sm->CurrentScope->GetTypeSymbol(cls_ctx->NewType->WithoutGenerics().get())->IsDirectlyThreadHazard = true;
    sm->CurrentScope->GetTypeSymbol(cls_ctx->OldType.get())->IsDirectlyThreadHazard = true;
  }

  // Mark a function as being a "unit test" (makes it non-callable
  // etc).
  else if (fq_name == A::kTest) {
    const auto fun_ctx = _Ctx->To<FunctionPrototypeAst>();
    if (fun_ctx) { fun_ctx->TestAnnotation = this; }
    if (fun_ctx) { fun_ctx->Visibility = {Visibility::kPublic, this}; }
  }

  // Mark a function as being inlinable via llvm.
  else if (fq_name == A::kLlvmInline) {
    const auto fun_ctx = _Ctx->To<FunctionPrototypeAst>();
    if (fun_ctx) { fun_ctx->InlineAnnotation = {this, fq_name}; }
  }

  // Mark a function as being always inlined via llvm.
  else if (fq_name == A::kLlvmAlwaysInline) {
    const auto fun_ctx = _Ctx->To<FunctionPrototypeAst>();
    if (fun_ctx) { fun_ctx->InlineAnnotation = {this, fq_name}; }
  }

  // Mark a function as being never inlined via llvm.
  else if (fq_name == A::kLlvmNoInline) {
    const auto fun_ctx = _Ctx->To<FunctionPrototypeAst>();
    if (fun_ctx) { fun_ctx->InlineAnnotation = {this, fq_name}; }
  }

  // Mark a function as being "hot" via llvm.
  else if (fq_name == A::kLlvmHot) {
    const auto fun_ctx = _Ctx->To<FunctionPrototypeAst>();
    if (fun_ctx) { fun_ctx->TemperatureAnnotation = this; }
  }

  // Mark a function as being "cold" via llvm.
  else if (fq_name == A::kLlvmCold) {
    const auto fun_ctx = _Ctx->To<FunctionPrototypeAst>();
    if (fun_ctx) { fun_ctx->TemperatureAnnotation = this; }
  }
}

auto AnnotationAst::Stage7_AnalyseSemantics(
  ScopeManager *sm, CompilerMetaData *meta) -> void {
  // Todo: Validate "Void" return type on annotation + test.

  // Convert the target into a function call to ensure it exists
  // a function (previous checks were just for the identifier
  // part).
  auto [fn, fn_ptr] = MakeUniqueAndRaw<PostfixExpressionOperatorFunctionCallAst>(
    std::move(GnArgGroup), std::move(FnArgGroup), nullptr);
  fn->Source.OriginalExpr = this;
  const auto pf = MakeUnique<PostfixExpressionAst>(
    AstClone(Name), std::move(fn));
  pf->Stage7_AnalyseSemantics(sm, meta);

  // Restore the function and generic arguments off of the
  // transformed function call.
  GnArgGroup = std::move(fn_ptr->GnArgGroup);
  FnArgGroup = std::move(fn_ptr->FnArgGroup);

  // Check the target function is an annotation (via the "!annotation"
  // annotation).
  const auto overload = fn_ptr->Target();
  RaiseIf<analyse::errors::SppAnnotationTargetNotAnAnnotationError>(
    not overload->GetAnnotationInfo(),
    {_Scope}, ERR_ARGS(*this, *overload));

  // Stamp the overload as the "target" of this annotation. This
  // will be used for extensively for custom annotations.
  _Target = overload;
}

auto AnnotationAst::Stage9_CompTimeResolve(
  ScopeManager *sm, CompilerMetaData *meta) -> void {
  using analyse::utils::annotation_utils::AnnotationInfo;
  using analyse::errors::SppCalledAnnotationAppliedToInvalidAstError;

  // Load up different asts casts that an annotation may apply to.
  // These can receive the property fine-tune updates based on the
  // annotation.
  const auto annotation_info = _Target->GetAnnotationInfo();
  const auto outer_mod_ctx = _Ctx->GetAstCtx()->To<ModulePrototypeAst>();
  const auto outer_sup_ctx = _Ctx->GetAstCtx()->To<SupPrototypeFunctionsAst>();
  const auto outer_ext_ctx = _Ctx->GetAstCtx()->To<SupPrototypeExtensionAst>();

  // Todo: Maybe do this in stage7, with stage9 evaluation? needs cmp args.
  // Evaluate the context that this annotation can be applied to.
  const auto annotation_scope_name = INJECT_CODE("std::annotations", parse_expression);
  const auto annotation_scope = const_cast<Scope*>(
    sm->CurrentScope->ConvertPostfixToNestedScope(annotation_scope_name.get()));
  auto tm = ScopeManager(sm->GlobalScope, annotation_scope);

  const auto allowed_ctx = [&] {
    const auto _meta_guard = MetaGuard(meta);
    annotation_info->Definition->FnArgGroup->At("target")->Val->Stage7_AnalyseSemantics(&tm, meta);
    annotation_info->Definition->FnArgGroup->At("target")->Val->Stage9_CompTimeResolve(&tm, meta);
    const auto result = std::move(meta->CmpResult);
    return result->To<IntegerLiteralAst>()->CppVal<std::uint64_t>();
  }();

  const auto target = annotation_info->Definition->FnArgGroup->At("target");

  // Error for incompatible asts when classes are not valid
  // targets.
  RaiseIf<SppCalledAnnotationAppliedToInvalidAstError>(
    _Ctx->To<ClassPrototypeAst>() and not(allowed_ctx & AnnotationInfo::kClassContext),
    {annotation_scope, sm->CurrentScope, sm->CurrentScope}, ERR_ARGS(*_Ctx, *this, *target));

  // Error for incompatible asts when free functions are not
  // valid targets.
  RaiseIf<SppCalledAnnotationAppliedToInvalidAstError>(
    _Ctx->To<FunctionPrototypeAst>() and outer_mod_ctx and not(allowed_ctx & AnnotationInfo::kFunctionCtx),
    {annotation_scope, sm->CurrentScope, sm->CurrentScope}, ERR_ARGS(*_Ctx, *this, *target));

  // Error for incompatible asts when methods are not valid
  // targets.
  RaiseIf<SppCalledAnnotationAppliedToInvalidAstError>(
    _Ctx->To<FunctionPrototypeAst>() and outer_sup_ctx and not(allowed_ctx & AnnotationInfo::kMethodCtx),
    {annotation_scope, sm->CurrentScope, sm->CurrentScope}, ERR_ARGS(*_Ctx, *this, *target));

  // Error for incompatible asts when overriding methods are
  // not valid targets.
  RaiseIf<SppCalledAnnotationAppliedToInvalidAstError>(
    _Ctx->To<FunctionPrototypeAst>() and outer_ext_ctx and not(allowed_ctx & AnnotationInfo::kExtensionContext),
    {annotation_scope, sm->CurrentScope, sm->CurrentScope}, ERR_ARGS(*_Ctx, *this, *target));

  // Error for incompatible asts when type statements are not
  // valid targets.
  RaiseIf<SppCalledAnnotationAppliedToInvalidAstError>(
    _Ctx->To<TypeStatementAst>() and not(allowed_ctx & AnnotationInfo::kTypeStmtCtx),
    {annotation_scope, sm->CurrentScope, sm->CurrentScope}, ERR_ARGS(*_Ctx, *this, *target));

  // Error for incompatible asts when cmp statements are not
  // valid targets.
  RaiseIf<SppCalledAnnotationAppliedToInvalidAstError>(
    _Ctx->To<CmpStatementAst>() and not(allowed_ctx & AnnotationInfo::kCmpStmtCtx),
    {annotation_scope, sm->CurrentScope, sm->CurrentScope}, ERR_ARGS(*_Ctx, *this, *target));
}

SPP_MOD_END
