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


auto spp::codegen::SizeOf(
    analyse::scopes::ScopeManager const &sm,
    Shared<asts::TypeAst> const &type)
    -> std::size_t {
    //
    using analyse::utils::type_utils::TypeEq;
    using namespace asts::generate::common_types_precompiled;

    // Borrows (mapped to pointers) are pointer-sized.
    if (type->GetConvention() != nullptr) {
        return sizeof(void*);
    }

    // Void is 0 bytes.
    if (TypeEq(*type->WithoutGenerics(), *VOID, *sm.CurrentScope, *sm.CurrentScope)) { return 0; }

    // Boolean is 1 byte.
    if (TypeEq(*type->WithoutGenerics(), *BOOL, *sm.CurrentScope, *sm.CurrentScope)) { return 1; }

    // 8-bit numbers are 1 byte.
    if (TypeEq(*type->WithoutGenerics(), *S8, *sm.CurrentScope, *sm.CurrentScope)) { return 1; }
    if (TypeEq(*type->WithoutGenerics(), *U8, *sm.CurrentScope, *sm.CurrentScope)) { return 1; }
    if (TypeEq(*type->WithoutGenerics(), *F8, *sm.CurrentScope, *sm.CurrentScope)) { return 1; }

    // 16-bit numbers are 2 bytes.
    if (TypeEq(*type->WithoutGenerics(), *S16, *sm.CurrentScope, *sm.CurrentScope)) { return 2; }
    if (TypeEq(*type->WithoutGenerics(), *U16, *sm.CurrentScope, *sm.CurrentScope)) { return 2; }
    if (TypeEq(*type->WithoutGenerics(), *F16, *sm.CurrentScope, *sm.CurrentScope)) { return 2; }

    // 32-bit numbers are 4 bytes.
    if (TypeEq(*type->WithoutGenerics(), *S32, *sm.CurrentScope, *sm.CurrentScope)) { return 4; }
    if (TypeEq(*type->WithoutGenerics(), *U32, *sm.CurrentScope, *sm.CurrentScope)) { return 4; }
    if (TypeEq(*type->WithoutGenerics(), *F32, *sm.CurrentScope, *sm.CurrentScope)) { return 4; }

    // 64-bit numbers are 8 bytes.
    if (TypeEq(*type->WithoutGenerics(), *S64, *sm.CurrentScope, *sm.CurrentScope)) { return 8; }
    if (TypeEq(*type->WithoutGenerics(), *U64, *sm.CurrentScope, *sm.CurrentScope)) { return 8; }
    if (TypeEq(*type->WithoutGenerics(), *F64, *sm.CurrentScope, *sm.CurrentScope)) { return 8; }

    // 128-bit numbers are 16 bytes.
    if (TypeEq(*type->WithoutGenerics(), *S128, *sm.CurrentScope, *sm.CurrentScope)) { return 16; }
    if (TypeEq(*type->WithoutGenerics(), *U128, *sm.CurrentScope, *sm.CurrentScope)) { return 16; }
    if (TypeEq(*type->WithoutGenerics(), *F128, *sm.CurrentScope, *sm.CurrentScope)) { return 16; }

    // 256-bit numbers are 32 bytes.
    if (TypeEq(*type->WithoutGenerics(), *S256, *sm.CurrentScope, *sm.CurrentScope)) { return 32; }
    if (TypeEq(*type->WithoutGenerics(), *U256, *sm.CurrentScope, *sm.CurrentScope)) { return 32; }

    // Sizes based on pointer size.
    if (TypeEq(*type->WithoutGenerics(), *SSIZE, *sm.CurrentScope, *sm.CurrentScope)) { return sizeof(std::size_t); }
    if (TypeEq(*type->WithoutGenerics(), *USIZE, *sm.CurrentScope, *sm.CurrentScope)) { return sizeof(std::size_t); }

    // Smart pointers (heap wrappers, ignore attributes).
    if (TypeEq(*type->WithoutGenerics(), *SINGLE, *sm.CurrentScope, *sm.CurrentScope)) { return sizeof(void*); }
    if (TypeEq(*type->WithoutGenerics(), *SHARED, *sm.CurrentScope, *sm.CurrentScope)) { return sizeof(void*) + 2 * sizeof(std::size_t); }
    if (TypeEq(*type->WithoutGenerics(), *SHADOW, *sm.CurrentScope, *sm.CurrentScope)) { return sizeof(void*); }

    // Functions are pointer-sized.
    if (TypeEq(*type->WithoutGenerics(), *FUN_MOV, *sm.CurrentScope, *sm.CurrentScope)) { return sizeof(void*); }
    if (TypeEq(*type->WithoutGenerics(), *FUN_MUT, *sm.CurrentScope, *sm.CurrentScope)) { return sizeof(void*); }
    if (TypeEq(*type->WithoutGenerics(), *FUN_REF, *sm.CurrentScope, *sm.CurrentScope)) { return sizeof(void*); }

    // Array (fixed length) size is element size * length.
    if (TypeEq(*type->WithoutGenerics(), *ARR, *sm.CurrentScope, *sm.CurrentScope)) {
        const auto element_type = type->TypeParts().Back()->GnArgGroup->TypeAt("T")->Val;
        const auto length = std::stoll(type->TypeParts().Back()->GnArgGroup->CompAt("n")->Val->To<asts::IntegerLiteralAst>()->Val->TokenData);
        return SizeOf(sm, element_type) * static_cast<std::size_t>(length);
    }

    // Tuple (sum the sizes of its contained types).
    if (TypeEq(*type->WithoutGenerics(), *TUP, *sm.CurrentScope, *sm.CurrentScope)) {
        const auto all_types = type->TypeParts().Back()->GnArgGroup->GetTypeArgs()
            | genex::views::transform([&sm](auto &&x) { return SizeOf(sm, x->Val); })
            | genex::to<Vec>();
        const auto total_size = genex::fold_left_first(all_types, std::plus{});
        return total_size;
    }

    // Variant size is the maximum of its contained types + a discriminator (usize).
    if (TypeEq(*type->WithoutGenerics(), *VAR, *sm.CurrentScope, *sm.CurrentScope)) {
        auto min_size = std::numeric_limits<std::size_t>::max();
        for (auto const &inner_type : type->TypeParts().Back()->GnArgGroup->GetTypeArgs()) {
            min_size = std::min(min_size, SizeOf(sm, inner_type->Val));
        }
        return min_size + sizeof(std::size_t);
    }

    // Otherwise, sum the attributes of the struct/class.
    const auto all_types = analyse::utils::type_utils::GetAllAttrs(*type, &sm)
        | genex::views::transform([](auto &&x) { return x.Second->FqName(); })
        | genex::views::transform([&sm](auto &&x) { return SizeOf(sm, x); })
        | genex::to<Vec>();
    const auto total_size = genex::fold_left_first(all_types, std::plus{});
    return total_size;
}
