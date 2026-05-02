module;
#include <spp/macros.hpp>

export module spp.utils.ptr;
import spp.utils.types;
import std;

namespace spp {
    SPP_EXP_FUN template <typename Out, typename In>
    requires (std::derived_from<Out, In> or std::derived_from<In, Out>)
    SPP_ATTR_NODISCARD SPP_ATTR_ALWAYS_INLINE
    inline auto dynamic_unique_cast(Unique<In> &&ptr) -> Unique<Out> {
        if (ptr == nullptr) { return nullptr; }
        return Unique<Out>(dynamic_cast<Out*>(ptr.release()));
    }

    SPP_EXP_FUN template <typename Out, typename In>
    requires (std::derived_from<Out, In> )
    SPP_ATTR_NODISCARD SPP_ATTR_ALWAYS_INLINE
    inline auto dynamic_shared_cast(Unique<In> &&ptr) -> Shared<Out> {
        if (ptr == nullptr) { return nullptr; }
        return Shared<Out>(std::move(ptr));
    }

    SPP_EXP_FUN template <typename Out, typename In>
    requires (std::derived_from<In, Out> or std::derived_from<Out, In>)
    SPP_ATTR_NODISCARD SPP_ATTR_ALWAYS_INLINE
    inline auto dynamic_shared_cast(Shared<In> const &ptr) -> Shared<Out> {
        if (ptr == nullptr) { return nullptr; }
        return std::dynamic_pointer_cast<Out>(ptr);
    }

    SPP_EXP_FUN template <typename In>
    requires (not std::is_const_v<In> and not std::is_reference_v<In>)
    SPP_ATTR_NODISCARD SPP_ATTR_ALWAYS_INLINE
    inline auto const_shared_cast(Shared<In> const &ptr) -> Shared<std::add_const_t<std::remove_reference_t<In>>> {
        if (ptr == nullptr) { return nullptr; }
        using AddedConstRef = std::add_const_t<std::remove_reference_t<In>>;
        return std::const_pointer_cast<AddedConstRef>(ptr);
    }

    SPP_EXP_FUN template <typename In>
    requires (std::is_const_v<In> and not std::is_reference_v<In>)
    SPP_ATTR_NODISCARD SPP_ATTR_ALWAYS_INLINE
    inline auto mut_shared_cast(Shared<In> const &ptr) -> Shared<std::remove_const_t<std::remove_reference_t<In>>> {
        if (ptr == nullptr) { return nullptr; }
        using RemovedConstRef = std::remove_const_t<std::remove_reference_t<In>>;
        return std::const_pointer_cast<RemovedConstRef>(ptr);
    }
}

namespace spp::utils::ptr {
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
            return ptr->AnkerlHash();
        }
    };
}
