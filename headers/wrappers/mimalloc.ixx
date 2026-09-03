module;
#ifndef SPP_NO_MIMALLOC
#include <mimalloc-new-delete.h>
#include <mimalloc.h>
#endif

export module mimalloc;

#ifndef SPP_NO_MIMALLOC
export using ::operator delete;
export using ::operator delete[];
export using ::operator new;
export using ::operator new[];
export using ::mi_option_e;
export using ::mi_option_disable;
#endif
