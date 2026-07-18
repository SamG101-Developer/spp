module spp.codegen.llvm_layout;
import genex;

auto spp::codegen::SortMembersForSppLayout(
    Vec<llvm::Type*> const &field_types,
    spp::codegen::LLvmCtx const *ctx)
    -> Pair<Vec<llvm::Type*>, ankerl::unordered_dense::map<std::size_t, std::size_t>> {
    // Based on the ABI detected size of each type, re-order for minimal total object size (minimize padding). Return
    // the re-ordered fields and the field index mapping.
    auto const &dl = ctx->Module->getDataLayout();
    auto order = genex::views::iota(0uz, field_types.Len()) | genex::to<Vec>();
    order |= genex::actions::stable_sort([&](auto a, auto b) {
        // Sort first on alignment, moving the smaller alignments first.
        const auto align_a = dl.getABITypeAlign(field_types[a]).value();
        const auto align_b = dl.getABITypeAlign(field_types[b]).value();
        if (align_a != align_b) { return align_a > align_b; }

        // Then sort on type sizes, moving the smaller sizes first.
        const auto size_a = dl.getTypeAllocSize(field_types[a]).getFixedValue();
        const auto size_b = dl.getTypeAllocSize(field_types[b]).getFixedValue();
        return size_a > size_b;
    });

    // Build the sorted types and the index map.
    auto sorted_types = Vec<llvm::Type*>(field_types.Len());
    auto index_map = ankerl::unordered_dense::map<std::size_t, std::size_t>(field_types.Len());
    for (auto new_idx = 0uz; new_idx < field_types.Len(); new_idx++) {
        sorted_types[new_idx] = field_types[order[new_idx]];
        index_map[order[new_idx]] = new_idx;
    }

    return MakePair(std::move(sorted_types), std::move(index_map));
}
