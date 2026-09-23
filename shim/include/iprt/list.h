#ifndef ___SHIM_IPRT_LIST_H___
#define ___SHIM_IPRT_LIST_H___

#include "cdefs.h"

typedef struct _RTLISTNODE {
    struct _RTLISTNODE *pNext;
    struct _RTLISTNODE *pPrev;
} RTLISTNODE, *PRTLISTNODE;

#endif
