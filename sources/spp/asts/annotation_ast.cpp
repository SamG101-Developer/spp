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
import spp.asts.expression_ast;
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

    /// The visibility a visibility annotation names, or Nullopt
    /// for any other annotation.
    auto VisibilityOf(const analyse::utils::annotation_utils::BuiltinAnnotation builtin) -> Opt<Visibility> {
      using A = analyse::utils::annotation_utils::BuiltinAnnotation;
      switch (builtin) {
        case A::kPublic: return Visibility::kPublic;
        case A::kPackage: return Visibility::kPackage;
        case A::kProtected: return Visibility::kProtected;
        case A::kPrivate: return Visibility::kPrivate;
        default: return Nullopt;
      }
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
  ast->Builtin = Builtin;
  ast->_BuiltinResolved = _BuiltinResolved;
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
  IMPORT_UTILS;
  // Get the fully qualified name of the annotation, to bypass
  // "use"-imports annotations. Needed to check if we are
  // currently analysing a "!annotation" annotation.
  if (not _ResolveBuiltin(sm)) { return; }
  const auto func_ctx = _Ctx->To<FunctionPrototypeAst>();

  // Early-apply the visibility annotations as some stage 5
  // mechanics need it in asts that can have visibility.
  if (const auto vis = VisibilityOf(Builtin); vis.has_value()) {
    if (const auto vis_ctx = _Ctx->To<mixins::VisibilityAst>()) {
      vis_ctx->Visibility = {*vis, this};
    }
  }

  // If this is a "!annotation" annotation, check that it is
  // a valid target (unique for this ast), as a cmp function,
  // so future steps can read off it properly. Root of all
  // annotations.
  if (Builtin == annotation_utils::BuiltinAnnotation::kAnnotation) {
    RaiseIf<SppAnnotationTargetNotACmpFunctionError>(
      not(func_ctx and func_ctx->TokCmp),
      ERR_ARGS(*this, *_Ctx));
    func_ctx->MarkAsAnnotation();
    func_ctx->GetAnnotationInfo()->Definition = this;
  }
}

auto AnnotationAst::Stage5_LoadSupScopes(
  ScopeManager *sm, CompilerMetaData *meta) -> void {
  IMPORT_UTILS;

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

  // Resolved in stage 4, unless its function was not found yet.
  if (not _BuiltinResolved) { _ResolveBuiltin(sm); }

  // For the known builtin annotations, they will attempt to
  // modify their contextual objects if possible, for required
  // behaviour like virtual and abstract tags on function
  // prototypes. If the context is incorrect, nothing happens,
  // and a later stage will pickup the error, as there is a
  // unified target-checking mechanism (custom and builtin).
  using A = annotation_utils::BuiltinAnnotation;
  const auto fun_ctx = _Ctx->To<FunctionPrototypeAst>();
  const auto cls_ctx = _Ctx->To<ClassPrototypeAst>();
  switch (Builtin) {
    // An intrinsic function is a "builtin", and the compiler will
    // generate raw LLVM IR for it. This typically leverages LLVM
    // intrinsics, but sometimes involves larger functions that
    // cannot be expression in S++ (pointer maths for example).
    case A::kIntrinsic:
      if (fun_ctx) { fun_ctx->BuiltinAnnotation = this; }
      break;

    // Mark a visibility-enabled ast with the visibility it names.
    case A::kPublic:
    case A::kPackage:
    case A::kProtected:
    case A::kPrivate:
      if (const auto vis_ctx = _Ctx->To<mixins::VisibilityAst>()) { vis_ctx->Visibility = {*VisibilityOf(Builtin), this}; }
      break;

    // Mark a method as "virtual" (overridable) or "abstract" (to
    // be overridden).
    case A::kVirtualMethod:
      if (fun_ctx) { fun_ctx->VirtualAnnotation = this; }
      break;
    case A::kAbstractMethod:
      if (fun_ctx) { fun_ctx->AbstractAnnotation = this; }
      break;

    // Mark a function as "ffi", additionally providing a linkage
    // name, and a unit test as non-callable: both are public.
    case A::kFfi:
      if (fun_ctx) { fun_ctx->FfiAnnotation = this; }
      if (fun_ctx) { fun_ctx->Visibility = {Visibility::kPublic, this}; }
      break;
    case A::kTest:
      if (fun_ctx) { fun_ctx->TestAnnotation = this; }
      if (fun_ctx) { fun_ctx->Visibility = {Visibility::kPublic, this}; }
      break;

    // Mark a class as a "zero type" (Todo: does this have an attr
    // count check?), or as a thread hazard, so neither it nor
    // anything holding one may cross a thread boundary.
    case A::kZeroType:
      if (cls_ctx) {
        sm->CurrentScope->FindHeadSymbol(*cls_ctx->Name)->IsDirectlyZeroType = true;
        cls_ctx->ZeroTypeAnnotation = this;
      }
      break;
    case A::kThreadHazard:
      if (cls_ctx) { sm->CurrentScope->FindHeadSymbol(*cls_ctx->Name)->IsDirectlyThreadHazard = true; }
      break;

    // Mark a function's llvm inlining (which kind is read off
    // "Builtin" at codegen), or its temperature.
    case A::kLlvmInline:
    case A::kLlvmAlwaysInline:
    case A::kLlvmNoInline:
      if (fun_ctx) { fun_ctx->InlineAnnotation = this; }
      break;
    case A::kLlvmHot:
    case A::kLlvmCold:
      if (fun_ctx) { fun_ctx->TemperatureAnnotation = this; }
      break;

    // A custom annotation, "!annotation" (stage 4), "!cfg" (before
    // analysis) and "!versioned" (documentation) do nothing here.
    default:
      break;
  }
}

auto AnnotationAst::_ResolveBuiltin(
  ScopeManager *sm) -> bool {
  const auto sym = sm->CurrentScope->FindVarSymbolOutermost(*Name).first;
  if (sym == nullptr) { return false; }
  Builtin = analyse::utils::annotation_utils::BuiltinAnnotationOf(sym->FqName()->ToString());
  _BuiltinResolved = true;
  return true;
}

auto AnnotationAst::Stage7_AnalyseSemantics(
  ScopeManager *sm, CompilerMetaData *meta) -> void {
  IMPORT_UTILS;
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
  // annotation). The target function is shown from its own scope,
  // which can be another file than the annotation.
  const auto overload = fn_ptr->Target();
  RaiseIf<SppAnnotationTargetNotAnAnnotationError>(
    not overload->GetAnnotationInfo(),
    ERR_ARGS(*this, *overload));

  // Stamp the overload as the "target" of this annotation. This
  // will be used for extensively for custom annotations.
  _Target = overload;
}

auto AnnotationAst::Stage9_CompTimeResolve(
  ScopeManager *sm, CompilerMetaData *meta) -> void {
  IMPORT_UTILS;
  using annotation_utils::AnnotationInfo;

  // Load up different asts casts that an annotation may apply to.
  // These can receive the property fine-tune updates based on the
  // annotation.
  const auto annotation_info = _Target->GetAnnotationInfo();
  const auto outer_mod_ctx = _Ctx->GetAstCtx()->To<ModulePrototypeAst>();
  const auto outer_sup_ctx = _Ctx->GetAstCtx()->To<SupPrototypeFunctionsAst>();
  const auto outer_ext_ctx = _Ctx->GetAstCtx()->To<SupPrototypeExtensionAst>();

  // The mask reads sup constants (attached in stage 5.5) and runs
  // "cmp" functions (typed in stage 9), so it cannot be checked
  // before the stage 5 effects. Each of those only modifies a
  // function or a class, whichever its annotation applies to.
  // Evaluate the context that this annotation can be applied to.
  const auto annotation_scope_name = INJECT_CODE("std::annotations", ParseExpression);
  const auto annotation_scope = const_cast<Scope*>(
    sm->CurrentScope->FindNsScope(annotation_scope_name.get()));
  auto tm = sm->NewManagerAt(annotation_scope);

  const auto allowed_ctx = [&] {
    const auto _meta_guard = MetaGuard(meta);
    annotation_info->Definition->FnArgGroup->At("target")->Val->Stage7_AnalyseSemantics(&tm, meta);
    annotation_info->Definition->FnArgGroup->At("target")->Val->Stage9_CompTimeResolve(&tm, meta);
    const auto result = std::move(meta->CompTimeResult);
    return result->To<IntegerLiteralAst>()->CppVal<std::uint64_t>();
  }();

  const auto target = annotation_info->Definition->FnArgGroup->At("target");

  // Error for incompatible asts when classes are not valid
  // targets.
  RaiseIf<SppCalledAnnotationAppliedToInvalidAstError>(
    _Ctx->To<ClassPrototypeAst>() and not(allowed_ctx & AnnotationInfo::kClassContext),
    ERR_ARGS(*_Ctx, *this, *target));

  // Error for incompatible asts when free functions are not
  // valid targets.
  RaiseIf<SppCalledAnnotationAppliedToInvalidAstError>(
    _Ctx->To<FunctionPrototypeAst>() and outer_mod_ctx and not(allowed_ctx & AnnotationInfo::kFunctionCtx),
    ERR_ARGS(*_Ctx, *this, *target));

  // Error for incompatible asts when methods are not valid
  // targets.
  RaiseIf<SppCalledAnnotationAppliedToInvalidAstError>(
    _Ctx->To<FunctionPrototypeAst>() and outer_sup_ctx and not(allowed_ctx & AnnotationInfo::kMethodCtx),
    ERR_ARGS(*_Ctx, *this, *target));

  // Error for incompatible asts when overriding methods are
  // not valid targets.
  RaiseIf<SppCalledAnnotationAppliedToInvalidAstError>(
    _Ctx->To<FunctionPrototypeAst>() and outer_ext_ctx and not(allowed_ctx & AnnotationInfo::kExtensionContext),
    ERR_ARGS(*_Ctx, *this, *target));

  // Error for incompatible asts when type statements are not
  // valid targets.
  RaiseIf<SppCalledAnnotationAppliedToInvalidAstError>(
    _Ctx->To<TypeStatementAst>() and not(allowed_ctx & AnnotationInfo::kTypeStmtCtx),
    ERR_ARGS(*_Ctx, *this, *target));

  // Error for incompatible asts when cmp statements are not
  // valid targets.
  RaiseIf<SppCalledAnnotationAppliedToInvalidAstError>(
    _Ctx->To<CmpStatementAst>() and not(allowed_ctx & AnnotationInfo::kCmpStmtCtx),
    ERR_ARGS(*_Ctx, *this, *target));
}

SPP_MOD_END
