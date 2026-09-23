#ifndef ___SHIM_IPRT_MEM_H___
#define ___SHIM_IPRT_MEM_H___

#include "cdefs.h"
#include <stdlib.h>

RT_C_DECLS_BEGIN

void *RTMemAlloc(size_t cb);
void *RTMemAllocZ(size_t cb);
void *RTMemRealloc(void *pv, size_t cb);
void  RTMemFree(void *pv);

RT_C_DECLS_END

#endif /* ___SHIM_IPRT_MEM_H___ */
