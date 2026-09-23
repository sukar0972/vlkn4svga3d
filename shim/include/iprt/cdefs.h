#ifndef ___SHIM_IPRT_CDEFS_H___
#define ___SHIM_IPRT_CDEFS_H___

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#ifdef __cplusplus
# define RT_C_DECLS_BEGIN extern "C" {
# define RT_C_DECLS_END   }
#else
# define RT_C_DECLS_BEGIN
# define RT_C_DECLS_END
#endif

#ifndef DECLCALLBACK
# define DECLCALLBACK(type) type
#endif

#ifndef DECLINLINE
# define DECLINLINE(type) static inline type
#endif

#ifndef RT_NOREF
# define RT_NOREF(var) ((void)(var))
#endif

#ifndef NOREF
# define NOREF(var) ((void)(var))
#endif

#ifndef RT_ELEMENTS
# define RT_ELEMENTS(arr) (sizeof(arr) / sizeof((arr)[0]))
#endif

#ifndef RT_BOOL
# define RT_BOOL(v) (!!(v))
#endif

#ifndef RT_LIKELY
# define RT_LIKELY(expr) (expr)
#endif

#ifndef RT_UNLIKELY
# define RT_UNLIKELY(expr) (expr)
#endif

#ifndef RT_ZERO
# define RT_ZERO(var) memset(&(var), 0, sizeof(var))
#endif

#ifndef RT_BZERO
# define RT_BZERO(pv, cb) memset((pv), 0, (cb))
#endif

#ifndef RT_ALIGN
# define RT_ALIGN(val, align) (((val) + (align) - 1) & ~((align) - 1))
#endif

#ifndef RT_ALIGN_32
# define RT_ALIGN_32(val, align) RT_ALIGN(val, align)
#endif

#ifndef RT_BIT_32
# define RT_BIT_32(bit) (1U << (bit))
#endif

#ifndef RT_BIT_64
# define RT_BIT_64(bit) (1ULL << (bit))
#endif

#ifndef RT_ABS
# define RT_ABS(v) ((v) < 0 ? -(v) : (v))
#endif

#ifndef RT_MIN
# define RT_MIN(a,b) ((a) < (b) ? (a) : (b))
#endif

#ifndef RT_MAX
# define RT_MAX(a,b) ((a) > (b) ? (a) : (b))
#endif

#ifndef RT_CLAMP
# define RT_CLAMP(v, min, max) RT_MAX(min, RT_MIN(v, max))
#endif

#ifndef RT_OFFSETOF
# define RT_OFFSETOF(type, member) offsetof(type, member)
#endif

#ifndef RT_SRC_POS
# define RT_SRC_POS __FILE__, __LINE__, __FUNCTION__
#endif

#ifndef ASMCompilerBarrier
# define ASMCompilerBarrier() __asm__ __volatile__("" ::: "memory")
#endif

#ifndef ARCH_BITS
# define ARCH_BITS 64
#endif

#ifndef HC_ARCH_BITS
# define HC_ARCH_BITS 64
#endif

#ifndef CTX_SUFF
# define CTX_SUFF(var) var##R3
#endif

#ifndef R3PTRTYPE
# define R3PTRTYPE(type) type
#endif

#ifndef R0PTRTYPE
# define R0PTRTYPE(type) type
#endif

#ifndef RCPTRTYPE
# define RCPTRTYPE(type) type
#endif

#ifndef R3R0PTRTYPE
# define R3R0PTRTYPE(type) type
#endif

#ifndef RT_UNTRUSTED_VOLATILE_HOST
# define RT_UNTRUSTED_VOLATILE_HOST volatile
#endif

#ifndef RT_UNTRUSTED_VOLATILE_GUEST
# define RT_UNTRUSTED_VOLATILE_GUEST volatile
#endif

#define _1K     1024
#define _1M     (1024 * 1024)
#define _1G     (1024 * 1024 * 1024)

#endif /* ___SHIM_IPRT_CDEFS_H___ */
