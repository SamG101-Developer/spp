module;
#include <spp/macros.hpp>

export module spp.analyse.utils.annotation_utils;
import std;

namespace spp::asts {
    SPP_EXP_CLS struct AnnotationAst;
}


namespace spp::analyse::utils::annotation_utils {
    SPP_EXP_CLS struct AnnotationInfo {
        constexpr static auto FUNCTION_CTX = 1;
        constexpr static auto METHOD_CTX = 2;
        constexpr static auto CLASS_CTX = 4;
        constexpr static auto TYPE_CTX = 8;
        constexpr static auto CMP_CTX = 16;
        constexpr static auto ALL_CTX = FUNCTION_CTX | METHOD_CTX | CLASS_CTX | TYPE_CTX | CMP_CTX;

        std::uint32_t ctx = 0;
        asts::AnnotationAst *definition = nullptr;
        bool is_builtin = false;

        AnnotationInfo() = default;
        AnnotationInfo(AnnotationInfo const &) = default;
        ~AnnotationInfo() = default;
    };
}
