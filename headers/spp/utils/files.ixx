module;
#include <spp/macros.hpp>

export module spp.utils.files;
import spp.utils.types;
import std;

namespace spp::utils::files {
    /**
     * Simple function to read the contents of a file into a string.
     * @param path The path to the file to read.
     * @return The contents of the file as a string.
     */
    SPP_EXP_FUN auto ReadFile(std::filesystem::path const &path) -> Str;

    /**
     * Simple function to write a string to a file.
     * @param path The path to the file to write to.
     * @param content The content to write to the file.
     */
    SPP_EXP_FUN auto WriteFile(std::filesystem::path const &path, Str const &content) -> void;

    /**
     * Convert a filesystem path to a string for human presentation, using the
     * correct member for the standard library version: display_string() on
     * libstdc++ 17+ (where string() is deprecated), string() on older releases.
     * The result may be lossily transcoded, so only use it for logs, error
     * messages and other display output -- never to reopen the path.
     * @param path The path to stringify.
     * @return The path as a display string.
     */
    SPP_EXP_FUN auto DisplayString(std::filesystem::path const &path) -> Str;

    /**
     * Convert a filesystem path to its native-encoded string, using the correct
     * member for the standard library version: native_encoded_string() on
     * libstdc++ 17+ (where string() is deprecated), string() on older releases.
     * The result faithfully round-trips the path, so use it when the string is
     * fed back to the OS/shell, used to reopen the file, or compared against
     * native path components (e.g. preferred_separator).
     * @param path The path to stringify.
     * @return The path as a native-encoded string.
     */
    SPP_EXP_FUN auto NativeString(std::filesystem::path const &path) -> Str;

    SPP_EXP_CLS enum class LockType { Shared, Exclusive };

    SPP_EXP_CLS struct FileLock;
    SPP_EXP_CLS struct FileLockException;

    SPP_EXP_FUN auto GlobSpp(Str const &path) -> Vec<std::filesystem::path>;
}
