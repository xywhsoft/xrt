#include "../internal/xrt_console.h"
#include <errno.h>
#include <stdlib.h>
#if !defined(_WIN32) && !defined(_WIN64)
#include <unistd.h>
#include <sys/ioctl.h>
#endif

#if defined(XRT_FEATURE_CONSOLE_SCREEN)
static bool __xrtScreenError(xerrkind Kind, cstr Message)
{
    __xrtErrorSetDetail(Kind, "xrt.console", XCONSOLE_ERROR_TERMINAL, "terminal", Message, NULL);
    return false;
}
static bool __xrtScreenSystem(cstr Operation)
{
#if defined(_WIN32) || defined(_WIN64)
    int Code = (int)GetLastError();
#else
    int Code = errno;
#endif
    __xrtErrorSetSystem("xrt.console", XCONSOLE_ERROR_TERMINAL, Operation, Code, "terminal operation failed");
    return false;
}

XRT_API bool xrtConsoleSize(xconsolestream Stream, uint32* Columns, uint32* Rows)
{
    if (Columns == NULL || Rows == NULL || (Stream != XCONSOLE_STDOUT && Stream != XCONSOLE_STDERR))
        return __xrtScreenError(XERR_ARGUMENT, "invalid terminal size argument");
    *Columns = *Rows = 0;
    if (!xrtConsoleIsTerminal(Stream)) return true;
#if defined(_WIN32) || defined(_WIN64)
    CONSOLE_SCREEN_BUFFER_INFO Info;
    if (!GetConsoleScreenBufferInfo(GetStdHandle(Stream == XCONSOLE_STDOUT ? STD_OUTPUT_HANDLE : STD_ERROR_HANDLE), &Info))
        return __xrtScreenSystem("size");
    *Columns = (uint32)(Info.srWindow.Right - Info.srWindow.Left + 1);
    *Rows = (uint32)(Info.srWindow.Bottom - Info.srWindow.Top + 1);
#else
    struct winsize Size;
    if (ioctl(Stream == XCONSOLE_STDOUT ? STDOUT_FILENO : STDERR_FILENO, TIOCGWINSZ, &Size) != 0)
        return __xrtScreenSystem("size");
    *Columns = Size.ws_col; *Rows = Size.ws_row;
#endif
    return true;
}
XRT_API int xrtConsoleColorMode(xconsolestream Stream)
{
    const char* Term;
    if ((Stream != XCONSOLE_STDOUT && Stream != XCONSOLE_STDERR)) {
        (void)__xrtScreenError(XERR_ARGUMENT, "invalid console stream"); return -1;
    }
    if (!xrtConsoleIsTerminal(Stream) || getenv("NO_COLOR") != NULL) return 0;
    Term = getenv("TERM");
    if (Term != NULL && strcmp(Term, "dumb") == 0) return 0;
#if defined(_WIN32) || defined(_WIN64)
    DWORD Mode;
    if (GetConsoleMode(GetStdHandle(Stream == XCONSOLE_STDOUT ? STD_OUTPUT_HANDLE : STD_ERROR_HANDLE), &Mode) &&
        (Mode & ENABLE_VIRTUAL_TERMINAL_PROCESSING)) return 3;
    return 1;
#else
    { const char* Color = getenv("COLORTERM");
      if (Color != NULL && (strcmp(Color,"truecolor") == 0 || strcmp(Color,"24bit") == 0)) return 3; }
    return Term != NULL && strstr(Term, "256color") != NULL ? 2 : 1;
#endif
}
bool __xrtTerminalStyle(char* Text, size_t Capacity, int32 Fg, int32 Bg, uint32 Attributes)
{
    int Length, Extra;
    int32 Colors[2] = {Fg, Bg};
    if ((Attributes & ~31u) != 0) return __xrtScreenError(XERR_RANGE, "invalid text attributes");
    for (unsigned i=0; i<2; ++i)
        if (Colors[i] < -1 || Colors[i] > 0x1ffffff || (Colors[i] > 255 && Colors[i] < 0x1000000))
            return __xrtScreenError(XERR_RANGE, "invalid terminal color");
    Length = snprintf(Text, Capacity, "\x1b[0%s%s%s%s%s",
        Attributes&1?";1":"", Attributes&2?";2":"", Attributes&4?";3":"", Attributes&8?";4":"", Attributes&16?";7":"");
    if (Length < 0 || (size_t)Length >= Capacity) return __xrtScreenError(XERR_INTERNAL, "style buffer overflow");
    for (unsigned i=0; i<2; ++i) {
        int32 Color = Colors[i];
        if (Color < 0) continue;
        if (Color < 8) Extra = snprintf(Text+Length, Capacity-(size_t)Length, ";%d", (i?40:30)+Color);
        else if (Color < 16) Extra = snprintf(Text+Length, Capacity-(size_t)Length, ";%d", (i?100:90)+Color-8);
        else if (Color <= 255) Extra = snprintf(Text+Length, Capacity-(size_t)Length, ";%d;5;%d", i?48:38, Color);
        else Extra = snprintf(Text+Length, Capacity-(size_t)Length, ";%d;2;%d;%d;%d", i?48:38, (Color>>16)&255, (Color>>8)&255, Color&255);
        if (Extra < 0 || (size_t)Extra >= Capacity-(size_t)Length) return __xrtScreenError(XERR_INTERNAL, "style buffer overflow");
        Length += Extra;
    }
    if ((size_t)Length+2 > Capacity) return __xrtScreenError(XERR_INTERNAL, "style buffer overflow");
    Text[Length++] = 'm'; Text[Length] = 0; return true;
}
XRT_API bool xrtConsoleWriteStyled(xconsolestream Stream, xstrview Text, int32 Fg, int32 Bg, uint32 Attributes)
{
    char Style[128]; xconsolewriter Writer; bool Ok;
#if defined(_WIN32) || defined(_WIN64)
    DWORD Mode = 0; bool Changed = false;
#endif
    if (!__xrtTerminalStyle(Style, sizeof(Style), Fg, Bg, Attributes) || !__xrtConsoleValidateText(Text)) return false;
    if (!__xrtConsoleWriterOpen(Stream, &Writer)) return false;
    if (!xrtConsoleIsTerminal(Stream) || xrtConsoleColorMode(Stream) == 0) {
        Ok = __xrtConsoleWriterWrite(&Writer, Text.Data, Text.Size);
        __xrtConsoleWriterClose(&Writer); return Ok;
    }
#if defined(_WIN32) || defined(_WIN64)
    if (!GetConsoleMode((HANDLE)Writer.Console, &Mode)) { __xrtConsoleWriterClose(&Writer); return __xrtScreenSystem("style"); }
    if (!(Mode & ENABLE_VIRTUAL_TERMINAL_PROCESSING)) {
        if (!SetConsoleMode((HANDLE)Writer.Console, Mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING)) {
            __xrtConsoleWriterClose(&Writer); return __xrtScreenSystem("style");
        }
        Changed = true;
    }
#endif
    Ok = __xrtConsoleWriterWrite(&Writer, Style, strlen(Style)) && __xrtConsoleWriterWrite(&Writer, Text.Data, Text.Size);
    { xerror* Error = Ok ? NULL : xrtTakeError();
      if (!__xrtConsoleWriterWrite(&Writer, "\x1b[0m", 4)) Ok = false;
#if defined(_WIN32) || defined(_WIN64)
      if (Changed && !SetConsoleMode((HANDLE)Writer.Console, Mode)) { (void)__xrtScreenSystem("restore-style"); Ok = false; }
#endif
      if (Error != NULL) xrtSetErrorTake(Error); }
    __xrtConsoleWriterClose(&Writer); return Ok;
}
#endif
