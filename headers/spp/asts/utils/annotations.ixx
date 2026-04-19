module;
#include <spp/macros.hpp>

export module spp.asts.utils:annotations;
import std;

namespace spp::asts::utils {
    SPP_EXP_CLS struct AnnotationInfo {
        constexpr static auto FUNCTION_CTX = 1;
        constexpr static auto METHOD_CTX = 2;
        constexpr static auto EXT_METHOD_CTX = 4;
        constexpr static auto CLASS_CTX = 8;
        constexpr static auto TYPE_CTX = 16;
        constexpr static auto CMP_CTX = 32;

        std::uint32_t ctx = 0;
        void *definition = nullptr;
        bool is_builtin = false;

        AnnotationInfo() = default;
        AnnotationInfo(AnnotationInfo const &) = default;
        ~AnnotationInfo() = default;
    };
}
