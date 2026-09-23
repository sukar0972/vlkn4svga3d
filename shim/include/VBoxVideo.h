#ifndef ___SHIM_VBOXVIDEO_H___
#define ___SHIM_VBOXVIDEO_H___

#include <stdint.h>
#include <stdbool.h>

struct DisplayState;
typedef struct DisplayState DisplayState;

struct VBEHeader {
    uint8_t u[32];
};
typedef struct VBEHeader VBEHeader;

typedef struct _VBVAINFOVIEW {
    uint32_t u32ViewIndex;
} VBVAINFOVIEW;

typedef struct _VBVAINFOSCREEN {
    uint32_t u32ScreenIndex;
} VBVAINFOSCREEN;

typedef struct _VBOXVHWACMD {
    uint32_t u32Flags;
} VBOXVHWACMD;

#endif
