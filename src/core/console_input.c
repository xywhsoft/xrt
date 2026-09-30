#include "../internal/xrt_console.h"
#include "../internal/xrt_io.h"
#include <errno.h>
#if !defined(_WIN32) && !defined(_WIN64)
#include <unistd.h>
#endif

#if defined(XRT_FEATURE_CONSOLE_INPUT)

static int __xrtConsoleInputError(xerrkind Kind, int Code, cstr Message)
{
    __xrtErrorSetDetail(Kind, "xrt.console", Code, "read", Message, NULL);
    return -1;
}

static int __xrtConsoleInputByte(unsigned char* Byte)
{
#if defined(_WIN32) || defined(_WIN64)
    DWORD Done;
    if (!ReadFile(GetStdHandle(STD_INPUT_HANDLE), Byte, 1, &Done, NULL)) {
        DWORD Code = GetLastError();
        if (Code == ERROR_BROKEN_PIPE || Code == ERROR_HANDLE_EOF) return 0;
        __xrtErrorSetSystem("xrt.console", XCONSOLE_ERROR_READ, "read", (int)Code, "stdin read failed");
        return -1;
    }
    return Done ? 1 : 0;
#else
    ssize_t Done;
    do { Done = read(STDIN_FILENO, Byte, 1); } while (Done < 0 && errno == EINTR);
    if (Done >= 0) return Done ? 1 : 0;
    __xrtErrorSetSystem("xrt.console", XCONSOLE_ERROR_READ, "read", errno, "stdin read failed");
    return -1;
#endif
}

static int __xrtConsoleInputScalar(uint32* Codepoint)
{
    unsigned char Byte;
    uint32 Value, Minimum;
    unsigned Remaining;
    int Status;
#if defined(_WIN32) || defined(_WIN64)
    HANDLE Input = GetStdHandle(STD_INPUT_HANDLE);
    DWORD Mode, Done;
    WCHAR Unit, Low;
    if (GetConsoleMode(Input, &Mode)) {
        if (!ReadConsoleW(Input, &Unit, 1, &Done, NULL)) {
            __xrtErrorSetSystem("xrt.console", XCONSOLE_ERROR_READ, "read", (int)GetLastError(), "console read failed");
            return -1;
        }
        if (!Done) return 0;
        Value = (uint32)Unit;
        if (Value >= 0xd800 && Value <= 0xdbff) {
            if (!ReadConsoleW(Input, &Low, 1, &Done, NULL) || Done != 1 || Low < 0xdc00 || Low > 0xdfff)
                return __xrtConsoleInputError(XERR_IO, XCONSOLE_ERROR_UTF8, "invalid UTF-16 input");
            Value = 0x10000 + ((Value - 0xd800) << 10) + ((uint32)Low - 0xdc00);
        } else if (Value >= 0xdc00 && Value <= 0xdfff)
            return __xrtConsoleInputError(XERR_IO, XCONSOLE_ERROR_UTF8, "unpaired UTF-16 surrogate");
        *Codepoint = Value;
        return 1;
    }
#endif
    Status = __xrtConsoleInputByte(&Byte);
    if (Status <= 0) return Status;
    if (Byte < 0x80) { *Codepoint = Byte; return 1; }
    if (Byte >= 0xc2 && Byte <= 0xdf) { Value = Byte & 31; Remaining = 1; Minimum = 0x80; }
    else if (Byte >= 0xe0 && Byte <= 0xef) { Value = Byte & 15; Remaining = 2; Minimum = 0x800; }
    else if (Byte >= 0xf0 && Byte <= 0xf4) { Value = Byte & 7; Remaining = 3; Minimum = 0x10000; }
    else return __xrtConsoleInputError(XERR_IO, XCONSOLE_ERROR_UTF8, "invalid UTF-8 input");
    while (Remaining--) {
        Status = __xrtConsoleInputByte(&Byte);
        if (Status < 0) return -1;
        if (Status == 0 || (Byte & 0xc0) != 0x80)
            return __xrtConsoleInputError(XERR_IO, XCONSOLE_ERROR_UTF8, "truncated or invalid UTF-8 input");
        Value = (Value << 6) | (Byte & 63);
    }
    if (Value < Minimum || Value > 0x10ffff || (Value >= 0xd800 && Value <= 0xdfff))
        return __xrtConsoleInputError(XERR_IO, XCONSOLE_ERROR_UTF8, "invalid Unicode scalar input");
    *Codepoint = Value;
    return 1;
}

XRT_API int xrtConsoleReadChar(uint32* Codepoint)
{
    int Token, Status;
    if (Codepoint == NULL) return __xrtConsoleInputError(XERR_ARGUMENT, XCONSOLE_ERROR_READ, "null codepoint output");
    if (!__xrtStandardInputAcquire(&Token)) return -1;
    Status = __xrtConsoleInputScalar(Codepoint);
    __xrtStandardInputRelease(&Token);
    return Status;
}

XRT_API xbuffer* xrtConsoleReadLine(size_t MaxBytes)
{
    int Token, Status;
    xbuffer* Buffer = NULL;
    uint32 Codepoint;
    size_t Count;
    unsigned char Encoded[4];
    bool PendingCR = false;
    if (!__xrtStandardInputAcquire(&Token)) return NULL;
    for (;;) {
        Status = __xrtConsoleInputScalar(&Codepoint);
        if (Status < 0) goto fail;
        if (Status == 0) break;
        if (Buffer == NULL && (Buffer = xrtBufferCreate()) == NULL) goto fail;
        if (Codepoint == '\n') { PendingCR = false; break; }
        if (PendingCR) {
            if (xrtBufferView(Buffer).Size >= MaxBytes) {
                (void)__xrtConsoleInputError(XERR_RANGE, XCONSOLE_ERROR_LIMIT, "console line limit exceeded"); goto fail;
            }
            if (!xrtBufferAppendByte(Buffer, '\r')) goto fail;
            PendingCR = false;
        }
        if (Codepoint == '\r') { PendingCR = true; continue; }
        if (Codepoint < 0x80) { Encoded[0] = (unsigned char)Codepoint; Count = 1; }
        else if (Codepoint < 0x800) { Encoded[0] = (unsigned char)(0xc0 | (Codepoint >> 6)); Encoded[1] = (unsigned char)(0x80 | (Codepoint & 63)); Count = 2; }
        else if (Codepoint < 0x10000) { Encoded[0] = (unsigned char)(0xe0 | (Codepoint >> 12)); Encoded[1] = (unsigned char)(0x80 | ((Codepoint >> 6) & 63)); Encoded[2] = (unsigned char)(0x80 | (Codepoint & 63)); Count = 3; }
        else { Encoded[0] = (unsigned char)(0xf0 | (Codepoint >> 18)); Encoded[1] = (unsigned char)(0x80 | ((Codepoint >> 12) & 63)); Encoded[2] = (unsigned char)(0x80 | ((Codepoint >> 6) & 63)); Encoded[3] = (unsigned char)(0x80 | (Codepoint & 63)); Count = 4; }
        if (Count > MaxBytes || xrtBufferView(Buffer).Size > MaxBytes - Count) {
            (void)__xrtConsoleInputError(XERR_RANGE, XCONSOLE_ERROR_LIMIT, "console line limit exceeded");
            goto fail;
        }
        if (!xrtBufferAppend(Buffer, (xbytesview){Encoded, Count})) goto fail;
    }
    if (PendingCR) {
        if (xrtBufferView(Buffer).Size >= MaxBytes) {
            (void)__xrtConsoleInputError(XERR_RANGE, XCONSOLE_ERROR_LIMIT, "console line limit exceeded"); goto fail;
        }
        if (!xrtBufferAppendByte(Buffer, '\r')) goto fail;
    }
    __xrtStandardInputRelease(&Token);
    return Buffer;
fail:
    xrtBufferDestroy(Buffer);
    __xrtStandardInputRelease(&Token);
    return NULL;
}
#endif
