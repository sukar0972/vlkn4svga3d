#ifndef ___SHIM_VBOX_LOG_H___
#define ___SHIM_VBOX_LOG_H___

#include <stdio.h>
#include <stdbool.h>

#define LOG_GROUP_DEV_VMSVGA            1

#ifdef DEBUG_LOGS
# define Log(args)                       do { printf args; } while (0)
# define LogFunc(args)                   do { printf args; } while (0)
# define LogRel(args)                    do { printf args; } while (0)
# define LogRelMax(args)                 do { printf args; } while (0)
# define LogFlow(args)                   do { printf args; } while (0)
# define Log3(args)                      do { printf args; } while (0)
# define Log4(args)                      do { printf args; } while (0)
#else
# define Log(args)                       do { } while (0)
# define LogFunc(args)                   do { } while (0)
# define LogRel(args)                    do { } while (0)
# define LogRelMax(args)                 do { } while (0)
# define LogFlow(args)                   do { } while (0)
# define Log3(args)                      do { } while (0)
# define Log4(args)                      do { } while (0)
#endif

static inline bool RTLogRelSetBuffering(bool fBuffered) { (void)fBuffered; return false; }

#endif /* ___SHIM_VBOX_LOG_H___ */
