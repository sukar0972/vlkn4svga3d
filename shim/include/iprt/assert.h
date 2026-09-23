#ifndef ___SHIM_IPRT_ASSERT_H___
#define ___SHIM_IPRT_ASSERT_H___

#include "cdefs.h"
#include <assert.h>

#ifndef RT_SUCCESS
# define RT_SUCCESS(rc)      ((int)(rc) >= 0)
#endif
#ifndef RT_FAILURE
# define RT_FAILURE(rc)      ((int)(rc) < 0)
#endif
#ifndef RT_SUCCESS_NP
# define RT_SUCCESS_NP(rc)   ((int)(rc) >= 0)
#endif

#ifndef Assert
# define Assert(expr)        do { (void)(expr); } while (0)
#endif

#ifndef AssertReturn
# define AssertReturn(expr, rc) do { if (!(expr)) return (rc); } while (0)
#endif

#ifndef AssertRCReturn
# define AssertRCReturn(rcExpr, retVal) do { int _rc = (rcExpr); if (RT_FAILURE(_rc)) return (retVal); } while (0)
#endif

#ifndef AssertRCReturnStmt
# define AssertRCReturnStmt(rcExpr, stmt, retVal) do { int _rc = (rcExpr); if (RT_FAILURE(_rc)) { stmt; return (retVal); } } while (0)
#endif

#ifndef AssertMsgReturn
# define AssertMsgReturn(expr, msg, rc) do { if (!(expr)) return (rc); } while (0)
#endif

#ifndef AssertMsgReturnStmt
# define AssertMsgReturnStmt(expr, msg, stmt, retVal) do { if (!(expr)) { stmt; return (retVal); } } while (0)
#endif

#ifndef AssertMsgFailed
# define AssertMsgFailed(msg) do { } while (0)
#endif

#ifndef AssertFailed
# define AssertFailed()      do { } while (0)
#endif

#ifndef AssertFailedReturn
# define AssertFailedReturn(rc) return (rc)
#endif

#ifndef AssertMsgFailedReturn
# define AssertMsgFailedReturn(msg, rc) do { (void)(msg); return (rc); } while (0)
#endif

#ifndef AssertBreak
# define AssertBreak(expr) do { if (!(expr)) break; } while (0)
#endif

#ifndef AssertMsgBreak
# define AssertMsgBreak(expr, msg) do { if (!(expr)) { (void)(msg); break; } } while (0)
#endif

#ifndef AssertFailedBreak
# define AssertFailedBreak() do { break; } while (0)
#endif

#ifndef AssertRCBreak
# define AssertRCBreak(rc) do { if (RT_FAILURE(rc)) break; } while (0)
#endif

#ifndef AssertMsgBreakStmt
# define AssertMsgBreakStmt(expr, msg, stmt) do { if (!(expr)) { (void)(msg); stmt; break; } } while (0)
#endif

#ifndef AssertMsg
# define AssertMsg(expr, msg) do { (void)(expr); } while (0)
#endif

#ifndef AssertCompile
# define AssertCompile(expr) static_assert(expr, #expr)
#endif

#ifndef AssertCompileSize
# define AssertCompileSize(type, size) static_assert(sizeof(type) == (size), "sizeof(" #type ") == " #size)
#endif

#ifndef AssertCompileMemberAlignment
# define AssertCompileMemberAlignment(type, member, align) static_assert(alignof(type) >= 1, "alignment")
#endif

#ifndef AssertPtrReturn
# define AssertPtrReturn(ptr, rc) do { if (!(ptr)) return (rc); } while (0)
#endif

#ifndef AssertRC
# define AssertRC(rc)        do { (void)(rc); } while (0)
#endif

#ifndef RTAssertMsg2Weak
# define RTAssertMsg2Weak(msg) do { } while (0)
#endif

#endif /* ___SHIM_IPRT_ASSERT_H___ */
