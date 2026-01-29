module;
#include <spp/macros.hpp>

module spp.utils.files;
import std;
import sys;


auto spp::utils::files::read_file(
    std::filesystem::path const &path)
    -> std::string {
    // Create an input file stream.
    auto in = std::ifstream(path, std::ios::in);
    auto buf = std::ostringstream();
    buf << in.rdbuf();
    return buf.str();
}


auto spp::utils::files::write_file(
    std::filesystem::path const &path,
    std::string const &content)
    -> void {
    // Create an output file stream.
    auto out = std::ofstream(path);
    out << content;
}


auto spp::utils::files::FileLock::open_file()
    -> void {
    m_fd = sys::open(m_path.c_str(), sys::O_RDWR | sys::O_CREAT, 0666);
    if (m_fd == -1) {
        throw std::runtime_error("Failed to open file: " + m_path.string());
    }
}


spp::utils::files::FileLock::FileLock(std::filesystem::path const &path) :
    m_path(path),
    m_fd(0),
    m_locked(false),
    m_lock_type(LockType::Exclusive) {
    open_file();
}


spp::utils::files::FileLock::~FileLock() noexcept {
    unlock();
    if (m_fd != -1) {
        sys::close(m_fd);
    }
}


auto spp::utils::files::FileLock::lock(
    const LockType type,
    const bool non_blocking)
    -> void {
    if (m_locked) {
        throw std::runtime_error("File is already locked");
    }

    sys::flock fl{};
    fl.l_type = type == LockType::Exclusive ? sys::F_WRLCK : sys::F_RDLCK;
    fl.l_whence = sys::SEEK_SET;

    const auto cmd = non_blocking ? sys::F_SETLK : sys::F_SETLKW;
    if (sys::fcntl(m_fd, cmd, &fl) == -1) {
        throw std::runtime_error("Failed to lock file: " + m_path.string());
    }

    m_locked = true;
    m_lock_type = type;
}


auto spp::utils::files::FileLock::unlock() noexcept
    -> void {
    if (!m_locked) {
        return;
    }

    sys::flock fl{};
    fl.l_type = sys::F_UNLCK;
    fl.l_whence = sys::SEEK_SET;

    sys::fcntl(m_fd, sys::F_SETLK, &fl);
    m_locked = false;
}


SPP_ATTR_NODISCARD auto spp::utils::files::FileLock::is_locked() const noexcept
    -> bool {
    return m_locked;
}


SPP_ATTR_NODISCARD auto spp::utils::files::FileLock::path() const noexcept
    -> std::filesystem::path const& {
    return m_path;
}


SPP_ATTR_NODISCARD auto spp::utils::files::FileLock::lock_type() const noexcept
    -> LockType {
    return m_lock_type;
}
