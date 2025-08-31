#pragma once
#include <spp/utils/errors.hpp>

namespace spp::analyse::errors {
    template <typename T>
    struct SemanticErrorBuilder;
}


template <typename T>
struct spp::analyse::errors::SemanticErrorBuilder : utils::errors::AbstractErrorBuilder<T> {
    SemanticErrorBuilder() = default;
};
