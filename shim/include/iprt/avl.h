#ifndef ___SHIM_IPRT_AVL_H___
#define ___SHIM_IPRT_AVL_H___

#include "cdefs.h"
#include "types.h"

RT_C_DECLS_BEGIN

typedef struct _AVLU32NODECORE {
    uint32_t                Key;
    struct _AVLU32NODECORE *pLeft;
    struct _AVLU32NODECORE *pRight;
    unsigned char           uchHeight;
} AVLU32NODECORE, *PAVLU32NODECORE;

typedef PAVLU32NODECORE AVLU32TREE;
typedef AVLU32TREE     *PAVLU32TREE;

typedef int (*PFNAVLU32CALLBACK)(PAVLU32NODECORE pNode, void *pvParam);

PAVLU32NODECORE RTAvlU32Get(PAVLU32TREE pTree, uint32_t Key);
bool            RTAvlU32Insert(PAVLU32TREE pTree, PAVLU32NODECORE pNode);
PAVLU32NODECORE RTAvlU32Remove(PAVLU32TREE pTree, uint32_t Key);
int             RTAvlU32Destroy(PAVLU32TREE pTree, PFNAVLU32CALLBACK pfnCallback, void *pvParam);

RT_C_DECLS_END

#endif /* ___SHIM_IPRT_AVL_H___ */
