#ifndef XRT_CONSOLE_H
#define XRT_CONSOLE_H

#include <xrt/core.h>
#include <xrt/error.h>
#if defined(XRT_FEATURE_CONSOLE_INPUT) || defined(XRT_FEATURE_CONSOLE_TERMINAL)
#include <xrt/buffer.h>
#endif

#if defined(XRT_FEATURE_CONSOLE_INPUT) && (!defined(XRT_FEATURE_CONSOLE) || !defined(XRT_FEATURE_BUFFER) || !defined(XRT_FEATURE_IO_STANDARD))
#error "Console input requires console, buffer and standard IO"
#endif

#if defined(XRT_FEATURE_CONSOLE_SCREEN) && !defined(XRT_FEATURE_CONSOLE)
#error "Console screen requires console"
#endif
#if defined(XRT_FEATURE_CONSOLE_TERMINAL) && (!defined(XRT_FEATURE_CONSOLE_SCREEN) || !defined(XRT_FEATURE_BUFFER) || !defined(XRT_FEATURE_IO_STANDARD) || !defined(XRT_FEATURE_THREAD))
#error "Console terminal requires console screen, buffer, standard IO and thread"
#endif



#if defined(XRT_FEATURE_CONSOLE)

/* 标准输出流名称跨平台稳定，不直接暴露 FILE 或原生句柄。 */
typedef enum xconsolestream {
	XCONSOLE_STDOUT = 1,
	XCONSOLE_STDERR
} xconsolestream;



/* Console 错误代码在 xrt.console 域内稳定。 */
typedef enum xconsoleerror {
	XCONSOLE_ERROR_STREAM = 1,
	XCONSOLE_ERROR_UTF8,
	XCONSOLE_ERROR_WRITE,
	XCONSOLE_ERROR_FLUSH,
	XCONSOLE_ERROR_READ,
	XCONSOLE_ERROR_LIMIT,
	XCONSOLE_ERROR_STATE,
	XCONSOLE_ERROR_TERMINAL
} xconsoleerror;



XRT_EXTERN_C_BEGIN



/* 写入 UTF-8 文本；真实 Windows 控制台转换为 UTF-16，重定向时保留原始 UTF-8 字节。 */
XRT_API bool xrtConsoleWrite(xconsolestream Stream, xstrview Text);



/* 原子写入 UTF-8 文本和一个换行符。 */
XRT_API bool xrtConsoleWriteLine(xconsolestream Stream, xstrview Text);



/* 刷新指定标准输出流。 */
XRT_API bool xrtConsoleFlush(xconsolestream Stream);



/* 判断指定标准输出流当前是否连接交互终端；非终端是正常结果，不设置错误。 */
XRT_API bool xrtConsoleIsTerminal(xconsolestream Stream);

#if defined(XRT_FEATURE_CONSOLE_INPUT)
/* Strict Unicode scalar input: 1 value, 0 EOF, -1 error. No read-ahead. */
XRT_API int xrtConsoleReadChar(uint32* pCodepoint);
/* EOF before a character is a normal NULL result; an empty line owns an empty buffer. */
XRT_API xbuffer* xrtConsoleReadLine(size_t iMaxBytes);
#endif

#if defined(XRT_FEATURE_CONSOLE_TERMINAL)
typedef struct xconsolesession xconsolesession;
typedef enum xconsoleeventkind {
    XCONSOLE_EVENT_TEXT = 1, XCONSOLE_EVENT_KEY, XCONSOLE_EVENT_RESIZE,
    XCONSOLE_EVENT_PASTE, XCONSOLE_EVENT_MOUSE, XCONSOLE_EVENT_CLOSED
} xconsoleeventkind;
typedef enum xconsolekey {
    XCONSOLE_KEY_ESCAPE = 0x110000, XCONSOLE_KEY_ENTER, XCONSOLE_KEY_TAB,
    XCONSOLE_KEY_BACKSPACE, XCONSOLE_KEY_UP, XCONSOLE_KEY_DOWN,
    XCONSOLE_KEY_LEFT, XCONSOLE_KEY_RIGHT, XCONSOLE_KEY_HOME,
    XCONSOLE_KEY_END, XCONSOLE_KEY_INSERT, XCONSOLE_KEY_DELETE,
    XCONSOLE_KEY_PAGE_UP, XCONSOLE_KEY_PAGE_DOWN, XCONSOLE_KEY_F1
} xconsolekey;
typedef struct xconsoleevent {
    xconsoleeventkind Kind;
    uint32 Key, Modifiers, Repeat;
    int32 X, Y, Wheel;
    uint32 Columns, Rows;
    bool Down;
    xbuffer* Text; /* owned UTF-8; may contain NUL */
} xconsoleevent;

/* flags: raw=1, mouse=2, bracketed-paste=4, alternate-screen=8. Thread-affine, exclusive stdin. */
XRT_API xconsolesession* xrtConsoleSessionOpen(uint32 Flags);
XRT_API bool xrtConsoleSessionClose(xconsolesession* pSession);
XRT_API void xrtConsoleSessionDestroy(xconsolesession* pSession);
XRT_API bool xrtConsoleSessionClosed(const xconsolesession* pSession);
XRT_API bool xrtConsoleSessionPasteSupported(const xconsolesession* pSession);
XRT_API bool xrtConsoleSessionWrite(xconsolesession* pSession, xstrview Text);
XRT_API bool xrtConsoleSessionFlush(xconsolesession* pSession);
XRT_API bool xrtConsoleSessionMove(xconsolesession* pSession, uint32 X, uint32 Y);
XRT_API bool xrtConsoleSessionClear(xconsolesession* pSession, int Mode);
XRT_API bool xrtConsoleSessionCursor(xconsolesession* pSession, bool Visible);
XRT_API bool xrtConsoleSessionStyle(xconsolesession* pSession, int32 Foreground, int32 Background, uint32 Attributes);
/* NULL without error is timeout; CLOSED is emitted once. Returned event is owned. */
XRT_API xconsoleevent* xrtConsoleSessionRead(xconsolesession* pSession, int TimeoutMs);
XRT_API void xrtConsoleEventDestroy(xconsoleevent* pEvent);
#endif

#if defined(XRT_FEATURE_CONSOLE_SCREEN)
/* Queries never change terminal modes. Nonterminal size is 0,0. */
XRT_API bool xrtConsoleSize(xconsolestream Stream, uint32* pColumns, uint32* pRows);
XRT_API int xrtConsoleColorMode(xconsolestream Stream);
/* -1 default, 0..255 palette, 0x1000000|RGB true color; attributes bits 1,2,4,8,16. */
XRT_API bool xrtConsoleWriteStyled(xconsolestream Stream, xstrview Text, int32 Foreground, int32 Background, uint32 Attributes);
#endif



XRT_EXTERN_C_END

#endif

#endif
