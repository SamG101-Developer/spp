#pragma once

#define raises_from_vec(v) \
    WithFormatters(v | std::views::transform([](auto *scope) { return scope->GetErrorFormatter(); }) | std::ranges::to<Vec>()) \
    .Raise()

#define ERR_ARGS(...) \
    [&]() { return std::forward_as_tuple(__VA_ARGS__); }

#define USE_CACHED_TYPE_INFERENCE \
    if (_CachedInferredType != nullptr) { return _CachedInferredType.Get(); }

#define CACHE_TYPE_INFERENCE_AND_RETURN(What) \
    _CachedInferredType = std::move((What));  \
    return _CachedInferredType.Get()
