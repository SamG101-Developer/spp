module;
#include <Glob-1.0/glob/glob.h>

export module glob;


export namespace glob {
    using ::glob::glob;
    using ::glob::rglob;
}
