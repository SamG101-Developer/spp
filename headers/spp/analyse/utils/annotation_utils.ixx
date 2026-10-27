module;
#include <spp/macros.hpp>

export module spp.analyse.utils.annotation_utils;
import spp.utils.types;
import std;

use(spp::analyse::utils::annotation_utils, struct AnnotationInfo);
use(spp::analyse::utils::annotation_utils, enum class BuiltinAnnotation : std::uint8_t);
use(spp::asts, struct AnnotationAst);

/// The AnnotationInfo holds a small set of metadata about an
/// annotation, used to enforce the validity of an annotation
/// application like !virtual_method
SPP_EXP_CLS struct spp::analyse::utils::annotation_utils::AnnotationInfo {
  constexpr static auto kFunctionCtx = 1;
  constexpr static auto kMethodCtx = 2;
  constexpr static auto kExtensionContext = 4;
  constexpr static auto kClassContext = 8;
  constexpr static auto kTypeStmtCtx = 16;
  constexpr static auto kCmpStmtCtx = 32;

  /// For functions that themselves are annotations, like "fun
  /// public" is, bind the !annotation annotation onto it. This
  /// is the function's annotation-info's "definition".
  AnnotationAst *Definition = nullptr;

  AnnotationInfo() = default;
  AnnotationInfo(AnnotationInfo const &) = default;
  ~AnnotationInfo() = default;
};

/// The builtin annotations: each binds behaviour to what it
/// annotates (a function, a class, a visibility), so the compiler
/// acts on it. Anything else is "kNone" (a custom annotation).
/// "kCfg" is acted on before analysis, by its written name
/// ("compiler/cfg.cpp"), and "kVersioned" is documentation only.
SPP_EXP_CLS enum class spp::analyse::utils::annotation_utils::BuiltinAnnotation : std::uint8_t {
  kNone,
  kAnnotation,
  kIntrinsic,
  kPublic,
  kPackage,
  kProtected,
  kPrivate,
  kVirtualMethod,
  kAbstractMethod,
  kFfi,
  kZeroType,
  kThreadHazard,
  kTest,
  kCfg,
  kVersioned,
  kLlvmInline,
  kLlvmAlwaysInline,
  kLlvmNoInline,
  kLlvmHot,
  kLlvmCold,
};

namespace spp::analyse::utils::annotation_utils {
  /// Each builtin annotation's function by its fully qualified
  /// name: the one place an annotation is told apart by spelling.
  SPP_EXP_CMP constexpr auto kBuiltinAnnotationNames = std::to_array<Pair<StrView, BuiltinAnnotation>>({
    {"std::annotations::annotation", BuiltinAnnotation::kAnnotation},
    {"std::annotations::intrinsic", BuiltinAnnotation::kIntrinsic},
    {"std::annotations::public", BuiltinAnnotation::kPublic},
    {"std::annotations::package", BuiltinAnnotation::kPackage},
    {"std::annotations::protected", BuiltinAnnotation::kProtected},
    {"std::annotations::private", BuiltinAnnotation::kPrivate},
    {"std::annotations::virtual_method", BuiltinAnnotation::kVirtualMethod},
    {"std::annotations::abstract_method", BuiltinAnnotation::kAbstractMethod},
    {"std::annotations::ffi", BuiltinAnnotation::kFfi},
    {"std::annotations::zero_type", BuiltinAnnotation::kZeroType},
    {"std::annotations::thread_hazard", BuiltinAnnotation::kThreadHazard},
    {"std::annotations::test", BuiltinAnnotation::kTest},
    {"std::annotations::cfg", BuiltinAnnotation::kCfg},
    {"std::annotations::versioned", BuiltinAnnotation::kVersioned},
    {"std::llvm::inline", BuiltinAnnotation::kLlvmInline},
    {"std::llvm::always_inline", BuiltinAnnotation::kLlvmAlwaysInline},
    {"std::llvm::no_inline", BuiltinAnnotation::kLlvmNoInline},
    {"std::llvm::hot", BuiltinAnnotation::kLlvmHot},
    {"std::llvm::cold", BuiltinAnnotation::kLlvmCold},
  });

  /// Which builtin the annotation function "fq_name" (its fully
  /// qualified name) is, or "kNone".
  SPP_EXP_FUN inline auto BuiltinAnnotationOf(const StrView fq_name) -> BuiltinAnnotation {
    for (auto const &[name, builtin] : kBuiltinAnnotationNames) {
      if (name == fq_name) { return builtin; }
    }
    return BuiltinAnnotation::kNone;
  }
}
