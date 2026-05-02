module;
#include <spp/macros.hpp>

export module spp.analyse.utils.annotation_utils;
import std;

namespace spp::asts {
    SPP_EXP_CLS struct AnnotationAst;
}

namespace spp::analyse::utils::annotation_utils {
    SPP_EXP_CLS struct AnnotationInfo {
        constexpr static auto kFunctionCtx = 1;
        constexpr static auto kMethodCtx = 2;
        constexpr static auto kExtensionContext = 4;
        constexpr static auto kClassContext = 8;
        constexpr static auto kTypeStmtCtx = 16;
        constexpr static auto kCmpStmtCtx = 32;

        std::uint32_t Ctx = 0;
        asts::AnnotationAst *Definition = nullptr;
        bool IsBuiltin = false;

        AnnotationInfo() = default;
        AnnotationInfo(AnnotationInfo const &) = default;
        ~AnnotationInfo() = default;
    };
}
