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
        return Unique<Out>(dynamic_cast<Out*>(ptr.Release()));
    }
}

namespace spp::utils::ptr {
    SPP_EXP_CLS template <typename T>
    struct ptr_eq {
        auto operator()(T lhs, T rhs) const -> bool {
            if (lhs == nullptr && rhs == nullptr) { return true; }
            if (lhs == nullptr || rhs == nullptr) { return false; }
            return *lhs == *rhs;
        }
    };

    SPP_EXP_CLS template <typename T>
    struct ptr_cmp {
        auto operator()(T lhs, T rhs) const -> bool {
            if (lhs == nullptr && rhs == nullptr) { return false; }
            if (lhs == nullptr) { return true; }
            if (rhs == nullptr) { return false; }
            return *lhs < *rhs;
        }
    };

    SPP_EXP_CLS template <typename T>
    struct ptr_hash {
        auto operator()(T ptr) const -> std::size_t {
            if (ptr == nullptr) { return 0; }
            return ptr->AnkerlHash();
        }
    };
}
