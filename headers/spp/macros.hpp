#ifndef MACROS_HPP
#define MACROS_HPP


#define SPP_ATTR_NODISCARD [[nodiscard]]

#define SPP_ATTR_DEPRECATED [[deprecated]]

#define SPP_ATTR_MAYBE_UNUSED [[maybe_unused]]

#define SPP_ATTR_NORETURN [[noreturn]]

#define SPP_ATTR_FALLTHROUGH [[fallthrough]]

#define SPP_ATTR_LIKELY [[likely]]

#define SPP_ATTR_UNLIKELY [[unlikely]]

#define SPP_ATTR_NO_UNIQUE_ADDRESS [[no_unique_address]]

#define SPP_ATTR_ASSUME(x) [[assume(x)]]

#define SPP_ATTR_ALWAYS_INLINE [[gnu::always_inline]] inline

#define SPP_ATTR_NOINLINE [[gnu::noinline]]

#define SPP_ATTR_UNREACHABLE [[gnu::unreachable]]


#endif //MACROS_HPP
