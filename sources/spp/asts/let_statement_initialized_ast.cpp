module;
#include <spp/macros.hpp>
#include <spp/analyse/macros.hpp>

module spp.asts.let_statement_initialized_ast;
import spp.analyse.errors.semantic_error;
import spp.analyse.errors.semantic_error_builder;
import spp.analyse.scopes.scope_manager;
import spp.analyse.scopes.symbols;
import spp.analyse.utils.expr_utils;
import spp.analyse.utils.fn_values;
import spp.analyse.utils.type_compare;
import spp.analyse.utils.type_resolution;
import spp.asts.identifier_ast;
import spp.asts.local_variable_ast;
import spp.asts.local_variable_destructure_array_ast;
import spp.asts.local_variable_destructure_attribute_binding_ast;
import spp.asts.local_variable_destructure_object_ast;
import spp.asts.local_variable_destructure_skip_multiple_arguments_ast;
import spp.asts.local_variable_destructure_tuple_ast;
import spp.asts.local_variable_single_identifier_alias_ast;
import spp.asts.local_variable_single_identifier_ast;
import spp.asts.token_ast;
import spp.asts.type_ast;
import spp.asts.meta.compiler_meta_data;
import spp.asts.utils.ast_utils;
import spp.lex.tokens;

SPP_MOD_BEGIN
LetStatementInitializedAst::LetStatementInitializedAst(
  decltype(TokLet) &&tok_let,
  decltype(Var) &&var,
  decltype(Type) type,
  decltype(TokAssign) &&tok_assign,
  decltype(Val) &&val) :
  TokLet(std::move(tok_let)),
  Var(std::move(var)),
  Type(std::move(type)),
  TokAssign(std::move(tok_assign)),
  Val(std::move(val)) {
  using lex::SppTokenType;
  SPP_SET_AST_TO_DEFAULT_IF_NULLPTR(
    this->TokLet, SppTokenType::KW_LET, "let");
  SPP_SET_AST_TO_DEFAULT_IF_NULLPTR(
    this->TokAssign, SppTokenType::TK_ASSIGN, "=");
}

LetStatementInitializedAst::~LetStatementInitializedAst() = default;

auto LetStatementInitializedAst::PosStart() const -> std::size_t {
  // Use the "let" token.
  return TokLet->PosStart();
}

auto LetStatementInitializedAst::PosEnd() const -> std::size_t {
  // Use the value.
  return Val->PosEnd();
}

auto LetStatementInitializedAst::Clone() const -> Unique<Ast> {
  // Clone all the members of the ast.
  return MakeUnique<LetStatementInitializedAst>(
    AstClone(TokLet),
    AstClone(Var),
    AstClone(Type),
    AstClone(TokAssign),
    AstClone(Val));
}

auto LetStatementInitializedAst::ToString() const -> Str {
  SPP_STRING_START;
  SPP_STRING_APPEND(TokLet).append(" ");
  SPP_STRING_APPEND(Var);
  SPP_STRING_APPEND(Type).append(" ");
  SPP_STRING_APPEND(TokAssign).append(" ");
  SPP_STRING_APPEND(Val);
  SPP_STRING_END;
}

namespace {
  /**
   * The names a destructure binds, as the code after it sees them. "ExtractNames" gives an aliased binding by the
   * attribute it reads ("val" for "val as val1"), which is what matching the attributes needs, but two such bindings
   * of the one attribute ("Some(val as a), Some(val as b)") bind different names.
   * @param var The destructure (or single identifier) to walk.
   * @param out The bound names, appended to in binding order.
   */
  auto CollectBoundNames(LocalVariableAst const &var, spp::Vec<IdentifierAst const*> &out) -> void {
    if (auto const *single = var.To<LocalVariableSingleIdentifierAst>(); single != nullptr) {
      out.EmplaceBack(single->Alias != nullptr ? single->Alias->Name.get() : single->Name.get());
    }
    else if (auto const *tup = var.To<LocalVariableDestructureTupleAst>(); tup != nullptr) {
      for (auto const &elem : tup->Elems) { CollectBoundNames(*elem, out); }
    }
    else if (auto const *arr = var.To<LocalVariableDestructureArrayAst>(); arr != nullptr) {
      for (auto const &elem : arr->Elems) { CollectBoundNames(*elem, out); }
    }
    else if (auto const *obj = var.To<LocalVariableDestructureObjectAst>(); obj != nullptr) {
      for (auto const &elem : obj->Elems) { CollectBoundNames(*elem, out); }
    }
    else if (auto const *attr = var.To<LocalVariableDestructureAttributeBindingAst>(); attr != nullptr) {
      CollectBoundNames(*attr->Val, out);
    }
    else if (auto const *rest = var.To<LocalVariableDestructureSkipMultipleArgumentsAst>();
      rest != nullptr and rest->Binding != nullptr) {
      CollectBoundNames(*rest->Binding, out);
    }
  }
}

auto LetStatementInitializedAst::Stage7_AnalyseSemantics(
  ScopeManager *sm, CompilerMetaData *meta) -> void {
  IMPORT_UTILS;

  // An explicit type can only be applied if the left-hand-side
  // is a single identifier.
  RaiseIf<SppInvalidLocalVariableTypeAnnotationError>(
    Type != nullptr and Var->To<LocalVariableSingleIdentifierAst>() == nullptr,
    {sm->CurrentScope}, ERR_ARGS(*Type, *Var));

  // Analyse the type if it has been given.
  const auto written_type = Type;
  if (Type != nullptr) {
    Type = type_resolution::AnalyseWrittenType(*Type, *sm, *meta);
  }

  // Add the type into the return type overload resolver.
  const auto _meta_guard = MetaGuard(meta);
  meta->ReturnTypeOverloadResolverType = Type != nullptr
    ? MakeShared<TypeRef>(TypeRef::Of(*Type, *sm->CurrentScope))
    : nullptr;

  // Check the value is a valid expression type.
  Val->Stage7_AnalyseSemantics(sm, meta);
  RaiseIf<SppInvalidPrimaryExpressionError>(
    not expr_utils::IsPrimaryExprTypeValid(*Val, *sm),
    {sm->CurrentScope}, ERR_ARGS(*Val.get()));

  meta->AssignmentTarget = Var->ExtractName();

  // Ensure the value's type matches the type (if given),
  // including variant matching.
  if (Type != nullptr) {
    meta->AssignmentTargetType = Type;
    const auto val_type = Val->InferType(sm, meta);
    if (not type_compare::Assignable(*Type, *val_type, *sm->CurrentScope, *sm->CurrentScope)) {
      // Shown as written, with the type it resolved to beside it where they differ ("S32 (aka ...)").
      const auto resolved = TypeRef::Of(*Type, *sm->CurrentScope).AstIn(*sm->CurrentScope);
      const auto shown = resolved != nullptr ? resolved->WithSourceSpanOf(*written_type) : Type;
      Raise<SppTypeMismatchError>({sm->CurrentScope}, ERR_ARGS(*written_type, *shown, *Val, *val_type));
    }

    // A function named as the value stands for the overload
    // the declared type asks for.
    fn_values::InstantiateFnValue(
      TypeRef::Of(*val_type, *sm->CurrentScope),
      TypeRef::Of(*Type, *sm->CurrentScope), sm, meta);
  }

  // A destructure binds each name once. A repeat was bound
  // over the first silently, in a "let" and in a case pattern
  // (which lowers to one of these) alike.
  auto bound_names = Vec<IdentifierAst const*>();
  CollectBoundNames(*Var, bound_names);
  for (auto i = 0uz; i < bound_names.Len(); ++i) {
    for (auto j = i + 1; j < bound_names.Len(); ++j) {
      RaiseIf<SppIdentifierDuplicateError>(
        *bound_names[i] == *bound_names[j], {sm->CurrentScope},
        ERR_ARGS(*bound_names[i], *bound_names[j], "destructure binding"));
    }
  }

  meta->LetStatementExplicitType = Type;
  meta->LetStatementValue = Val.get();
  Var->Stage7_AnalyseSemantics(sm, meta);
}

auto LetStatementInitializedAst::Stage8_CheckMemory(
  ScopeManager *sm, CompilerMetaData *meta) -> void {
  // Check the variable's memory (which in turn checks the
  // value's memory - must be done this way for destructuring).
  const auto _meta_guard = MetaGuard(meta);
  meta->AssignmentTarget = Var->ExtractName();
  meta->LetStatementValue = Val.get();
  Var->Stage8_CheckMemory(sm, meta);
}

auto LetStatementInitializedAst::Stage9_CompTimeResolve(
  ScopeManager *sm, CompilerMetaData *meta) -> void {
  // Fix variable shadowing, where a newer version of the symbol
  // is gotten because stage7 added it, when we are trying to use
  // the original.
  auto shadowed = Vec<Shared<VariableSymbol>>();
  for (auto const &target : Var->ExtractNames()) {
    if (auto sym = sm->CurrentScope->RemVarSymbol(target.get()); sym != nullptr) {
      shadowed.EmplaceBack(std::move(sym));
    }
  }

  Val->Stage9_CompTimeResolve(sm, meta);
  for (auto const &sym : shadowed) { sm->CurrentScope->AddVarSymbol(sym); }

  // Assign the comptime value to the variable.
  const auto _meta_guard = MetaGuard(meta);
  meta->AssignmentTarget = Var->ExtractName();
  meta->LetStatementValue = Val.get();
  Var->Stage9_CompTimeResolve(sm, meta);
}

auto LetStatementInitializedAst::Stage11_CodeGen(
  ScopeManager *sm, CompilerMetaData *meta, codegen::LlvmCtx *ctx) -> llvm::Value* {
  // Setup a lot of meta information for the local variable to
  // correctly generate the value.
  // Todo: Inconsistent with lower level stages?
  const auto _meta_guard = MetaGuard(meta);
  meta->AssignmentTarget = Var->ExtractName();
  meta->AssignmentTargetType = Type;
  auto const *const single = Var->To<LocalVariableSingleIdentifierAst>();
  const auto val_type = Type ? Type
    : single != nullptr ? single->InferValueType(*Val, sm, meta)
    : Val->InferType(sm, meta);

  meta->AssignmentTargetType = val_type;
  meta->LetStatementExplicitType = val_type;
  meta->LetStatementValue = Val.get();

  // Delegate the code generation to the variable, after setting
  // up the meta. Note that the "alloca" is returned even though
  // this isn't an expression, for parent nodes that might need it.
  // It's a hacky solution that should live on "meta" but no harm
  // in doing it this way.
  const auto alloca = Var->Stage11_CodeGen(sm, meta, ctx);
  return alloca;
}

SPP_MOD_END
