#include "test_macros.hpp"

import spp.codegen.llvm_artefact;
import spp.utils.files;

// The loader flags "[binary.link] harden" asks a COFF or Mach-O linker for, read back out of the image: each check is
// run on the smallest header that carries the flag, so no linker for either is needed.

namespace {
  /// A file of its own under the test's temporary root, removed when the test ends.
  struct ScratchFile {
    spp::Path Path = std::filesystem::temp_directory_path()
      / ("spp_image_hardening_" + std::to_string(std::random_device()()));
    explicit ScratchFile(spp::Str const &bytes) { std::ofstream(Path, std::ios::binary) << bytes; }
    ~ScratchFile() { std::filesystem::remove(Path); }
  };

  /// "v" written over "bytes" at "at", little endian, "n" bytes wide.
  auto PutLe(spp::Str &bytes, const std::size_t at, const std::uint64_t v, const std::size_t n) -> void {
    for (auto i = 0uz; i < n; ++i) { bytes[at + i] = static_cast<char>(v >> (8 * i)); }
  }

  constexpr auto kRelocsStripped = 0x0001u;
  constexpr auto kHighEntropyVa = 0x0020u;
  constexpr auto kDynamicBase = 0x0040u;
  constexpr auto kNxCompat = 0x0100u;

  /// A PE image with no sections, 64-bit (PE32+) or not, with the COFF "characteristics" and the optional header's
  /// "dll_flags".
  auto PeImage(const bool wide, const std::uint16_t characteristics, const std::uint16_t dll_flags) -> spp::Str {
    const auto optional_size = wide ? 240uz : 224uz;
    auto bytes = spp::Str(0x40 + 4 + 20 + optional_size, '\0');
    bytes[0] = 'M';
    bytes[1] = 'Z';
    PutLe(bytes, 0x3c, 0x40, 4);
    bytes[0x40] = 'P';
    bytes[0x41] = 'E';
    PutLe(bytes, 0x44, wide ? 0x8664 : 0x14c, 2);
    PutLe(bytes, 0x44 + 16, optional_size, 2);
    PutLe(bytes, 0x44 + 18, characteristics | 0x0002u, 2);
    const auto optional = 0x44 + 20uz;
    PutLe(bytes, optional, wide ? 0x20b : 0x10b, 2);
    PutLe(bytes, optional + 70, dll_flags, 2);
    PutLe(bytes, optional + (wide ? 108 : 92), 16, 4);
    return bytes;
  }

  constexpr auto kMhPie = 0x00200000u;
  constexpr auto kMhAllowStackExecution = 0x00020000u;

  /// A 64-bit x86-64 Mach-O executable with no load commands, and the header's "flags".
  auto MachOImage(const std::uint32_t flags) -> spp::Str {
    auto bytes = spp::Str(32, '\0');
    PutLe(bytes, 0, 0xfeedfacf, 4);
    PutLe(bytes, 4, 0x01000007, 4);
    PutLe(bytes, 8, 3, 4);
    PutLe(bytes, 12, 2, 4);
    PutLe(bytes, 24, flags, 4);
    return bytes;
  }

  /// What "UnhardenedImage" finds in "bytes": empty for nothing, the error's text prefixed "error: " when unreadable.
  auto Finding(spp::Str const &bytes) -> spp::Str {
    const auto file = ScratchFile(bytes);
    const auto found = spp::codegen::UnhardenedImage(spp::utils::files::NativeString(file.Path).c_str());
    if (not found) { return "error: " + found.error(); }
    return found->value_or("");
  }
}

TEST(TestImageHardening, test_a_hardened_pe32_plus_image_has_no_finding) {
  EXPECT_EQ(Finding(PeImage(true, 0, kDynamicBase | kHighEntropyVa | kNxCompat)), "");
}

TEST(TestImageHardening, test_a_pe_image_without_each_flag_is_found) {
  EXPECT_EQ(Finding(PeImage(true, 0, kHighEntropyVa | kNxCompat)), "a fixed load address");
  EXPECT_EQ(Finding(PeImage(true, 0, kDynamicBase | kNxCompat)), "a low-entropy load address");
  EXPECT_EQ(Finding(PeImage(true, 0, kDynamicBase | kHighEntropyVa)), "DEP off");
  EXPECT_EQ(
    Finding(PeImage(true, kRelocsStripped, kDynamicBase | kHighEntropyVa | kNxCompat)),
    "its relocations stripped, so it cannot be moved");
}

TEST(TestImageHardening, test_a_pe32_image_needs_no_high_entropy_address) {
  EXPECT_EQ(Finding(PeImage(false, 0, kDynamicBase | kNxCompat)), "");
}

TEST(TestImageHardening, test_a_macho_image_without_each_flag_is_found) {
  EXPECT_EQ(Finding(MachOImage(kMhPie)), "");
  EXPECT_EQ(Finding(MachOImage(0)), "a fixed load address");
  EXPECT_EQ(Finding(MachOImage(kMhPie | kMhAllowStackExecution)), "an executable stack");
}

TEST(TestImageHardening, test_an_unreadable_file_is_an_error) {
  EXPECT_TRUE(Finding("not an object file").starts_with("error: "));
}
