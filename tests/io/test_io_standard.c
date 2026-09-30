#include <xrt/io.h>
#include "../test_console_redirect.h"
#include <assert.h>
#include <string.h>

int main(void)
{
    testconsoleredirect Redirect;
    unsigned char Data[] = {'a', 0, 0xff, '\n'};
    char Actual[32];
    size_t Count = 0;
    xwriter* Writer;
    xreader* Reader = xrtReaderStdin();
    assert(Reader != NULL && !xrtReaderCanSeek(Reader));
    assert(xrtReaderStdin() == NULL && xrtErrorKind(xrtGetError()) == XERR_STATE);
    xrtClearError();
    assert(xrtReaderDestroy(Reader)); /* Never closes the process stream. */
    testConsoleRedirectBegin(&Redirect, stdout);
    Writer = xrtWriterStdout();
    assert(Writer != NULL && !xrtWriterCanSeek(Writer));
    assert(xrtWriterWriteFull(Writer, Data, sizeof(Data), &Count));
    assert(Count == sizeof(Data) && xrtWriterFlush(Writer));
    assert(xrtWriterDestroy(Writer));
    assert(fwrite("Z", 1, 1, stdout) == 1);
    testConsoleRedirectEnd(&Redirect);
    assert(testConsoleRedirectRead(&Redirect, Actual, sizeof(Actual)) == 5);
    assert(memcmp(Actual, Data, sizeof(Data)) == 0 && Actual[4] == 'Z');
    testConsoleRedirectUnit(&Redirect);
    testConsoleRedirectBegin(&Redirect, stderr);
    Writer = xrtWriterStderr();
    assert(Writer != NULL && xrtWriterWriteFull(Writer, Data, sizeof(Data), NULL));
    assert(xrtWriterDestroy(Writer));
    testConsoleRedirectEnd(&Redirect);
    assert(testConsoleRedirectRead(&Redirect, Actual, sizeof(Actual)) == sizeof(Data));
    assert(memcmp(Actual, Data, sizeof(Data)) == 0);
    testConsoleRedirectUnit(&Redirect);
    return 0;
}
