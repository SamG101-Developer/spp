module;
#include <spp/macros.hpp>

module spp.codegen.llvm_variant;
import spp.analyse.scopes.scope;
import spp.analyse.scopes.symbols;
import spp.analyse.utils.type_compare;
import spp.analyse.utils.type_members;
import spp.analyse.utils.type_predicates;
import spp.asts.class_prototype_ast;
import spp.asts.type_ast;
import spp.codegen.llvm_alloca;
import spp.codegen.llvm_ctx;
import spp.codegen.llvm_sym_info;
import spp.codegen.llvm_type;
import spp.utils.types;
import genex;
import llvm;
import std;

namespace spp::codegen {
  namespace {
    /**
     * Rebuild a value as the same class at a different instantiation, attribute by attribute, widening each one whose
     * type changed. This is what a value written as a single alternative of a variant-typed generic argument needs:
     * "Pass(val=Some(..))" lowers as a "Pass[Some[T]]" where a "Pass[Opt[T]]" is wanted, and the two differ in the tag
     * and payload of that one attribute rather than in how their bytes are arranged.
     * @param llvm_val The value being converted, lowered as its own type.
     * @param target_type The instantiation being converted to.
     * @param source_type The type of @p llvm_val .
     * @param scope The scope to resolve both types against.
     * @param name The name to give the generated value in the ir.
     * @param ctx The LLVM context containing all codegen info.
     * @return The rebuilt value, or @c nullptr when the two do not line up attribute for attribute.
     */
    auto CoerceStructurally(
      llvm::Value *llvm_val,
      asts::TypeAst const &target_type,
      asts::TypeAst const &source_type,
      analyse::scopes::Scope const &scope,
      Str const &name,
      LlvmCtx *ctx)
      -> llvm::Value* {
      //
      using analyse::utils::type_compare::TypeEq;
      using analyse::utils::type_members::GetAllAttrs;

      // The two are the same class at different instantiations, so they have the same attributes in the same
      // declaration order and differ only where a generic argument does. Each attribute is carried over one at a time,
      // widening the ones whose type changed; a plain bitwise copy would leave an alternative's bytes where a
      // variant's tag and payload belong. Nothing is assumed about the physical order - each side maps its own
      // declaration index through its own field index map.

      // Get the source and target type symbols. These are
      // used to get the llvm type information from.
      const auto target_sym = scope.GetTypeSymbol(&target_type);
      const auto source_sym = scope.GetTypeSymbol(&source_type);
      if (target_sym == nullptr or source_sym == nullptr) { return nullptr; }

      // Extract the llvm type information needed for the
      // field mapping / rebuilding.
      const auto target_llvm_type = llvm::dyn_cast_or_null<llvm::StructType>(target_sym->LlvmInfo->LlvmType);
      const auto source_llvm_type = llvm::dyn_cast_or_null<llvm::StructType>(source_sym->LlvmInfo->LlvmType);
      if (target_llvm_type == nullptr or source_llvm_type == nullptr) { return nullptr; }

      // Get the attributes from the source and target
      // types.
      const auto target_attrs = GetAllAttrs(target_type, scope);
      const auto source_attrs = GetAllAttrs(source_type, scope);
      if (target_attrs.IsEmpty() or target_attrs.Len() != source_attrs.Len()) { return nullptr; }

      // A class whose lowered struct carries more than
      // its own attributes (the fat-pointer fields a
      // "Gen"/"Fun" superimposition prepends) shifts every
      // declared index by an amount this does not track,
      // so leave it alone.
      if (target_llvm_type->getNumElements() != target_attrs.Len()
        or source_llvm_type->getNumElements() != source_attrs.Len()) { return nullptr; }

      const auto llvm_index = [](LlvmTypeSymInfo const &info, const std::size_t spp_index) {
        const auto it = info.FieldIndexMap.find(spp_index);
        return static_cast<unsigned>(it != info.FieldIndexMap.end() ? it->second : spp_index);
      };

      const auto source_slot = LlvmEntryAlloca(source_llvm_type, name + ".widen.from", ctx);
      ctx->Builder.CreateStore(llvm_val, source_slot);
      const auto target_slot = LlvmEntryAlloca(target_llvm_type, name + ".widen.to", ctx);
      ctx->Builder.CreateStore(llvm::Constant::getNullValue(target_llvm_type), target_slot);

      for (auto i = 0uz; i < target_attrs.Len(); ++i) {
        const auto target_index = llvm_index(*target_sym->LlvmInfo, i);
        const auto source_index = llvm_index(*source_sym->LlvmInfo, i);
        const auto field_uid = name + ".widen." + std::to_string(i);

        // The attribute's own type, named in full so it
        // resolves from the scope the two are being compared
        // in.
        const auto target_attr_type = spp::get<1>(target_attrs[i])->FqName();
        const auto source_attr_type = spp::get<1>(source_attrs[i])->FqName();

        auto *field_val = static_cast<llvm::Value*>(ctx->Builder.CreateLoad(
          source_llvm_type->getElementType(source_index),
          ctx->Builder.CreateStructGEP(source_llvm_type, source_slot, source_index, field_uid + ".from.ptr"),
          field_uid + ".from"));

        if (not TypeEq(*target_attr_type, *source_attr_type, scope, scope, false)) {
          field_val = CoerceToVariant(field_val, *target_attr_type, *source_attr_type, scope, field_uid, ctx);
        }

        if (field_val == nullptr or field_val->getType() != target_llvm_type->getElementType(target_index)) {
          return nullptr;
        }
        ctx->Builder.CreateStore(
          field_val, ctx->Builder.CreateStructGEP(target_llvm_type, target_slot, target_index, field_uid + ".to.ptr"));
      }

      return ctx->Builder.CreateLoad(target_llvm_type, target_slot, name + ".widen");
    }
  }
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
  using analyse::utils::type_compare::DedupVariableInnerTypes;
  using analyse::utils::type_compare::TypeEq;

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
  using analyse::utils::type_compare::DedupVariableInnerTypes;
  using analyse::utils::type_predicates::IsTypeVariant;
  using analyse::utils::type_compare::TypeEq;

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

    // The alternative is matched with the variant rule turned off, but only at the top level - the comparison of the
    // generic arguments below it turns it back on, so a "Pass[Opt[T]]" answers to a "Pass[Some[T]]". That is wanted,
    // it is how a value written as one alternative is accepted at all, but it means the source is not necessarily laid
    // out as the alternative it matched: copying it into the payload as-is leaves the narrower thing's bytes where the
    // alternative's belong, and the variant reads back as neither. The lowered types say whether that happened.
    auto *member_val = llvm_val;
    const auto members = DedupVariableInnerTypes(target_type, scope);
    if (const auto member_sym = *tag < members.Len() ? scope.GetTypeSymbol(members[*tag].get()) : nullptr;
      member_sym != nullptr and member_sym->LlvmInfo->LlvmType != llvm_val->getType()) {
      if (const auto rebuilt = CoerceStructurally(
        llvm_val, *members[*tag], source_type, scope, name, ctx); rebuilt != nullptr) {
        member_val = rebuilt;
      }
    }
    return BuildVariant(member_val, target_llvm_type, *tag, name, ctx);
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
