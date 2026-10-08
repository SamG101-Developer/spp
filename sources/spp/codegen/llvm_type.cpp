module;
#include <spp/macros.hpp>

module spp.codegen.llvm_type;
import spp.analyse.scopes.comp_key;
import spp.analyse.scopes.scope;
import spp.analyse.scopes.scope_manager;
import spp.analyse.scopes.symbols;
import spp.analyse.utils.type_compare;
import spp.analyse.utils.type_predicates;
import spp.asts.boolean_literal_ast;
import spp.asts.class_prototype_ast;
import spp.asts.convention_ast;
import spp.asts.function_parameter_ast;
import spp.asts.function_parameter_group_ast;
import spp.asts.function_prototype_ast;
import spp.asts.generic_argument_ast;
import spp.asts.generic_argument_group_ast;
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
import spp.codegen.llvm_sym_info;
import spp.codegen.llvm_variant;
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

// Largest alignment a variant payload buffer will be built out of. Anything needing more than a 16 byte alignment is
// vector/extended precision territory, which the layout code does not model either.
constexpr std::uint64_t kMaxVariantPayloadAlign = 16;

namespace spp::codegen {
  namespace {
    auto AdoptLlvmTypeInfo(
      LlvmTypeSymbolInfo &target,
      LlvmTypeSymbolInfo const &source)
      -> void {
      target.LlvmType = source.LlvmType;
      target.FieldIndexMap = source.FieldIndexMap;
    }

    auto GetFloatIntrinsic(const std::size_t bit_width) -> llvm::fltSemantics const& {
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

    /**
     * The fat pointer fields of a type: "FunXXX" closures are represented by a { fn_ptr, env_ptr } pair. The total
     * field count (for example a stateful type superimposing a FunXXX type) is mirrored in
     * "type_members::GetSuperimposedFatPointerFieldCount"; this only adds the LLVM-specific type materialization on top.
     * @return The pair in a 1-item vector (for future expansion), or nothing for a type that is not a function type.
     */
    auto GetFatPointerFields(
      analyse::scopes::TypeSymbol const &sym,
      analyse::scopes::Scope const &scope,
      LlvmCtx const *ctx)
      -> std::optional<Vec<llvm::Type*>> {
      using analyse::utils::type_predicates::IsTypeFunction;
      const auto ptr_ty = llvm::PointerType::get(*ctx->Context, 0);
      if (IsTypeFunction(analyse::scopes::TypeRef::ForKindCheck(sym, scope), scope)) {
        return Vec<llvm::Type*>{ptr_ty, ptr_ty};
      }
      return std::nullopt;
    }
  }
}

auto spp::codegen::RegisterLlvmTypeInfo(
  ClassPrototypeAst const *cls_proto,
  ScopeManager const &sm, LlvmCtx const *ctx) -> void {
  // $ types are function "mock" types (a $-type generated per
  // function that superimposes n FunXXXs over itself). A function
  // used as a value is one of these mocks, so it lowers to the
  // same { fn_ptr, env_ptr } pair as the function type it extends.
  if (cls_proto->Name->IsCompilerGeneratedType()) {
    const auto mock_sym = cls_proto->GetClsSymbol();
    const auto ptr_ty = llvm::PointerType::get(*ctx->Context, 0);
    mock_sym->LlvmInfo->LlvmType = llvm::StructType::get(*ctx->Context, {ptr_ty, ptr_ty});
    return;
  }

  // Push the scope into the the registration function that accepts
  // a scope and context.
  RegisterLlvmTypeInfo(cls_proto->GetAstScope(), sm, ctx);
}

auto spp::codegen::RegisterLlvmTypeInfo(
  Scope const *scope, ScopeManager const &sm,
  LlvmCtx const *ctx) -> void {
  // Get the class symbol from the scope that owns it. This
  // pulls the correct generic instantiation for struct types.
  const auto cls_sym = scope->LinkedTypeSymbol;

  // A "$" mock reached through its scope rather than its
  // prototype - the shape a closure's own type has - lowers
  // to the same { fn_ptr, env_ptr } pair as the function
  // type it superimposes. Walking its (empty) definition
  // instead would measure it as a zero-sized struct.
  if (cls_sym != nullptr and cls_sym->IsMock()) {
    const auto mock_ptr_ty = llvm::PointerType::get(*ctx->Context, 0);
    cls_sym->LlvmInfo->LlvmType = llvm::StructType::get(*ctx->Context, {mock_ptr_ty, mock_ptr_ty});
    return;
  }

  // For compiler known types, specialize the llvm type symbols.
  const auto parts = scope->GetAncestors()
    | genex::views::drop_last(1)
    | genex::views::transform([](auto *x) { return x->NonGnScope->NameAsString(); })
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
    const auto bit_width = U64Of(scope->LinkedTypeSymbol->CompArgId("w"));
    if (not bit_width.has_value()) { return; }
    const auto w = static_cast<unsigned>(*bit_width);
    cls_sym->LlvmInfo->LlvmType = llvm::Type::getIntNTy(*ctx->Context, w);
    return;
  }

  // Lower S++ "F[8|16|32|64|128]" to the llvm "f[8|16|32|64|128]" type.
  if (parts == kSizedFloatParts) {
    const auto bit_width = U64Of(scope->LinkedTypeSymbol->CompArgId("w"));
    if (not bit_width.has_value()) { return; }
    const auto w = static_cast<unsigned>(*bit_width);
    cls_sym->LlvmInfo->LlvmType = llvm::Type::getFloatingPointTy(*ctx->Context, GetFloatIntrinsic(w));
    return;
  }

  // Lower S++ Arr" to the llvm "[T * n]" type.
  if (parts == kArrParts) {
    // The template ("Arr", named as written) has no layout; only an instantiation carries its element type and length.
    if (not cls_sym->IsConcrete or cls_sym->InstanceOf == nullptr) { return; }
    const auto length = analyse::scopes::U64Of(cls_sym->CompArgId("n"));
    const auto elem_ref = cls_sym->TypeArgRef("T");
    auto const *const elem_sym = elem_ref.Symbol;
    if (length.has_value() and elem_sym != nullptr) {
      if (elem_sym->LlvmInfo->LlvmType == nullptr and elem_sym->Type != nullptr) {
        RegisterLlvmTypeInfo(elem_sym->Type, sm, ctx);
      }
      if (const auto elem_llvm_type = GetLlvmTypeOf(elem_ref, ctx); elem_llvm_type != nullptr) {
        cls_sym->LlvmInfo->LlvmType = llvm::ArrayType::get(elem_llvm_type, *length);
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
  if (const auto fields = GetFatPointerFields(*cls_sym, *scope, ctx); fields.has_value()) {
    cls_sym->LlvmInfo->LlvmType = llvm::StructType::get(*ctx->Context, fields->ToStdVector());
    return;
  }

  // If the type already exists in LLVM, skip.
  if (const auto llvm_type = llvm::StructType::getTypeByName(*ctx->Context, mangle::MangleTypeName(*cls_sym));
    llvm_type != nullptr) {
    cls_sym->LlvmInfo->LlvmType = llvm_type;
    return;
  }

  // Lower S++ "Var" (the "A or B" variant type) to a
  // { tag, payload } pair. This includes all sorts of
  // internal processing for tag setup.
  if (parts == kVarParts) {
    const auto struct_type = llvm::StructType::create(*ctx->Context, mangle::MangleTypeName(*cls_sym));
    cls_sym->LlvmInfo->LlvmType = struct_type;

    auto const &dl = ctx->Module->getDataLayout();
    auto max_size = static_cast<std::uint64_t>(0);
    auto max_align = static_cast<std::uint64_t>(1);

    // The members are named relative to the variant, so
    // they are measured from the variant's own scope rather
    // than from wherever the registration walk happens to
    // be.
    const auto member_sm = ScopeManager(
      sm.GlobalScope, const_cast<Scope*>(scope));

    const auto variant_ref = analyse::scopes::TypeRef::Of(*cls_sym, *scope);
    for (auto const &member : analyse::utils::type_compare::VariantMemberRefs(variant_ref, *scope)) {
      const auto member_sym = member.Symbol;
      if (member_sym == nullptr) { continue; }

      // A variant can be registered before its members
      // are, so lower any member still missing its llvm
      // type.
      if (member_sym->LlvmInfo->LlvmType == nullptr and member_sym->Type != nullptr) {
        RegisterLlvmTypeInfo(member_sym->Type, sm, ctx);
      }

      EnsureLlvmTypeComplete(*member_sym, member_sm, ctx);
      const auto member_llvm_type = GetLlvmTypeOf(member, ctx);
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
    *ctx->Context, mangle::MangleTypeName(*cls_sym));
}

auto spp::codegen::GetLlvmType(
  TypeSymbol const &type_sym,
  LlvmCtx const *ctx)
  -> llvm::Type* {
  // A borrow is a pointer to the borrowee whatever the borrowee
  // is, so nothing has to be lowered to answer for it.
  if (type_sym.Convention != asts::ConventionTag::MOV) { return llvm::PointerType::get(*ctx->Context, 0); }

  // Otherwise lower it now if nothing has yet. Types are minted
  // right through monomorphisation and code generation, so "has
  // something already registered this?" is not a question with
  // a stable answer - asking for the type is what makes it exist.
  if (type_sym.LlvmInfo->LlvmType == nullptr and ctx->Sm != nullptr) {
    EnsureLlvmTypeComplete(type_sym, *ctx->Sm, ctx);
  }
  return type_sym.LlvmInfo->LlvmType;
}

auto spp::codegen::GetLlvmTypeOf(
  TypeRef const &ref, LlvmCtx const *ctx) -> llvm::Type* {
  if (ref.IsBorrowed()) { return llvm::PointerType::get(*ctx->Context, 0); }
  return ref.Symbol != nullptr ? GetLlvmType(*ref.Symbol, ctx) : nullptr;
}

auto spp::codegen::EnsureLlvmTypeComplete(
  TypeSymbol const &type_sym,
  ScopeManager const &sm,
  LlvmCtx const *ctx)
  -> void {
  // A symbol that names another type without carrying its
  // prototype - "Self", which links to the class it stands
  // for (see "MakeSelfTypeSymbol") - is completed as that class
  // and then adopts the result. It cannot be completed as
  // itself: there is no prototype on it to read a layout
  // from, and the guard below would turn it away.
  auto linked_sym = type_sym.AsBound();

  // An alias ("type Opt[T] = Some[T] or None") has a prototype
  // of its own, but it is the target that has the layout, and
  // both mangle to the same name: lowering the alias as itself
  // would register an empty struct under the target's name.
  if (type_sym.LlvmInfo->LlvmType == nullptr and type_sym.Alias != nullptr) {
    if (const auto target = type_sym.AliasTarget(*sm.CurrentScope); target != &type_sym) {
      linked_sym = target;
    }
  }

  if (linked_sym != &type_sym) {
    // Stand-ins can name each other - the "Self" of a scope
    // whose class scope carries another "Self" - and following
    // the chain would then never end. Same guard, and for the
    // same reason, as the layout walk below.
    static thread_local auto in_progress = Set<TypeSymbol const*>();
    if (not in_progress.insert(&type_sym).second) { return; }

    EnsureLlvmTypeComplete(*linked_sym, sm, ctx);
    AdoptLlvmTypeInfo(*type_sym.LlvmInfo, *linked_sym->LlvmInfo);
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
    // symbol stays un-lowered however many times it is asked for.
    if (type_sym.LlvmInfo->LlvmType == nullptr and type_sym.LinkedSymbol() != &type_sym) {
      AdoptLlvmTypeInfo(*type_sym.LlvmInfo, *type_sym.LinkedSymbol()->LlvmInfo);
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
