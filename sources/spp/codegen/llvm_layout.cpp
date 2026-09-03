module spp.codegen.llvm_layout;
import spp.codegen.llvm_type;
import genex;

auto spp::codegen::DropValuelessFields(
  Vec<llvm::Type*> const &field_types)
  -> Vec<std::size_t> {
  // A field carrying no value takes no storage, and llvm has
  // no member type to give it, so it is not laid out at all.
  // A generic attribute instantiated at "Void" is how one
  // arises without anything odd being written: the "val" of a
  // "Pass[T=Void]" is still an attribute, still named and
  // still destructured, and simply has nothing behind it.
  return genex::views::iota(0uz, field_types.Len())
    | genex::views::filter([&](auto const i) { return not IsValuelessType(field_types[i]); })
    | genex::to<Vec>();
}

auto spp::codegen::SortMembersForSppLayout(
  Vec<llvm::Type*> const &field_types,
  LlvmCtx const *ctx)
  -> Pair<Vec<llvm::Type*>, Map<std::size_t, std::size_t>> {
  // Based on the ABI detected size of each type, re-order for
  // minimal total object size (minimize padding). Return the
  // re-ordered fields and the field index mapping.
  auto const &dl = ctx->Module->getDataLayout();
  auto order = DropValuelessFields(field_types);
  order |= genex::actions::stable_sort([&](auto a, auto b) {
    // Sort first on alignment, moving the smaller alignments
    // first.
    const auto align_a = dl.getABITypeAlign(field_types[a]).value();
    const auto align_b = dl.getABITypeAlign(field_types[b]).value();
    if (align_a != align_b) { return align_a > align_b; }

    // Then sort on type sizes, moving the smaller sizes first.
    const auto size_a = dl.getTypeAllocSize(field_types[a]).getFixedValue();
    const auto size_b = dl.getTypeAllocSize(field_types[b]).getFixedValue();
    return size_a > size_b;
  });

  // Build the sorted types and the index map. A dropped field
  // has no physical position, so its declaration index is simply
  // absent from the map.
  auto sorted_types = Vec<llvm::Type*>(order.Len());
  auto index_map = Map<std::size_t, std::size_t>(order.Len());
  for (auto new_idx = 0uz; new_idx < order.Len(); new_idx++) {
    sorted_types[new_idx] = field_types[order[new_idx]];
    index_map[order[new_idx]] = new_idx;
  }

  return {std::move(sorted_types), std::move(index_map)};
}

auto spp::codegen::GetPhysicalFieldIndex(
  LlvmTypeSymInfo const &sym_info,
  const std::size_t decl_index)
  -> std::uint32_t {
  // An empty map means the layout preserved the declaration
  // order (the C and packed layouts).
  const auto it = sym_info.FieldIndexMap.find(decl_index);
  const auto index = it != sym_info.FieldIndexMap.end() ? it->second : decl_index;
  return static_cast<std::uint32_t>(index);
}
