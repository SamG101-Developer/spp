module spp.codegen.llvm_size;
import spp.analyse.scopes.scope_manager;
import spp.analyse.scopes.symbols;
import spp.analyse.utils.type_compare;
import spp.analyse.utils.type_members;
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
    asts::TypeAst const &type)
    -> Layout {
    //
    using namespace spp;
    using codegen::Layout;
    using analyse::utils::type_compare::DedupVariableInnerTypes;
    using analyse::utils::type_compare::TypeEq;
    using namespace asts::generate::common_types_precompiled;

    if (const auto param_sym = sm.CurrentScope->GetTypeSymbol(&type);
      param_sym != nullptr and param_sym->IsGeneric and param_sym->LinkedScope != nullptr
      and param_sym->LinkedScope->TySym != nullptr and param_sym->LinkedScope->TySym.get() != param_sym) {
      return LayoutOf(sm, *param_sym->LinkedScope->TySym->FqName());
    }

    // Borrows (mapped to pointers) are pointer-sized.
    if (type.GetConvention() != nullptr) {
      return ScalarLayout(sizeof(void*));
    }

    const auto IsScalar = [&sm](asts::TypeAst const &candidate, asts::TypeAst const &name) {
      const auto name_sym = sm.GlobalScope->GetTypeSymbol(&name);
      return name_sym != nullptr and TypeEq(candidate, *name_sym->FqName(), *sm.CurrentScope, *sm.GlobalScope);
    };

    // Void is 0 bytes.
    if (IsScalar(type, *VOID)) { return Layout{0, 1}; }

    // Boolean is 1 byte.
    if (IsScalar(type, *BOOL)) { return ScalarLayout(1); }

    // 8-bit numbers are 1 byte.
    if (IsScalar(type, *S8)) { return ScalarLayout(1); }
    if (IsScalar(type, *U8)) { return ScalarLayout(1); }
    if (IsScalar(type, *F8)) { return ScalarLayout(1); }

    // 16-bit numbers are 2 bytes.
    if (IsScalar(type, *S16)) { return ScalarLayout(2); }
    if (IsScalar(type, *U16)) { return ScalarLayout(2); }
    if (IsScalar(type, *F16)) { return ScalarLayout(2); }

    // 32-bit numbers are 4 bytes.
    if (IsScalar(type, *S32)) { return ScalarLayout(4); }
    if (IsScalar(type, *U32)) { return ScalarLayout(4); }
    if (IsScalar(type, *F32)) { return ScalarLayout(4); }

    // 64-bit numbers are 8 bytes.
    if (IsScalar(type, *S64)) { return ScalarLayout(8); }
    if (IsScalar(type, *U64)) { return ScalarLayout(8); }
    if (IsScalar(type, *F64)) { return ScalarLayout(8); }

    // 128-bit numbers are 16 bytes.
    if (IsScalar(type, *S128)) { return ScalarLayout(16); }
    if (IsScalar(type, *U128)) { return ScalarLayout(16); }
    if (IsScalar(type, *F128)) { return ScalarLayout(16); }

    // 256-bit numbers are 32 bytes (aligned to 16, the widest alignment the target specifies).
    if (IsScalar(type, *S256)) { return ScalarLayout(32); }
    if (IsScalar(type, *U256)) { return ScalarLayout(32); }

    // Sizes based on pointer size.
    if (IsScalar(type, *SSIZE)) {
      return ScalarLayout(sizeof(std::size_t));
    }
    if (IsScalar(type, *USIZE)) {
      return ScalarLayout(sizeof(std::size_t));
    }

    // A function value is a fat pointer: the code paired with
    // the environment it closes over, so it is two pointers wide,
    // not one. A "$" mock is a function used as a value, so it
    // shares that shape.
    if (TypeEq(*type.WithoutGenerics(), *FUN_MOV, *sm.CurrentScope, *sm.CurrentScope) or
      TypeEq(*type.WithoutGenerics(), *FUN_MUT, *sm.CurrentScope, *sm.CurrentScope) or
      TypeEq(*type.WithoutGenerics(), *FUN_REF, *sm.CurrentScope, *sm.CurrentScope) or
      type.IsCompilerGeneratedType()) {
      return Layout{.Size = 2 * sizeof(void*), .Align = alignof(void*)};
    }

    // "NonNull[T]" is lowered to a bare llvm pointer rather than to a struct wrapping one (see
    // "RegisterLlvmTypeInfo"), so it measures as a pointer; walking its attributes would measure it as empty.
    if (TypeEq(*type.WithoutGenerics(), *NON_NULL, *sm.CurrentScope, *sm.CurrentScope)) {
      return ScalarLayout(sizeof(void*));
    }

    // A generator is *not* a fat pointer: it is the bare
    // "llvm.coro.begin" handle, one pointer wide. The frame it refers
    // to belongs to the llvm coroutine intrinsics.
    if (TypeEq(*type.WithoutGenerics(), *GEN, *sm.CurrentScope, *sm.CurrentScope) or
      TypeEq(*type.WithoutGenerics(), *GEN_ONCE, *sm.CurrentScope, *sm.CurrentScope)) {
      return ScalarLayout(sizeof(void*));
    }

    // An array holds its elements end to end, each padded up to the element alignment, and is aligned like one element.
    if (TypeEq(*type.WithoutGenerics(), *ARR, *sm.CurrentScope, *sm.CurrentScope)) {
      const auto element_type = type.LastTypePart()->GnArgGroup->TypeAt("T")->Val;
      const auto length = std::stoll(
        type.LastTypePart()->GnArgGroup->CompAt("n")->Val->To<asts::IntegerLiteralAst>()->Val->TokenData);
      const auto element_layout = LayoutOf(sm, *element_type);
      return Layout{.Size = element_layout.Size * static_cast<std::size_t>(length), .Align = element_layout.Align};
    }

    // A tuple lowers to a struct of its generic arguments, keeping declaration order, so the elements are laid out in
    // that order rather than being sorted the way a class's attributes are.
    if (TypeEq(*type.WithoutGenerics(), *TUP, *sm.CurrentScope, *sm.CurrentScope)) {
      const auto elems = type.LastTypePart()->GnArgGroup->GetTypeArgs();
      auto elem_layouts = Vec<Layout>();
      elem_layouts.Reserve(elems.Len());
      for (auto const *elem : elems) {
        elem_layouts.EmplaceBack(LayoutOf(sm, *elem->Val));
      }
      return AggregateLayout(elem_layouts);
    }

    // A variant lowers to a discriminant paired with a payload buffer wide enough for its largest member, built out of
    // the widest integer any member needs to be aligned to (see "RegisterLlvmTypeInfo").
    if (TypeEq(*type.WithoutGenerics(), *VAR, *sm.CurrentScope, *sm.CurrentScope)) {
      auto max_size = 0uz;
      auto max_align = 1uz;
      for (auto const &inner_type : DedupVariableInnerTypes(type, *sm.CurrentScope)) {
        const auto inner_layout = LayoutOf(sm, *inner_type);
        max_size = std::max(max_size, inner_layout.Size);
        max_align = std::max(max_align, inner_layout.Align);
      }

      const auto payload_elem_size = std::min(max_align, 16uz);
      const auto payload_size = (max_size + payload_elem_size - 1) / payload_elem_size * payload_elem_size;
      const auto tag_layout = ScalarLayout(sizeof(std::size_t));
      return AggregateLayout(Vec{tag_layout, Layout{.Size = payload_size, .Align = payload_elem_size}});
    }

    // A type with no scope behind it has no attributes to walk;
    // a bare generic parameter reached while a template being
    // measured is the usual case. "GetAllAttrs" would read the
    // superimposition list off the scope it does not have, so
    // provide an empty layout instead (a template is never laid
    // out for real anyway)
    const auto type_sym = sm.CurrentScope->GetTypeSymbol(&type);
    if (type_sym == nullptr or type_sym->LinkedScope == nullptr) {
      return Layout{.Size = 0, .Align = 1};
    }

    // Otherwise lay out the attributes of the struct/class, in
    // the order the S++ layout puts them in: widest alignment
    // first, then largest, which is what minimizes the padding
    // between them.
    auto attr_layouts = Vec<Layout>();
    for (auto const &attr : analyse::utils::type_members::GetAllAttrs(type, sm)) {
      attr_layouts.EmplaceBack(LayoutOf(sm, *spp::get<1>(attr)->FqName()));
    }
    attr_layouts |= genex::actions::stable_sort([](auto const &a, auto const &b) {
      return a.Align != b.Align ? a.Align > b.Align : a.Size > b.Size;
    });
    return AggregateLayout(attr_layouts);
  }
}

auto spp::codegen::SizeOf(
  analyse::scopes::ScopeManager const &sm,
  asts::TypeAst const &type)
  -> std::size_t {
  // The size of a type is the size of the object it lowers to, padding included.
  return LayoutOf(sm, type).Size;
}

auto spp::codegen::AlignOf(
  analyse::scopes::ScopeManager const &sm,
  asts::TypeAst const &type)
  -> std::size_t {
  // The alignment of a type is the alignment of the object it lowers to.
  return LayoutOf(sm, type).Align;
}
