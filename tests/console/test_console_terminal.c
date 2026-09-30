#include <xrt/console.h>
#include "../test_console_redirect.h"
#include <assert.h>

int main(void)
{
    testconsoleredirect Redirect;
    uint32 Columns=99, Rows=99;
    char Output[64];
    xconsolesession* Session;
    testConsoleRedirectBegin(&Redirect, stdout);
    assert(!xrtConsoleIsTerminal(XCONSOLE_STDOUT));
    assert(xrtConsoleSize(XCONSOLE_STDOUT,&Columns,&Rows) && Columns==0 && Rows==0);
    assert(xrtConsoleColorMode(XCONSOLE_STDOUT)==0);
    assert(xrtConsoleWriteStyled(XCONSOLE_STDOUT,XRT_STR_LITERAL("a\0b"),1,-1,1));
    assert(!xrtConsoleWriteStyled(XCONSOLE_STDOUT,XRT_STR_LITERAL("must not write"),256,-1,0));
    xrtClearError();
    Session=xrtConsoleSessionOpen(1);
    assert(Session==NULL && xrtGetError()!=NULL);
    xrtClearError();
    /* Failure must release the input lease so another open is not misreported as busy. */
    Session=xrtConsoleSessionOpen(1);
    assert(Session==NULL && xrtErrorKind(xrtGetError())==XERR_UNSUPPORTED);
    xrtClearError();
    testConsoleRedirectEnd(&Redirect);
    assert(testConsoleRedirectRead(&Redirect,Output,sizeof(Output))==3);
    assert(memcmp(Output,"a\0b",3)==0);
    testConsoleRedirectUnit(&Redirect);
    assert(xrtConsoleSessionClose(NULL));
    xrtConsoleSessionDestroy(NULL); xrtConsoleEventDestroy(NULL);
    return 0;
}
