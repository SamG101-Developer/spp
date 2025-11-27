module;
#include <fcntl.h>
#include <unistd.h>


export module sys;


export namespace sys {
    using ::open;
    using ::close;
    using ::flock;
}
