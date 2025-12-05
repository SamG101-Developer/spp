#pragma once
#include <boost/preprocessor.hpp>


/**
 * Define an advanced macro expander using Boost.PP, which expands @code RAISE_FROM(scope_1, scope_2)@endcode into
 * @code.with_formatters(scope_1.get_error_formatter(), scope_2.get_error_formatter()).raise()@endcode. This allows
 * multiple scopes to be passed to the error builder for formatting.
 */
#define raises_from(...)                          \
    with_formatters({GET_FORMATTERS(__VA_ARGS__)}) \
    .raise()

#define GET_FORMATTERS(...) \
    BOOST_PP_SEQ_ENUM(BOOST_PP_SEQ_TRANSFORM(GET_FORMATTER, BOOST_PP_EMPTY(), BOOST_PP_VARIADIC_TO_SEQ(__VA_ARGS__)))

#define GET_FORMATTER(_1, _2, scope) \
    scope->get_error_formatter()
