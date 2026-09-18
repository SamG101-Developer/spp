module;
#include <spp/macros.hpp>

export module spp.analyse.utils.annotation_utils;
import std;

use(spp::asts, struct AnnotationAst);
use(spp::analyse::utils::annotation_utils, struct AnnotationInfo);
use(spp::analyse::utils::annotation_utils, struct BuiltinAnnotations);

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

  std::uint32_t Ctx = 0; // Dead code?

  /// For functions that themselves are annotations, like "fun
  /// public" is, bind the !annotation annotation onto it. This
  /// is the function's annotation-info's "definition".
  AnnotationAst *Definition = nullptr;

  AnnotationInfo() = default;
  AnnotationInfo(AnnotationInfo const &) = default;
  ~AnnotationInfo() = default;
};

/// A list of the fully qualified builtin annotation names,
/// used for binding specific behaviour to functions or types
/// etc. Checked against as a lexical comparison.
SPP_EXP_CLS struct spp::analyse::utils::annotation_utils::BuiltinAnnotations {
  constexpr static auto kIntrinsic = "std::annotations::intrinsic";
  constexpr static auto kPublic = "std::annotations::public";
  constexpr static auto kPackage = "std::annotations::package";
  constexpr static auto kProtected = "std::annotations::protected";
  constexpr static auto kPrivate = "std::annotations::private";
  constexpr static auto kVirtualMethod = "std::annotations::virtual_method";
  constexpr static auto kAbstractMethod = "std::annotations::abstract_method";
  constexpr static auto kFfi = "std::annotations::ffi";
  constexpr static auto kZeroType = "std::annotations::zero_type";
  constexpr static auto kThreadHazard = "std::annotations::thread_hazard";
  constexpr static auto kTest = "std::annotations::test";
  constexpr static auto kCfg = "std::annotations::cfg";
  constexpr static auto kVersioned = "std::annotations::versioned";
  constexpr static auto kLlvmInline = "std::llvm::inline";
  constexpr static auto kLlvmAlwaysInline = "std::llvm::always_inline";
  constexpr static auto kLlvmNoInline = "std::llvm::noinline";
  constexpr static auto kLlvmHot = "std::llvm::hot";
  constexpr static auto kLlvmCold = "std::llvm::cold";
};
