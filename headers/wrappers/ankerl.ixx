module;
#include <ankerl/unordered_dense.h>

export module ankerl;


export namespace ankerl {
    namespace unordered_dense {
        using ::ankerl::unordered_dense::map;
        using ::ankerl::unordered_dense::set;
    }

    template <typename T>
    struct ptr_eq {
        auto operator()(T const &lhs, T const &rhs) const -> bool {
            if (lhs == nullptr && rhs == nullptr) { return true; }
            if (lhs == nullptr || rhs == nullptr) { return false; }
            return *lhs == *rhs;
        }
    };

    template <typename T>
    struct ptr_hash {
        auto operator()(T const &ptr) const -> std::size_t {
            if (ptr == nullptr) { return 0; }
            return ptr->ankerl_hash();
        }
    };
}
