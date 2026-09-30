#include <xrt/console.h>
#include "../test_console_redirect.h"
#include <assert.h>

int main(void)
{
    testconsoleredirect Redirect;
    xbuffer* Line;
    uint32 Codepoint;
    const char Input[] = "\n" "a\0b\r\n" "\xe4\xbd\xa0\xf0\x9f\x98\x80";
    (void)testConsoleRedirectRead;
    testConsoleRedirectBegin(&Redirect, stdin);
    assert(fwrite(Input, 1, sizeof(Input)-1, Redirect.File) == sizeof(Input)-1);
    assert(fflush(Redirect.File) == 0 && fseek(Redirect.File, 0, SEEK_SET) == 0);
    Line = xrtConsoleReadLine(0);
    assert(Line != NULL && xrtBufferView(Line).Size == 0);
    xrtBufferDestroy(Line);
    Line = xrtConsoleReadLine(3);
    assert(Line != NULL && xrtBufferView(Line).Size == 3);
    assert(memcmp(xrtBufferView(Line).Data, "a\0b", 3) == 0);
    xrtBufferDestroy(Line);
    assert(xrtConsoleReadChar(&Codepoint) == 1 && Codepoint == 0x4f60);
    assert(xrtConsoleReadChar(&Codepoint) == 1 && Codepoint == 0x1f600);
    assert(xrtConsoleReadChar(&Codepoint) == 0);
    assert(xrtConsoleReadLine(8) == NULL && xrtGetError() == NULL);
    testConsoleRedirectEnd(&Redirect);
    testConsoleRedirectUnit(&Redirect);
    testConsoleRedirectBegin(&Redirect, stdin);
    assert(fwrite("\xc0\xaf", 1, 2, Redirect.File) == 2);
    assert(fflush(Redirect.File) == 0 && fseek(Redirect.File, 0, SEEK_SET) == 0);
    assert(xrtConsoleReadChar(&Codepoint) == -1 && xrtGetError() != NULL);
    xrtClearError();
    testConsoleRedirectEnd(&Redirect);
    testConsoleRedirectUnit(&Redirect);
    return 0;
}
