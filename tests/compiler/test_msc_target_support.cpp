#include "test_macros.hpp"

import spp.codegen.llvm_target;
import spp.utils.files;
import llvm;

// What a target can be built with, as LLVM reports it, and that an error LLVM reports stops the object being kept.

TEST(TestTargetSupport, test_safe_stack_where_llvm_has_an_unsafe_stack_pointer) {
  for (const auto triple : {
    "x86_64-unknown-linux-gnu", "i686-unknown-linux-gnu", "aarch64-unknown-linux-gnu", "riscv64-unknown-linux-gnu",
    "s390x-unknown-linux-gnu", "sparc64-unknown-linux-gnu", "x86_64-apple-macos"}) {
    EXPECT_TRUE(spp::codegen::HasSafeStack(triple)) << triple;
  }
}

TEST(TestTargetSupport, test_no_safe_stack_where_llvm_has_no_unsafe_stack_pointer) {
  for (const auto triple : {
    "powerpc64le-unknown-linux-gnu", "powerpc64-unknown-linux-gnu", "powerpc-unknown-linux-gnu",
    "wasm32-unknown-wasip1"}) {
    EXPECT_FALSE(spp::codegen::HasSafeStack(triple)) << triple;
  }
}

TEST(TestTargetSupport, test_safe_stack_canary_where_llvm_has_stack_chk_fail) {
  for (const auto triple : {
    "x86_64-unknown-linux-gnu", "aarch64-unknown-linux-gnu", "x86_64-unknown-linux-musl", "x86_64-apple-macos",
    "aarch64-apple-macos", "x86_64-w64-windows-gnu"}) {
    EXPECT_TRUE(spp::codegen::HasSafeStackCanary(triple)) << triple;
  }
}

TEST(TestTargetSupport, test_no_safe_stack_canary_on_msvc) {
  for (const auto triple : {"x86_64-pc-windows-msvc", "aarch64-pc-windows-msvc", "i686-pc-windows-msvc"}) {
    EXPECT_FALSE(spp::codegen::HasSafeStackCanary(triple)) << triple;
  }
}

TEST(TestTargetSupport, test_no_object_after_an_llvm_error) {
  const auto path = std::filesystem::temp_directory_path()
    / ("spp_target_support_" + std::to_string(std::random_device()()) + ".o");
  auto context = llvm::LLVMContext();
  auto mod = llvm::Module("m", context);
  spp::codegen::ApplyTargetToModule(&mod);
  context.emitError("an error the backend reported");

  EXPECT_FALSE(spp::codegen::EmitObjectFile(&mod, spp::utils::files::NativeString(path).c_str()));
  EXPECT_FALSE(std::filesystem::exists(path));
}

TEST(TestTargetSupport, test_object_without_an_llvm_error) {
  const auto path = std::filesystem::temp_directory_path()
    / ("spp_target_support_" + std::to_string(std::random_device()()) + ".o");
  auto context = llvm::LLVMContext();
  auto mod = llvm::Module("m", context);
  spp::codegen::ApplyTargetToModule(&mod);

  EXPECT_TRUE(spp::codegen::EmitObjectFile(&mod, spp::utils::files::NativeString(path).c_str()));
  EXPECT_TRUE(std::filesystem::exists(path));
  std::filesystem::remove(path);
}
