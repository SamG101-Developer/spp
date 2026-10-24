module;
#include <spp/macros.hpp>

module spp.codegen.llvm_coerce;
import spp.analyse.scopes.scope;
import spp.analyse.scopes.scope_manager;
import spp.analyse.scopes.symbols;
import spp.analyse.utils.fn_values;
import spp.analyse.utils.marker_sups;
import spp.analyse.utils.type_compare;
import spp.analyse.utils.type_members;
import spp.analyse.utils.type_predicates;
import spp.asts.function_prototype_ast;
import spp.asts.type_ast;
import spp.codegen.llvm_alloca;
import spp.codegen.llvm_ctx;
import spp.codegen.llvm_fn;
import spp.codegen.llvm_layout;
import spp.codegen.llvm_sym_info;
import spp.codegen.llvm_type;
import spp.codegen.llvm_variant;
import spp.utils.ice;
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
     * @param sm The scope manager, standing in the scope both types are resolved against.
     * @param name The name to give the generated value in the ir.
     * @param ctx The LLVM context containing all codegen info.
     * @return The rebuilt value, or @c nullptr when the two do not line up attribute for attribute.
     */
    auto CoerceStructurally(
      llvm::Value *llvm_val,
      analyse::scopes::TypeRef const &target,
      analyse::scopes::TypeRef const &source,
      analyse::scopes::ScopeManager const &sm,
      Str const &name,
      LlvmCtx *ctx)
      -> llvm::Value* {
      //
      using analyse::scopes::TypeRef;
      using analyse::utils::type_compare::TypeEq;
      using analyse::utils::type_members::GetAllAttrs;
      auto const &scope = *sm.CurrentScope;

      // The two are the same class at different instantiations, so they have the same attributes in the same
      // declaration order and differ only where a generic argument does. Each attribute is carried over one at a time,
      // widening the ones whose type changed; a plain bitwise copy would leave an alternative's bytes where a
      // variant's tag and payload belong. Nothing is assumed about the physical order - each side maps its own
      // declaration index through its own field index map.

      // Get the source and target type symbols. These are
      // used to get the llvm type information from.
      const auto target_sym = target.Symbol;
      const auto source_sym = source.Symbol;
      if (target_sym == nullptr or source_sym == nullptr) {
        Ice("structural coercion between types with no type symbol");
      }

      // Extract the llvm type information needed for the
      // field mapping / rebuilding.
      const auto target_llvm_type = llvm::dyn_cast_or_null<llvm::StructType>(target_sym->LlvmInfo->LlvmType);
      const auto source_llvm_type = llvm::dyn_cast_or_null<llvm::StructType>(source_sym->LlvmInfo->LlvmType);
      if (target_llvm_type == nullptr or source_llvm_type == nullptr) { return nullptr; }

      // Get the attributes from the source and target
      // types.
      const auto target_attrs = GetAllAttrs(*target_sym);
      const auto source_attrs = GetAllAttrs(*source_sym);
      if (target_attrs.empty() or target_attrs.size() != source_attrs.size()) { return nullptr; }

      // A class whose lowered struct carries more than
      // its own attributes (the fat-pointer fields a
      // "Gen"/"Fun" superimposition prepends) shifts every
      // declared index by an amount this does not track,
      // so leave it alone.
      if (target_llvm_type->getNumElements() != target_attrs.size()
        or source_llvm_type->getNumElements() != source_attrs.size()) { return nullptr; }

      const auto llvm_index = [](LlvmTypeSymbolInfo const &info, const std::size_t spp_index) {
        const auto it = info.FieldIndexMap.find(spp_index);
        return static_cast<unsigned>(it != info.FieldIndexMap.end() ? it->second : spp_index);
      };

      const auto source_slot = LlvmEntryAlloca(source_llvm_type, name + ".widen.from", ctx);
      ctx->Builder.CreateStore(llvm_val, source_slot);
      const auto target_slot = LlvmEntryAlloca(target_llvm_type, name + ".widen.to", ctx);
      ctx->Builder.CreateStore(llvm::Constant::getNullValue(target_llvm_type), target_slot);

      for (auto i = 0uz; i < target_attrs.size(); ++i) {
        const auto target_index = llvm_index(*target_sym->LlvmInfo, i);
        const auto source_index = llvm_index(*source_sym->LlvmInfo, i);
        const auto field_uid = name + ".widen." + std::to_string(i);

        // The attribute's own type, resolved from the scope the
        // two are being compared in.
        const auto target_attr_type = spp::get<1>(target_attrs[i]);
        const auto source_attr_type = spp::get<1>(source_attrs[i]);

        auto *field_val = static_cast<llvm::Value*>(ctx->Builder.CreateLoad(
          source_llvm_type->getElementType(source_index),
          ctx->Builder.CreateStructGEP(source_llvm_type, source_slot, source_index, field_uid + ".from.ptr"),
          field_uid + ".from"));

        if (not TypeEq(target_attr_type, source_attr_type, scope, scope)) {
          field_val = CoerceToSlot(field_val, target_attr_type, source_attr_type, sm, field_uid, ctx);
        }

        if (field_val == nullptr or field_val->getType() != target_llvm_type->getElementType(target_index)) {
          return nullptr;
        }
        ctx->Builder.CreateStore(
          field_val, ctx->Builder.CreateStructGEP(target_llvm_type, target_slot, target_index, field_uid + ".to.ptr"));
      }

      return ctx->Builder.CreateLoad(target_llvm_type, target_slot, name + ".widen");
    }

    /// A constant laid out byte for byte as "type", from constants
    /// at byte offsets in it: a packed struct of them, with zero
    /// bytes between and after. A constant aggregate is built so
    /// when one of its parts is not of its slot's own type: a
    /// variant's payload holding a member, which LLVM cannot read
    /// as the payload's integers once the member holds a pointer.
    auto PackedConstant(
      llvm::Type *type, Vec<Pair<std::uint64_t, llvm::Constant*>> const &parts, LlvmCtx *ctx) -> llvm::Constant* {
      auto const &dl = ctx->Module->getDataLayout();
      auto fields = Vec<llvm::Constant*>();
      auto at = std::uint64_t{0};
      const auto pad = [&](const std::uint64_t to) {
        if (to <= at) { return; }
        fields.emplace_back(llvm::Constant::getNullValue(llvm::ArrayType::get(ctx->Builder.getInt8Ty(), to - at)));
      };
      for (auto const &[offset, part] : parts) {
        pad(offset);
        fields.emplace_back(part);
        at = offset + dl.getTypeAllocSize(part->getType()).getFixedValue();
      }
      pad(dl.getTypeAllocSize(type).getFixedValue());
      return llvm::ConstantStruct::getAnon(*ctx->Context, fields, true);
    }

    /// The byte offset of field "index" of the struct "type".
    auto FieldOffset(llvm::Type *type, const unsigned index, LlvmCtx *ctx) -> std::uint64_t {
      const auto i32 = ctx->Builder.getInt32Ty();
      llvm::Value *const indices[] = {llvm::ConstantInt::get(i32, 0), llvm::ConstantInt::get(i32, index)};
      return static_cast<std::uint64_t>(ctx->Module->getDataLayout().getIndexedOffsetInType(type, indices));
    }

    /// A value naming a function that is going into a function
    /// type is replaced by the pair for the overload analysis
    /// chose there.
    auto CoerceToFnValue(
      llvm::Value *llvm_val, TypeRef const &target,
      TypeRef const &source, ScopeManager const &sm,
      LlvmCtx const *ctx) -> llvm::Value* {
      // A named function carries nothing at runtime, so whatever was
      // loaded for it is dropped and the chosen overload built.
      const auto fn = llvm_val != nullptr
        ? analyse::utils::fn_values::FindFnValue(source, target, sm)
        : nullptr;
      if (fn == nullptr or fn->GetLlvmFn() == nullptr or fn->GetLlvmFn()->Target == nullptr) { return llvm_val; }
      const auto fn_sup = analyse::utils::marker_sups::FindFnSup(*target.Symbol->FqName(), *sm.CurrentScope);
      return BuildFnValue(*fn->GetLlvmFn()->Target, fn_sup, ctx);
    }

    /// A member placed into a variant ("S32" into "S32 or Str"),
    /// or a variant into a wider one ("S32 or Str" into "S32 or Str
    /// or Bool", a guaranteed subset).
    auto CoerceToVariant(
      llvm::Value *llvm_val,
      analyse::scopes::TypeRef const &target,
      analyse::scopes::TypeRef const &source,
      analyse::scopes::ScopeManager const &sm,
      Str const &name,
      LlvmCtx *ctx)
      -> llvm::Value* {
      //
      using analyse::scopes::TypeRef;
      using analyse::utils::type_compare::TypeEq;
      using analyse::utils::type_members::VariantMemberRefs;
      using analyse::utils::type_predicates::IsTypeVariant;
      auto const &scope = *sm.CurrentScope;

      if (llvm_val == nullptr or target.Symbol == nullptr or source.Symbol == nullptr) { return llvm_val; }

      // Only a variant target ever needs a coercion, and a value
      // already of the target type is one.
      if (not IsTypeVariant(target, scope)) { return llvm_val; }
      if (TypeEq(target, source, scope, scope)) { return llvm_val; }

      // Asked for rather than read, as a type is only lowered when
      // something first needs it.
      const auto target_llvm_type = GetLlvmTypeOf(target.WithoutConvention(), ctx);
      SPP_ASSERT(target_llvm_type != nullptr);

      // A member value (source) is wrapped: tagged and copied into
      // the payload.
      if (not IsTypeVariant(source, scope)) {
        const auto tag = GetVariantIndexOfMember(target, source, scope);
        if (not tag.has_value()) { return llvm_val; }

        // A constant ("cmp x: Opt[S32] = Some(val=1)") cannot be
        // tagged and copied through a stack slot, so it is built in
        // place: the tag, then the member where the payload starts.
        if (const auto member_const = llvm::dyn_cast<llvm::Constant>(llvm_val);
          ctx->InConstantContext and member_const != nullptr) {
          auto parts = Vec<Pair<std::uint64_t, llvm::Constant*>>{
            {0, llvm::ConstantInt::get(GetVariantTagType(ctx), *tag)}};
          if (not IsValuelessType(member_const->getType())) {
            parts.emplace_back(FieldOffset(target_llvm_type, 1, ctx), member_const);
          }
          return PackedConstant(target_llvm_type, parts, ctx);
        }

        // The alternative is matched with the variant rule turned off, but only at the top level - the comparison of
        // the generic arguments below it turns it back on, so a "Pass[Opt[T]]" answers to a "Pass[Some[T]]". That is
        // wanted, it is how a value written as one alternative is accepted at all, but it means the source is not
        // necessarily laid out as the alternative it matched: copying it into the payload as-is leaves the narrower
        // thing's bytes where the alternative's belong, and the variant reads back as neither. The lowered types say
        // whether that happened.
        auto *member_val = llvm_val;
        const auto members = VariantMemberRefs(target, scope);
        if (const auto member_sym = *tag < members.size() ? members[*tag].Symbol : nullptr;
          member_sym != nullptr and member_sym->LlvmInfo->LlvmType != llvm_val->getType()) {
          if (const auto rebuilt = CoerceStructurally(
            llvm_val, members[*tag], source, sm, name, ctx); rebuilt != nullptr) {
            member_val = rebuilt;
          }
        }
        return BuildVariant(member_val, target_llvm_type, *tag, name, ctx);
      }

      // Otherwise, we need to widen one variant into another, like
      // "Str or Bool" into "Str or Bool or S32". As the order is not
      // guaranteed to match, a mapping is needed.
      const auto source_llvm_type = GetLlvmTypeOf(source.WithoutConvention(), ctx);
      SPP_ASSERT(source_llvm_type != nullptr);

      auto tag_map = Vec<std::uint64_t>();
      auto is_identity_map = true;
      const auto source_members = VariantMemberRefs(source, scope);
      for (auto i = 0uz; i < source_members.size(); ++i) {
        const auto target_tag = GetVariantIndexOfMember(target, source_members[i], scope);
        if (not target_tag.has_value()) { return llvm_val; }
        is_identity_map = is_identity_map and *target_tag == static_cast<std::uint64_t>(i);
        tag_map.emplace_back(*target_tag);
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
        target_tag = llvm::ConstantInt::get(tag_type, tag_map.back());
        for (auto i = tag_map.size() - 1; i > 0; --i) {
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
  }
}

auto spp::codegen::BuildAggregate(
  llvm::Type *type, Vec<Pair<unsigned, llvm::Value*>> const &elems, Str const &name, LlvmCtx *ctx)
  -> llvm::Value* {
  const auto slot_type = [type](const unsigned index) {
    return type->isArrayTy() ? type->getArrayElementType() : type->getStructElementType(index);
  };
  const auto reshaped = genex::any_of(
    elems, [&](auto const &e) { return e.second->getType() != slot_type(e.first); });
  if (not reshaped) {
    auto out = static_cast<llvm::Value*>(llvm::PoisonValue::get(type));
    for (auto const &[index, elem] : elems) { out = ctx->Builder.CreateInsertValue(out, elem, {index}, name); }
    return out;
  }
  auto const &dl = ctx->Module->getDataLayout();
  auto parts = Vec<Pair<std::uint64_t, llvm::Constant*>>();
  for (auto const &[index, elem] : elems) {
    const auto elem_const = llvm::dyn_cast<llvm::Constant>(elem);
    if (elem_const == nullptr) { Ice("an element of another shape than its slot's outside a constant"); }
    const auto offset = type->isArrayTy()
      ? index * dl.getTypeAllocSize(type->getArrayElementType()).getFixedValue()
      : FieldOffset(type, index, ctx);
    parts.emplace_back(offset, elem_const);
  }
  genex::actions::sort(parts, {}, [](auto const &p) { return p.first; });
  return PackedConstant(type, parts, ctx);
}

auto spp::codegen::CoerceToSlot(
  llvm::Value *llvm_val,
  analyse::scopes::TypeRef const &target,
  analyse::scopes::TypeRef const &source,
  analyse::scopes::ScopeManager const &sm,
  Str const &name,
  LlvmCtx *ctx)
  -> llvm::Value* {
  //
  using analyse::utils::type_predicates::IsTypeArray;
  using analyse::utils::type_predicates::IsTypeTuple;
  auto const &scope = *sm.CurrentScope;

  // A "!" value never exists, so whatever consumes it is dead
  // code - but it still has to be valid IR, so it stands in as
  // poison of the type the consumer expects.
  if (llvm_val != nullptr and source.IsNever and not target.IsNever) {
    if (const auto target_llvm_type = GetLlvmTypeOf(target, ctx);
      not IsValuelessType(target_llvm_type)) {
      return llvm::PoisonValue::get(target_llvm_type);
    }
  }
  if (llvm_val == nullptr or target.Symbol == nullptr or source.Symbol == nullptr) { return llvm_val; }

  // A tuple or an array is coerced element by element when the two types differ: "(Some[T], U64)" into "(Opt[T],
  // U64)" differs only in the variant inside it, and "($f, U64)" into "(FunRef[..], U64)" only in the function value.
  // The second pair lower alike, as a function's mock and its value are both "{ fn_ptr, env_ptr }", so they are told
  // apart by identity as well. Not by "TypeEq": its comparison of the generic arguments lets a variant take one of
  // its members.
  const auto target_llvm_type = GetLlvmTypeOf(target, ctx);
  const auto source_llvm_type = GetLlvmTypeOf(source, ctx);
  const auto differ = target_llvm_type != nullptr and source_llvm_type != nullptr
    and (target_llvm_type != source_llvm_type or target.Id != source.Id);

  // A tuple has no attributes for "CoerceStructurally" to walk, and its layout may reorder the elements, so each side
  // maps a declared index through its own field index map.
  if (differ and IsTypeTuple(target, scope) and IsTypeTuple(source, scope)) {
    const auto target_args = target.Symbol->TypeArgRefs();
    const auto source_args = source.Symbol->TypeArgRefs();
    if (target_args.size() == source_args.size()) {
      auto elems = Vec<Pair<unsigned, llvm::Value*>>();
      for (auto i = 0uz; i < target_args.size(); ++i) {
        const auto elem_name = name + ".tup." + std::to_string(i);
        auto elem = ctx->Builder.CreateExtractValue(
          llvm_val, {GetPhysicalFieldIndex(*source.Symbol->LlvmInfo, i)}, elem_name + ".from");
        elem = CoerceToSlot(elem, target_args[i], source_args[i], sm, elem_name, ctx);
        elems.emplace_back(GetPhysicalFieldIndex(*target.Symbol->LlvmInfo, i), elem);
      }
      return BuildAggregate(target_llvm_type, elems, name + ".tup", ctx);
    }
  }

  // An array keeps its elements in order, all of the one type "T".
  if (differ and IsTypeArray(target, scope) and IsTypeArray(source, scope)) {
    const auto target_arr_type = llvm::dyn_cast<llvm::ArrayType>(target_llvm_type);
    const auto source_arr_type = llvm::dyn_cast<llvm::ArrayType>(source_llvm_type);
    if (target_arr_type != nullptr and source_arr_type != nullptr
      and target_arr_type->getNumElements() == source_arr_type->getNumElements()) {
      const auto target_elem = target.Symbol->TypeArgRef("T");
      const auto source_elem = source.Symbol->TypeArgRef("T");
      auto elems = Vec<Pair<unsigned, llvm::Value*>>();
      for (auto i = 0u; i < static_cast<unsigned>(target_arr_type->getNumElements()); ++i) {
        const auto elem_name = name + ".arr." + std::to_string(i);
        const auto elem = ctx->Builder.CreateExtractValue(llvm_val, {i}, elem_name + ".from");
        elems.emplace_back(i, CoerceToSlot(elem, target_elem, source_elem, sm, elem_name, ctx));
      }
      return BuildAggregate(target_llvm_type, elems, name + ".arr", ctx);
    }
  }

  // Otherwise the value itself is the function value or the
  // variant member, if it is either.
  llvm_val = CoerceToFnValue(llvm_val, target, source, sm, ctx);
  return CoerceToVariant(llvm_val, target, source, sm, name, ctx);
}
