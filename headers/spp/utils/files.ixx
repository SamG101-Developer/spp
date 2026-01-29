module;
#include <spp/macros.hpp>

export module spp.utils.files;
import std;


namespace spp::utils::files {
    /**
     * Simple function to read the contents of a file into a string.
     * @param path The path to the file to read.
     * @return The contents of the file as a string.
     */
    SPP_EXP_FUN auto read_file(std::filesystem::path const &path) -> std::string;

    /**
     * Simple function to write a string to a file.
     * @param path The path to the file to write to.
     * @param content The content to write to the file.
     */
    SPP_EXP_FUN auto write_file(std::filesystem::path const &path, std::string const &content) -> void;

    SPP_EXP_ENUM enum class LockType { Shared, Exclusive };

    SPP_EXP_CLS struct FileLock;
    SPP_EXP_CLS struct FileLockException;
}


SPP_EXP_CLS struct spp::utils::files::FileLock {
private:
    std::filesystem::path m_path;
    int m_fd;
    bool m_locked;
    LockType m_lock_type;

    auto open_file() -> void;

public:
    explicit FileLock(std::filesystem::path const &path);
    ~FileLock() noexcept;

    FileLock(FileLock const &) = delete;
    auto operator=(FileLock const &) -> FileLock& = delete;
    FileLock(FileLock &&) noexcept = delete;
    auto operator=(FileLock &&) noexcept -> FileLock& = delete;

    auto lock(LockType type, bool non_blocking = false) -> void;
    auto unlock() noexcept -> void;

    SPP_ATTR_NODISCARD auto is_locked() const noexcept -> bool;
    SPP_ATTR_NODISCARD auto path() const noexcept -> std::filesystem::path const&;
    SPP_ATTR_NODISCARD auto lock_type() const noexcept -> LockType;
};
