module;
#include <spp/macros.hpp>
#include <spp/analyse/macros.hpp>

module spp.asts.object_initializer_ast;
import spp.analyse.errors.semantic_error;
import spp.analyse.errors.semantic_error_builder;
import spp.analyse.scopes.scope;
import spp.analyse.scopes.scope_manager;
import spp.analyse.scopes.symbols;
import spp.analyse.utils.type_members;
import spp.analyse.utils.type_predicates;
import spp.analyse.utils.type_utils;
import spp.asts.class_attribute_ast;
import spp.asts.class_implementation_ast;
import spp.asts.class_prototype_ast;
import spp.asts.convention_ast;
import spp.asts.identifier_ast;
import spp.asts.object_initializer_argument_ast;
import spp.asts.object_initializer_argument_group_ast;
import spp.asts.object_initializer_argument_keyword_ast;
import spp.asts.token_ast;
import spp.asts.type_ast;
import spp.asts.meta.compiler_meta_data;
import spp.asts.utils.ast_utils;
import spp.codegen.llvm_alloca;
import spp.codegen.llvm_layout;
import spp.codegen.llvm_sym_info;
import spp.codegen.llvm_type;
import spp.codegen.llvm_variant;
import spp.utils.algorithms;
import spp.utils.uid;
import genex;
import llvm;

SPP_MOD_BEGIN
spp::asts::ObjectInitializerAst::ObjectInitializerAst(
  decltype(Type) type,
  decltype(ArgGroup) &&arg_group) :
  Type(std::move(type)),
  ArgGroup(std::move(arg_group)) {
  SPP_SET_AST_TO_DEFAULT_IF_NULLPTR(this->ArgGroup);
  Source.OriginalType = AstClone(Type);
}

spp::asts::ObjectInitializerAst::~ObjectInitializerAst() = default;

auto spp::asts::ObjectInitializerAst::PosStart() const
  -> std::size_t {
  // Use the type.
  return Source.OriginalType->PosStart();
}

auto spp::asts::ObjectInitializerAst::PosEnd() const
  -> std::size_t {
  // Use the argument group.
  return ArgGroup->PosEnd();
}

auto spp::asts::ObjectInitializerAst::Clone() const
  -> Unique<Ast> {
  // Clone all the members of the ast.
  return MakeUnique<ObjectInitializerAst>(
    AstClone(Type),
    AstClone(ArgGroup));
}

auto spp::asts::ObjectInitializerAst::ToString() const
  -> Str {
  SPP_STRING_START;
  SPP_STRING_APPEND(Type);
  SPP_STRING_APPEND(ArgGroup);
  SPP_STRING_END;
}

auto spp::asts::ObjectInitializerAst::Stage7_AnalyseSemantics(
  analyse::scopes::ScopeManager *sm,
  meta::CompilerMetaData *meta)
  -> void {
  //
  using analyse::errors::SppSecondClassBorrowViolationError;
  using analyse::errors::SppObjectInitializerVariantError;
  using analyse::errors::SppObjectInitializerGeneratorError;
  using analyse::utils::type_predicates::IsTypeBorrowed;
  using analyse::utils::type_predicates::IsTypeVariant;
  using analyse::utils::type_utils::GetGenAndYieldTypes;

  // Get the base class symbol (no generics) and check it exists.
  {
    const auto _meta_guard = meta::MetaGuard(meta);
    meta->SkipTypeAnalysisGenericChecks = true;
    Type->WithoutGenerics()->Stage7_AnalyseSemantics(sm, meta);
  }

  // Check this type isn't a borrow violation.
  RaiseIf<SppSecondClassBorrowViolationError>(
    IsTypeBorrowed(*Type, *sm),
    {sm->CurrentScope}, ERR_ARGS(*this, *Source.OriginalType, "object initializer"));
  const auto named_cls_sym = sm->CurrentScope->GetTypeSymbol(Type->WithoutGenerics().get());

  // "Self(...)" names the class it stands for, and the attribute walk below reads that class's prototype - which a
  // stand-in symbol does not carry. An unbound generic parameter is prototype-less in the same way but links to the
  // dummy scope standing in for it rather than to a class, so "A()" is left alone and takes the generic path.
  const auto base_cls_sym = Type->IsSelfType() and named_cls_sym != nullptr
    ? named_cls_sym->AsClassSymbol()
    : named_cls_sym;

  // If the type is a variant type, prevent instantiation.
  RaiseIf<SppObjectInitializerVariantError>(
    IsTypeVariant(*base_cls_sym->FqName(), *sm->CurrentScope),
    {sm->CurrentScope}, ERR_ARGS(*Source.OriginalType));

  // Prepare the object initializer arguments.
  {
    const auto _meta_guard = meta::MetaGuard(meta);
    meta->ObjectInitType = Type->WithoutGenerics();
    ArgGroup->Stage6_PreAnalyseSemantics(sm, meta);
  }

  // Determine the generic inference source and target
  // values.
  auto generic_infer_source = ArgGroup->Args
    | genex::views::transform([sm, meta](auto const &x) {
      return MakePair(x->Name, x->Val->InferType(sm, meta));
    })
    | genex::to<Vec>();

  auto generic_infer_target = not base_cls_sym->IsGeneric
    ? base_cls_sym->Type->Impl->Members
    | genex::views::ptr
    | genex::views::cast_dynamic<ClassAttributeAst*>()
    | genex::views::transform([&](auto const &x) {
      return MakePair(x->Name, base_cls_sym->LinkedScope->GetTypeSymbol(x->Type.get())->FqName());
    })
    | genex::to<Vec>()
    : spp::Vec<std::pair<std::shared_ptr<IdentifierAst>, std::shared_ptr<TypeAst>>>();

  {
    const auto _meta_guard = meta::MetaGuard(meta);
    meta->InferSource = MakeShared<meta::GenericInferenceBindings>(
      generic_infer_source.begin(), generic_infer_source.end());
    meta->InferTarget = MakeShared<meta::GenericInferenceBindings>(
      generic_infer_target.begin(), generic_infer_target.end());
    Type->Stage7_AnalyseSemantics(sm, meta);
    Type = sm->CurrentScope->GetTypeSymbol(Type.get())->FqName();
  }

  // A generator cannot be initialized either.
  const auto [gen_type, _, _] = GetGenAndYieldTypes(
    *Type, *sm->CurrentScope, *Source.OriginalType, "object initializer", false);
  RaiseIf<SppObjectInitializerGeneratorError>(
    gen_type != nullptr, {sm->CurrentScope},
    ERR_ARGS(*Source.OriginalType, *gen_type));

  const auto _meta_guard = meta::MetaGuard(meta);
  meta->ObjectInitType = Type;
  ArgGroup->Stage7_AnalyseSemantics(sm, meta);
}

auto spp::asts::ObjectInitializerAst::Stage8_CheckMemory(
  analyse::scopes::ScopeManager *sm,
  meta::CompilerMetaData *meta)
  -> void {
  // Check the memory of the object argument group.
  ArgGroup->Stage8_CheckMemory(sm, meta);
}

auto spp::asts::ObjectInitializerAst::Stage9_CompTimeResolve(
  analyse::scopes::ScopeManager *sm,
  meta::CompilerMetaData *meta)
  -> void {
  // Convert the inner elements to compile-time values.
  auto cmp_elems = ObjectInitializerArgumentGroupAst::NewEmpty();
  for (auto const &elem : ArgGroup->Args) {
    elem->Stage9_CompTimeResolve(sm, meta);
    auto cmp_arg = MakeUnique<ObjectInitializerArgumentKeywordAst>(elem->Name, nullptr, std::move(meta->CmpResult));
    cmp_elems->Args.EmplaceBack(std::move(cmp_arg));
  }

  // Wrap the compile-time array value.
  meta->CmpResult = MakeUnique<ObjectInitializerAst>(
    Type, std::move(cmp_elems));
}

auto spp::asts::ObjectInitializerAst::Stage11_CodeGen(
  analyse::scopes::ScopeManager *sm,
  meta::CompilerMetaData *meta,
  codegen::LlvmCtx *ctx)
  -> llvm::Value* {
  //
  using analyse::utils::type_members::GetAllAttrs;
  using analyse::utils::type_predicates::GetSuperimposedFatPointerFieldCount;

  // Create an empty struct based on the llvm type - will
  // never be a borrow so always stack allocated, not a
  // pointer.
  const auto uid = "." + spp::utils::Uid(this);
  const auto type_sym = sm->CurrentScope->GetTypeSymbol(Type.get());

  const auto llvm_type = codegen::GetLlvmType(*type_sym, ctx);
  SPP_ASSERT(llvm_type != nullptr);

  const auto attrs = GetAllAttrs(*type_sym->FqName(), *sm->CurrentScope);
  const auto attr_names = attrs
    | spp::views::tuple_nth<0>
    | genex::to<Vec>();

  // A type carrying no value has no value to build, so
  // prevent any code generation (or further GEPs into it).
  if (codegen::IsValuelessType(llvm_type)) { return nullptr; }

  // Types with no attributes have nothing to fill in, so
  // they initialize to their zero value. This covers the
  // compiler-known primitives (which don't even lower to
  // structs) and the fat pointer types. Also the base
  // cases for the recursive default initialization.
  if (attr_names.IsEmpty()) { return llvm::Constant::getNullValue(llvm_type); }

  // A class superimposing "Gen"/"GenOnce"/a "FunXXX" gets
  // that interface's fat-pointer fields prepended ahead of
  // its own declared attributes. An object initializer only
  // ever fills in the class's own attributes, never the
  // synthesized fields, so every declared index has to be
  // shifted past them.
  const auto fat_pointer_field_count = GetSuperimposedFatPointerFieldCount(
    *type_sym->FqName(), *sm->CurrentScope);

  // Where an argument's attribute sits in the type's own
  // declaration order. Every argument names an attribute -
  // analysis rejects a name that is not one - so the search
  // always finds it.
  const auto spp_attr_index_of = [&](IdentifierAst const &name) {
    const auto index = genex::position(attr_names, [&name](auto const &attr_name) { return *attr_name == name; });
    SPP_ASSERT(index >= 0);
    return static_cast<std::size_t>(index);
  };

  // The physical field order isn't the declaration order,
  // because the S++ layout re-orders the fields to minimize
  // padding, so every attribute's index has to be resolved
  // through the type's field index map.
  const auto llvm_attr_index = [&](const std::size_t attr_index) {
    return codegen::GetPhysicalFieldIndex(
      *type_sym->LlvmInfo, fat_pointer_field_count + attr_index);
  };

  // Runtime pathway.
  if (not ctx->InConstantContext) {
    // Every argument is generated up front, paired with the
    // physical field it fills, because whether the aggregate
    // as a whole is constant cannot be known until they have
    // been.
    auto arg_values = Vec<Pair<std::uint32_t, llvm::Value*>>();
    arg_values.Reserve(ArgGroup->Args.Len());
    for (auto const &arg : ArgGroup->Args) {
      // No value for Void type arguments (from the generic
      // implementations), so skip them.
      auto val = arg->Val->Stage11_CodeGen(sm, meta, ctx);
      if (val == nullptr) { continue; }

      // An attribute whose type is a variant takes any one of
      // its member types, and the value has to be tagged and
      // widened on the way in - the same coercion a by-value
      // argument gets at a function call.
      const auto attr_index = spp_attr_index_of(*arg->Name);
      if (const auto attr_type_sym = spp::get<1>(attrs[attr_index]); attr_type_sym != nullptr) {
        val = codegen::CoerceToVariant(
          val, *attr_type_sym->FqName(), *arg->Val->InferType(sm, meta),
          *sm->CurrentScope, "obj_init.variant" + uid, ctx);
      }

      arg_values.EmplaceBack(MakePair(llvm_attr_index(attr_index), val));
    }

    // If every field is filled by a constant of the right
    // type then so is the aggregate, and it can be produced
    // as a value rather than materialised.
    const auto llvm_struct_type = llvm::dyn_cast<llvm::StructType>(llvm_type);
    auto llvm_ct_fields = Vec<llvm::Constant*>(
      llvm_struct_type != nullptr ? llvm_struct_type->getNumElements() : 0uz, nullptr);
    auto all_fields_constant = llvm_struct_type != nullptr and arg_values.Len() == llvm_ct_fields.Len();

    if (all_fields_constant) {
      for (auto const &[index, val] : arg_values) {
        if (not llvm::isa<llvm::Constant>(val) or val->getType() != llvm_struct_type->getElementType(index)) {
          all_fields_constant = false;
          break;
        }
        llvm_ct_fields[index] = llvm::cast<llvm::Constant>(val);
      }
    }

    if (all_fields_constant and genex::all_of(llvm_ct_fields, [](auto const *f) { return f != nullptr; })) {
      return llvm::ConstantStruct::get(llvm_struct_type, llvm_ct_fields.ToStdVector());
    }

    // Set each field value in the aggregate.
    const auto aggregate = codegen::LlvmEntryAlloca(
      llvm_type, "obj_init.aggregate" + uid, ctx);
    for (auto const &[index, val] : arg_values) {
      const auto attr_ptr = ctx->Builder.CreateStructGEP(
        llvm_type, aggregate, index, "obj_init.field" + uid);
      SPP_ASSERT(attr_ptr != nullptr);
      ctx->Builder.CreateStore(val, attr_ptr);
    }

    // Return the aggregate.
    SPP_ASSERT(aggregate != nullptr);
    return ctx->Builder.CreateLoad(llvm_type, aggregate, "obj_init.result" + uid);
  }

  // Set each field value in the constant, indexed by its
  // physical position in the struct. The vector is sized
  // by the struct rather than by the attribute count
  // (for fat pointer shifting).
  const auto struct_type = llvm::cast<llvm::StructType>(llvm_type);
  auto comp_fields = Vec<llvm::Constant*>(struct_type->getNumElements(), nullptr);
  for (auto const &arg : ArgGroup->Args) {
    const auto comp_val = arg->Val->Stage11_CodeGen(sm, meta, ctx);
    if (comp_val == nullptr) { continue; }
    comp_fields[llvm_attr_index(spp_attr_index_of(*arg->Name))] = llvm::cast<llvm::Constant>(comp_val);
  }

  // Anything the arguments did not cover - a synthesized
  // fat-pointer field, or an attribute this initializer
  // leaves out, still needs a value, because a constant has
  // to give one for every field.
  for (auto i = 0uz; i < comp_fields.Len(); ++i) {
    if (comp_fields[i] != nullptr) { continue; }
    comp_fields[i] = llvm::Constant::getNullValue(
      struct_type->getElementType(static_cast<unsigned>(i)));
  }

  // Return the constant struct.
  return llvm::ConstantStruct::get(struct_type, comp_fields.ToStdVector());
}

auto spp::asts::ObjectInitializerAst::InferType(
  analyse::scopes::ScopeManager *sm,
  meta::CompilerMetaData *)
  -> Shared<TypeAst> {
  // The type of the object initializer is the type being
  // initialized. The conventions are added for dummy types
  // being created into values during other ast's analysis.
  // Types cannot be instantiated as borrows in user code.
  // Todo: tidy this by splitting into lines.
  return sm->CurrentScope->GetTypeSymbol(Type.get())->FqName()->WithConvention(AstClone(Type->GetConvention()));
}

auto spp::asts::ObjectInitializerAst::InferTypeForDisplay(
  analyse::scopes::ScopeManager *,
  meta::CompilerMetaData *)
  -> Shared<TypeAst> {
  // Use the source original type.
  return Source.OriginalType;
}

auto spp::asts::ObjectInitializerAst::SubstituteGenericsExpr(
  Vec<GenericArgumentAst*> const &args) const
  -> Shared<ExpressionAst> {
  // The initialiser names its type outright, and each
  // of its arguments is an expression in its own right.
  auto arg_group = AstClone(ArgGroup);
  for (auto const &arg : arg_group->Args) { arg->Val = AstClone(arg->Val->SubstituteGenericsExpr(args)); }
  return MakeShared<ObjectInitializerAst>(Type->SubstituteGenerics(args), std::move(arg_group));
}

auto spp::asts::ObjectInitializerAst::IsAllowedInDefault() const
  -> bool {
  // Check the argument group for validity of being used
  // in the default context.
  return ArgGroup->IsAllowedInDefault();
}

SPP_MOD_END
