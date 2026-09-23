#ifndef ___SHIM_VBOX_VMM_PDMDEV_H___
#define ___SHIM_VBOX_VMM_PDMDEV_H___

#include <iprt/cdefs.h>
#include <iprt/types.h>
#include <VBox/types.h>

RT_C_DECLS_BEGIN

typedef struct _PDMCRITSECT {
    uint32_t u;
} PDMCRITSECT, *PPDMCRITSECT;

typedef struct _PDMLED {
    uint32_t u;
} PDMLED, *PPDMLED;

typedef struct _PDMIBASE {
    uint32_t u;
} PDMIBASE, *PPDMIBASE;

typedef struct _PDMIDISPLAYPORT {
    uint32_t u;
} PDMIDISPLAYPORT, *PPDMIDISPLAYPORT;

typedef struct _PDMILEDPORTS {
    uint32_t u;
} PDMILEDPORTS, *PPDMILEDPORTS;

typedef struct _PDMIDISPLAYCONNECTOR {
    uint32_t u;
} PDMIDISPLAYCONNECTOR, *PPDMIDISPLAYCONNECTOR;

typedef struct _PDMILEDCONNECTORS {
    uint32_t u;
} PDMILEDCONNECTORS, *PPDMILEDCONNECTORS;

typedef struct _PDMIDISPLAYVBVACALLBACKS {
    uint32_t u;
} PDMIDISPLAYVBVACALLBACKS, *PPDMIDISPLAYVBVACALLBACKS;

typedef struct _PDMPCIDEV {
    uint32_t u;
} PDMPCIDEV, *PPDMPCIDEV;

typedef struct _PDMDEVINS {
    void *pvInstance;
} PDMDEVINS, *PPDMDEVINS;

typedef PDMDEVINS  PDMDEVINSR3;
typedef PDMDEVINS *PPDMDEVINSR3;
typedef PDMDEVINS  PDMDEVINSR0;
typedef PDMDEVINS *PPDMDEVINSR0;
typedef PDMDEVINS  PDMDEVINSRC;
typedef PDMDEVINS *PPDMDEVINSRC;

typedef struct _SSMHANDLE *PSSMHANDLE;
typedef struct _SSMFIELD {
    const char *pszName;
} SSMFIELD;

typedef int PCIADDRESSSPACE;
#define PCI_ADDRESS_SPACE_MEM 0
#define PCI_ADDRESS_SPACE_IO  1

int PDMDevHlpVMSetError(PPDMDEVINS pDevIns, int rc, const char *pszFile, unsigned uLine, const char *pszFunction, const char *pszFormat, ...);

void *RTLdrGetSystemSymbol(const char *pszModule, const char *pszSymbol);

RT_C_DECLS_END

#endif /* ___SHIM_VBOX_VMM_PDMDEV_H___ */
