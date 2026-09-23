#ifndef ___SHIM_D3DX9SHADER_H___
#define ___SHIM_D3DX9SHADER_H___

#include "d3d9.h"

struct ID3DXBuffer : public IUnknown {
    virtual LPVOID WINAPI GetBufferPointer(void) = 0;
    virtual DWORD  WINAPI GetBufferSize(void) = 0;
};
typedef struct ID3DXBuffer *LPD3DXBUFFER;

#endif
