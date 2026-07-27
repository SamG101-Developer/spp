module;
#include <mimalloc-new-delete.h>

export module mimalloc;

export using ::operator delete;
export using ::operator delete[];
export using ::operator new;
export using ::operator new[];
