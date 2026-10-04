#include "../test.h"

static void testDirectoryCreateSweep(cstr Name, cstr Path)
{
    bool Recovered = false;
    for ( uint64 Point = 0; Point < UINT64_C(128); Point++ ) {
        xrtClearError();
        testRequire(xrtMemDebugFailAfter(Point), "directory create injection failed");
        bool Created = xrtDirCreateAll(Path);
        bool Triggered = xrtMemDebugFailTriggered();
        xerrkind Kind = xrtErrorKind(xrtGetError());
        xrtMemDebugFailClear();
        printf("[diagnostic] DIR_CREATE %s point=%llu created=%u triggered=%u kind=%u\n",
            Name, (unsigned long long)Point, (unsigned)Created, (unsigned)Triggered, (unsigned)Kind);
        testRequire(Created ? !Triggered : Triggered && Kind == XERR_MEMORY,
            "directory creation replaced a memory error or ignored the fault");
        xrtClearError(); testRequire(xrtMemDebugReset(), "directory creation retained allocations");
        if ( Created ) { Recovered = true; break; }
    }
    testRequire(Recovered, "directory create allocation bound exceeded");
    xrtSetErrorInfo(XERR_ARGUMENT, "test.dir-create", 71, "caller diagnostic");
    const xerror* Prior = xrtGetError();
    testRequire(xrtDirCreateAll(Path) && xrtGetError() == Prior,
        "successful existing-directory creation replaced the caller diagnostic");
    xrtClearError(); testRequire(xrtMemDebugReset(), "directory success retained allocations");
    printf("[PASS] DIR_CREATE %s\n", Name);
}

int main(int argc, char** argv)
{
    static const cstr Existing = "out/dir-create-diagnostic/existing/child";
    static const cstr File = "out/dir-create-diagnostic/existing/file";
    char NativeRoot[1024]; xpathparts Parts;
    testRequire(argc == 1 || (argc == 2 && (strcmp(argv[1], "existing") == 0 || strcmp(argv[1], "root") == 0)),
        "directory create usage: [existing|root]");
    testRequire(xrtDirCreateAll(Existing), "directory create baseline failed");
    str Absolute = xrtPathAbs(".");
    testRequire(Absolute != NULL && xrtPathParse(xrtStrView(Absolute), XPATH_NATIVE, &Parts) &&
        Parts.Root.Size > 0 && Parts.Root.Size < sizeof(NativeRoot), "directory native-root setup failed");
    memcpy(NativeRoot, Parts.Root.Data, Parts.Root.Size); NativeRoot[Parts.Root.Size] = '\0';
    xrtFree(Absolute); xrtClearError(); testRequire(xrtMemDebugReset(), "directory baseline retained allocations");
    if ( argc == 1 || strcmp(argv[1], "existing") == 0 ) testDirectoryCreateSweep("existing", Existing);
    if ( argc == 1 || strcmp(argv[1], "root") == 0 ) testDirectoryCreateSweep("root", NativeRoot);
    testRequire(xrtFileWriteAll(File, XRT_BYTES_LITERAL("fixture")), "directory regular-file control setup failed");
    xrtClearError();
    testRequire(!xrtDirCreateAll("out/dir-create-diagnostic/existing/file/child") &&
        xrtErrorKind(xrtGetError()) != XERR_NONE, "directory creation accepted a regular-file parent");
    xrtClearError(); testRequire(xrtMemDebugReset(), "directory regular-file control retained allocations");
    puts("[PASS] DIR_CREATE regular-file-parent");
    return 0;
}
