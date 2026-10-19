module;
#if !defined(SPP_NO_MIMALLOC)
#if !defined(SPP_MIMALLOC_STATIC)
#include <mimalloc-new-delete.h>
#endif
#include <mimalloc.h>
#endif

export module mimalloc;

#if !defined(SPP_NO_MIMALLOC)
#if !defined(SPP_MIMALLOC_STATIC)
export using ::operator delete;
export using ::operator delete[];
export using ::operator new;
export using ::operator new[];
#endif
export using ::mi_option_e;
export using ::mi_option_disable;
#endif
