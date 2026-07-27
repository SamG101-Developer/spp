module spp.codegen.llvm_size;
import spp.analyse.scopes.scope_manager;
import spp.analyse.scopes.symbols;
import spp.analyse.utils.type_utils;
import spp.asts.ast;
import spp.asts.class_attribute_ast;
import spp.asts.generic_argument_comp_ast;
import spp.asts.generic_argument_group_ast;
import spp.asts.generic_argument_type_ast;
import spp.asts.identifier_ast;
import spp.asts.integer_literal_ast;
import spp.asts.token_ast;
import spp.asts.type_ast;
import spp.asts.type_identifier_ast;
import spp.asts.generate.common_types_precompiled;
import spp.asts.utils.ast_utils;
import genex;

namespace spp::codegen {
  struct Layout {
    std::size_t Size;
    std::size_t Align;
  };

  static auto RoundUpTo(const std::size_t size, const std::size_t align) -> std::size_t {
    return align <= 1 ? size : (size + align - 1) / align * align;
  }

  static auto ScalarLayout(const std::size_t size) -> Layout {
    return Layout{.Size = size, .Align = std::min(size, 16uz)};
  }

  static auto AggregateLayout(Vec<Layout> const &fields) -> Layout {
    auto size = 0uz;
    auto align = 1uz;
    for (auto const &field : fields) {
      align = std::max(align, field.Align);
      size = RoundUpTo(size, field.Align) + field.Size;
    }
    return Layout{.Size = RoundUpTo(size, align), .Align = align};
  }

  static auto LayoutOf(
    analyse::scopes::ScopeManager const &sm,
    Shared<asts::TypeAst> const &type)
    -> Layout {
    //
    using namespace spp;
    using codegen::Layout;
    using analyse::utils::type_utils::DedupVariableInnerTypes;
    using analyse::utils::type_utils::TypeEq;
    using namespace asts::generate::common_types_precompiled;

    // Borrows (mapped to pointers) are pointer-sized.
    if (type->GetConvention() != nullptr) {
      return ScalarLayout(sizeof(void*));
    }

    // Void is 0 bytes.
    if (TypeEq(*type->WithoutGenerics(), *VOID, *sm.CurrentScope, *sm.CurrentScope)) { return Layout{0, 1}; }

    // Boolean is 1 byte.
    if (TypeEq(*type->WithoutGenerics(), *BOOL, *sm.CurrentScope, *sm.CurrentScope)) { return ScalarLayout(1); }

    // 8-bit numbers are 1 byte.
    if (TypeEq(*type->WithoutGenerics(), *S8, *sm.CurrentScope, *sm.CurrentScope)) { return ScalarLayout(1); }
    if (TypeEq(*type->WithoutGenerics(), *U8, *sm.CurrentScope, *sm.CurrentScope)) { return ScalarLayout(1); }
    if (TypeEq(*type->WithoutGenerics(), *F8, *sm.CurrentScope, *sm.CurrentScope)) { return ScalarLayout(1); }

    // 16-bit numbers are 2 bytes.
    if (TypeEq(*type->WithoutGenerics(), *S16, *sm.CurrentScope, *sm.CurrentScope)) { return ScalarLayout(2); }
    if (TypeEq(*type->WithoutGenerics(), *U16, *sm.CurrentScope, *sm.CurrentScope)) { return ScalarLayout(2); }
    if (TypeEq(*type->WithoutGenerics(), *F16, *sm.CurrentScope, *sm.CurrentScope)) { return ScalarLayout(2); }

    // 32-bit numbers are 4 bytes.
    if (TypeEq(*type->WithoutGenerics(), *S32, *sm.CurrentScope, *sm.CurrentScope)) { return ScalarLayout(4); }
    if (TypeEq(*type->WithoutGenerics(), *U32, *sm.CurrentScope, *sm.CurrentScope)) { return ScalarLayout(4); }
    if (TypeEq(*type->WithoutGenerics(), *F32, *sm.CurrentScope, *sm.CurrentScope)) { return ScalarLayout(4); }

    // 64-bit numbers are 8 bytes.
    if (TypeEq(*type->WithoutGenerics(), *S64, *sm.CurrentScope, *sm.CurrentScope)) { return ScalarLayout(8); }
    if (TypeEq(*type->WithoutGenerics(), *U64, *sm.CurrentScope, *sm.CurrentScope)) { return ScalarLayout(8); }
    if (TypeEq(*type->WithoutGenerics(), *F64, *sm.CurrentScope, *sm.CurrentScope)) { return ScalarLayout(8); }

    // 128-bit numbers are 16 bytes.
    if (TypeEq(*type->WithoutGenerics(), *S128, *sm.CurrentScope, *sm.CurrentScope)) { return ScalarLayout(16); }
    if (TypeEq(*type->WithoutGenerics(), *U128, *sm.CurrentScope, *sm.CurrentScope)) { return ScalarLayout(16); }
    if (TypeEq(*type->WithoutGenerics(), *F128, *sm.CurrentScope, *sm.CurrentScope)) { return ScalarLayout(16); }

    // 256-bit numbers are 32 bytes (aligned to 16, the widest alignment the target specifies).
    if (TypeEq(*type->WithoutGenerics(), *S256, *sm.CurrentScope, *sm.CurrentScope)) { return ScalarLayout(32); }
    if (TypeEq(*type->WithoutGenerics(), *U256, *sm.CurrentScope, *sm.CurrentScope)) { return ScalarLayout(32); }

    // Sizes based on pointer size.
    if (TypeEq(*type->WithoutGenerics(), *SSIZE, *sm.CurrentScope, *sm.CurrentScope)) {
      return ScalarLayout(sizeof(std::size_t));
    }
    if (TypeEq(*type->WithoutGenerics(), *USIZE, *sm.CurrentScope, *sm.CurrentScope)) {
      return ScalarLayout(sizeof(std::size_t));
    }

    // Smart pointers (heap wrappers, ignore attributes).
    if (TypeEq(*type->WithoutGenerics(), *SINGLE, *sm.CurrentScope, *sm.CurrentScope)) {
      return ScalarLayout(sizeof(void*));
    }
    if (TypeEq(*type->WithoutGenerics(), *SHARED, *sm.CurrentScope, *sm.CurrentScope)) {
      return Layout{sizeof(void*) + 2 * sizeof(std::size_t), alignof(void*)};
    }
    if (TypeEq(*type->WithoutGenerics(), *SHADOW, *sm.CurrentScope, *sm.CurrentScope)) {
      return ScalarLayout(sizeof(void*));
    }

    // Functions and generators are fat pointers: the code (or resume function) paired with the environment it closes
    // over, so they are two pointers wide, not one.
    if (TypeEq(*type->WithoutGenerics(), *FUN_MOV, *sm.CurrentScope, *sm.CurrentScope) or
      TypeEq(*type->WithoutGenerics(), *FUN_MUT, *sm.CurrentScope, *sm.CurrentScope) or
      TypeEq(*type->WithoutGenerics(), *FUN_REF, *sm.CurrentScope, *sm.CurrentScope) or
      TypeEq(*type->WithoutGenerics(), *GEN, *sm.CurrentScope, *sm.CurrentScope) or
      TypeEq(*type->WithoutGenerics(), *GEN_ONCE, *sm.CurrentScope, *sm.CurrentScope) or
      type->IsCompilerGeneratedType()) {
      return Layout{.Size = 2 * sizeof(void*), .Align = alignof(void*)};
    }

    // An array holds its elements end to end, each padded up to the element alignment, and is aligned like one element.
    if (TypeEq(*type->WithoutGenerics(), *ARR, *sm.CurrentScope, *sm.CurrentScope)) {
      const auto element_type = type->LastTypePart()->GnArgGroup->TypeAt("T")->Val;
      const auto length = std::stoll(
        type->LastTypePart()->GnArgGroup->CompAt("n")->Val->To<asts::IntegerLiteralAst>()->Val->TokenData);
      const auto element_layout = LayoutOf(sm, element_type);
      return Layout{.Size = element_layout.Size * static_cast<std::size_t>(length), .Align = element_layout.Align};
    }

    // A tuple lowers to a struct of its generic arguments, keeping declaration order, so the elements are laid out in
    // that order rather than being sorted the way a class's attributes are.
    if (TypeEq(*type->WithoutGenerics(), *TUP, *sm.CurrentScope, *sm.CurrentScope)) {
      const auto elems = type->LastTypePart()->GnArgGroup->GetTypeArgs();
      auto elem_layouts = Vec<Layout>();
      elem_layouts.Reserve(elems.Len());
      for (auto const *elem : elems) {
        elem_layouts.EmplaceBack(LayoutOf(sm, elem->Val));
      }
      return AggregateLayout(elem_layouts);
    }

    // A variant lowers to a discriminant paired with a payload buffer wide enough for its largest member, built out of
    // the widest integer any member needs to be aligned to (see "RegisterLlvmTypeInfo").
    if (TypeEq(*type->WithoutGenerics(), *VAR, *sm.CurrentScope, *sm.CurrentScope)) {
      auto max_size = 0uz;
      auto max_align = 1uz;
      for (auto const &inner_type : DedupVariableInnerTypes(*type, *sm.CurrentScope)) {
        const auto inner_layout = LayoutOf(sm, inner_type);
        max_size = std::max(max_size, inner_layout.Size);
        max_align = std::max(max_align, inner_layout.Align);
      }

      const auto payload_elem_size = std::min(max_align, 16uz);
      const auto payload_size = (max_size + payload_elem_size - 1) / payload_elem_size * payload_elem_size;
      const auto tag_layout = ScalarLayout(sizeof(std::size_t));
      return AggregateLayout(Vec<Layout>{tag_layout, Layout{.Size = payload_size, .Align = payload_elem_size}});
    }

    // Otherwise lay out the attributes of the struct/class, in the order the S++ layout puts them in: widest alignment
    // first, then largest, which is what minimizes the padding between them.
    auto attr_layouts = Vec<Layout>();
    for (auto const &attr : analyse::utils::type_utils::GetAllAttrs(*type, sm)) {
      attr_layouts.EmplaceBack(LayoutOf(sm, std::get<1>(attr)->FqName()));
    }
    attr_layouts |= genex::actions::stable_sort([](auto const &a, auto const &b) {
      return a.Align != b.Align ? a.Align > b.Align : a.Size > b.Size;
    });
    return AggregateLayout(attr_layouts);
  }
}

auto spp::codegen::SizeOf(
  analyse::scopes::ScopeManager const &sm,
  Shared<asts::TypeAst> const &type)
  -> std::size_t {
  // The size of a type is the size of the object it lowers to, padding included.
  return LayoutOf(sm, type).Size;
}
