#include "../internal/xrt_console.h"
#include "../internal/xrt_io.h"
#include <xrt/thread.h>
#include <xrt/time.h>
#include <errno.h>
#include <math.h>
#include <stdlib.h>
#if !defined(_WIN32) && !defined(_WIN64)
#include <unistd.h>
#include <termios.h>
#include <sys/ioctl.h>
#include <poll.h>
#include <time.h>
#endif

#if defined(XRT_FEATURE_CONSOLE_TERMINAL)
enum { XRT_CONSOLE_QUEUE_LIMIT = 16 * 1024 * 1024, XRT_CONSOLE_EVENT_LIMIT = 1024 * 1024 };
struct xconsolesession {
    uint64 Thread;
    uint32 Flags, Columns, Rows;
    bool Active, InputEnded, ClosedEvent, CursorChanged;
    xbuffer Output, Input;
    double EscapeSince;
#if defined(_WIN32) || defined(_WIN64)
    HANDLE In, Out;
    DWORD InputMode, OutputMode;
    CONSOLE_CURSOR_INFO Cursor;
    WCHAR HighSurrogate;
#else
    struct termios InputMode;
#endif
};

static bool __xrtTerminalError(xerrkind Kind, cstr Message)
{
    __xrtErrorSetDetail(Kind, "xrt.console", XCONSOLE_ERROR_TERMINAL, "terminal", Message, NULL);
    return false;
}
static bool __xrtTerminalSystem(cstr Operation)
{
#if defined(_WIN32) || defined(_WIN64)
    int Code = (int)GetLastError();
#else
    int Code = errno;
#endif
    __xrtErrorSetSystem("xrt.console", XCONSOLE_ERROR_TERMINAL, Operation, Code, "terminal operation failed");
    return false;
}
static double __xrtTerminalNow(void)
{
    return xrtTimer();
}
static bool __xrtSessionCheck(const xconsolesession* Session)
{
    if (Session == NULL || !Session->Active) return __xrtTerminalError(XERR_CLOSED, "terminal session is closed");
    if (Session->Thread != xrtThreadCurrentId()) return __xrtTerminalError(XERR_STATE, "terminal session belongs to another thread");
    return true;
}


XRT_API xconsolesession* xrtConsoleSessionOpen(uint32 Flags)
{
    xconsolesession* Session; xerror* Error;
    if (Flags & ~15u) { (void)__xrtTerminalError(XERR_RANGE, "invalid terminal session flags"); return NULL; }
    Session = (xconsolesession*)xrtCalloc(1, sizeof(*Session));
    if (Session == NULL) return NULL;
    if (!__xrtStandardInputAcquire(Session)) { xrtFree(Session); return NULL; }
    Session->Thread = xrtThreadCurrentId(); Session->Flags = Flags;
    (void)xrtBufferInit(&Session->Input); (void)xrtBufferInit(&Session->Output);
    if (!xrtConsoleIsTerminal(XCONSOLE_STDOUT)) {
        (void)__xrtTerminalError(XERR_UNSUPPORTED, "terminal session requires interactive stdin and stdout"); goto fail;
    }
#if defined(_WIN32) || defined(_WIN64)
    Session->In = GetStdHandle(STD_INPUT_HANDLE); Session->Out = GetStdHandle(STD_OUTPUT_HANDLE);
    if (!GetConsoleMode(Session->In, &Session->InputMode) || !GetConsoleMode(Session->Out, &Session->OutputMode) ||
        !GetConsoleCursorInfo(Session->Out, &Session->Cursor)) { (void)__xrtTerminalSystem("open"); goto fail; }
    { DWORD Input = Session->InputMode | ENABLE_WINDOW_INPUT | ENABLE_EXTENDED_FLAGS;
      if (Flags & 1) Input &= ~(ENABLE_ECHO_INPUT | ENABLE_LINE_INPUT | ENABLE_PROCESSED_INPUT);
      Input &= ~ENABLE_QUICK_EDIT_MODE;
      if (Flags & 2) Input |= ENABLE_MOUSE_INPUT; else Input &= ~ENABLE_MOUSE_INPUT;
      if (!SetConsoleMode(Session->Out, Session->OutputMode | ENABLE_VIRTUAL_TERMINAL_PROCESSING)) {
          (void)__xrtTerminalSystem("output-mode"); goto fail; }
      if (!SetConsoleMode(Session->In, Input)) {
          Error = xrtTakeError(); (void)__xrtTerminalSystem("input-mode");
          (void)SetConsoleMode(Session->Out, Session->OutputMode); if (Error != NULL) xrtSetErrorTake(Error); goto fail; } }
#else
    if (!isatty(STDIN_FILENO) || tcgetattr(STDIN_FILENO, &Session->InputMode) != 0) {
        (void)__xrtTerminalError(XERR_UNSUPPORTED, "terminal input is not interactive"); goto fail; }
    { struct termios Input = Session->InputMode;
      if (Flags & 1) { Input.c_iflag &= ~(BRKINT|ICRNL|INLCR|IGNCR|INPCK|ISTRIP|IXON|PARMRK);
        Input.c_cflag = (Input.c_cflag & ~(CSIZE|PARENB)) | CS8;
        Input.c_lflag &= ~(ECHO|ICANON|IEXTEN|ISIG); Input.c_cc[VMIN] = 1; Input.c_cc[VTIME] = 0; }
      if (tcsetattr(STDIN_FILENO, TCSANOW, &Input) != 0) { (void)__xrtTerminalSystem("input-mode"); goto fail; } }
#endif
    Session->Active = true;
    if (!xrtConsoleSize(XCONSOLE_STDOUT, &Session->Columns, &Session->Rows) ||
        ((Flags & 8) && !xrtConsoleWrite(XCONSOLE_STDOUT, XRT_STR_LITERAL("\x1b[?1049h"))) ||
#if !defined(_WIN32) && !defined(_WIN64)
        ((Flags & 4) && !xrtConsoleWrite(XCONSOLE_STDOUT, XRT_STR_LITERAL("\x1b[?2004h"))) ||
        ((Flags & 2) && !xrtConsoleWrite(XCONSOLE_STDOUT, XRT_STR_LITERAL("\x1b[?1002h\x1b[?1006h"))) ||
#endif
        !xrtConsoleFlush(XCONSOLE_STDOUT)) {
        Error = xrtTakeError(); (void)xrtConsoleSessionClose(Session);
        if (Error != NULL) xrtSetErrorTake(Error);
        goto fail;
    }
    return Session;
fail:
    __xrtStandardInputRelease(Session); xrtBufferUnit(&Session->Input); xrtBufferUnit(&Session->Output); xrtFree(Session); return NULL;
}
XRT_API bool xrtConsoleSessionClosed(const xconsolesession* Session) { return Session == NULL || !Session->Active; }
XRT_API bool xrtConsoleSessionPasteSupported(const xconsolesession* Session)
{
#if defined(_WIN32) || defined(_WIN64)
    (void)Session; return false; /* Native Windows input records cannot distinguish paste from typing. */
#else
    return Session != NULL && Session->Active && (Session->Flags & 4) != 0;
#endif
}
XRT_API bool xrtConsoleSessionWrite(xconsolesession* Session, xstrview Text)
{
    if (!__xrtSessionCheck(Session) || !__xrtConsoleValidateText(Text)) return false;
    if (Text.Size > XRT_CONSOLE_QUEUE_LIMIT || Session->Output.Size > XRT_CONSOLE_QUEUE_LIMIT - Text.Size)
        return __xrtTerminalError(XERR_RANGE, "terminal output queue limit exceeded");
    return xrtBufferAppend(&Session->Output, (xbytesview){(const unsigned char*)Text.Data, Text.Size});
}
XRT_API bool xrtConsoleSessionFlush(xconsolesession* Session)
{
    bool Ok;
    if (!__xrtSessionCheck(Session)) return false;
    Ok = xrtConsoleWrite(XCONSOLE_STDOUT, (xstrview){(const char*)Session->Output.Data, Session->Output.Size});
    /* A write error may be partial: discard the batch, never replay duplicated bytes. */
    xrtBufferClear(&Session->Output);
    return Ok && xrtConsoleFlush(XCONSOLE_STDOUT);
}
XRT_API bool xrtConsoleSessionMove(xconsolesession* Session, uint32 X, uint32 Y)
{
    char Text[64]; int Length;
    if (X == UINT32_MAX || Y == UINT32_MAX) return __xrtTerminalError(XERR_RANGE, "cursor position overflow");
    Length = snprintf(Text, sizeof(Text), "\x1b[%u;%uH", Y+1u, X+1u);
    return Length > 0 && xrtConsoleSessionWrite(Session, (xstrview){Text, (size_t)Length});
}
XRT_API bool xrtConsoleSessionClear(xconsolesession* Session, int Mode)
{
    if (Mode < 0 || Mode > 2) return __xrtTerminalError(XERR_RANGE, "invalid clear mode");
    return xrtConsoleSessionWrite(Session, Mode == 0 ? XRT_STR_LITERAL("\x1b[2J\x1b[H") :
        Mode == 1 ? XRT_STR_LITERAL("\x1b[2K") : XRT_STR_LITERAL("\x1b[J"));
}
XRT_API bool xrtConsoleSessionCursor(xconsolesession* Session, bool Visible)
{
    if (!__xrtSessionCheck(Session)) return false;
    if (!xrtConsoleSessionWrite(Session, Visible ? XRT_STR_LITERAL("\x1b[?25h") : XRT_STR_LITERAL("\x1b[?25l"))) return false;
    Session->CursorChanged = true; return true;
}
XRT_API bool xrtConsoleSessionStyle(xconsolesession* Session, int32 Fg, int32 Bg, uint32 Attributes)
{
    char Text[128];
    return __xrtTerminalStyle(Text, sizeof(Text), Fg, Bg, Attributes) && xrtConsoleSessionWrite(Session, (xstrview){Text, strlen(Text)});
}
XRT_API bool xrtConsoleSessionClose(xconsolesession* Session)
{
    bool Ok = true; xerror* Error = NULL;
    if (Session == NULL || !Session->Active) return true;
    if (!__xrtSessionCheck(Session)) return false;
    if (!xrtConsoleSessionFlush(Session)) { Ok = false; Error = xrtTakeError(); }
    /* Restoration always continues after any output failure. */
    if (!xrtConsoleWrite(XCONSOLE_STDOUT, XRT_STR_LITERAL("\x1b[0m"))) Ok = false;
#if !defined(_WIN32) && !defined(_WIN64)
    if ((Session->Flags & 2) && !xrtConsoleWrite(XCONSOLE_STDOUT, XRT_STR_LITERAL("\x1b[?1002l\x1b[?1006l"))) Ok = false;
    if ((Session->Flags & 4) && !xrtConsoleWrite(XCONSOLE_STDOUT, XRT_STR_LITERAL("\x1b[?2004l"))) Ok = false;
    if (Session->CursorChanged && !xrtConsoleWrite(XCONSOLE_STDOUT, XRT_STR_LITERAL("\x1b[?25h"))) Ok = false;
#endif
    if ((Session->Flags & 8) && !xrtConsoleWrite(XCONSOLE_STDOUT, XRT_STR_LITERAL("\x1b[?1049l"))) Ok = false;
    if (!xrtConsoleFlush(XCONSOLE_STDOUT)) Ok = false;
#if defined(_WIN32) || defined(_WIN64)
    if (!SetConsoleMode(Session->In, Session->InputMode)) { (void)__xrtTerminalSystem("restore-input"); Ok = false; }
    if (!SetConsoleCursorInfo(Session->Out, &Session->Cursor)) { (void)__xrtTerminalSystem("restore-cursor"); Ok = false; }
    if (!SetConsoleMode(Session->Out, Session->OutputMode)) { (void)__xrtTerminalSystem("restore-output"); Ok = false; }
#else
    if (tcsetattr(STDIN_FILENO, TCSANOW, &Session->InputMode) != 0) { (void)__xrtTerminalSystem("restore-input"); Ok = false; }
#endif
    Session->Active = false; __xrtStandardInputRelease(Session);
    if (Error != NULL) xrtSetErrorTake(Error);
    return Ok;
}
XRT_API void xrtConsoleSessionDestroy(xconsolesession* Session)
{
    if (Session == NULL) return;
    /* Destruction owns the last resource edge. Cleanup is not an event operation
     * and must restore process modes even if the final owner is another thread.
     * Concurrent operations/destruction on a C session remain forbidden. */
    Session->Thread = xrtThreadCurrentId();
    (void)xrtConsoleSessionClose(Session); xrtBufferUnit(&Session->Input); xrtBufferUnit(&Session->Output); xrtFree(Session);
}
XRT_API void xrtConsoleEventDestroy(xconsoleevent* Event)
{ if (Event != NULL) { xrtBufferDestroy(Event->Text); xrtFree(Event); } }
static xconsoleevent* __xrtConsoleEvent(xconsoleeventkind Kind)
{
    xconsoleevent* Event = (xconsoleevent*)xrtCalloc(1, sizeof(*Event));
    if (Event != NULL) { Event->Kind = Kind; Event->Repeat = 1; Event->Down = true; }
    return Event;
}
static bool __xrtConsoleEventText(xconsoleevent* Event, const void* Text, size_t Size)
{
    if (!__xrtConsoleValidateText((xstrview){(const char*)Text, Size})) return false;
    Event->Text = xrtBufferFrom((xbytesview){(const unsigned char*)Text, Size});
    return Event->Text != NULL;
}

#if defined(_WIN32) || defined(_WIN64)
static uint32 __xrtConsoleWindowsKey(WORD Key)
{
    switch (Key) {
    case VK_ESCAPE: return XCONSOLE_KEY_ESCAPE; case VK_RETURN: return XCONSOLE_KEY_ENTER;
    case VK_TAB: return XCONSOLE_KEY_TAB; case VK_BACK: return XCONSOLE_KEY_BACKSPACE;
    case VK_UP: return XCONSOLE_KEY_UP; case VK_DOWN: return XCONSOLE_KEY_DOWN;
    case VK_LEFT: return XCONSOLE_KEY_LEFT; case VK_RIGHT: return XCONSOLE_KEY_RIGHT;
    case VK_HOME: return XCONSOLE_KEY_HOME; case VK_END: return XCONSOLE_KEY_END;
    case VK_INSERT: return XCONSOLE_KEY_INSERT; case VK_DELETE: return XCONSOLE_KEY_DELETE;
    case VK_PRIOR: return XCONSOLE_KEY_PAGE_UP; case VK_NEXT: return XCONSOLE_KEY_PAGE_DOWN;
    default: return Key >= VK_F1 && Key <= VK_F24 ? XCONSOLE_KEY_F1 + Key - VK_F1 : Key;
    }
}
static uint32 __xrtConsoleWindowsModifiers(DWORD State)
{ return (State & SHIFT_PRESSED ? 1u:0u) | (State & (LEFT_CTRL_PRESSED|RIGHT_CTRL_PRESSED) ? 2u:0u) |
    (State & (LEFT_ALT_PRESSED|RIGHT_ALT_PRESSED) ? 4u:0u); }
XRT_API xconsoleevent* xrtConsoleSessionRead(xconsolesession* Session, int TimeoutMs)
{
    double Deadline; INPUT_RECORD Record; DWORD Done, Wait; xconsoleevent* Event;
    if (!__xrtSessionCheck(Session)) return NULL;
    if (TimeoutMs < -1) { (void)__xrtTerminalError(XERR_RANGE, "invalid event timeout"); return NULL; }
    Deadline = TimeoutMs >= 0 ? __xrtTerminalNow() + (double)TimeoutMs / 1000.0 : 0;
    for (;;) {
        double Now = __xrtTerminalNow();
        if (!isfinite(Now)) return NULL;
        DWORD Remaining = TimeoutMs < 0 ? INFINITE : Now >= Deadline ? 0 : (DWORD)ceil((Deadline-Now)*1000.0);
        Wait = WaitForSingleObject(Session->In, Remaining);
        if (Wait == WAIT_TIMEOUT) return NULL;
        if (Wait != WAIT_OBJECT_0 || !ReadConsoleInputW(Session->In, &Record, 1, &Done)) {
            (void)__xrtTerminalSystem("read-event"); return NULL; }
        if (Done == 0) return NULL;
        if (Record.EventType == WINDOW_BUFFER_SIZE_EVENT) {
            uint32 Columns, Rows;
            if (!xrtConsoleSize(XCONSOLE_STDOUT, &Columns, &Rows)) return NULL;
            if (Columns == Session->Columns && Rows == Session->Rows) continue;
            Event = __xrtConsoleEvent(XCONSOLE_EVENT_RESIZE); if (Event == NULL) return NULL;
            Session->Columns = Event->Columns = Columns; Session->Rows = Event->Rows = Rows; return Event;
        }
        if (Record.EventType == MOUSE_EVENT && (Session->Flags & 2)) {
            MOUSE_EVENT_RECORD* Mouse = &Record.Event.MouseEvent;
            CONSOLE_SCREEN_BUFFER_INFO Info;
            Event = __xrtConsoleEvent(XCONSOLE_EVENT_MOUSE); if (Event == NULL) return NULL;
            Event->X = Mouse->dwMousePosition.X; Event->Y = Mouse->dwMousePosition.Y;
            if (GetConsoleScreenBufferInfo(Session->Out, &Info)) {
                Event->X -= Info.srWindow.Left; Event->Y -= Info.srWindow.Top;
            }
            Event->Key = Mouse->dwButtonState & 0xffffu; Event->Down = Event->Key != 0;
            Event->Modifiers = __xrtConsoleWindowsModifiers(Mouse->dwControlKeyState);
            if (Mouse->dwEventFlags & MOUSE_WHEELED) Event->Wheel = (int16)HIWORD(Mouse->dwButtonState) / WHEEL_DELTA;
            return Event;
        }
        if (Record.EventType == KEY_EVENT) {
            KEY_EVENT_RECORD* Key = &Record.Event.KeyEvent; uint32 Scalar = Key->uChar.UnicodeChar;
            uint32 Modifiers = __xrtConsoleWindowsModifiers(Key->dwControlKeyState);
            unsigned char Text[4]; size_t Length;
            /* UTF-16 key-up records are not standalone Unicode scalars. */
            if (!Key->bKeyDown && Scalar >= 0xd800 && Scalar <= 0xdfff) continue;
            if (Key->bKeyDown && Scalar >= 0xd800 && Scalar <= 0xdbff) { Session->HighSurrogate = (WCHAR)Scalar; continue; }
            if (Key->bKeyDown && Scalar >= 0xdc00 && Scalar <= 0xdfff) {
                if (Session->HighSurrogate == 0) { (void)__xrtTerminalError(XERR_IO, "unpaired input surrogate"); return NULL; }
                Scalar = 0x10000u + (((uint32)Session->HighSurrogate-0xd800u)<<10) + Scalar-0xdc00u;
                Session->HighSurrogate = 0;
            } else if (Key->bKeyDown && Session->HighSurrogate != 0) {
                Session->HighSurrogate = 0; (void)__xrtTerminalError(XERR_IO, "incomplete input surrogate"); return NULL;
            }
            if (Key->wVirtualKeyCode == VK_SHIFT || Key->wVirtualKeyCode == VK_CONTROL || Key->wVirtualKeyCode == VK_MENU) continue;
            /* AltGr often sets both Ctrl and Alt but still produces printable
             * Unicode. Use the delivered scalar, not modifiers, to classify text. */
            Event = __xrtConsoleEvent(Key->bKeyDown && Scalar >= 32 ? XCONSOLE_EVENT_TEXT : XCONSOLE_EVENT_KEY);
            if (Event == NULL) return NULL;
            Event->Key = Event->Kind == XCONSOLE_EVENT_TEXT ? Scalar : __xrtConsoleWindowsKey(Key->wVirtualKeyCode);
            Event->Down = Key->bKeyDown != 0; Event->Modifiers = Modifiers; Event->Repeat = Key->wRepeatCount ? Key->wRepeatCount : 1;
            if (Event->Kind != XCONSOLE_EVENT_TEXT) return Event;
            if (Scalar < 0x80) { Text[0] = (unsigned char)Scalar; Length = 1; }
            else if (Scalar < 0x800) { Text[0] = (unsigned char)(0xc0|(Scalar>>6)); Text[1] = (unsigned char)(0x80|(Scalar&63)); Length = 2; }
            else if (Scalar < 0x10000) { Text[0] = (unsigned char)(0xe0|(Scalar>>12)); Text[1] = (unsigned char)(0x80|((Scalar>>6)&63)); Text[2] = (unsigned char)(0x80|(Scalar&63)); Length = 3; }
            else { Text[0] = (unsigned char)(0xf0|(Scalar>>18)); Text[1] = (unsigned char)(0x80|((Scalar>>12)&63)); Text[2] = (unsigned char)(0x80|((Scalar>>6)&63)); Text[3] = (unsigned char)(0x80|(Scalar&63)); Length = 4; }
            if (__xrtConsoleEventText(Event, Text, Length)) return Event;
            xrtConsoleEventDestroy(Event); return NULL;
        }
        if (TimeoutMs >= 0 && __xrtTerminalNow() >= Deadline) return NULL;
    }
}
#else
static void __xrtConsoleConsume(xconsolesession* Session, size_t Count)
{ memmove(Session->Input.Data, Session->Input.Data+Count, Session->Input.Size-Count); (void)xrtBufferResize(&Session->Input, Session->Input.Size-Count); Session->EscapeSince = 0; }
/* Never pass terminal-controlled numbers to scanf: overflowing numeric input
 * must not invoke undefined conversion behavior. Accept only complete fields. */
static bool __xrtConsoleNumbers(const unsigned char* Data, size_t Size, uint32* Values, size_t Count)
{
    size_t Offset = 0;
    for (size_t Field = 0; Field < Count; ++Field) {
        uint32 Value = 0; size_t Begin = Offset;
        while (Offset < Size && Data[Offset] >= '0' && Data[Offset] <= '9') {
            unsigned Digit = Data[Offset++] - '0';
            if (Value > (UINT32_MAX - Digit) / 10u) return false;
            Value = Value * 10u + Digit;
        }
        if (Offset == Begin) return false;
        Values[Field] = Value;
        if (Field + 1 < Count && (Offset >= Size || Data[Offset++] != ';')) return false;
    }
    return Offset == Size;
}
static uint32 __xrtConsoleScalar(const unsigned char* Data, size_t Count)
{
    uint32 Scalar = Data[0] & (Count == 1 ? 127u : Count == 2 ? 31u : Count == 3 ? 15u : 7u);
    for (size_t i = 1; i < Count; ++i) Scalar = (Scalar << 6) | (Data[i] & 63u);
    return Scalar;
}
/* Parse only complete bounded events; an incomplete sequence remains owned by the session. */
static xconsoleevent* __xrtConsoleParseEvent(xconsolesession* Session)
{
    const unsigned char* Data = Session->Input.Data; size_t Size = Session->Input.Size, Count=1;
    xconsoleevent* Event; uint32 Key=0, Modifiers=0; bool Text=false; size_t TextOffset=0;
    if (Size == 0) return NULL;
    if (Data[0] == 0x1b) {
        if (Session->EscapeSince == 0) Session->EscapeSince = __xrtTerminalNow();
        if (Size >= 6 && memcmp(Data,"\x1b[200~",6)==0 && (Session->Flags & 4)) {
            size_t End;
            for (End=6; End+6<=Size; ++End) if (memcmp(Data+End,"\x1b[201~",6)==0) break;
            if (End+6>Size) return NULL;
            if (End-6>XRT_CONSOLE_EVENT_LIMIT) { (void)__xrtTerminalError(XERR_RANGE,"paste limit exceeded"); return NULL; }
            Event=__xrtConsoleEvent(XCONSOLE_EVENT_PASTE); if (Event==NULL) return NULL;
            if (!__xrtConsoleEventText(Event,Data+6,End-6)) { xrtConsoleEventDestroy(Event); return NULL; }
            __xrtConsoleConsume(Session,End+6); return Event;
        }
        if (Size >= 3 && (Data[1]=='[' || Data[1]=='O')) {
            size_t End=2;
            while (End<Size && End<64 && !(Data[End]>=0x40 && Data[End]<=0x7e)) ++End;
            if (End<Size && End<64) {
                uint32 Values[3]={0,0,0}, First=0, Second=0, Third=0;
                size_t Length=End-2; bool Valid;
                if (Length!=0 && Data[2]=='<' && (Session->Flags & 2) && (Data[End]=='M'||Data[End]=='m') &&
                    __xrtConsoleNumbers(Data+3,Length-1,Values,3) &&
                    (First=Values[0],Second=Values[1],Third=Values[2],Second!=0 && Third!=0 && Second<=INT32_MAX && Third<=INT32_MAX)) {
                    Event=__xrtConsoleEvent(XCONSOLE_EVENT_MOUSE); if (Event==NULL) return NULL;
                    Event->X=(int32)Second-1; Event->Y=(int32)Third-1;
                    Event->Key=Data[End]=='m' || (First&64u) || (First&3u)==3 ? 0u :
                        (First&3u)==0 ? 1u : (First&3u)==1 ? 4u : 2u;
                    Event->Modifiers=(First&4?1u:0u)|(First&8?4u:0u)|(First&16?2u:0u);
                    Event->Down=Data[End]=='M' && Event->Key!=0; Event->Wheel=First&64 ? (First&1?-1:1) : 0;
                    __xrtConsoleConsume(Session,End+1); return Event;
                }
                Valid=Length==0 || __xrtConsoleNumbers(Data+2,Length,Values,1) || __xrtConsoleNumbers(Data+2,Length,Values,2);
                First=Values[0]; Second=Values[1];
                if (Second>16) Valid=false;
                if (Second>=2 && Second<=16) {
                    uint32 Bits=Second-1;
                    Modifiers=(Bits&1u)|(Bits&2u?4u:0u)|(Bits&4u?2u:0u)|(Bits&8u);
                }
                if (Valid) switch (Data[End]) {
                case 'A':Key=XCONSOLE_KEY_UP;break;case 'B':Key=XCONSOLE_KEY_DOWN;break;
                case 'C':Key=XCONSOLE_KEY_RIGHT;break;case 'D':Key=XCONSOLE_KEY_LEFT;break;
                case 'H':Key=XCONSOLE_KEY_HOME;break;case 'F':Key=XCONSOLE_KEY_END;break;
                case 'P':Key=XCONSOLE_KEY_F1;break;case 'Q':Key=XCONSOLE_KEY_F1+1;break;
                case 'R':Key=XCONSOLE_KEY_F1+2;break;case 'S':Key=XCONSOLE_KEY_F1+3;break;
                case 'Z':Key=XCONSOLE_KEY_TAB;Modifiers=1;break;
                case '~': switch(First) {
                    case 1:case 7:Key=XCONSOLE_KEY_HOME;break;case 4:case 8:Key=XCONSOLE_KEY_END;break;
                    case 2:Key=XCONSOLE_KEY_INSERT;break;case 3:Key=XCONSOLE_KEY_DELETE;break;
                    case 5:Key=XCONSOLE_KEY_PAGE_UP;break;case 6:Key=XCONSOLE_KEY_PAGE_DOWN;break;
                    default: { const unsigned Codes[]={11,12,13,14,15,17,18,19,20,21,23,24};
                      for (unsigned i=0;i<12;++i) if (Codes[i]==First) Key=XCONSOLE_KEY_F1+i; } } break;
                default: break;
                }
                if (Key!=0) Count=End+1;
            } else if (Size<64 && __xrtTerminalNow()-Session->EscapeSince<0.030) return NULL;
        } else if (Size>=2 && Data[1]!='[' && Data[1]!='O' && Data[1]!=0x1b) {
            size_t Bytes=Data[1]<0x80?1:Data[1]>=0xc2&&Data[1]<=0xdf?2:Data[1]>=0xe0&&Data[1]<=0xef?3:Data[1]>=0xf0&&Data[1]<=0xf4?4:0;
            if (Bytes==0) { (void)__xrtTerminalError(XERR_IO,"invalid Alt event UTF-8"); return NULL; }
            if (Size<Bytes+1) return NULL;
            TextOffset=1; Count=Bytes+1; Text=true; Modifiers=4;
        } else if ((Size==1 || (Size==2 && (Data[1]=='[' || Data[1]=='O'))) &&
            __xrtTerminalNow()-Session->EscapeSince<0.030 && !Session->InputEnded) return NULL;
        if (Key==0 && !Text) Key=XCONSOLE_KEY_ESCAPE;
    } else if (Data[0]<32 || Data[0]==127) {
        Key=Data[0]==13||Data[0]==10?XCONSOLE_KEY_ENTER:Data[0]==9?XCONSOLE_KEY_TAB:
            Data[0]==127||Data[0]==8?XCONSOLE_KEY_BACKSPACE:Data[0]+64;
        if (Data[0]<32 && Data[0]!=13 && Data[0]!=10 && Data[0]!=9 && Data[0]!=8) Modifiers=2;
    } else {
        Count=Data[0]<0x80?1:Data[0]>=0xc2&&Data[0]<=0xdf?2:Data[0]>=0xe0&&Data[0]<=0xef?3:Data[0]>=0xf0&&Data[0]<=0xf4?4:0;
        if (Count==0) { (void)__xrtTerminalError(XERR_IO,"invalid event UTF-8"); return NULL; }
        if (Size<Count) return NULL;
        Text=true;
    }
    Event=__xrtConsoleEvent(Text?XCONSOLE_EVENT_TEXT:XCONSOLE_EVENT_KEY); if (Event==NULL) return NULL;
    Event->Key=Key; Event->Modifiers=Modifiers;
    if (Text) {
        if (!__xrtConsoleEventText(Event,Data+TextOffset,Count-TextOffset)) { xrtConsoleEventDestroy(Event); return NULL; }
        Event->Key=__xrtConsoleScalar(Data+TextOffset,Count-TextOffset);
    }
    __xrtConsoleConsume(Session,Count); return Event;
}
XRT_API xconsoleevent* xrtConsoleSessionRead(xconsolesession* Session, int TimeoutMs)
{
    double Deadline; bool First=true;
    if (!__xrtSessionCheck(Session)) return NULL;
    if (TimeoutMs < -1) { (void)__xrtTerminalError(XERR_RANGE,"invalid event timeout"); return NULL; }
    Deadline=TimeoutMs>=0?__xrtTerminalNow()+(double)TimeoutMs/1000.0:0;
    for (;;) {
        struct pollfd Poll={STDIN_FILENO,POLLIN,0}; uint32 Columns,Rows; xconsoleevent* Event;
        unsigned char Data[256]; ssize_t Read; int Status,Wait=50; double Now=__xrtTerminalNow();
        if (!isfinite(Now)) return NULL;
        if (!xrtConsoleSize(XCONSOLE_STDOUT,&Columns,&Rows)) return NULL;
        if (Columns!=Session->Columns || Rows!=Session->Rows) {
            Event=__xrtConsoleEvent(XCONSOLE_EVENT_RESIZE); if (Event==NULL) return NULL;
            Session->Columns=Event->Columns=Columns; Session->Rows=Event->Rows=Rows; return Event;
        }
        Event=__xrtConsoleParseEvent(Session);
        if (Event!=NULL || xrtGetError()!=NULL) return Event;
        if (Session->InputEnded) {
            if (Session->Input.Size!=0) { (void)__xrtTerminalError(XERR_IO,"incomplete event at EOF"); return NULL; }
            if (Session->ClosedEvent) return NULL;
            Event=__xrtConsoleEvent(XCONSOLE_EVENT_CLOSED); if (Event!=NULL) Session->ClosedEvent=true; return Event;
        }
        if (!First && TimeoutMs>=0 && Now>=Deadline) return NULL;
        if (TimeoutMs>=0) { double Remaining=ceil((Deadline>Now?Deadline-Now:0)*1000.0); if (Wait>Remaining) Wait=(int)Remaining; }
        if (Session->EscapeSince!=0 && Wait>10) Wait=10;
        First=false;
        Status=poll(&Poll,1,Wait);
        if (Status<0 && errno==EINTR) continue;
        if (Status<0 || (Poll.revents&(POLLERR|POLLNVAL))) { (void)__xrtTerminalSystem("poll"); return NULL; }
        if (Status==0) continue;
        do { Read=read(STDIN_FILENO,Data,sizeof(Data)); } while (Read<0 && errno==EINTR);
        if (Read<0) { (void)__xrtTerminalSystem("read-event"); return NULL; }
        if (Read==0) { Session->InputEnded=true; continue; }
        if (Session->Input.Size>XRT_CONSOLE_EVENT_LIMIT+12u-(size_t)Read) {
            (void)__xrtTerminalError(XERR_RANGE,"event input limit exceeded"); return NULL; }
        if (!xrtBufferAppend(&Session->Input,(xbytesview){Data,(size_t)Read})) return NULL;
    }
}
#endif
#endif
