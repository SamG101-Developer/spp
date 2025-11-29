module;
#include <genex/to_container.hpp>
#include <genex/algorithms/fold_left_first.hpp>
#include <genex/views/cast_dynamic.hpp>
#include <genex/views/transform.hpp>

module spp.codegen.llvm_size;
import spp.analyse.scopes.scope_manager;
import spp.analyse.utils.type_utils;
import spp.asts.ast;
import spp.asts.class_attribute_ast;
import spp.asts.generic_argument_comp_ast;
import spp.asts.generic_argument_group_ast;
import spp.asts.generic_argument_type_ast;
import spp.asts.integer_literal_ast;
import spp.asts.token_ast;
import spp.asts.type_ast;
import spp.asts.type_identifier_ast;
import spp.asts.generate.common_types_precompiled;
import spp.asts.utils.ast_utils;


auto spp::codegen::size_of(
    analyse::scopes::ScopeManager const &sm,
    std::shared_ptr<asts::TypeAst> const &type)
    -> std::size_t {
    // Borrows (mapped to pointers) are pointer-sized.
    if (type->get_convention() != nullptr) {
        return sizeof(void*);
    }

    // Void is 0 bytes.
    if (analyse::utils::type_utils::symbolic_eq(*type->without_generics(), *asts::generate::common_types_precompiled::VOID, *sm.current_scope, *sm.current_scope)) {
        return 0;
    }

    // Boolean is 1 byte.
    if (analyse::utils::type_utils::symbolic_eq(*type->without_generics(), *asts::generate::common_types_precompiled::BOOL, *sm.current_scope, *sm.current_scope)) {
        return 1;
    }

    // 8-bit numbers are 1 byte.
    if (analyse::utils::type_utils::symbolic_eq(*type->without_generics(), *asts::generate::common_types_precompiled::S8, *sm.current_scope, *sm.current_scope)) {
        return 1;
    }
    if (analyse::utils::type_utils::symbolic_eq(*type->without_generics(), *asts::generate::common_types_precompiled::U8, *sm.current_scope, *sm.current_scope)) {
        return 1;
    }
    if (analyse::utils::type_utils::symbolic_eq(*type->without_generics(), *asts::generate::common_types_precompiled::F8, *sm.current_scope, *sm.current_scope)) {
        return 1;
    }

    // 16-bit numbers are 2 bytes.
    if (analyse::utils::type_utils::symbolic_eq(*type->without_generics(), *asts::generate::common_types_precompiled::S16, *sm.current_scope, *sm.current_scope)) {
        return 2;
    }
    if (analyse::utils::type_utils::symbolic_eq(*type->without_generics(), *asts::generate::common_types_precompiled::U16, *sm.current_scope, *sm.current_scope)) {
        return 2;
    }
    if (analyse::utils::type_utils::symbolic_eq(*type->without_generics(), *asts::generate::common_types_precompiled::F16, *sm.current_scope, *sm.current_scope)) {
        return 2;
    }

    // 32-bit numbers are 4 bytes.
    if (analyse::utils::type_utils::symbolic_eq(*type->without_generics(), *asts::generate::common_types_precompiled::S32, *sm.current_scope, *sm.current_scope)) {
        return 4;
    }
    if (analyse::utils::type_utils::symbolic_eq(*type->without_generics(), *asts::generate::common_types_precompiled::U32, *sm.current_scope, *sm.current_scope)) {
        return 4;
    }
    if (analyse::utils::type_utils::symbolic_eq(*type->without_generics(), *asts::generate::common_types_precompiled::F32, *sm.current_scope, *sm.current_scope)) {
        return 4;
    }

    // 64-bit numbers are 8 bytes.
    if (analyse::utils::type_utils::symbolic_eq(*type->without_generics(), *asts::generate::common_types_precompiled::S64, *sm.current_scope, *sm.current_scope)) {
        return 8;
    }
    if (analyse::utils::type_utils::symbolic_eq(*type->without_generics(), *asts::generate::common_types_precompiled::U64, *sm.current_scope, *sm.current_scope)) {
        return 8;
    }
    if (analyse::utils::type_utils::symbolic_eq(*type->without_generics(), *asts::generate::common_types_precompiled::F64, *sm.current_scope, *sm.current_scope)) {
        return 8;
    }

    // 128-bit numbers are 16 bytes.
    if (analyse::utils::type_utils::symbolic_eq(*type->without_generics(), *asts::generate::common_types_precompiled::S128, *sm.current_scope, *sm.current_scope)) {
        return 16;
    }
    if (analyse::utils::type_utils::symbolic_eq(*type->without_generics(), *asts::generate::common_types_precompiled::U128, *sm.current_scope, *sm.current_scope)) {
        return 16;
    }
    if (analyse::utils::type_utils::symbolic_eq(*type->without_generics(), *asts::generate::common_types_precompiled::F128, *sm.current_scope, *sm.current_scope)) {
        return 16;
    }

    // 256-bit numbers are 32 bytes.
    if (analyse::utils::type_utils::symbolic_eq(*type->without_generics(), *asts::generate::common_types_precompiled::S256, *sm.current_scope, *sm.current_scope)) {
        return 32;
    }
    if (analyse::utils::type_utils::symbolic_eq(*type->without_generics(), *asts::generate::common_types_precompiled::U256, *sm.current_scope, *sm.current_scope)) {
        return 32;
    }

    // Sizes based on pointer size.
    if (analyse::utils::type_utils::symbolic_eq(*type->without_generics(), *asts::generate::common_types_precompiled::SSIZE, *sm.current_scope, *sm.current_scope)) {
        return sizeof(std::size_t);
    }
    if (analyse::utils::type_utils::symbolic_eq(*type->without_generics(), *asts::generate::common_types_precompiled::USIZE, *sm.current_scope, *sm.current_scope)) {
        return sizeof(std::size_t);
    }

    // Smart pointers (heap wrappers, ignore attributes).
    if (analyse::utils::type_utils::symbolic_eq(*type->without_generics(), *asts::generate::common_types_precompiled::SINGLE, *sm.current_scope, *sm.current_scope)) {
        return sizeof(void*);
    }
    if (analyse::utils::type_utils::symbolic_eq(*type->without_generics(), *asts::generate::common_types_precompiled::SHARED, *sm.current_scope, *sm.current_scope)) {
        return sizeof(void*) + 2 * sizeof(std::size_t);
    }
    if (analyse::utils::type_utils::symbolic_eq(*type->without_generics(), *asts::generate::common_types_precompiled::SHADOW, *sm.current_scope, *sm.current_scope)) {
        return sizeof(void*);
    }

    // Functions are pointer-sized.
    if (analyse::utils::type_utils::symbolic_eq(*type->without_generics(), *asts::generate::common_types_precompiled::FUN_MOV, *sm.current_scope, *sm.current_scope)) {
        return sizeof(void*);
    }
    if (analyse::utils::type_utils::symbolic_eq(*type->without_generics(), *asts::generate::common_types_precompiled::FUN_MUT, *sm.current_scope, *sm.current_scope)) {
        return sizeof(void*);
    }
    if (analyse::utils::type_utils::symbolic_eq(*type->without_generics(), *asts::generate::common_types_precompiled::FUN_REF, *sm.current_scope, *sm.current_scope)) {
        return sizeof(void*);
    }

    // Array (fixed length) size is element size * length.
    if (analyse::utils::type_utils::symbolic_eq(*type->without_generics(), *asts::generate::common_types_precompiled::ARR, *sm.current_scope, *sm.current_scope)) {
        const auto element_type = type->type_parts().back()->generic_arg_group->type_at("T")->val;
        const auto length = std::stoll(asts::ast_cast<asts::IntegerLiteralAst>(type->type_parts().back()->generic_arg_group->comp_at("n")->val.get())->val->token_data);
        return size_of(sm, element_type) * static_cast<std::size_t>(length);
    }

    // Tuple (sum the sizes of its contained types).
    if (analyse::utils::type_utils::symbolic_eq(*type->without_generics(), *asts::generate::common_types_precompiled::TUP, *sm.current_scope, *sm.current_scope)) {
        const auto all_types = type->type_parts().back()->generic_arg_group->get_type_args()
            | genex::views::transform([&sm](auto &&x) { return size_of(sm, x->val); })
            | genex::to<std::vector>();
        const auto total_size = genex::algorithms::fold_left_first(all_types, std::plus{});
        return total_size;
    }

    // Variant size is the maximum of its contained types + a discriminator (usize).
    if (analyse::utils::type_utils::symbolic_eq(*type->without_generics(), *asts::generate::common_types_precompiled::VAR, *sm.current_scope, *sm.current_scope)) {
        auto min_size = std::numeric_limits<std::size_t>::max();
        for (auto const &inner_type : type->type_parts().back()->generic_arg_group->get_type_args()) {
            min_size = std::min(min_size, size_of(sm, inner_type->val));
        }
        return min_size + sizeof(std::size_t);
    }

    // Otherwise, sum the attributes of the struct/class.
    const auto all_types = analyse::utils::type_utils::get_all_attrs(*type, &sm)
        | genex::views::transform([](auto &&x) { return std::get<0>(x)->type; })
        | genex::views::transform([&sm](auto &&x) { return size_of(sm, x); })
        | genex::to<std::vector>();
    const auto total_size = genex::algorithms::fold_left_first(all_types, std::plus{});
    return total_size;
}
