module;
#include <spp/macros.hpp>
#include <spp/analyse/macros.hpp>

module spp.asts.object_initializer_ast;
import spp.analyse.errors.semantic_error;
import spp.analyse.errors.semantic_error_builder;
import spp.analyse.scopes.scope;
import spp.analyse.scopes.scope_manager;
import spp.analyse.scopes.symbols;
import spp.analyse.utils.marker_sups;
import spp.analyse.utils.self_type;
import spp.analyse.utils.type_members;
import spp.analyse.utils.type_predicates;
import spp.analyse.utils.type_resolution;
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
import spp.asts.type_identifier_ast;
import spp.asts.meta.compiler_meta_data;
import spp.asts.utils.ast_utils;
import spp.codegen.llvm_alloca;
import spp.codegen.llvm_fn;
import spp.codegen.llvm_layout;
import spp.codegen.llvm_sym_info;
import spp.codegen.llvm_type;
import spp.codegen.llvm_variant;
import spp.utils.algorithms;
import spp.utils.uid;
import genex;
import llvm;

SPP_MOD_BEGIN
ObjectInitializerAst::ObjectInitializerAst(
  decltype(Type) type,
  decltype(ArgGroup) &&arg_group) :
  Type(std::move(type)),
  ArgGroup(std::move(arg_group)) {
  SPP_SET_AST_TO_DEFAULT_IF_NULLPTR(this->ArgGroup);
  Source.OriginalType = AstClone(Type);
}

ObjectInitializerAst::~ObjectInitializerAst() = default;

auto ObjectInitializerAst::PosStart() const -> std::size_t {
  // Use the type.
  return Source.OriginalType->PosStart();
}

auto ObjectInitializerAst::PosEnd() const -> std::size_t {
  // Use the argument group.
  return ArgGroup->PosEnd();
}

auto ObjectInitializerAst::Clone() const -> Unique<Ast> {
  // Clone all the members of the ast. The constructor records the
  // type it is given as the written one, but "Type" may already be
  // the resolved type by now, so the written one is carried over.
  auto ast = MakeUnique<ObjectInitializerAst>(
    AstClone(Type),
    AstClone(ArgGroup));
  ast->Source = Source;
  return ast;
}

auto ObjectInitializerAst::ToString() const -> Str {
  SPP_STRING_START;
  SPP_STRING_APPEND(Type);
  SPP_STRING_APPEND(ArgGroup);
  SPP_STRING_END;
}

auto ObjectInitializerAst::Stage7_AnalyseSemantics(
  ScopeManager *sm, CompilerMetaData *meta) -> void {
  IMPORT_UTILS;

  // Get the base class symbol (no generics) and check it exists.
  {
    const auto _meta_guard = MetaGuard(meta);
    meta->SkipTypeAnalysisGnChecks = true;
    Type->WithoutGns()->Stage7_AnalyseSemantics(sm, meta);
  }

  // Check this type isn't a borrow violation.
  RaiseIf<SppSecondClassBorrowViolationError>(
    type_predicates::IsTypeBorrowed(*Type, *sm),
    {sm->CurrentScope}, ERR_ARGS(*this, *Source.OriginalType, "object initializer"));
  const auto named_cls_sym = sm->CurrentScope->FindHeadSymbol(*Type);

  // "Self(...)" names the class it stands for, and the
  // attribute walk below reads that class's prototype - which
  // a stand-in symbol does not carry. An unbound generic
  // parameter is prototype-less in the same way but links to
  // the dummy scope standing in for it rather than to a
  // class, so "A()" is left alone and takes the generic path.
  const auto base_cls_sym = Type->IsSelfType() and named_cls_sym != nullptr
    ? named_cls_sym->AsBound()
    : named_cls_sym;

  // If the type is a variant type, prevent instantiation.
  RaiseIf<SppObjectInitializerVariantError>(
    type_predicates::IsTypeVariant(TypeRef::ForKindCheck(*base_cls_sym, *sm->CurrentScope), *sm->CurrentScope),
    {sm->CurrentScope}, ERR_ARGS(*Source.OriginalType));

  // A zero type has exactly one value, written as the type's name
  // ("None", not "None()"). The rule is about what the author
  // wrote: a generic parameter ("A()", or a "Self" standing for
  // one) may become a zero type, and is the only way to ask for a
  // value of whatever it becomes, so it is never an error - in the
  // template, or in an instance, where it is bound to an argument.
  if (Source.IsWritten) {
    auto const *written = named_cls_sym;
    if (written != nullptr and written->IsSelf() and written->LinkedSymbol() != nullptr) {
      written = written->LinkedSymbol();
    }
    // An alias resolves to what it names; a "Self" to the type it
    // was followed to above.
    auto const *resolved = TypeRef::Of(*Type, *sm->CurrentScope).Symbol;
    if (resolved == nullptr or resolved->IsSelf()) { resolved = written; }
    RaiseIf<SppObjectInitializerZeroTypeError>(
      written != nullptr and not written->IsGn() and resolved != nullptr and resolved->IsZeroType(),
      {sm->CurrentScope}, ERR_ARGS(*this, *Source.OriginalType));
  }

  // Prepare the object initializer arguments. The type is passed
  // as written, generics and all: the class is found without
  // them, but an argument that is an overloaded call reads the
  // attribute's type through them ("MyType[T=Str](a=g())").
  {
    const auto _meta_guard = MetaGuard(meta);
    meta->ObjectInitType = Type;
    ArgGroup->Stage6_PreAnalyseSemantics(sm, meta);
  }

  // The attributes' types are what the type's arguments are inferred from: each argument's type (pointed at the
  // argument, so a conflict names where it came from rather than "<generated code>") against the declared type of the
  // attribute it initialises.
  auto equations = Vec<Tup<Shared<IdentifierAst>, Shared<TypeAst>, Shared<TypeAst>>>();
  if (not base_cls_sym->IsGn()) {
    for (const auto attr : base_cls_sym->Type->Impl->Members
         | genex::views::ptr
         | genex::views::cast_dynamic<ClassAttributeAst*>()) {
      const auto attr_sym = base_cls_sym->LinkedScope->FindTypeSymbol(attr->Type.get());
      if (attr_sym == nullptr) { continue; }
      for (auto const &arg : ArgGroup->Args) {
        if (arg->Name == nullptr or *arg->Name != *attr->Name) { continue; }
        equations.EmplaceBack(attr->Name, arg->Val->InferType(sm, meta)->WithSourceSpanAt(*arg->Val), attr_sym->FqName());
      }
    }
  }

  Type = self_type::SubstituteSelf(*Type, sm->CurrentScope->FindEnclosingSelfType(*meta).get(), *sm->CurrentScope)
    ->WithSourceSpanOf(*Type);
  Type->LastTypePart()->InferFromAttributes(std::move(equations));
  Type->Stage7_AnalyseSemantics(sm, meta);

  // A generator cannot be initialized either.
  auto const *const gen_sym = marker_sups::FindGenSup(
    TypeRef::Of(*Type, *sm->CurrentScope), *sm->CurrentScope, *Source.OriginalType,
    [&] { return Type; }, "object initializer", false).Symbol;
  if (gen_sym != nullptr) {
    const auto gen_type = gen_sym->FqName();
    Raise<SppObjectInitializerGeneratorError>({sm->CurrentScope}, ERR_ARGS(*Source.OriginalType, *gen_type));
  }

  const auto _meta_guard = MetaGuard(meta);
  meta->ObjectInitType = Type;
  ArgGroup->Stage7_AnalyseSemantics(sm, meta);
}

auto ObjectInitializerAst::Stage8_CheckMemory(
  ScopeManager *sm, CompilerMetaData *meta) -> void {
  // Check the memory of the object argument group.
  ArgGroup->Stage8_CheckMemory(sm, meta);
}

auto ObjectInitializerAst::Stage9_CompTimeResolve(
  ScopeManager *sm, CompilerMetaData *meta) -> void {
  // Convert the inner elements to compile-time values.
  auto cmp_elems = ObjectInitializerArgumentGroupAst::NewEmpty();
  for (auto const &elem : ArgGroup->Args) {
    elem->Stage9_CompTimeResolve(sm, meta);
    auto cmp_arg = MakeUnique<ObjectInitializerArgumentKeywordAst>(
      elem->Name, nullptr, std::move(meta->CompTimeResult));
    cmp_elems->Args.EmplaceBack(std::move(cmp_arg));
  }

  // Wrap the compile-time array value.
  meta->CompTimeResult = MakeUnique<ObjectInitializerAst>(
    Type, std::move(cmp_elems));
}

auto ObjectInitializerAst::Stage11_CodeGen(
  ScopeManager *sm, CompilerMetaData *meta, codegen::LlvmCtx *ctx) -> llvm::Value* {
  IMPORT_UTILS_AND_UID;

  // Create an empty struct based on the llvm type - will
  // never be a borrow so always stack allocated, not a
  // pointer.
  const auto uid = "." + Uid();
  const auto type_sym = sm->CurrentScope->FindTypeSymbol(Type.get());

  const auto llvm_type = codegen::GetLlvmType(*type_sym, ctx);
  SPP_ASSERT(llvm_type != nullptr);

  const auto attrs = type_members::GetAllAttrs(*type_sym);
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
  const auto fat_pointer_field_count = type_members::GetSuperimposedFatPointerFieldCount(
    *type_sym);

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
      if (auto const &attr_ref = spp::get<1>(attrs[attr_index]); attr_ref.Symbol != nullptr) {
        val = codegen::CoerceToFnValue(
          val, attr_ref,
          arg->Val->InferTypeRef(sm, meta), *sm, ctx);
        val = codegen::CoerceToVariant(
          val, attr_ref,
          arg->Val->InferTypeRef(sm, meta), *sm->CurrentScope, "obj_init.variant" + uid, ctx);
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

auto ObjectInitializerAst::InferTypeRef(
  ScopeManager *sm, CompilerMetaData *) -> TypeRef {
  // The type being initialized, held as written.
  return TypeRef::Of(*Type, *sm->CurrentScope);
}

auto ObjectInitializerAst::ReadExpr(
  analyse::scopes::ExprSubst const &sub) const -> Shared<ExpressionAst> {
  // The initialiser names its type outright, and each
  // of its arguments is an expression in its own right.
  auto arg_group = AstClone(ArgGroup);
  for (auto const &arg : arg_group->Args) { arg->Val = AstClone(arg->Val->ReadExpr(sub)); }
  return MakeShared<ObjectInitializerAst>(Type->ReadExprType(sub), std::move(arg_group));
}

auto ObjectInitializerAst::IsAllowedInDefault() const -> bool {
  // Check the argument group for validity of being used
  // in the default context.
  return ArgGroup->IsAllowedInDefault();
}

SPP_MOD_END
