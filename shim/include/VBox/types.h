#ifndef ___SHIM_VBOX_TYPES_H___
#define ___SHIM_VBOX_TYPES_H___

#include <iprt/types.h>

#ifndef PAGE_SIZE
# define PAGE_SIZE 4096
#endif

typedef void *PTMTIMERR3;
typedef const struct _DBGFINFOHLP *PCDBGFINFOHLP;

struct VMSVGAINFOFLAGS32;
typedef struct VMSVGAINFOFLAGS32 *PVMSVGAINFOFLAGS32;
typedef const struct VMSVGAINFOFLAGS32 *PCVMSVGAINFOFLAGS32;

#endif
