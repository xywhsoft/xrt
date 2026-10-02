/* The first failing allocation must preserve its category through process
 * creation, worker setup, stopping and joining, and release every owned item. */
#define XRT_MODULE_PROCESS_RUN
#define XRT_MODULE_MEMORY_DEBUG
#define XRT_IMPLEMENTATION
#include "../../single/xrt.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#if defined(_WIN32) || defined(_WIN64)
#include <fcntl.h>
#include <io.h>
#endif

int main(int argc, char** argv)
{
    if (argc == 2 && !strcmp(argv[1], "--echo")) {
#if defined(_WIN32) || defined(_WIN64)
        _setmode(_fileno(stdin), _O_BINARY);
        _setmode(_fileno(stdout), _O_BINARY);
#endif
        unsigned char data[256]; size_t size;
        while ((size = fread(data, 1, sizeof(data), stdin)) != 0)
            if (fwrite(data, 1, size, stdout) != size) return 1;
        return ferror(stdin) ? 1 : 0;
    }
    const cstr args[] = {"--echo"};
    const xprocessenv env[] = {{"XRT_PROCESS_PREFIX", "value"}};
    const unsigned char raw[] = {'a', 0, 'b'};
    xprocessconfig config;
    xprocessrunoptions options;
    assert(xrtProcessConfigInit(&config));
    assert(xrtProcessRunOptionsInit(&options));
    config.Program = argv[0]; config.Args = args; config.ArgCount = 1;
    config.Env = env; config.EnvCount = 1; config.WorkDir = ".";
    config.Stdin.Mode = XPROCESS_IO_PIPE;
    options.Input = (xbytesview){raw, sizeof(raw)};
    options.StopGrace = 10000;
    assert(xrtMemDebugEnable(true));
    bool complete = false; unsigned failures = 0;
    for (size_t point = 0; point < 512; ++point) {
        xmemdebugsnapshot before, after; xprocessresult result;
        xrtMemDebugSnapshot(&before); assert(xrtMemDebugFailAfter(point));
        bool ok = xrtProcessRun(&config, &options, &result);
        bool hit = xrtMemDebugFailTriggered(); xrtMemDebugFailClear();
        if (hit && (ok || xrtErrorKind(xrtGetError()) != XERR_MEMORY))
            fprintf(stderr, "point=%zu ok=%d kind=%d message=%s\n", point, ok,
                (int)xrtErrorKind(xrtGetError()), xrtGetError() ? xrtErrorMessage(xrtGetError()) : "none");
        if (hit) { assert(!ok && xrtErrorKind(xrtGetError()) == XERR_MEMORY); ++failures; }
        else {
            assert(ok && xrtProcessResultSuccess(&result) && !xrtGetError());
            assert(result.StdoutSize == sizeof(raw) && !memcmp(result.Stdout, raw, sizeof(raw)));
        }
        xrtProcessResultUnit(&result); xrtClearError(); xrtMemDebugSnapshot(&after);
        assert(before.LiveCount == after.LiveCount && before.LiveBytes == after.LiveBytes);
        assert(after.AllocCount - before.AllocCount == after.FreeCount - before.FreeCount);
        assert(before.InvalidFreeCount == after.InvalidFreeCount);
        assert(before.DoubleFreeCount == after.DoubleFreeCount);
        if (!hit) { complete = true; break; }
    }
    assert(complete);
    printf("process run prefix: %u allocation failures and 1 completion, balanced\n", failures);
    return 0;
}
