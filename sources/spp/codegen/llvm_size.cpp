module spp.codegen.llvm_size;
import spp.analyse.scopes.scope_manager;
import spp.analyse.scopes.symbols;
import spp.analyse.utils.type_compare;
import spp.analyse.utils.type_members;
import spp.analyse.utils.type_predicates;
import spp.asts.ast;
import spp.asts.class_attribute_ast;
import spp.asts.generic_argument_ast;
import spp.asts.generic_argument_group_ast;
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

  // A function value is a fat pointer: the code paired with the environment it closes over, so it is two pointers wide,
  // not one.
  static constexpr auto kFatPointerLayout = Layout{.Size = 2 * sizeof(void*), .Align = alignof(void*)};

  static auto LayoutOf(
    analyse::scopes::ScopeManager const &sm,
    analyse::scopes::TypeRef const &ref)
    -> Layout {
    //
    using namespace spp;
    using codegen::Layout;
    using analyse::scopes::TypeRef;
    using analyse::utils::type_compare::VariantMemberRefs;
    using analyse::utils::type_predicates::IsTypeArray;
    using analyse::utils::type_predicates::IsTypeFunction;
    using analyse::utils::type_predicates::IsTypeGenerator;
    using analyse::utils::type_predicates::IsTypeTuple;
    using analyse::utils::type_predicates::IsTypeVariant;
    using namespace asts::generate::common_types_precompiled;
    auto const &scope = *sm.CurrentScope;

    // Borrows (mapped to pointers) are pointer-sized, a borrowed binding ("T=&S32") included.
    if (ref.IsBorrowed()) {
      return ScalarLayout(sizeof(void*));
    }

    // A type resolving to nothing has nothing to measure. (A "$" mock, a function used as a value, is registered
    // globally, so it always resolves.) A binding is measured as its bound type.
    if (ref.Symbol == nullptr) {
      return Layout{.Size = 0, .Align = 1};
    }
    auto const &sym = *ref.Symbol->AsBound();

    // A scalar is matched by identity: the scope a symbol links to is the type itself, reached alike through an alias
    // ("S32" is an instance of "SizedInteger") or a binding. Not by template, which every sized integer shares.
    const auto IsScalar = [&sm, &sym](asts::TypeAst const &name) {
      const auto name_sym = sm.GlobalScope->FindTypeSymbol(&name);
      return name_sym != nullptr and sym.LinkedScope != nullptr and sym.LinkedScope == name_sym->LinkedScope;
    };

    // Void is 0 bytes.
    if (IsScalar(*VOID)) { return Layout{0, 1}; }

    // Boolean is 1 byte.
    if (IsScalar(*BOOL)) { return ScalarLayout(1); }

    // 8-bit numbers are 1 byte.
    if (IsScalar(*S8)) { return ScalarLayout(1); }
    if (IsScalar(*U8)) { return ScalarLayout(1); }
    if (IsScalar(*F8)) { return ScalarLayout(1); }

    // 16-bit numbers are 2 bytes.
    if (IsScalar(*S16)) { return ScalarLayout(2); }
    if (IsScalar(*U16)) { return ScalarLayout(2); }
    if (IsScalar(*F16)) { return ScalarLayout(2); }

    // 32-bit numbers are 4 bytes.
    if (IsScalar(*S32)) { return ScalarLayout(4); }
    if (IsScalar(*U32)) { return ScalarLayout(4); }
    if (IsScalar(*F32)) { return ScalarLayout(4); }

    // 64-bit numbers are 8 bytes.
    if (IsScalar(*S64)) { return ScalarLayout(8); }
    if (IsScalar(*U64)) { return ScalarLayout(8); }
    if (IsScalar(*F64)) { return ScalarLayout(8); }

    // 128-bit numbers are 16 bytes.
    if (IsScalar(*S128)) { return ScalarLayout(16); }
    if (IsScalar(*U128)) { return ScalarLayout(16); }
    if (IsScalar(*F128)) { return ScalarLayout(16); }

    // 256-bit numbers are 32 bytes (aligned to 16, the widest alignment the target specifies).
    if (IsScalar(*S256)) { return ScalarLayout(32); }
    if (IsScalar(*U256)) { return ScalarLayout(32); }

    // Sizes based on pointer size.
    if (IsScalar(*SSIZE)) {
      return ScalarLayout(sizeof(std::size_t));
    }
    if (IsScalar(*USIZE)) {
      return ScalarLayout(sizeof(std::size_t));
    }

    // A "$" mock is a function used as a value, so it shares a function value's shape.
    if (IsTypeFunction(ref, scope) or sym.Name->IsCompilerGeneratedType()) {
      return kFatPointerLayout;
    }

    // "NonNull[T]" is lowered to a bare llvm pointer rather than to a struct wrapping one (see
    // "RegisterLlvmTypeInfo"), so it measures as a pointer; walking its attributes would measure it as empty.
    if (ref.IsA(*NON_NULL, scope)) {
      return ScalarLayout(sizeof(void*));
    }

    // A generator is *not* a fat pointer: it is the bare
    // "llvm.coro.begin" handle, one pointer wide. The frame it refers
    // to belongs to the llvm coroutine intrinsics.
    if (IsTypeGenerator(ref, scope)) {
      return ScalarLayout(sizeof(void*));
    }

    // An array holds its elements end to end, each padded up to the element alignment, and is aligned like one element.
    // The length and the element type are the instantiation's own bindings of "n" and "T".
    if (IsTypeArray(ref, scope)) {
      const auto length = std::stoll(sym.CompArg("n")->To<asts::IntegerLiteralAst>()->Val->TokenData);
      const auto element_layout = LayoutOf(sm, sym.TypeArgRef("T"));
      return Layout{.Size = element_layout.Size * static_cast<std::size_t>(length), .Align = element_layout.Align};
    }

    // A tuple's and a variant's arguments are left positional, so they have no bindings to read; they are read from
    // the instantiation's own name, which an alias shares through the scope it links to.
    auto const &cls = *sym.LinkedSymbol();

    // A tuple lowers to a struct of its generic arguments, keeping declaration order, so the elements are laid out in
    // that order rather than being sorted the way a class's attributes are.
    if (IsTypeTuple(ref, scope)) {
      const auto elems = sym.TypeArgs();
      auto elem_layouts = Vec<Layout>();
      elem_layouts.Reserve(elems.Len());
      for (auto const &elem : elems) {
        elem_layouts.EmplaceBack(LayoutOf(sm, TypeRef::Of(*elem, scope)));
      }
      return AggregateLayout(elem_layouts);
    }

    // A variant lowers to a discriminant paired with a payload buffer wide enough for its largest member, built out of
    // the widest integer any member needs to be aligned to (see "RegisterLlvmTypeInfo").
    if (IsTypeVariant(ref, scope)) {
      auto max_size = 0uz;
      auto max_align = 1uz;
      const auto self_ref = TypeRef::Of(const_cast<analyse::scopes::TypeSymbol&>(cls), scope);
      for (auto const &member : VariantMemberRefs(self_ref, scope)) {
        const auto inner_layout = LayoutOf(sm, member);
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
    if (sym.LinkedScope == nullptr) {
      return Layout{.Size = 0, .Align = 1};
    }

    // Otherwise lay out the attributes of the struct/class, in
    // the order the S++ layout puts them in: widest alignment
    // first, then largest, which is what minimizes the padding
    // between them.
    auto attr_layouts = Vec<Layout>();
    for (auto const &attr : analyse::utils::type_members::GetAllAttrs(sym)) {
      attr_layouts.EmplaceBack(LayoutOf(sm, spp::get<1>(attr)));
    }
    attr_layouts |= genex::actions::stable_sort([](auto const &a, auto const &b) {
      return a.Align != b.Align ? a.Align > b.Align : a.Size > b.Size;
    });
    return AggregateLayout(attr_layouts);
  }
}

auto spp::codegen::SizeOf(
  analyse::scopes::ScopeManager const &sm,
  analyse::scopes::TypeRef const &ref)
  -> std::size_t {
  // The size of a type is the size of the object it lowers to, padding included.
  return LayoutOf(sm, ref).Size;
}

auto spp::codegen::AlignOf(
  analyse::scopes::ScopeManager const &sm,
  analyse::scopes::TypeRef const &ref)
  -> std::size_t {
  // The alignment of a type is the alignment of the object it lowers to.
  return LayoutOf(sm, ref).Align;
}
