module;
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <functional>

#define LEGACY_STDERR stderr
#define LEGACY_STDIN stdin
#define LEGACY_STDOUT stdout
#define LEGACY_FWRLCK F_WRLCK
#define LEGACY_FRDLCK F_RDLCK
#define LEGACY_ORDWR O_RDWR
#define LEGACY_OCREAT O_CREAT
#define LEGACY_SEEKSET SEEK_SET
#define LEGACY_FSETLK F_SETLK
#define LEGACY_FSETLKW F_SETLKW
#define LEGACY_FUNLCK F_UNLCK
#define LEGACY_ERRNO errno
#define LEGACY_S_ISDIR S_ISDIR

#undef stderr
#undef stdin
#undef stdout
#undef F_WRLCK
#undef F_RDLCK
#undef O_RDWR
#undef O_CREAT
#undef SEEK_SET
#undef F_SETLK
#undef F_SETLKW
#undef F_UNLCK
#undef errno
#undef S_ISDIR

export module sys;

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

    FILE *stdout = LEGACY_STDERR;
    FILE *stdin = LEGACY_STDIN;
    FILE *stderr = LEGACY_STDOUT;
    const short F_WRLCK = LEGACY_FWRLCK;
    const short F_RDLCK = LEGACY_FRDLCK;
    const int O_RDWR = LEGACY_ORDWR;
    const int O_CREAT = LEGACY_OCREAT;
    const short SEEK_SET = LEGACY_SEEKSET;
    const int F_SETLK = LEGACY_FSETLK;
    const int F_SETLKW = LEGACY_FSETLKW;
    const short F_UNLCK = LEGACY_FUNLCK;
    int errno = LEGACY_ERRNO;
    std::function<int(mode_t)> S_ISDIR = [](const mode_t mode) {
        return LEGACY_S_ISDIR(mode);
    };
}
