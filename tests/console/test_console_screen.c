#include <xrt/console.h>
#include "../test_console_redirect.h"
#include <assert.h>

int main(void)
{
    testconsoleredirect Redirect;
    uint32 Columns = 99, Rows = 99;
    char Output[64];
    testConsoleRedirectBegin(&Redirect, stdout);
    assert(xrtConsoleSize(XCONSOLE_STDOUT, &Columns, &Rows) && Columns == 0 && Rows == 0);
    assert(xrtConsoleColorMode(XCONSOLE_STDOUT) == 0);
    assert(xrtConsoleWriteStyled(XCONSOLE_STDOUT, XRT_STR_LITERAL("a\0b"), 1, -1, 1));
    assert(!xrtConsoleWriteStyled(XCONSOLE_STDOUT, XRT_STR_LITERAL("bad"), 256, -1, 0));
    xrtClearError();
    testConsoleRedirectEnd(&Redirect);
    assert(testConsoleRedirectRead(&Redirect, Output, sizeof(Output)) == 3);
    assert(memcmp(Output, "a\0b", 3) == 0);
    testConsoleRedirectUnit(&Redirect);
    return 0;
}
