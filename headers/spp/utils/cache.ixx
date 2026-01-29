module;
#include <spp/macros.hpp>

export module spp.utils.cache;
import std;


namespace spp::utils {
    /**
     * The Cache type is a simple key-value store that allows for caching of values based on keys. It provides methods
     * to set, get, and check for the existence of keys in the cache. This is useful for optimizing performance by
     * avoiding redundant computations. It is optimized for extreme random access, with no iteration support at all.
     * @tparam K The type of the keys in the cache.
     * @tparam V The type of the values in the cache.
     */
    SPP_EXP_CLS template <typename K, typename V>
    requires std::is_pointer_v<K>
    class Cache {
        std::vector<std::pair<K, V>> m_cache;

    public:
        /**
         * Set a value in the cache for a given key.
         * @param[in] key The key to set the value for.
         * @param[in] value The value to set in the cache.
         */
        auto set(K key, V const &value) -> void {
            m_cache.emplace_back(key, value);
        }

        /**
         * Get a value from the cache for a given key.
         * @note This is undefined behavior if the key does not exist.
         * @param[in] key The key to get the value for.
         * @return The value associated with the key.
         */
        SPP_ATTR_HOT SPP_ATTR_ALWAYS_INLINE
        auto get(K key) const noexcept -> V {
            for (const auto &[k, v] : m_cache) {
                if (k == key) { return v; }
            }
            std::unreachable();
        }

        /**
         * Check if a key exists in the cache.
         * @param[in] key The key to check for existence.
         * @return True if the key exists in the cache, false otherwise.
         */
        SPP_ATTR_HOT SPP_ATTR_ALWAYS_INLINE
        auto contains(K key) const noexcept -> bool {
            for (const auto &[k, v] : m_cache) {
                if (k == key) { return true; }
            }
            return false;
        }
    };
}
