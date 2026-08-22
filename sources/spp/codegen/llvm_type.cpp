module;
#include <spp/macros.hpp>

module spp.codegen.llvm_type;
import spp.analyse.scopes.scope_manager;
import spp.analyse.scopes.symbols;
import spp.analyse.utils.type_utils;
import spp.asts.boolean_literal_ast;
import spp.asts.class_prototype_ast;
import spp.asts.function_parameter_ast;
import spp.asts.function_parameter_group_ast;
import spp.asts.function_prototype_ast;
import spp.asts.generic_argument_comp_ast;
import spp.asts.generic_argument_group_ast;
import spp.asts.generic_argument_type_ast;
import spp.asts.identifier_ast;
import spp.asts.integer_literal_ast;
import spp.asts.token_ast;
import spp.asts.type_ast;
import spp.asts.type_identifier_ast;
import spp.asts.generate.common_types_precompiled;
import spp.codegen.llvm_alloca;
import spp.codegen.llvm_ctx;
import spp.codegen.llvm_mangle;
import spp.codegen.llvm_size;
import spp.lex.tokens;
import spp.utils.types;
import genex;
import llvm;
import std;

const spp::Vec<spp::Str> kVoidParts = {"std", "void", "Void"};
const spp::Vec<spp::Str> kBoolParts = {"std", "boolean", "Bool"};
const spp::Vec<spp::Str> kSizedIntegerParts = {"std", "num", "sized_integer", "SizedInteger"};
const spp::Vec<spp::Str> kSizedFloatParts = {"std", "num", "sized_floating_point", "SizedFloatingPoint"};
const spp::Vec<spp::Str> kArrParts = {"std", "array", "Arr"};
const spp::Vec<spp::Str> kGeneratedParts = {"std", "generator", "Generated"};
const spp::Vec<spp::Str> kGenParts = {"std", "generator", "Gen"};
const spp::Vec<spp::Str> kGenOnceParts = {"std", "generator", "GenOnce"};
const spp::Vec<spp::Str> kVarParts = {"std", "variant", "Var"};
const spp::Vec<spp::Str> kNonNullParts = {"std", "mem", "pointer", "NonNull"};

// Width of a variant's discriminant. Matches the "sizeof(std::size_t)" discriminator that "SizeOf" accounts for.
constexpr auto kVariantTagBits = 64u;

// Largest alignment a variant payload buffer will be built out of. Anything needing more than a 16 byte alignment is
// vector/extended precision territory, which the layout code does not model either.
constexpr auto kMaxVariantPayloadAlign = 16uz;

static auto GetFloatIntrinsic(const std::size_t bit_width) -> llvm::fltSemantics const& {
  switch (bit_width) {
    case 8: { return llvm::APFloatBase::IEEEhalf(); }
    case 16: { return llvm::APFloatBase::IEEEhalf(); }
    case 32: { return llvm::APFloatBase::IEEEsingle(); }
    case 64: { return llvm::APFloatBase::IEEEdouble(); }
    case 128: { return llvm::APFloatBase::IEEEquad(); }
    default: std::unreachable();
  }
  std::unreachable();
}

auto spp::codegen::GetFatPointerFields(
  asts::TypeAst const &type,
  analyse::scopes::Scope const &scope,
  LlvmCtx const *ctx)
  -> std::optional<Vec<llvm::Type*>> {
  //
  using analyse::utils::type_utils::IsTypeFunc;

  // "FunXXX" closures are represented by a { fn_ptr, env_ptr }
  // pair. The total field count (for example a stateful type
  // superimposing a FunXXX type, is mirrored in the function
  // "GetSuperimposedFatPointerFieldCount"; this function only
  // adds the LLVM-specific type materialization on top.
  const auto ptr_ty = llvm::PointerType::get(*ctx->Context, 0);
  if (IsTypeFunc(type, scope)) { return Vec<llvm::Type*>{ptr_ty, ptr_ty}; }
  return std::nullopt;
}

auto spp::codegen::RegisterLlvmTypeInfo(
  asts::ClassPrototypeAst const *cls_proto,
  analyse::scopes::ScopeManager const &sm,
  LlvmCtx const *ctx)
  -> void {
  // $ types are function "mock" types (a $-type generated per
  // function that superimposes n FunXXXs over itself). A function
  // used as a value is one of these mocks, so it lowers to the
  // same { fn_ptr, env_ptr } pair as the function type it extends.
  if (cls_proto->Name->IsCompilerGeneratedType()) {
    const auto mock_sym = cls_proto->GetClsSym();
    const auto ptr_ty = llvm::PointerType::get(*ctx->Context, 0);
    mock_sym->LlvmInfo->LlvmType = llvm::StructType::get(*ctx->Context, {ptr_ty, ptr_ty});
    return;
  }

  // Push the scope into the the registration function that accepts
  // a scope and context.
  RegisterLlvmTypeInfo(cls_proto->GetAstScope(), sm, ctx);
}

auto spp::codegen::RegisterLlvmTypeInfo(
  analyse::scopes::Scope const *scope,
  analyse::scopes::ScopeManager const &sm,
  LlvmCtx const *ctx)
  -> void {
  // Get the class symbol from the scope that owns it. This pulls
  // the correct generic instantiation for struct types.
  const auto cls_sym = scope->TySym;

  // For compiler known types, specialize the llvm type symbols.
  const auto parts = scope->Ancestors()
    | genex::views::drop_last(1)
    | genex::views::transform([](auto *x) { return x->NonGenericScope->NameAsString(); })
    | genex::views::reverse
    | genex::to<Vec>();

  // Lower S++ "Void" to the llvm "void" type.
  if (parts == kVoidParts) {
    cls_sym->LlvmInfo->LlvmType = llvm::Type::getVoidTy(*ctx->Context);
    return;
  }

  // Lower S++ "Bool" to the llvm "i1" type.
  if (parts == kBoolParts) {
    cls_sym->LlvmInfo->LlvmType = llvm::Type::getInt1Ty(*ctx->Context);
    return;
  }

  // Lower S++ "NonNull[T]" to a bare llvm pointer.
  if (parts == kNonNullParts) {
    cls_sym->LlvmInfo->LlvmType = llvm::PointerType::get(*ctx->Context, 0);
    return;
  }

  // Lower S++ "S/U[8|16|32|64|128]" to the llvm "i[8|16|32|64|128]" type (llvm integers carry no signedness).
  if (parts == kSizedIntegerParts) {
    const auto bit_width_ast = scope->TySym->FqName()->LastTypePart()->GnArgGroup->CompAt("w")->Val->To<
      asts::IntegerLiteralAst>();
    if (bit_width_ast == nullptr) { return; }
    const auto w = static_cast<unsigned>(std::stoi(bit_width_ast->Val->TokenData));;
    cls_sym->LlvmInfo->LlvmType = llvm::Type::getIntNTy(*ctx->Context, w);
    return;
  }

  // Lower S++ "F[8|16|32|64|128]" to the llvm "f[8|16|32|64|128]" type.
  if (parts == kSizedFloatParts) {
    const auto bit_width_ast = scope->TySym->FqName()->LastTypePart()->GnArgGroup->CompAt("w")->Val->To<
      asts::IntegerLiteralAst>();
    if (bit_width_ast == nullptr) { return; }
    const auto w = static_cast<unsigned>(std::stoi(bit_width_ast->Val->TokenData));;
    cls_sym->LlvmInfo->LlvmType = llvm::Type::getFloatingPointTy(*ctx->Context, GetFloatIntrinsic(w));
    return;
  }

  // Lower S++ Arr" to the llvm "[T * n]" type.
  if (parts == kArrParts) {
    const auto gn_arg_group = cls_sym->FqName()->LastTypePart()->GnArgGroup.get();
    const auto length_ast = gn_arg_group->CompAt("n")->Val->To<asts::IntegerLiteralAst>();
    const auto elem_sym = scope->GetTypeSymbol(gn_arg_group->TypeAt("T")->Val.get());
    if (length_ast != nullptr and elem_sym != nullptr) {
      if (elem_sym->LlvmInfo->LlvmType == nullptr and elem_sym->Type != nullptr) {
        RegisterLlvmTypeInfo(elem_sym->Type, sm, ctx);
      }
      if (const auto elem_llvm_type = GetLlvmType(*elem_sym, ctx); elem_llvm_type != nullptr) {
        cls_sym->LlvmInfo->LlvmType = llvm::ArrayType::get(elem_llvm_type, std::stoull(length_ast->Val->TokenData));
      }
    }
    return;
  }

  // "Generated[Yield]" ("send"'s return type) shares the { ptr, ptr }
  // shape too, but it is a private, compiler-internal type nothing
  // ever superimposes, so it's handled directly here by name rather
  // than through "GetFatPointerFields".
  if (parts == kGeneratedParts) {
    const auto ptr_ty = llvm::PointerType::get(*ctx->Context, 0);
    cls_sym->LlvmInfo->LlvmType = llvm::StructType::get(*ctx->Context, {ptr_ty, ptr_ty});
    return;
  }

  // A generator is the bare "llvm.coro.begin" handle, one pointer
  // wide.
  if (parts == kGenParts or parts == kGenOnceParts) {
    cls_sym->LlvmInfo->LlvmType = llvm::PointerType::get(*ctx->Context, 0);
    return;
  }

  // Lower the "Fun*" family to a { fn_ptr, env_ptr } fat
  // pointer. Allows for compatibility with closures too;
  // one uniform system for all function type storage.
  if (const auto fields = GetFatPointerFields(*cls_sym->FqName(), *scope, ctx); fields.has_value()) {
    cls_sym->LlvmInfo->LlvmType = llvm::StructType::get(*ctx->Context, fields->ToStdVector());
    return;
  }

  // If the type already exists in LLVM, skip.
  if (const auto llvm_type = llvm::StructType::getTypeByName(*ctx->Context, mangle::mangle_type_name(*cls_sym));
    llvm_type != nullptr) {
    cls_sym->LlvmInfo->LlvmType = llvm_type;
    return;
  }

  // Lower S++ "Var" (the "A or B" variant type) to a
  // { tag, payload } pair. This includes all sorts of
  // internal processing for tag setup.
  if (parts == kVarParts) {
    const auto struct_type = llvm::StructType::create(*ctx->Context, mangle::mangle_type_name(*cls_sym));
    cls_sym->LlvmInfo->LlvmType = struct_type;

    auto const &dl = ctx->Module->getDataLayout();
    auto max_size = 0uz;
    auto max_align = 1uz;

    // The members are named relative to the variant, so
    // they are measured from the variant's own scope rather
    // than from wherever the registration walk happens to
    // be.
    const auto member_sm = analyse::scopes::ScopeManager(
      sm.GlobalScope, const_cast<analyse::scopes::Scope*>(scope));

    for (auto const &member : analyse::utils::type_utils::DedupVariableInnerTypes(*cls_sym->FqName(), *scope)) {
      const auto member_sym = scope->GetTypeSymbol(member.get());
      if (member_sym == nullptr) { continue; }

      // A variant can be registered before its members
      // are, so lower any member still missing its llvm
      // type.
      if (member_sym->LlvmInfo->LlvmType == nullptr and member_sym->Type != nullptr) {
        RegisterLlvmTypeInfo(member_sym->Type, sm, ctx);
      }

      EnsureLlvmTypeComplete(*member_sym, member_sm, ctx);
      const auto member_llvm_type = GetLlvmType(*member_sym, ctx);
      if (member_llvm_type == nullptr or not member_llvm_type->isSized()) { continue; }
      max_size = std::max(max_size, dl.getTypeAllocSize(member_llvm_type).getFixedValue());
      max_align = std::max(max_align, dl.getABITypeAlign(member_llvm_type).value());
    }

    // Build the payload out of the widest integer any member
    // needs to be aligned to, rather than out of bytes: a
    // "[n x i8]" buffer is only ever byte aligned, so storing
    // a member into it would be under-aligned.
    const auto payload_elem_type = llvm::Type::getIntNTy(
      *ctx->Context, static_cast<unsigned>(std::min(max_align, kMaxVariantPayloadAlign) * 8));
    const auto payload_elem_size = dl.getTypeAllocSize(payload_elem_type).getFixedValue();
    const auto payload_type = llvm::ArrayType::get(
      payload_elem_type, (max_size + payload_elem_size - 1) / payload_elem_size);

    struct_type->setBody({GetVariantTagType(ctx), payload_type}, false);
    return;
  }

  // Empty struct, will fill in stage_10 when all attributes'
  // types have been generated.
  cls_sym->LlvmInfo->LlvmType = llvm::StructType::create(
    *ctx->Context, mangle::mangle_type_name(*cls_sym));
}

auto spp::codegen::GetLlvmType(
  analyse::scopes::TypeSymbol const &type_sym,
  LlvmCtx const *ctx)
  -> llvm::Type* {
  // A borrow is a pointer to the borrowee whatever the borrowee
  // is, so nothing has to be lowered to answer for it.
  if (type_sym.Convention != nullptr) { return llvm::PointerType::get(*ctx->Context, 0); }

  // Otherwise lower it now if nothing has yet. Types are minted
  // right through monomorphisation and code generation, so "has
  // something already registered this?" is not a question with
  // a stable answer - asking for the type is what makes it exist.
  if (type_sym.LlvmInfo->LlvmType == nullptr and ctx->Sm != nullptr) {
    EnsureLlvmTypeComplete(type_sym, *ctx->Sm, ctx);
  }
  return type_sym.LlvmInfo->LlvmType;
}

auto spp::codegen::EnsureLlvmTypeComplete(
  analyse::scopes::TypeSymbol const &type_sym,
  analyse::scopes::ScopeManager const &sm,
  LlvmCtx const *ctx)
  -> void {
  // A symbol that names another type without carrying its
  // prototype - "Self", which links to the class it stands
  // for (see "AddSelfTypeSym") - is completed as that class
  // and then adopts the result. It cannot be completed as
  // itself: there is no prototype on it to read a layout
  // from, and the guard below would turn it away.
  const auto linked_sym = type_sym.AsClassSymbol();
  if (linked_sym != &type_sym) {
    // Stand-ins can name each other - the "Self" of a scope
    // whose class scope carries another "Self" - and following
    // the chain would then never end. Same guard, and for the
    // same reason, as the layout walk below.
    static thread_local auto in_progress = Set<analyse::scopes::TypeSymbol const*>();
    if (not in_progress.insert(&type_sym).second) { return; }

    EnsureLlvmTypeComplete(*linked_sym, sm, ctx);
    type_sym.LlvmInfo->LlvmType = linked_sym->LlvmInfo->LlvmType;
    in_progress.erase(&type_sym);
    return;
  }

  // Nothing to complete without a prototype behind the symbol
  // (a bare generic parameter, say).
  if (type_sym.Type == nullptr) { return; }

  // Ensure the type is complete. This is an on-demand walk of
  // the type's definition, including fields.
  if (type_sym.LlvmInfo->LlvmType == nullptr) {
    if (type_sym.LinkedScope != nullptr) { RegisterLlvmTypeInfo(type_sym.LinkedScope, sm, ctx); }
    else { RegisterLlvmTypeInfo(type_sym.Type, sm, ctx); }

    // A symbol standing in for another type - a generic parameter
    // bound to one, "T" inside an instantiation - is not the
    // symbol the registration above writes to; that writes to the
    // one owned by the linked scope. Adopt the result, or this
    // symbol stays un-lowered however many times it is asked for,
    // which is what "AttachLlvmTypeInfo" does eagerly for the
    // aliases it can see at the end of Stage8. It cannot see these:
    // a binding minted while monomorphising did not exist yet.
    if (type_sym.LlvmInfo->LlvmType == nullptr
      and type_sym.LinkedScope != nullptr
      and type_sym.LinkedScope->TySym != nullptr) {
      type_sym.LlvmInfo->LlvmType = type_sym.LinkedScope->TySym->LlvmInfo->LlvmType;
    }
  }

  // Only a named struct can be half-built; everything else is
  // complete the moment it is lowered. A struct that is still
  // opaque is the placeholder a class gets at registration,
  // waiting for Stage10 to derive its body.
  const auto struct_type = llvm::dyn_cast_or_null<llvm::StructType>(type_sym.LlvmInfo->LlvmType);
  if (struct_type == nullptr or not struct_type->isOpaque()) { return; }

  // Laying the body out asks for the sizes of the attributes,
  // which comes back through here for each of them, so a type
  // that contains itself would recurse forever. Prevent this
  // with a guard.
  static thread_local auto in_progress = Set<llvm::StructType const*>();
  if (not in_progress.insert(struct_type).second) { return; }
  type_sym.Type->FillLlvmLayout(&sm, &type_sym, ctx);
  in_progress.erase(struct_type);
}

auto spp::codegen::IsValuelessType(
  llvm::Type const *type)
  -> bool {
  return type == nullptr or type->isVoidTy();
}

auto spp::codegen::GetLlvmTypeOf(
  asts::TypeAst const &type,
  analyse::scopes::Scope const &scope,
  LlvmCtx const *ctx)
  -> llvm::Type* {
  // A borrow is a pointer to the borrowee regardless of what
  // the borrowee is, and "GetTypeSymbol" resolves through to
  // the borrowee's symbol, losing the convention that made it
  // a pointer, so the type is asked directly first.
  if (type.GetConvention() != nullptr) { return llvm::PointerType::get(*ctx->Context, 0); }
  const auto type_sym = scope.GetTypeSymbol(&type);
  return type_sym != nullptr ? GetLlvmType(*type_sym, ctx) : nullptr;
}

auto spp::codegen::GetVariantTagType(
  LlvmCtx const *ctx)
  -> llvm::IntegerType* {
  // Every variant discriminates its members with the same
  // integer width (64 bits).
  return llvm::Type::getIntNTy(*ctx->Context, kVariantTagBits);
}

auto spp::codegen::GetVariantIndexOfMember(
  asts::TypeAst const &variant_type,
  asts::TypeAst const &member_type,
  analyse::scopes::Scope const &scope)
  -> std::optional<std::uint64_t> {
  //
  using analyse::utils::type_utils::DedupVariableInnerTypes;
  using analyse::utils::type_utils::TypeEq;

  // Index the type in the list of member types of the variant.
  // Bind the list to a named local first, rather than piping
  // the returned temporary straight into a view over it.
  const auto members = DedupVariableInnerTypes(variant_type, scope);
  for (auto const &[i, member] : members | genex::views::enumerate) {
    if (TypeEq(*member, member_type, scope, scope, false)) {
      return static_cast<std::uint64_t>(i);
    }
  }
  return std::nullopt;
}

auto spp::codegen::GetVariantPayloadPtr(
  llvm::Value *variant_ptr,
  llvm::Type *variant_llvm_type,
  Str const &name,
  LlvmCtx *ctx)
  -> llvm::Value* {
  // The payload is the second field, behind the discriminant.
  return ctx->Builder.CreateStructGEP(variant_llvm_type, variant_ptr, 1, name);
}

auto spp::codegen::LoadVariantTag(
  llvm::Value *variant_ptr,
  llvm::Type *variant_llvm_type,
  Str const &name,
  LlvmCtx *ctx)
  -> llvm::Value* {
  // The discriminant is the first field, ahead of the payload.
  const auto tag_ptr = ctx->Builder.CreateStructGEP(variant_llvm_type, variant_ptr, 0, name + ".ptr");
  return ctx->Builder.CreateLoad(GetVariantTagType(ctx), tag_ptr, name);
}

auto spp::codegen::BuildVariant(
  llvm::Value *member_val,
  llvm::Type *variant_llvm_type,
  const std::uint64_t tag,
  Str const &name,
  LlvmCtx *ctx)
  -> llvm::Value* {
  // Build into a stack slot, because the payload is written
  // through a pointer rather than by value. The slot starts
  // zeroed, because the member rarely fills the whole payload,
  // and the whole struct is loaded back out at the end: the
  // bytes past the member would otherwise be stale stack data,
  // undef to the optimiser and a disclosure hazard the moment a
  // variant is ever copied out of the program. Everything the
  // "stores" below cover is dead-store-eliminated.
  const auto slot = LlvmEntryAlloca(variant_llvm_type, name + ".slot", ctx);
  ctx->Builder.CreateStore(
    llvm::Constant::getNullValue(variant_llvm_type), slot);

  const auto tag_ptr = ctx->Builder.CreateStructGEP(variant_llvm_type, slot, 0, name + ".tag.ptr");
  ctx->Builder.CreateStore(
    llvm::ConstantInt::get(GetVariantTagType(ctx), tag), tag_ptr);

  // A member that lowers to nothing (such as the stateless "None")
  // has no payload; the tag knows the type though.
  if (member_val != nullptr and not member_val->getType()->isVoidTy()) {
    ctx->Builder.CreateStore(member_val, GetVariantPayloadPtr(slot, variant_llvm_type, name + ".payload.ptr", ctx));
  }

  return ctx->Builder.CreateLoad(variant_llvm_type, slot, name);
}

auto spp::codegen::CoerceToVariant(
  llvm::Value *llvm_val,
  asts::TypeAst const &target_type,
  asts::TypeAst const &source_type,
  analyse::scopes::Scope const &scope,
  Str const &name,
  LlvmCtx *ctx)
  -> llvm::Value* {
  //
  using analyse::utils::type_utils::DedupVariableInnerTypes;
  using analyse::utils::type_utils::IsTypeVariant;
  using analyse::utils::type_utils::TypeEq;

  // Only a variant target ever needs a coercion, and a value
  // already of the target type is one.
  if (llvm_val == nullptr or not IsTypeVariant(target_type, scope)) { return llvm_val; }
  if (TypeEq(target_type, source_type, scope, scope, false)) { return llvm_val; }

  const auto target_llvm_type = scope.GetTypeSymbol(&target_type)->LlvmInfo->LlvmType;
  SPP_ASSERT(target_llvm_type != nullptr);

  // A member value (source) is wrapped: tagged and copied into
  // the payload.
  if (not IsTypeVariant(source_type, scope)) {
    const auto tag = GetVariantIndexOfMember(target_type, source_type, scope);
    if (not tag.has_value()) { return llvm_val; }
    return BuildVariant(llvm_val, target_llvm_type, *tag, name, ctx);
  }

  // Otherwise, we need to widen one variant into another, like
  // "Str or Bool" into "Str or Bool or S32". As the order is not
  // guaranteed to match, a mapping is needed.
  const auto source_llvm_type = scope.GetTypeSymbol(&source_type)->LlvmInfo->LlvmType;
  SPP_ASSERT(source_llvm_type != nullptr);

  auto tag_map = Vec<std::uint64_t>();
  auto is_identity_map = true;
  const auto source_members = DedupVariableInnerTypes(source_type, scope);
  for (auto const &[i, member] : source_members | genex::views::enumerate) {
    const auto target_tag = GetVariantIndexOfMember(target_type, *member, scope);
    if (not target_tag.has_value()) { return llvm_val; }
    is_identity_map = is_identity_map and *target_tag == static_cast<std::uint64_t>(i);
    tag_map.EmplaceBack(*target_tag);
  }

  // Spill the source to memory, because the payload is copied
  // through a pointer rather than by value.
  const auto source_slot = LlvmEntryAlloca(source_llvm_type, name + ".from.slot", ctx);
  ctx->Builder.CreateStore(llvm_val, source_slot);
  const auto source_tag = LoadVariantTag(source_slot, source_llvm_type, name + ".from.tag", ctx);

  // Translate the discriminant with a chain of selects, innermost
  // first. Variants have few members, so this stays smaller than
  // a lookup table, and it folds away entirely when the two
  // numberings happen to agree.
  const auto tag_type = GetVariantTagType(ctx);
  auto target_tag = static_cast<llvm::Value*>(source_tag);
  if (not is_identity_map) {
    target_tag = llvm::ConstantInt::get(tag_type, tag_map.Back());
    for (auto i = tag_map.Len() - 1; i > 0; --i) {
      const auto matches = ctx->Builder.CreateICmpEQ(
        source_tag, llvm::ConstantInt::get(tag_type, i - 1), name + ".from.is");
      target_tag = ctx->Builder.CreateSelect(
        matches, llvm::ConstantInt::get(tag_type, tag_map[i - 1]), target_tag, name + ".to.tag");
    }
  }

  // Write the translated discriminant and move the payload over.
  // The target's members are a superset of the source's, so its
  // payload buffer is always at least as large, and the source's
  // size is the amount worth copying. That leaves the target's
  // wider tail uncopied, so zero the slot first.
  const auto target_slot = LlvmEntryAlloca(target_llvm_type, name + ".to.slot", ctx);
  ctx->Builder.CreateStore(llvm::Constant::getNullValue(target_llvm_type), target_slot);
  ctx->Builder.CreateStore(
    target_tag, ctx->Builder.CreateStructGEP(target_llvm_type, target_slot, 0, name + ".to.tag.ptr"));

  auto const &dl = ctx->Module->getDataLayout();
  const auto source_payload_type = llvm::cast<llvm::StructType>(source_llvm_type)->getElementType(1);
  ctx->Builder.CreateMemCpy(
    GetVariantPayloadPtr(target_slot, target_llvm_type, name + ".to.payload.ptr", ctx),
    dl.getABITypeAlign(llvm::cast<llvm::StructType>(target_llvm_type)->getElementType(1)),
    GetVariantPayloadPtr(source_slot, source_llvm_type, name + ".from.payload.ptr", ctx),
    dl.getABITypeAlign(source_payload_type),
    dl.getTypeAllocSize(source_payload_type).getFixedValue());

  return ctx->Builder.CreateLoad(target_llvm_type, target_slot, name);
}
