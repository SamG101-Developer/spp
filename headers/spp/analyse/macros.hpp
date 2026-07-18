#pragma once

#define raises_from_vec(v) \
    WithFormatters(v | genex::views::transform([](auto *scope) { return scope->GetErrorFormatter(); }) | genex::to<Vec>()) \
    .Raise()

#define ERR_ARGS(...) \
    [&]() { return std::forward_as_tuple(__VA_ARGS__); }
