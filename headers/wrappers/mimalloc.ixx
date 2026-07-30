module;
// Debug builds deliberately don't use mimalloc: its release build randomizes the
// base address of its OS reservations from OS entropy, which defeats the
// debugger's disable-randomization, so heap pointers differ between runs and
// can't be compared against an address noted in an earlier run.
#ifdef NDEBUG
#include <mimalloc-new-delete.h>
#endif

export module mimalloc;

#ifdef NDEBUG
export using ::operator delete;
export using ::operator delete[];
export using ::operator new;
export using ::operator new[];
#endif
