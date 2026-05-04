module;
#include <spp/macros.hpp>

export module spp.analyse.utils.annotation_utils;
import std;

namespace spp::asts {
    SPP_EXP_CLS struct AnnotationAst;
}

namespace spp::analyse::utils::annotation_utils {
    SPP_EXP_CLS struct AnnotationInfo;
    SPP_EXP_CLS struct BuiltinAnnotations;
}

SPP_EXP_CLS struct spp::analyse::utils::annotation_utils::AnnotationInfo {
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

SPP_EXP_CLS struct spp::analyse::utils::annotation_utils::BuiltinAnnotations {
    constexpr static auto kCompilerBuiltin = "std::annotations::compiler_builtin";
    constexpr static auto kPublic = "std::annotations::public";
    constexpr static auto kPackage = "std::annotations::package";
    constexpr static auto kProtected = "std::annotations::protected";
    constexpr static auto kPrivate = "std::annotations::private";
    constexpr static auto kVirtualMethod = "std::annotations::virtual_method";
    constexpr static auto kAbstractMethod = "std::annotations::abstract_method";
    constexpr static auto kFfi = "std::annotations::ffi";
    constexpr static auto kLlvmInline = "std::llvm::inline";
    constexpr static auto kLlvmAlwaysInline = "std::llvm::always_inline";
    constexpr static auto kLlvmNoInline = "std::llvm::noinline";
    constexpr static auto kLlvmHot = "std::llvm::hot";
    constexpr static auto kLlvmCold = "std::llvm::cold";
};
