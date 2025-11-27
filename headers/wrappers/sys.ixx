module;
#include <fcntl.h>
#include <unistd.h>


export module sys;


export namespace sys {
    using ::fcntl;
    using ::open;
    using ::close;
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
}
