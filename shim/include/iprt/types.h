#ifndef ___SHIM_IPRT_TYPES_H___
#define ___SHIM_IPRT_TYPES_H___

#include "cdefs.h"
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

RT_C_DECLS_BEGIN

typedef int32_t     RTSTATUS;
typedef uint64_t    RTGCPHYS;
typedef uint64_t    RTGCPTR;
typedef uint64_t    RTGCPTR64;
typedef uint32_t    RTGCPTR32;
typedef void       *RTR3PTR;
typedef void       *RTR0PTR;
typedef void       *RTRCPTR;
typedef uint64_t    RTFOFF;
typedef uint64_t    RTUINTPTR;
typedef int64_t     RTINTPTR;
typedef uint32_t    RTIOPORT;

#define NIL_RTGCPHYS ((RTGCPHYS)~0ULL)

typedef void       *RTTHREAD;
typedef RTTHREAD   *PRTTHREAD;
#define NIL_RTTHREAD ((RTTHREAD)0)
#define RTTHREADTYPE_GUI 2
typedef int RTTHREADTYPE;

typedef void       *RTSEMEVENT;
typedef RTSEMEVENT *PRTSEMEVENT;
#define NIL_RTSEMEVENT ((RTSEMEVENT)0)

typedef void       *SUPSEMEVENT;
#define NIL_SUPSEMEVENT ((SUPSEMEVENT)0)

typedef struct _SUPDRVSESSION *PSUPDRVSESSION;

typedef struct _RTUUID {
    uint8_t au8[16];
} RTUUID;

typedef struct _STAMCOUNTER {
    uint64_t c;
} STAMCOUNTER;

typedef struct _STAMPROFILE {
    uint64_t c;
} STAMPROFILE;

typedef struct _STAMPROFILEADV {
    uint64_t c;
} STAMPROFILEADV;

typedef struct _STAMRATIO {
    uint64_t c;
} STAMRATIO;

RT_C_DECLS_END

#endif /* ___SHIM_IPRT_TYPES_H___ */
