module;
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>

export module sys;

#undef stderr
#undef stdin
#undef stdout

export namespace sys {
    using ::close;
    using ::chdir;
    using ::fcntl;
    using ::fdopen;
    using ::fileno;
    using ::isatty;
    using ::open;
    using ::read;
    using ::rmdir;
    using ::strcasecmp;
    using ::stat;
    using ::write;
    using ::flock;
    using ::ssize_t;

    #undef F_WRLCK
    constexpr auto F_WRLCK = 1;

    #undef F_RDLCK
    constexpr auto F_RDLCK = 0;

    #undef O_RDWR
    constexpr auto O_RDWR = 02;

    #undef O_CREAT
    constexpr auto O_CREAT = 0100;

    #undef SEEK_SET
    constexpr auto SEEK_SET = 0;

    #undef F_SETLK
    constexpr auto F_SETLK = 6;

    #undef F_SETLKW
    constexpr auto F_SETLKW = 7;

    #undef F_UNLCK
    constexpr auto F_UNLCK = 2;

    #undef errno
    auto errno = *__errno_location();

    #undef S_ISDIR
    auto S_ISDIR = [](const mode_t mode) {
        return __S_ISTYPE(mode, __S_IFDIR);
    };

    const auto stdout = ::stdout;
    const auto stdin = ::stdin;
    const auto stderr = ::stderr;
}
