#ifndef ___SHIM_IPRT_SEMAPHORE_H___
#define ___SHIM_IPRT_SEMAPHORE_H___

#include "cdefs.h"
#include "types.h"

RT_C_DECLS_BEGIN

int RTSemEventCreate(RTSEMEVENT *pSem);
int RTSemEventDestroy(RTSEMEVENT Sem);
int RTSemEventSignal(RTSEMEVENT Sem);
int RTSemEventWait(RTSEMEVENT Sem, uint32_t cMillies);

int RTThreadCreate(RTTHREAD *pThread, int (*pfnThread)(RTTHREAD, void *), void *pvUser, size_t cbStack, int enmType, unsigned fFlags, const char *pszName);
void RTThreadSleep(uint32_t cMillies);

RT_C_DECLS_END

#endif /* ___SHIM_IPRT_SEMAPHORE_H___ */
