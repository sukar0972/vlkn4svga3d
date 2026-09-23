#ifndef ___SHIM_WINDOWS_H___
#define ___SHIM_WINDOWS_H___

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Calling conventions */
#ifndef WINAPI
#define WINAPI
#endif
#ifndef CALLBACK
#define CALLBACK
#endif
#ifndef APIENTRY
#define APIENTRY
#endif
#ifndef DECLSPEC_DEPRECATED
#define DECLSPEC_DEPRECATED
#endif

/* Basic types */
typedef int                 BOOL;
typedef uint8_t             BYTE;
typedef uint16_t            WORD;
typedef uint32_t            DWORD;
typedef int32_t             LONG;
typedef uint32_t            ULONG;
typedef int16_t             SHORT;
typedef uint16_t            USHORT;
typedef int32_t             INT;
typedef uint32_t            UINT;
typedef int64_t             LONGLONG;
typedef uint64_t            ULONGLONG;
typedef float               FLOAT;
typedef double              DOUBLE;
typedef char                CHAR;
typedef unsigned char       UCHAR;
typedef void                VOID;
typedef void               *LPVOID;
typedef const void         *LPCVOID;
typedef void               *PVOID;
typedef char               *LPSTR;
typedef const char         *LPCSTR;
typedef wchar_t             WCHAR;
typedef wchar_t            *LPWSTR;
typedef const wchar_t      *LPCWSTR;

typedef intptr_t            INT_PTR;
typedef uintptr_t           UINT_PTR;
typedef intptr_t            LONG_PTR;
typedef uintptr_t           ULONG_PTR;

typedef ULONG_PTR           DWORD_PTR;
typedef UINT_PTR            WPARAM;
typedef LONG_PTR            LPARAM;
typedef LONG_PTR            LRESULT;

typedef int32_t             HRESULT;

/* Constants */
#ifndef TRUE
#define TRUE  1
#endif
#ifndef FALSE
#define FALSE 0
#endif

#ifndef NULL
#define NULL 0
#endif

#define MAX_PATH 260

/* Handles */
typedef void               *HANDLE;
typedef void               *HWND;
typedef void               *HDC;
typedef void               *HINSTANCE;
typedef void               *HMODULE;
typedef void               *HRGN;
typedef void               *HMONITOR;
typedef void               *HBITMAP;
typedef void               *HPALETTE;
typedef void               *HMENU;
typedef void               *HICON;
typedef void               *HCURSOR;
typedef void               *HBRUSH;

/* Structures */
typedef struct tagRECT {
    LONG left;
    LONG top;
    LONG right;
    LONG bottom;
} RECT, *PRECT, *LPRECT;
typedef const RECT *LPCRECT;

typedef struct tagPOINT {
    LONG x;
    LONG y;
} POINT, *PPOINT, *LPPOINT;

typedef struct tagSIZE {
    LONG cx;
    LONG cy;
} SIZE, *PSIZE, *LPSIZE;

typedef struct tagMSG {
    HWND   hwnd;
    UINT   message;
    WPARAM wParam;
    LPARAM lParam;
    DWORD  time;
    POINT  pt;
} MSG, *PMSG, *LPMSG;

typedef struct tagCREATESTRUCTA {
    LPVOID    lpCreateParams;
    HINSTANCE hInstance;
    HMENU     hMenu;
    HWND      hwndParent;
    int       cy;
    int       cx;
    int       y;
    int       x;
    LONG      style;
    LPCSTR    lpszName;
    LPCSTR    lpszClass;
    DWORD     dwExStyle;
} CREATESTRUCTA, *LPCREATESTRUCTA;

typedef struct tagCREATESTRUCTW {
    LPVOID    lpCreateParams;
    HINSTANCE hInstance;
    HMENU     hMenu;
    HWND      hwndParent;
    int       cy;
    int       cx;
    int       y;
    int       x;
    LONG      style;
    LPCWSTR   lpszName;
    LPCWSTR   lpszClass;
    DWORD     dwExStyle;
} CREATESTRUCTW, *LPCREATESTRUCTW;

#ifdef UNICODE
typedef CREATESTRUCTW CREATESTRUCT;
#else
typedef CREATESTRUCTA CREATESTRUCT;
#endif
typedef CREATESTRUCT *LPCREATESTRUCT;

typedef LRESULT (CALLBACK *WNDPROC)(HWND, UINT, WPARAM, LPARAM);

typedef struct tagWNDCLASSEXA {
    UINT      cbSize;
    UINT      style;
    WNDPROC   lpfnWndProc;
    int       cbClsExtra;
    int       cbWndExtra;
    HINSTANCE hInstance;
    HICON     hIcon;
    HCURSOR   hCursor;
    HBRUSH    hbrBackground;
    LPCSTR    lpszMenuName;
    LPCSTR    lpszClassName;
    HICON     hIconSm;
} WNDCLASSEXA, *PWNDCLASSEXA, *LPWNDCLASSEXA;

typedef struct tagWNDCLASSEXW {
    UINT      cbSize;
    UINT      style;
    WNDPROC   lpfnWndProc;
    int       cbClsExtra;
    int       cbWndExtra;
    HINSTANCE hInstance;
    HICON     hIcon;
    HCURSOR   hCursor;
    HBRUSH    hbrBackground;
    LPCWSTR   lpszMenuName;
    LPCWSTR   lpszClassName;
    HICON     hIconSm;
} WNDCLASSEXW, *PWNDCLASSEXW, *LPWNDCLASSEXW;

#ifdef UNICODE
typedef WNDCLASSEXW WNDCLASSEX;
#else
typedef WNDCLASSEXA WNDCLASSEX;
#endif

/* Window Styles */
#define WS_OVERLAPPED       0x00000000L
#define WS_POPUP            0x80000000L
#define WS_CHILD            0x40000000L
#define WS_MINIMIZE         0x20000000L
#define WS_VISIBLE          0x10000000L
#define WS_DISABLED         0x08000000L
#define WS_CLIPSIBLINGS     0x04000000L
#define WS_CLIPCHILDREN     0x02000000L
#define WS_MAXIMIZE         0x01000000L
#define WS_CAPTION          0x00C00000L
#define WS_BORDER           0x00800000L
#define WS_DLGFRAME         0x00400000L
#define WS_EX_NOPARENTNOTIFY 0x00000004L
#define WS_EX_TRANSPARENT    0x00000020L
#define WS_EX_TOOLWINDOW     0x00000080L
#define WS_EX_NOACTIVATE     0x08000000L

/* Window class styles */
#define CS_VREDRAW          0x0001
#define CS_HREDRAW          0x0002
#define CS_DBLCLKS          0x0008
#define CS_OWNDC            0x0020
#define CS_CLASSDC          0x0040
#define CS_PARENTDC         0x0080

/* ShowWindow Commands */
#define SW_HIDE             0
#define SW_SHOWNORMAL       1
#define SW_NORMAL           1
#define SW_SHOWMINIMIZED    2
#define SW_SHOWMAXIMIZED    3
#define SW_MAXIMIZE         3
#define SW_SHOWNOACTIVATE   4
#define SW_SHOW             5
#define SW_MINIMIZE         6
#define SW_SHOWMINNOACTIVE  7
#define SW_SHOWNA           8
#define SW_RESTORE          9

/* SetWindowPos Flags */
#define SWP_NOSIZE          0x0001
#define SWP_NOMOVE          0x0002
#define SWP_NOZORDER        0x0004
#define SWP_NOREDRAW        0x0008
#define SWP_NOACTIVATE      0x0010
#define SWP_FRAMECHANGED    0x0020
#define SWP_SHOWWINDOW      0x0040
#define SWP_HIDEWINDOW      0x0080

#define HWND_TOP            ((HWND)0)
#define HWND_BOTTOM         ((HWND)1)
#define HWND_TOPMOST        ((HWND)-1)
#define HWND_NOTOPMOST      ((HWND)-2)

/* Window messages */
#define WM_NULL             0x0000
#define WM_CREATE           0x0001
#define WM_DESTROY          0x0002
#define WM_MOVE             0x0003
#define WM_SIZE             0x0005
#define WM_CLOSE            0x0010
#define WM_QUIT             0x0012
#define WM_PAINT            0x000F
#define WM_USER             0x0400
#define WM_APP              0x8000

/* Macros */
#ifndef MAKEFOURCC
#define MAKEFOURCC(ch0, ch1, ch2, ch3) \
    ((DWORD)(BYTE)(ch0) | ((DWORD)(BYTE)(ch1) << 8) | \
    ((DWORD)(BYTE)(ch2) << 16) | ((DWORD)(BYTE)(ch3) << 24))
#endif

#ifndef LOWORD
#define LOWORD(l)           ((WORD)(((DWORD_PTR)(l)) & 0xffff))
#endif
#ifndef HIWORD
#define HIWORD(l)           ((WORD)((((DWORD_PTR)(l)) >> 16) & 0xffff))
#endif
#ifndef LOBYTE
#define LOBYTE(w)           ((BYTE)(((DWORD_PTR)(w)) & 0xff))
#endif
#ifndef HIBYTE
#define HIBYTE(w)           ((BYTE)((((DWORD_PTR)(w)) >> 8) & 0xff))
#endif

/* HRESULT codes */
#define S_OK                ((HRESULT)0x00000000L)
#define S_FALSE             ((HRESULT)0x00000001L)
#define E_NOTIMPL           ((HRESULT)0x80004001L)
#define E_NOINTERFACE       ((HRESULT)0x80004002L)
#define E_POINTER           ((HRESULT)0x80004003L)
#define E_ABORT             ((HRESULT)0x80004004L)
#define E_FAIL              ((HRESULT)0x80004005L)
#define E_UNEXPECTED        ((HRESULT)0x8000FFFFL)
#define E_OUTOFMEMORY       ((HRESULT)0x8007000EL)
#define E_INVALIDARG        ((HRESULT)0x80070057L)

#ifndef SUCCEEDED
#define SUCCEEDED(hr)       (((HRESULT)(hr)) >= 0)
#endif
#ifndef FAILED
#define FAILED(hr)          (((HRESULT)(hr)) < 0)
#endif

/* Stub Win32 APIs */
static inline DWORD GetLastError(void) { return 0; }
static inline void SetLastError(DWORD dw) { (void)dw; }

#ifdef __cplusplus
}
#endif

#endif /* ___SHIM_WINDOWS_H___ */
