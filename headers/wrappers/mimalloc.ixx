module;
// Debug builds deliberately don't use mimalloc: its release build randomizes the
// base address of its OS reservations from OS entropy, which defeats the
// debugger's disable-randomization, so heap pointers differ between runs and
// can't be compared against an address noted in an earlier run.
//
// Sanitized builds don't use it either (SPP_NO_MIMALLOC, set by the build) because
// mimalloc screws with the memory address analysing.
#if defined(NDEBUG) && !defined(SPP_NO_MIMALLOC)
#include <mimalloc-new-delete.h>
#include <mimalloc.h>
#endif

export module mimalloc;

#if defined(NDEBUG) && !defined(SPP_NO_MIMALLOC)
export using ::operator delete;
export using ::operator delete[];
export using ::operator new;
export using ::operator new[];
export using ::mi_option_e;
export using ::mi_option_disable;
#endif
