#ifndef ___SHIM_VBOX_VMM_PDMTHREAD_H___
#define ___SHIM_VBOX_VMM_PDMTHREAD_H___

#include <iprt/types.h>

typedef struct _PDMTHREAD {
    RTTHREAD hThread;
} PDMTHREAD, *PPDMTHREAD;

#endif
