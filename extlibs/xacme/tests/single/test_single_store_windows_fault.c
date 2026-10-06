/* Intercept only selected native file calls. The store uses its public API
 * and real filesystem; successful calls always reach the Windows API. */
#if defined(_WIN32) || defined(_WIN64)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

static BOOL WINAPI testStoreWrite(HANDLE, LPCVOID, DWORD, LPDWORD, LPOVERLAPPED);
static BOOL WINAPI testStoreFlush(HANDLE);
static BOOL WINAPI testStoreRename(LPCWSTR, LPCWSTR, DWORD);
#define WriteFile testStoreWrite
#define FlushFileBuffers testStoreFlush
#define MoveFileExW testStoreRename
#endif

#ifdef XACME_MODULE_XACME
#undef XACME_MODULE_XACME
#endif
#define XACME_MODULE_ACME_STORE
#define XRT_MODULE_MEMORY_DEBUG
#define XACME_IMPLEMENTATION
#include "../../include/xacme/features.h"
#ifndef XRT_IMPLEMENTATION
#define XRT_IMPLEMENTATION
#endif
#include "../../../../single/xrt.h"
#include "../../../../single/extlibs/xacme.h"

#if defined(_WIN32) || defined(_WIN64)
#undef WriteFile
#undef FlushFileBuffers
#undef MoveFileExW
#include "../test.h"
#include <wchar.h>

typedef enum teststorefault {
    STORE_NONE,
    STORE_TEXT_WRITE,
    STORE_KEY_PARTIAL,
    STORE_CURRENT_WRITE,
    STORE_ACCOUNT_PARTIAL,
    STORE_KEY_FLUSH,
    STORE_CURRENT_FLUSH,
    STORE_ACCOUNT_FLUSH,
    STORE_KEY_RENAME,
    STORE_CURRENT_RENAME,
    STORE_ACCOUNT_RENAME,
    STORE_CURRENT_TRANSIENT,
    STORE_CURRENT_AFTER_COMMIT,
    STORE_CURRENT_SUSTAINED
} teststorefault;

static teststorefault TestStoreFault;
static unsigned TestStoreHits;
static bool TestStoreInject;
static bool TestStoreArmed;

static void testStoreNormalize(wchar_t* Path)
{
    for ( size_t i = 0; Path[i] != L'\0'; i++ )
        if ( Path[i] == L'\\' ) Path[i] = L'/';
}

static bool testStoreHandlePath(HANDLE File, wchar_t* Path, size_t Capacity)
{
    DWORD Size = GetFinalPathNameByHandleW(File, Path, (DWORD)Capacity, FILE_NAME_NORMALIZED);
    if ( Size == 0 || Size >= Capacity ) return false;
    testStoreNormalize(Path);
    return true;
}

static bool testStoreMatchPath(const wchar_t* Path, unsigned Kind)
{
    bool Grant = wcsstr(Path, L"/certs/io.example.com/.grant-") != NULL;
    bool Account = wcsstr(Path, L"/accounts/") != NULL;
    if ( Kind == 0 ) return Grant && wcsstr(Path, L"/.xrt-write-") != NULL;
    if ( Kind == 1 ) return Grant && wcsstr(Path, L"/.xacme-key-") != NULL;
    if ( Kind == 2 ) return wcsstr(Path, L"/certs/io.example.com/.xacme-current-") != NULL;
    return Account && wcsstr(Path, L"/.xacme-key-") != NULL;
}

static BOOL testStoreFailure(void)
{
    if ( TestStoreInject && !TestStoreArmed ) {
        testRequire(xrtMemDebugFailAfter(0), "Windows store diagnostic injection failed");
        TestStoreArmed = true;
    }
    SetLastError(ERROR_WRITE_FAULT);
    return FALSE;
}

static BOOL WINAPI testStoreWrite(HANDLE File, LPCVOID Data, DWORD Size,
    LPDWORD Written, LPOVERLAPPED Overlapped)
{
    wchar_t Path[2048];
    unsigned Kind;
    if ( TestStoreFault >= STORE_TEXT_WRITE && TestStoreFault <= STORE_ACCOUNT_PARTIAL &&
        testStoreHandlePath(File, Path, sizeof(Path) / sizeof(Path[0])) ) {
        Kind = (unsigned)TestStoreFault - (unsigned)STORE_TEXT_WRITE;
        if ( testStoreMatchPath(Path, Kind) ) {
            TestStoreHits++;
            if ( (TestStoreFault == STORE_KEY_PARTIAL || TestStoreFault == STORE_ACCOUNT_PARTIAL) &&
                TestStoreHits == 1 && Size > 1 )
                return WriteFile(File, Data, 1, Written, Overlapped);
            *Written = 0;
            return testStoreFailure();
        }
    }
    return WriteFile(File, Data, Size, Written, Overlapped);
}

static BOOL WINAPI testStoreFlush(HANDLE File)
{
    wchar_t Path[2048];
    if ( TestStoreFault >= STORE_KEY_FLUSH && TestStoreFault <= STORE_ACCOUNT_FLUSH &&
        testStoreHandlePath(File, Path, sizeof(Path) / sizeof(Path[0])) ) {
        unsigned Kind = 1u + (unsigned)TestStoreFault - (unsigned)STORE_KEY_FLUSH;
        if ( testStoreMatchPath(Path, Kind) ) {
            TestStoreHits++;
            return testStoreFailure();
        }
    }
    return FlushFileBuffers(File);
}

static BOOL WINAPI testStoreRename(LPCWSTR Source, LPCWSTR Target, DWORD Flags)
{
    wchar_t Path[2048];
    size_t Size = wcslen(Target);
    if ( Size < sizeof(Path) / sizeof(Path[0]) ) {
        memcpy(Path, Target, (Size + 1u) * sizeof(Path[0]));
        testStoreNormalize(Path);
        bool Current = wcsstr(Path, L"/certs/io.example.com/current") != NULL;
        bool Match = (TestStoreFault == STORE_KEY_RENAME &&
            wcsstr(Path, L"/certs/io.example.com/.grant-") != NULL && wcsstr(Path, L"/key.pem") != NULL) ||
            (TestStoreFault == STORE_ACCOUNT_RENAME && wcsstr(Path, L"/accounts/") != NULL && wcsstr(Path, L"/account.pem") != NULL) ||
            (Current && TestStoreFault >= STORE_CURRENT_RENAME && TestStoreFault != STORE_ACCOUNT_RENAME);
        if ( Match ) {
            TestStoreHits++;
            if ( TestStoreFault == STORE_CURRENT_TRANSIENT && TestStoreHits > 2 )
                return MoveFileExW(Source, Target, Flags);
            if ( TestStoreFault == STORE_CURRENT_SUSTAINED && TestStoreHits > 12 )
                return MoveFileExW(Source, Target, Flags);
            if ( TestStoreFault == STORE_CURRENT_TRANSIENT || TestStoreFault == STORE_CURRENT_SUSTAINED ) {
                static const DWORD Errors[] = {
                    ERROR_ACCESS_DENIED, ERROR_SHARING_VIOLATION, ERROR_LOCK_VIOLATION
                };
                SetLastError(Errors[(TestStoreHits - 1u) % 3u]);
                return FALSE;
            }
            if ( TestStoreFault == STORE_CURRENT_AFTER_COMMIT && TestStoreHits == 1 ) {
                testRequire(MoveFileExW(Source, Target, Flags), "Windows store committed fault setup failed");
                return testStoreFailure();
            }
            if ( TestStoreFault == STORE_CURRENT_AFTER_COMMIT )
                return MoveFileExW(Source, Target, Flags);
            return testStoreFailure();
        }
    }
    return MoveFileExW(Source, Target, Flags);
}

/* Count all staged files and retained versions below a synthetic test root. */
static void testStoreScan(cstr Directory, unsigned* Generations, unsigned* Temps)
{
    char Pattern[1600]; WIN32_FIND_DATAA Entry;
    int Length = snprintf(Pattern, sizeof(Pattern), "%s/*", Directory);
    testRequire(Length > 0 && (size_t)Length < sizeof(Pattern), "Windows store scan path exceeded fixture bound");
    HANDLE Find = FindFirstFileA(Pattern, &Entry);
    testRequire(Find != INVALID_HANDLE_VALUE, "Windows store scan failed");
    do {
        if ( strcmp(Entry.cFileName, ".") == 0 || strcmp(Entry.cFileName, "..") == 0 ) continue;
        if ( (Entry.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0 ) {
            char Child[1600];
            testRequire((Entry.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) == 0, "Windows store fixture unexpectedly contains a reparse point");
            if ( strncmp(Entry.cFileName, ".grant-", 7) == 0 ) (*Generations)++;
            Length = snprintf(Child, sizeof(Child), "%s/%s", Directory, Entry.cFileName);
            testRequire(Length > 0 && (size_t)Length < sizeof(Child), "Windows store child path exceeded fixture bound");
            testStoreScan(Child, Generations, Temps);
        } else if ( strncmp(Entry.cFileName, ".xrt-write-", 11) == 0 ||
            strncmp(Entry.cFileName, ".xacme-key-", 11) == 0 ||
            strncmp(Entry.cFileName, ".xacme-current-", 15) == 0 ) (*Temps)++;
    } while ( FindNextFileA(Find, &Entry) );
    DWORD Error = GetLastError();
    testRequire(FindClose(Find) && Error == ERROR_NO_MORE_FILES, "Windows store scan did not end cleanly");
}

static void testStoreWindowsCase(teststorefault Fault, bool Inject)
{
    char Root[380];
    xacmeissuegrant Old = { (str)"old chain\n", (str)"old private key\n" };
    xacmeissuegrant New = { (str)"new chain\n", (str)"new private key\n" };
    xacmeissuegrant Loaded;
    unsigned BeforeGenerations = 0, BeforeTemps = 0, Generations = 0, Temps = 0;
    snprintf(Root, sizeof(Root), "%s/store_windows_fault_%u_%u", testOutRoot(), (unsigned)Fault, (unsigned)Inject);
    TestStoreFault = STORE_NONE;
    testRequire(xrtAcmeStoreSaveGrant(Root, "io.example.com", &Old, NULL) &&
        xrtAcmeStoreSaveAccount(Root, "https://ca.example/directory", "old account\n"), "Windows store fault baseline failed");
    testStoreScan(Root, &BeforeGenerations, &BeforeTemps);
    testRequire(BeforeTemps == 0, "Windows store baseline contains temporary files");
    xrtClearError(); TestStoreFault = Fault; TestStoreHits = 0; TestStoreInject = Inject; TestStoreArmed = false;
    bool Account = Fault == STORE_ACCOUNT_PARTIAL || Fault == STORE_ACCOUNT_FLUSH || Fault == STORE_ACCOUNT_RENAME;
    bool Saved = Account ? xrtAcmeStoreSaveAccount(Root, "https://ca.example/directory", "new account\n") :
        xrtAcmeStoreSaveGrant(Root, "io.example.com", &New, NULL);
    xerrkind Kind = xrtErrorKind(xrtGetError());
    bool Triggered = xrtMemDebugFailTriggered();
    TestStoreFault = STORE_NONE; xrtMemDebugFailClear();
    printf("[diagnostic] STORE_WINDOWS fault=%u oom=%u saved=%u hits=%u kind=%u triggered=%u\n",
        (unsigned)Fault, (unsigned)Inject, (unsigned)Saved, TestStoreHits, (unsigned)Kind, (unsigned)Triggered);
    testRequire(TestStoreHits > 0 && Triggered == Inject, "Windows store fault did not reach its native/diagnostic call");
    bool Transient = Fault == STORE_CURRENT_TRANSIENT || Fault == STORE_CURRENT_SUSTAINED;
    if ( Transient ) {
        unsigned ExpectedHits = Fault == STORE_CURRENT_TRANSIENT ? 3u : 13u;
        testRequire(Saved && TestStoreHits == ExpectedHits && Kind == XERR_NONE, "Windows store transient publish retry failed");
    } else {
        testRequire(!Saved && Kind == (Inject ? XERR_MEMORY : XERR_IO), "Windows store lost the file fault diagnostic");
    }
    xrtClearError();
    bool Committed = Transient || Fault == STORE_CURRENT_AFTER_COMMIT;
    testRequire(xrtAcmeStoreLoadGrant(Root, "io.example.com", &Loaded) &&
        strcmp(Loaded.sFullchainPem, Committed ? New.sFullchainPem : Old.sFullchainPem) == 0 &&
        strcmp(Loaded.sKeyPem, Committed ? New.sKeyPem : Old.sKeyPem) == 0, "Windows store fault lost the committed pair");
    xrtAcmeGrantUnit(&Loaded);
    str AccountPem = xrtAcmeStoreLoadAccount(Root, "https://ca.example/directory");
    testRequire(AccountPem != NULL && strcmp(AccountPem, "old account\n") == 0, "Windows store fault changed committed account");
    xrtFree(AccountPem);
    testStoreScan(Root, &Generations, &Temps);
    unsigned Retained = Fault == STORE_CURRENT_RENAME || Committed ? 1u : 0u;
    testRequire(Temps == 0 && Generations == BeforeGenerations + Retained, "Windows store fault leaked a partial file or discarded a published version");
    xrtClearError(); testRequire(xrtMemDebugReset(), "Windows store fault retained allocations");
    printf("[PASS] STORE_WINDOWS fault-%u/%s\n", (unsigned)Fault, Inject ? "oom" : "control");
}

int main(int argc, char** argv)
{
    if ( argc == 3 ) {
        unsigned Fault = (unsigned)strtoul(argv[1], NULL, 10);
        bool Inject = strcmp(argv[2], "oom") == 0;
        testRequire(Fault >= STORE_TEXT_WRITE && Fault <= STORE_CURRENT_SUSTAINED &&
            (Inject || strcmp(argv[2], "control") == 0), "Windows store fault usage: [number control|oom]");
        testStoreWindowsCase((teststorefault)Fault, Inject);
        return 0;
    }
    testRequire(argc == 1, "Windows store fault usage: [number control|oom]");
    for ( unsigned Fault = STORE_TEXT_WRITE; Fault <= STORE_ACCOUNT_RENAME; Fault++ ) {
        testStoreWindowsCase((teststorefault)Fault, false);
        testStoreWindowsCase((teststorefault)Fault, true);
    }
    testStoreWindowsCase(STORE_CURRENT_TRANSIENT, false);
    testStoreWindowsCase(STORE_CURRENT_SUSTAINED, false);
    testStoreWindowsCase(STORE_CURRENT_AFTER_COMMIT, false);
    testStoreWindowsCase(STORE_CURRENT_AFTER_COMMIT, true);
    return 0;
}
#else
int main(void) { return 0; }
#endif
