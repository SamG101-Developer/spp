module;
#include <spp/macros.hpp>

module spp.codegen.llvm_type;
import spp.analyse.scopes.comp_key;
import spp.analyse.scopes.scope;
import spp.analyse.scopes.scope_manager;
import spp.analyse.scopes.symbols;
import spp.analyse.utils.type_members;
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
import spp.utils.ice;
import spp.utils.numbers;
import spp.utils.types;
import genex;
import llvm;
import std;

// Largest alignment a variant payload buffer will be built out of. Anything needing more than a 16 byte alignment is
// vector/extended precision territory, which the layout code does not model either.
constexpr std::uint64_t kMaxVariantPayloadAlign = 16;

MSVC_DEVCOM_11179619_IMPL_UNIT_USING_DECL
use_alias(spp::analyse::scopes, Scope);
use_alias(spp::analyse::scopes, ScopeManager);
use_alias(spp::analyse::scopes, TypeRef);
use_alias(spp::analyse::scopes, TypeSymbol);
use_alias(spp::asts, FunctionPrototypeAst);
use_alias(spp::asts, TypeAst);

namespace spp::codegen {
  /// Given a source symbol, and a different target symbol ("Self" binding etc),
  /// copy fields off the source to the target that are needed for the complete
  /// LLVM type information - the llvm type and the field index map.
  static auto AdoptLlvmTypeInfo(
    LlvmTypeSymbolInfo &target, LlvmTypeSymbolInfo const &source) -> void;

  /// The LLVM type of "type_sym", lowered now if nothing has yet. A
  /// generic binding held by a borrow ("T=&Str") is the opaque
  /// pointer type: a caller that dropped its ref's own convention
  /// ("WithoutConvention") still gets the binding's. Everything
  /// outside asks "GetLlvmTypeOf" with a "TypeRef".
  static auto LlvmTypeOfSymbol(
    TypeSymbol const &type_sym, LlvmCtx const *ctx) -> llvm::Type*;
}

/// [CHECKED]
auto spp::codegen::AdoptLlvmTypeInfo(
  LlvmTypeSymbolInfo &target, LlvmTypeSymbolInfo const &source) -> void {
  // Copy the llvm type and field index map from the source symbol into the
  // target symbol.
  target.LlvmType = source.LlvmType;
  target.FieldIndexMap = source.FieldIndexMap;
}

/// [CHECKED]
auto spp::codegen::LlvmTypeOfSymbol(
  TypeSymbol const &type_sym, LlvmCtx const *ctx) -> llvm::Type* {
  // A borrow is a pointer to the borrowee whatever the borrowee is, so nothing
  // has to be lowered to answer for it.
  if (type_sym.Convention != asts::ConventionTag::MOV) {
    return llvm::PointerType::get(*ctx->Context, 0);
  }

  // Otherwise lower it now if nothing has yet. Types are minted right through
  // monomorphisation and code generation, so asking for the type is what makes
  // it exist.
  if (type_sym.LlvmInfo->LlvmType == nullptr and ctx->Sm != nullptr) {
    EnsureLlvmTypeComplete(type_sym, *ctx->Sm, ctx);
  }
  return type_sym.LlvmInfo->LlvmType;
}

/// [CHECKED]
auto spp::codegen::RegisterLlvmTypeInfo(
  Scope const *scope, ScopeManager const &sm, LlvmCtx const *ctx) -> void {
  // Get the class symbol from the scope that owns it. This pulls the correct
  // generic instantiation for struct types.
  using namespace spp::asts::generate::common_types_precompiled;
  using namespace spp::utils;
  const auto cls_sym = scope->LinkedTypeSymbol;

  // A "$" mock reached through its scope rather than its prototype - the shape
  // a closure's own type has - lowers to the same { fn_ptr, env_ptr } pair as
  // the function type it superimposes. Walking its (empty) definition instead
  // would measure it as a zero-sized struct.
  if (cls_sym != nullptr and cls_sym->IsMock()) {
    const auto mock_ptr_ty = llvm::PointerType::get(*ctx->Context, 0);
    cls_sym->LlvmInfo->LlvmType = llvm::StructType::get(
      *ctx->Context, {mock_ptr_ty, mock_ptr_ty});
    return;
  }

  // For compiler known types, specialise the llvm type symbols.
  const auto cls_ref = TypeRef::Of(*cls_sym, *scope);
  const auto is = [&](TypeAst const &tmpl) { return cls_ref.IsA(tmpl, *scope); };

  // Lower S++ "Void" to the llvm "void" type.
  if (is(*VOID)) {
    cls_sym->LlvmInfo->LlvmType = llvm::Type::getVoidTy(*ctx->Context);
    return;
  }

  // Lower S++ "Bool" to the llvm "i1" type.
  if (is(*BOOL)) {
    cls_sym->LlvmInfo->LlvmType = llvm::Type::getInt1Ty(*ctx->Context);
    return;
  }

  // Lower S++ "NonNull[T]" and the nullable "Ptr[T]" to a bare opaque llvm
  // pointer.
  if (is(*NON_NULL) or is(*PTR)) {
    cls_sym->LlvmInfo->LlvmType = llvm::PointerType::get(*ctx->Context, 0);
    return;
  }

  // Lower S++ "S/U[8|16|32|64|128]" to the llvm "i[8|16|32|64|128]" type (llvm
  // integers carry no signedness).
  if (is(*SIZED_INTEGER)) {
    const auto bit_width = U64Of(scope->LinkedTypeSymbol->CompArgId("w"));
    if (not bit_width.has_value()) { return; }
    const auto w = static_cast<unsigned>(*bit_width);
    cls_sym->LlvmInfo->LlvmType = llvm::Type::getIntNTy(*ctx->Context, w);
    return;
  }

  // Lower S++ "F8" (FP8 E4M3) to its bits, an llvm "i8": llvm has no 8-bit
  // float type. It is converted by "fpext" and "fptrunc" ("EmitF8Decode",
  // "EmitF8Encode"), and has no arithmetic.
  if (is(*F8)) {
    cls_sym->LlvmInfo->LlvmType = llvm::Type::getInt8Ty(*ctx->Context);
    return;
  }

  // Lower S++ "F[16|32|64|128]" to the llvm float of its format, using the
  // internal float semantics.
  if (is(*SIZED_FLOAT)) {
    const auto bit_width = U64Of(scope->LinkedTypeSymbol->CompArgId("w"));
    if (not bit_width.has_value()) { return; }
    const auto w = static_cast<unsigned>(*bit_width);
    cls_sym->LlvmInfo->LlvmType = llvm::Type::getFloatingPointTy(
      *ctx->Context, numbers::FloatSemantics(std::format("f{}", w)));
    return;
  }

  // Lower S++ "Arr" to the llvm "[T * n]" type. The template version has no
  // layout; generic instantiations are needed for the element type and length.
  if (is(*ARR)) {
    if (not cls_sym->IsConcrete or cls_sym->InstanceOf == nullptr) { return; }
    const auto length = analyse::scopes::U64Of(cls_sym->CompArgId("n"));
    const auto elem_ref = cls_sym->TypeArgRef("T");
    const auto elem_sym = elem_ref.Symbol;
    if (length.has_value() and elem_sym != nullptr) {
      if (elem_sym->LlvmInfo->LlvmType == nullptr and elem_sym->Type != nullptr) {
        RegisterLlvmTypeInfo(elem_sym->Type->GetAstScope(), sm, ctx);
      }
      if (const auto elem_llvm_type = GetLlvmTypeOf(elem_ref, ctx); elem_llvm_type != nullptr) {
        cls_sym->LlvmInfo->LlvmType = llvm::ArrayType::get(elem_llvm_type, *length);
      }
    }
    return;
  }

  // Lower S++ "Generated[Yield]" to the { ptr, ptr } shape (internal generator
  // yielded value wrapper type).
  if (is(*GENERATED)) {
    const auto ptr_ty = llvm::PointerType::get(*ctx->Context, 0);
    cls_sym->LlvmInfo->LlvmType = llvm::StructType::get(*ctx->Context, {ptr_ty, ptr_ty});
    return;
  }

  // A generator is the bare "llvm.coro.begin" handle, one pointer wide; the
  // "Fun*" family a { fn_ptr, env_ptr } fat pointer, which closures share: one
  // uniform system for all function type storage.
  switch (analyse::utils::type_members::FatPointerWidthOf(
    TypeRef::ForKindCheck(*cls_sym, *scope), *scope)) {
    case 1: {
      cls_sym->LlvmInfo->LlvmType = llvm::PointerType::get(
        *ctx->Context, 0);
      return;
    }
    case 2: {
      const auto ptr_ty = llvm::PointerType::get(*ctx->Context, 0);
      cls_sym->LlvmInfo->LlvmType = llvm::StructType::get(
        *ctx->Context, {ptr_ty, ptr_ty});
      return;
    }
    default: break;
  }

  // If the type already exists in LLVM, skip.
  if (const auto llvm_type = llvm::StructType::getTypeByName(
    *ctx->Context, mangle::MangleTypeName(*cls_sym));
    llvm_type != nullptr) {
    cls_sym->LlvmInfo->LlvmType = llvm_type;
    return;
  }

  // Lower S++ "Var" (the "A or B" variant type) to a { tag, payload } pair.
  // This includes all sorts of internal processing for tag setup.
  if (is(*VAR)) {
    const auto struct_type = llvm::StructType::create(
      *ctx->Context, mangle::MangleTypeName(*cls_sym));
    cls_sym->LlvmInfo->LlvmType = struct_type;

    auto const &dl = ctx->Module->getDataLayout();
    auto max_size = static_cast<std::uint64_t>(0);
    auto max_align = static_cast<std::uint64_t>(1);

    // The members are named relative to the variant, so they are measured from
    // the variant's own scope rather than from wherever the registration walk
    // happens to be.
    const auto member_sm = sm.NewManagerAt(const_cast<Scope*>(scope));

    for (auto const &member : analyse::utils::type_members::VariantMemberRefs(
           cls_ref, *scope)) {
      const auto member_sym = member.Symbol;
      if (member_sym == nullptr) { continue; }

      // A variant can be registered before its members are, so lower any member
      // still missing its llvm type.
      if (member_sym->LlvmInfo->LlvmType == nullptr and member_sym->Type != nullptr) {
        RegisterLlvmTypeInfo(member_sym->Type->GetAstScope(), sm, ctx);
      }

      EnsureLlvmTypeComplete(*member_sym, member_sm, ctx);
      const auto member_llvm_type = GetLlvmTypeOf(member, ctx);
      if (member_llvm_type == nullptr or not member_llvm_type->isSized()) { continue; }
      max_size = std::max(max_size, dl.getTypeAllocSize(member_llvm_type).getFixedValue());
      max_align = std::max(max_align, dl.getABITypeAlign(member_llvm_type).value());
    }

    // Build the payload out of the widest integer any member needs to be
    // aligned to, rather than out of bytes: a "[n x i8]" buffer is only ever
    // byte-aligned, so storing a member into it would be under-aligned.
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

/// [CHECKED]
auto spp::codegen::GetLlvmTypeOf(
  TypeRef const &ref, LlvmCtx const *ctx) -> llvm::Type* {
  // Return the opaque pointer for a borrowed type, otherwise use the internal
  // method to get the type off of the symbol.
  if (ref.IsBorrowed()) { return llvm::PointerType::get(*ctx->Context, 0); }
  return ref.Symbol != nullptr ? LlvmTypeOfSymbol(*ref.Symbol, ctx) : nullptr;
}

/// [CHECKED]
auto spp::codegen::EnsureLlvmTypeComplete(
  TypeSymbol const &type_sym, ScopeManager const &sm,
  LlvmCtx const *ctx) -> void {
  // A symbol that names another type without carrying its prototype, like
  // "Self" which links to the class it stands for, is completed as that class,
  // and then adopts the result.
  auto linked_sym = type_sym.AsBound();

  // An alias ("type Opt[T] = Some[T] or None") has a prototype of its own, but
  // it is the target that has the layout, so adjust the symbol accordingly.
  if (type_sym.LlvmInfo->LlvmType == nullptr and type_sym.Alias != nullptr) {
    if (const auto target = type_sym.AliasTarget(*sm.CurrentScope); target != &type_sym) {
      linked_sym = target;
    }
  }

  if (linked_sym != &type_sym) {
    // Stand-ins can name each other - the "Self" of a scope whose class scope
    // carries another "Self" - and following the chain would then never end.
    static thread_local auto in_progress = Set<TypeSymbol const*>();
    if (not in_progress.insert(&type_sym).second) { return; }
    EnsureLlvmTypeComplete(*linked_sym, sm, ctx);
    AdoptLlvmTypeInfo(*type_sym.LlvmInfo, *linked_sym->LlvmInfo);
    in_progress.erase(&type_sym);
    return;
  }

  // Nothing to complete without a prototype behind the symbol (a bare generic
  // parameter, for example).
  if (type_sym.Type == nullptr) { return; }

  // Ensure the type is complete. This is an on-demand walk of the type's
  // definition, including fields.
  if (type_sym.LlvmInfo->LlvmType == nullptr) {
    RegisterLlvmTypeInfo(
      type_sym.LinkedScope != nullptr ? type_sym.LinkedScope : type_sym.Type->GetAstScope(),
      sm, ctx);

    // A symbol standing in for another type - a generic parameter bound to one,
    // "T" inside an instantiation - needs to adopt the result.
    if (type_sym.LlvmInfo->LlvmType == nullptr and type_sym.LinkedSymbol() != &type_sym) {
      AdoptLlvmTypeInfo(*type_sym.LlvmInfo, *type_sym.LinkedSymbol()->LlvmInfo);
    }
  }

  // Only a named struct can be half-built; everything else is complete the
  // moment it is lowered. A struct that is still opaque is the placeholder a
  // class gets at registration, waiting for Stage10 to derive its body.
  const auto struct_type = llvm::dyn_cast_or_null<llvm::StructType>(
    type_sym.LlvmInfo->LlvmType);
  if (struct_type == nullptr or not struct_type->isOpaque()) { return; }

  // Laying the body out asks for the sizes of the attributes, which comes back
  // through here for each of them, so a type that contains itself would recurse
  // forever. Prevent this with a guard.
  static thread_local auto in_progress = Set<llvm::StructType const*>();
  if (not in_progress.insert(struct_type).second) { return; }
  type_sym.Type->FillLlvmLayout(&sm, &type_sym, ctx);
  in_progress.erase(struct_type);
}

/// [CHECKED]
auto spp::codegen::IsValuelessType(llvm::Type const *type) -> bool {
  return type == nullptr or type->isVoidTy();
}
