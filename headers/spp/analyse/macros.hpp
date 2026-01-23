#pragma once

#define raises_from_vec(v) \
    with_formatters(v | genex::views::transform([](auto *scope) { return scope->get_error_formatter(); }) | genex::to<std::vector>() ) \
    .raise()


#define ERR_ARGS(...) \
    [&]() { return std::forward_as_tuple(__VA_ARGS__); }
