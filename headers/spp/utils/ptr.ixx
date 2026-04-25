module;
#include <spp/macros.hpp>

export module spp.utils.ptr;
import std;


namespace spp::utils::ptr {
    SPP_EXP_FUN template <typename Out, typename In>
    requires (std::derived_from<Out, In> or std::derived_from<In, Out>)
    auto unique_cast(std::unique_ptr<In> &&ptr) -> std::unique_ptr<Out> {
        if (ptr == nullptr) { return nullptr; }
        return std::unique_ptr<Out>(dynamic_cast<Out*>(ptr.release()));
    }

    SPP_EXP_FUN template <typename Out, typename In>
    requires (std::derived_from<Out, In> or std::derived_from<In, Out>)
    auto shared_cast(std::shared_ptr<In> const &ptr) -> std::shared_ptr<Out> {
        if (ptr == nullptr) { return nullptr; }
        return std::dynamic_pointer_cast<Out>(ptr);
    }

    SPP_EXP_FUN template <typename Out, typename In>
    requires (std::same_as<std::remove_cvref_t<In>, std::remove_cvref_t<Out>> and not std::same_as<In, Out>)
    auto shared_const_cast(std::shared_ptr<In> const &ptr) -> std::shared_ptr<Out> {
        if (ptr == nullptr) { return nullptr; }
        return std::const_pointer_cast<Out>(ptr);
    }

    SPP_EXP_CLS template <typename T>
    struct ptr_eq {
        auto operator()(T const &lhs, T const &rhs) const -> bool {
            if (lhs == nullptr && rhs == nullptr) { return true; }
            if (lhs == nullptr || rhs == nullptr) { return false; }
            return *lhs == *rhs;
        }
    };

    SPP_EXP_CLS template <typename T>
    struct ptr_cmp {
        auto operator()(T const &lhs, T const &rhs) const -> bool {
            if (lhs == nullptr && rhs == nullptr) { return false; }
            if (lhs == nullptr) { return true; }
            if (rhs == nullptr) { return false; }
            return *lhs < *rhs;
        }
    };

    SPP_EXP_CLS template <typename T>
    struct ptr_hash {
        auto operator()(T const &ptr) const -> std::size_t {
            if (ptr == nullptr) { return 0; }
            return ptr->ankerl_hash();
        }
    };
}
