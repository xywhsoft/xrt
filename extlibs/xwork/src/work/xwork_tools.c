#include "../internal/xwork_internal.h"

static bool xwork__buf_append_escaped_bytes(xwork_buf *pBuf, const unsigned char *pData,
                                            size_t iSize)
{
    size_t i;
    for (i = 0u; i < iSize; ++i)
    {
        unsigned char c = pData[i];
        if (c == '\n' || c == '\r' || c == '\t' || (c >= 0x20u && c <= 0x7Eu))
        {
            if (!xwork__buf_append_char(pBuf, (char)c))
                return false;
        }
        else if (!xwork__buf_appendf(pBuf, "\\x%02X", (unsigned int)c))
        {
            return false;
        }
    }
    return true;
}

/* Process pipes are byte streams. Tool results, however, are JSON text and
 * therefore must be valid UTF-8 before they reach the model provider. */
bool xwork__buf_append_process_text(xwork_buf *pBuf, const void *pData, size_t iSize)
{
    const unsigned char *pBytes = (const unsigned char *)pData;
    if (iSize == 0u)
        return true;
    if (!pData)
        return false;
    if (memchr(pData, 0, iSize) == NULL &&
        xrtUtf8Valid((xstrview){(const char *)pData, iSize}, NULL))
    {
        return xwork__buf_append(pBuf, pData, iSize);
    }
#if defined(_WIN32)
    if (memchr(pData, 0, iSize) == NULL)
    {
        int iWide = MultiByteToWideChar(GetOEMCP(), 0, (const char *)pData, (int)iSize, NULL, 0);
        wchar_t *pWide =
            iWide > 0 ? (wchar_t *)malloc(((size_t)iWide + 1u) * sizeof(wchar_t)) : NULL;
        int iUtf8 = pWide ? MultiByteToWideChar(GetOEMCP(), 0, (const char *)pData, (int)iSize,
                                                pWide, iWide)
                          : 0;
        char *sConverted = NULL;
        if (iUtf8 > 0)
        {
            int iBytes = WideCharToMultiByte(CP_UTF8, 0, pWide, iWide, NULL, 0, NULL, NULL);
            sConverted = iBytes > 0 ? (char *)malloc((size_t)iBytes + 1u) : NULL;
            if (sConverted &&
                WideCharToMultiByte(CP_UTF8, 0, pWide, iWide, sConverted, iBytes, NULL, NULL) > 0)
                sConverted[iBytes] = '\0';
        }
        free(pWide);
        if (sConverted && sConverted[0] &&
            xrtUtf8Valid((xstrview){sConverted, strlen(sConverted)}, NULL))
        {
            bool bOk = xwork__buf_append_cstr(pBuf, sConverted);
            free(sConverted);
            return bOk;
        }
        free(sConverted);
    }
#endif
    return xwork__buf_append_escaped_bytes(pBuf, pBytes, iSize);
}

/* Image passthrough: magic sniff decides; the extension is not trusted. */
static const char *xwork__image_mime(const unsigned char *pData, size_t iSize)
{
    if (iSize >= 3u && pData[0] == 0xFF && pData[1] == 0xD8 && pData[2] == 0xFF)
    {
        return "image/jpeg";
    }
    if (iSize >= 8u && pData[0] == 0x89 && pData[1] == 'P' && pData[2] == 'N' && pData[3] == 'G' &&
        pData[4] == 0x0D && pData[5] == 0x0A && pData[6] == 0x1A && pData[7] == 0x0A)
    {
        return "image/png";
    }
    if (iSize >= 6u && (memcmp(pData, "GIF87a", 6u) == 0 || memcmp(pData, "GIF89a", 6u) == 0))
    {
        return "image/gif";
    }
    if (iSize >= 12u && memcmp(pData, "RIFF", 4u) == 0 && memcmp(pData + 8u, "WEBP", 4u) == 0)
    {
        return "image/webp";
    }
    if (iSize >= 2u && pData[0] == 'B' && pData[1] == 'M')
    {
        return "image/bmp";
    }
    return NULL;
}

xwork_result xwork__tool_fail(xwork_tool_output *pOutput, const char *sMessage)
{
    if (!xworkToolOutputSet(pOutput, false, sMessage))
        return XWORK_RESULT_ERROR;
    return XWORK_RESULT_OK;
}

static const char *xwork__process_error_reason(xerrkind eKind)
{
    switch (eKind)
    {
    case XERR_NOT_FOUND:
        return "not_found";
    case XERR_PERMISSION:
        return "permission_denied";
    case XERR_ARGUMENT:
    case XERR_TYPE:
    case XERR_VALUE:
    case XERR_RANGE:
        return "invalid_argument";
    case XERR_UNSUPPORTED:
        return "unsupported";
    case XERR_STATE:
    case XERR_CLOSED:
        return "invalid_state";
    case XERR_MEMORY:
        return "out_of_memory";
    case XERR_TIMEOUT:
        return "timeout";
    case XERR_CANCELLED:
        return "cancelled";
    case XERR_IO:
        return "io_error";
    default:
        return "unknown";
    }
}

static int xwork__diagnostic_prefix(const char *sText, size_t iLimit)
{
    size_t n = sText ? strlen(sText) : 0u;
    if (n <= iLimit)
        return (int)n;
    while (iLimit && ((unsigned char)sText[iLimit] & 0xC0u) == 0x80u)
        --iLimit;
    return (int)iLimit;
}

/* Format before cleanup/refresh replaces the thread error. Error Data can
 * contain arguments or environment secrets and is deliberately omitted. */
static xwork_result xwork__tool_process_fail(xwork_tool_output *pOutput, const char *sStage,
                                             const char *sProgram, const char *sCwd,
                                             const xerror *pCause)
{
    xwork_buf tText = {0};
    const xerror *pError = pCause;
    unsigned int iDepth = 0u;
    bool bOk =
        xwork__buf_appendf(&tText, "process_failure: stage=%s reason=%s\n", sStage,
                           xwork__process_error_reason(pCause ? xrtErrorKind(pCause) : XERR_NONE));
    if (bOk && sProgram)
        bOk = xwork__buf_appendf(&tText, "program: %.*s\n",
                                 xwork__diagnostic_prefix(sProgram, 512u), sProgram);
    if (bOk && sCwd)
        bOk = xwork__buf_appendf(&tText, "cwd: %.*s\n", xwork__diagnostic_prefix(sCwd, 512u), sCwd);
    while (bOk && pError && iDepth++ < 4u)
    {
        const char *sDomain = xrtErrorDomain(pError);
        const char *sOperation = xrtErrorOperation(pError);
        const char *sMessage = xrtErrorMessage(pError);
        bOk = xwork__buf_appendf(
            &tText, "cause: reason=%s domain=%.*s code=%d system_code=%d operation=%.*s\n%.*s\n",
            xwork__process_error_reason(xrtErrorKind(pError)),
            xwork__diagnostic_prefix(sDomain, 80u), sDomain ? sDomain : "",
            (int)xrtErrorCode(pError), (int)xrtErrorSystemCode(pError),
            xwork__diagnostic_prefix(sOperation, 120u), sOperation ? sOperation : "",
            xwork__diagnostic_prefix(sMessage, 240u), sMessage ? sMessage : "");
        if (bOk && xrtErrorSystemCode(pError))
        {
#if defined(_WIN32)
            wchar_t sWide[512];
            char sUtf8[2048];
            DWORD n = FormatMessageW(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                                     NULL, (DWORD)xrtErrorSystemCode(pError), 0, sWide, 512u, NULL);
            int iBytes = n ? WideCharToMultiByte(CP_UTF8, 0, sWide, (int)n, sUtf8,
                                                 (int)sizeof(sUtf8) - 1, NULL, NULL)
                           : 0;
            if (iBytes > 0)
            {
                sUtf8[iBytes] = 0;
                bOk = xwork__buf_appendf(&tText, "system_message: %s\n", sUtf8);
            }
#else
            const char *sSystem = strerror(xrtErrorSystemCode(pError));
            if (sSystem)
                bOk = xwork__buf_appendf(&tText, "system_message: %.*s\n",
                                         xwork__diagnostic_prefix(sSystem, 240u), sSystem);
#endif
        }
        pError = xrtErrorCause(pError);
    }
    if (bOk && !pCause)
        bOk = xwork__buf_append_cstr(
            &tText,
            "Native runtime did not provide a cause; this is not an approval-policy denial.\n");
    if (bOk)
        bOk = xworkToolOutputSet(pOutput, false, tText.pData);
    xwork__buf_unit(&tText);
    return bOk ? XWORK_RESULT_OK : XWORK_RESULT_ERROR;
}

/* Sequential, bounded process logs shared by exec, spawn and the host SDK.
 * Ordinary writes enter the OS cache; no per-chunk durable flush is required.
 * File-backed output retains the prefix, never overwrites it with a tail. */
#define XWORK_OUTPUT_INLINE (12u * 1024u)
#define XWORK_OUTPUT_MAX (64u * 1024u * 1024u)
#define XWORK_OUTPUT_STORE_MAX (1024u * 1024u * 1024u)
#define XWORK_OUTPUT_AGE (7u * 24u * 60u * 60u)

typedef struct xwork_output_stream
{
    xfile File;
    xwork_buf Preview;
    char *Path;
    char *Locator;
    uint64_t Received;
    uint64_t Stored;
    uint64_t FileBytes;
    bool Explicit;
    bool Lost;
} xwork_output_stream;

struct xwork_process_output
{
    xmutex *Lock;
    xroot Root;
    xroot Run;
    xfile Lease;
    char Id[33];
    char Error[96];
    int SystemCode;
    size_t InlineBytes;
    uint64_t MaxBytes;
    uint64_t Stored;
    bool FilesFromStart;
    bool MergeStderr;
    bool Finished;
    bool Reserved;
    struct xwork_process_output *ActiveNext;
    xwork_output_stream Streams[2];
};

/* POSIX fcntl locks are process-scoped: a second handle in this process can
 * acquire them, and closing that handle releases the process's lock. Protect
 * local collectors/writers with one mutex and skip local active leases before
 * opening them; native file locks then protect us against other processes. */
static xonce xwork__output_once;
static xmutex *xwork__output_gate;
static xwork_process_output *xwork__output_active;
static bool xwork__output_init(void *Unused)
{
    (void)Unused;
    xwork__output_gate = xrtMutexCreate();
    return xwork__output_gate != NULL;
}
static bool xwork__output_local_lock(void)
{
    return xrtOnce(&xwork__output_once, xwork__output_init, NULL) &&
           xrtMutexLock(xwork__output_gate);
}
static bool xwork__output_local_active(const char *Id)
{
    for (xwork_process_output *O = xwork__output_active; O; O = O->ActiveNext)
        if (!strcmp(O->Id, Id))
            return true;
    return false;
}

static bool xwork__output_name(const char *Name)
{
    size_t i;
    if (!Name || strlen(Name) != 32u)
        return false;
    for (i = 0; i < 32u; i++)
        if (!((Name[i] >= '0' && Name[i] <= '9') || (Name[i] >= 'a' && Name[i] <= 'f')))
            return false;
    return true;
}

static bool xwork__output_put(xvalue *V, const char *Key, const char *Text)
{
    return xrtValueObjectSetNew(V, xrtStrView(Key), xrtValueString(xrtStrView(Text ? Text : "")));
}
static bool xwork__output_uint(xvalue *V, const char *Key, uint64_t N)
{
    return xrtValueObjectSetNew(V, xrtStrView(Key), xrtValueUInt(N));
}
static bool xwork__output_bool(xvalue *V, const char *Key, bool B)
{
    return xrtValueObjectSetNew(V, xrtStrView(Key), xrtValueBool(B));
}
static bool xwork__output_get_uint(const xvalue *V, const char *Key, uint64_t *N)
{
    const xvalue *P = xrtValueObjectGet(V, xrtStrView(Key));
    int64_t I = 0;
    return xrtValueGetUInt(P, N) || (xrtValueGetInt(P, &I) && I >= 0 && (*N = (uint64_t)I, true));
}

static void xwork__output_error(xwork_process_output *O, const char *Reason)
{
    if (!O->Error[0])
    {
        snprintf(O->Error, sizeof(O->Error), "%s", Reason);
        O->SystemCode = xrtGetError() ? xrtErrorSystemCode(xrtGetError()) : 0;
    }
}

static xfile xwork__output_file(xroot Root, const char *Name, bool Exclusive)
{
    xfileoptions Opt;
    xrtFileOptionsInit(&Opt);
    Opt.Flags = XFILE_READ | XFILE_WRITE | XFILE_CREATE | XFILE_NOFOLLOW |
                (Exclusive ? XFILE_EXCLUSIVE : 0u);
    Opt.Mode = 0600u;
    Opt.Share = XFILE_SHARE_READ | XFILE_SHARE_WRITE | XFILE_SHARE_DELETE;
    return xrtRootFileOpen(Root, Name, &Opt);
}

static xroot xwork__output_root(xroot Root)
{
    xfileinfo Info;
    if (!Root)
        return NULL;
    if (!xrtRootStat(Root, "process", false, &Info))
    {
        if (xrtErrorKind(xrtGetError()) != XERR_NOT_FOUND)
            return NULL;
        if (!xrtRootDirCreate(Root, "process", 0700u) && xrtErrorKind(xrtGetError()) != XERR_EXISTS)
            return NULL;
    }
    else if (Info.Type != XFILE_TYPE_DIRECTORY)
        return NULL;
    return xrtRootOpenIn(Root, "process");
}

/* A collector holds the quota lock. An active writer keeps a separate lifetime
 * lock, so an idle/quiet command cannot be mistaken for an expired log. */
typedef struct xwork_output_old
{
    char Name[33];
    uint64_t Bytes;
    int64_t Ended;
} xwork_output_old;

static int xwork__output_old_compare(const void *A, const void *B)
{
    const xwork_output_old *X = A;
    const xwork_output_old *Y = B;
    return X->Ended < Y->Ended ? -1 : X->Ended > Y->Ended ? 1 : strcmp(X->Name, Y->Name);
}

static bool xwork__output_receipt_read(xroot Run, uint64_t *Limit, int64_t *Ended)
{
    char Text[256];
    xfileoptions Opt;
    xrtFileOptionsInit(&Opt);
    Opt.Flags = XFILE_READ | XFILE_NOFOLLOW;
    xfile F = xrtRootFileOpen(Run, ".receipt.json", &Opt);
    if (!F)
        return false;
    size_t N = 0;
    bool Read = xrtRead(F, Text, sizeof(Text) - 1u, &N);
    xrtClose(F);
    if (!Read || !N || N == sizeof(Text) - 1u)
        return false;
    Text[N] = 0;
    xvalue *V = xrtJsonParse(xrtStrView(Text));
    uint64_t Stamp = 0, Format = 0;
    bool Ok = V && xwork__output_get_uint(V, "format", &Format) && Format == 1u &&
              xwork__output_get_uint(V, "limit", Limit) &&
              xwork__output_get_uint(V, "ended_ms", &Stamp) && Stamp <= INT64_MAX;
    if (Ok)
        *Ended = (int64_t)Stamp;
    xrtValueRelease(V);
    if (Ok)
    {
        F = xrtRootFileOpen(Run, ".finished.json", &Opt);
        if (F)
        {
            N = 0;
            Read = xrtRead(F, Text, sizeof(Text) - 1u, &N);
            xrtClose(F);
            if (Read && N && N < sizeof(Text) - 1u)
            {
                Text[N] = 0;
                V = xrtJsonParse(xrtStrView(Text));
                if (V && xwork__output_get_uint(V, "ended_ms", &Stamp) && Stamp <= INT64_MAX)
                    *Ended = (int64_t)Stamp;
                xrtValueRelease(V);
            }
        }
    }
    return Ok;
}

static uint64_t xwork__output_scan(xroot Root, uint64_t Target, uint64_t MaxAgeSeconds, bool *Ok)
{
    xwork_output_old *Old = NULL;
    size_t Count = 0, Cap = 0;
    uint64_t Total = 0;
    int64_t Now = xllmUnixUs() / 1000;
    size_t Visited = 0;
    xdir Dir = xrtRootDirOpen(Root, ".", 0);
    xdirentry Entry;
    xdirnext Next = XDIR_NEXT_END;
    *Ok = Dir != NULL;
    while (Dir && (Next = xrtDirNext(Dir, &Entry)) == XDIR_NEXT_ITEM)
    {
        if (++Visited > 8192u)
        {
            *Ok = false;
            break;
        }
        if (!(Entry.Flags & XDIR_ENTRY_UTF8) || Entry.Name.Size != 32u)
            continue;
        char Name[33];
        memcpy(Name, Entry.Name.Data, 32u);
        Name[32] = 0;
        if (!xwork__output_name(Name))
            continue;
        xfileinfo Info;
        if (!xrtRootStat(Root, Name, false, &Info) || Info.Type != XFILE_TYPE_DIRECTORY)
            continue;
        xroot Run = xrtRootOpenIn(Root, Name);
        if (!Run)
        {
            *Ok = false;
            continue;
        }
        uint64_t Limit = 0, Bytes = 0;
        int64_t Ended = 0;
        if (!xwork__output_receipt_read(Run, &Limit, &Ended) || !Limit ||
            Limit > XWORK_OUTPUT_STORE_MAX)
        {
            xrtRootClose(Run);
            continue;
        }
        bool Local = xwork__output_local_active(Name);
        xfile Lease = Local ? NULL : xwork__output_file(Run, ".active.lock", false);
        bool Active = Local || !Lease || !xrtFileLock(Lease, XFILE_LOCK_EXCLUSIVE, false);
        if (Active)
            Bytes = Limit;
        else
        {
            /* Re-read completion only after the lifetime lock is acquired. A
             * finishing writer may have been publishing its immutable marker. */
            (void)xwork__output_receipt_read(Run, &Limit, &Ended);
            if (xrtRootStat(Run, "stdout.log", false, &Info) && Info.Type == XFILE_TYPE_FILE)
                Bytes += Info.Size;
            if (xrtRootStat(Run, "stderr.log", false, &Info) && Info.Type == XFILE_TYPE_FILE)
                Bytes += Info.Size;
            if (!Ended)
            { /* Unlocked unfinished receipt: process owner has gone. */
                if (xrtRootStat(Run, ".receipt.json", false, &Info))
                    Ended = Info.Modified;
                else
                    Ended = Now;
            }
            if (Count == Cap)
            {
                size_t Next = Cap ? Cap * 2u : 32u;
                void *P = realloc(Old, Next * sizeof(*Old));
                if (!P)
                {
                    *Ok = false;
                    xrtClose(Lease);
                    xrtRootClose(Run);
                    break;
                }
                Old = P;
                Cap = Next;
            }
            memcpy(Old[Count].Name, Name, 33u);
            Old[Count].Bytes = Bytes;
            Old[Count++].Ended = Ended;
        }
        if (UINT64_MAX - Total < Bytes)
        {
            *Ok = false;
            Total = UINT64_MAX;
        }
        else
            Total += Bytes;
        if (Lease)
            xrtClose(Lease);
        xrtRootClose(Run);
        if (Count > 4096u)
        {
            *Ok = false;
            break;
        }
    }
    if (Next == XDIR_NEXT_ERROR)
        *Ok = false;
    if (Dir)
        xrtDirClose(Dir);
    if (Count > 1u)
        qsort(Old, Count, sizeof(*Old), xwork__output_old_compare);
    for (size_t i = 0; i < Count; i++)
    {
        bool Expired = MaxAgeSeconds && Now >= Old[i].Ended &&
                       (uint64_t)(Now - Old[i].Ended) / 1000u >= MaxAgeSeconds;
        if (Total <= Target && !Expired)
            continue;
        xroot Run = xrtRootOpenIn(Root, Old[i].Name);
        if (!Run)
            continue;
        xfile Lease = xwork__output_file(Run, ".active.lock", false);
        bool Removed = false;
        if (Lease && xrtFileLock(Lease, XFILE_LOCK_EXCLUSIVE, false))
        {
            /* Only our fixed files are removed, never recursive arbitrary content. */
            bool A =
                xrtRootRemove(Run, "stdout.log") || xrtErrorKind(xrtGetError()) == XERR_NOT_FOUND;
            bool B =
                xrtRootRemove(Run, "stderr.log") || xrtErrorKind(xrtGetError()) == XERR_NOT_FOUND;
            if (A && B)
            {
                bool C = xrtRootRemove(Run, ".finished.json") ||
                         xrtErrorKind(xrtGetError()) == XERR_NOT_FOUND;
                if (C)
                    Removed = xrtRootRemove(Run, ".receipt.json");
            }
        }
        if (Lease)
            xrtClose(Lease);
        if (Removed)
        {
            (void)xrtRootRemove(Run, ".active.lock");
            Total = Total >= Old[i].Bytes ? Total - Old[i].Bytes : 0;
        }
        xrtRootClose(Run);
        if (Removed)
            (void)xrtRootRemove(Root, Old[i].Name);
    }
    free(Old);
    return Total;
}

bool xworkProcessOutputCollect(xroot Directory, uint64_t MaxBytes, uint64_t MaxAgeSeconds)
{
    if (!xwork__output_local_lock())
        return false;
    xroot Root = Directory ? xrtRootOpenIn(Directory, "process") : NULL;
    if (!Root)
    {
        bool Missing = Directory && xrtErrorKind(xrtGetError()) == XERR_NOT_FOUND;
        xrtMutexUnlock(xwork__output_gate);
        return Missing;
    }
    xfile Gate = xwork__output_file(Root, ".quota.lock", false);
    bool Ok = false;
    if (Gate && xrtFileLock(Gate, XFILE_LOCK_EXCLUSIVE, true))
    {
        uint64_t Total = xwork__output_scan(Root, MaxBytes ? MaxBytes : XWORK_OUTPUT_STORE_MAX,
                                            MaxAgeSeconds, &Ok);
        Ok = Ok && Total <= (MaxBytes ? MaxBytes : XWORK_OUTPUT_STORE_MAX);
    }
    if (Gate)
        xrtClose(Gate);
    xrtRootClose(Root);
    xrtMutexUnlock(xwork__output_gate);
    return Ok;
}

static bool xwork__output_receipt(xwork_process_output *O)
{
    if (!O->Run)
        return true;
    char Text[192];
    int N = snprintf(Text, sizeof(Text), "{\"format\":1,\"limit\":%llu,\"ended_ms\":%llu}\n",
                     (unsigned long long)O->MaxBytes,
                     (unsigned long long)(O->Finished ? xllmUnixUs() / 1000 : 0));
    xfileoptions Opt;
    xrtFileOptionsInit(&Opt);
    Opt.Flags = XFILE_WRITE | XFILE_CREATE | XFILE_EXCLUSIVE | XFILE_NOFOLLOW;
    Opt.Mode = 0600u;
    Opt.Share = XFILE_SHARE_READ;
    xfile F = xrtRootFileOpen(O->Run, O->Finished ? ".finished.json" : ".receipt.json", &Opt);
    bool Ok = F && N > 0 && xrtWriteFull(F, Text, (size_t)N, NULL);
    if (F)
        xrtClose(F);
    return Ok;
}

static bool xwork__output_reserve(xwork_process_output *O)
{
    if (O->Reserved)
        return true;
    if (!xwork__output_local_lock())
        return false;
    xroot Root = xwork__output_root(O->Root);
    if (!Root)
    {
        xrtMutexUnlock(xwork__output_gate);
        return false;
    }
    xfile Gate = xwork__output_file(Root, ".quota.lock", false);
    bool Ok = false;
    if (!Gate || !xrtFileLock(Gate, XFILE_LOCK_EXCLUSIVE, true))
        goto done;
    bool Scanned = false;
    uint64_t Total =
        xwork__output_scan(Root, XWORK_OUTPUT_STORE_MAX - O->MaxBytes, XWORK_OUTPUT_AGE, &Scanned);
    if (!Scanned || Total > XWORK_OUTPUT_STORE_MAX - O->MaxBytes)
    {
        xwork__output_error(O, "store_limit");
        goto done;
    }
    uint8_t Random[16];
    if (!xrtSecureRandom(Random, sizeof(Random)))
        goto done;
    for (size_t i = 0; i < sizeof(Random); i++)
    {
        static const char Hex[] = "0123456789abcdef";
        O->Id[i * 2u] = Hex[Random[i] >> 4];
        O->Id[i * 2u + 1u] = Hex[Random[i] & 15u];
    }
    if (!xrtRootDirCreate(Root, O->Id, 0700u))
        goto done;
    O->Run = xrtRootOpenIn(Root, O->Id);
    if (!O->Run)
        goto done;
    O->Lease = xwork__output_file(O->Run, ".active.lock", true);
    if (!O->Lease || !xrtFileLock(O->Lease, XFILE_LOCK_EXCLUSIVE, false) ||
        !xwork__output_receipt(O))
        goto done;
    O->Reserved = true;
    O->ActiveNext = xwork__output_active;
    xwork__output_active = O;
    Ok = true;
done:
    if (Gate)
        xrtClose(Gate);
    xrtRootClose(Root);
    xrtMutexUnlock(xwork__output_gate);
    return Ok;
}

static bool xwork__output_open(xwork_process_output *O, size_t Index)
{
    xwork_output_stream *S = &O->Streams[Index];
    if (S->File)
        return true;
    if (S->Explicit)
    {
        xfileoptions Opt;
        xrtFileOptionsInit(&Opt);
        Opt.Flags = XFILE_READ | XFILE_WRITE | XFILE_CREATE | XFILE_EXCLUSIVE | XFILE_NOFOLLOW;
        Opt.Mode = 0600u;
        Opt.Share = XFILE_SHARE_READ;
        S->File = xrtFileOpen(S->Path, &Opt);
        return S->File != NULL;
    }
    if (!xwork__output_reserve(O))
        return false;
    const char *Name = Index ? "stderr.log" : "stdout.log";
    S->File = xwork__output_file(O->Run, Name, true);
    if (!S->File)
        return false;
    char Relative[96];
    snprintf(Relative, sizeof(Relative), "process/%s/%s", O->Id, Name);
    char *Native = xrtPathJoin(xrtRootPath(O->Root), Relative);
    S->Path = Native ? xwork__strdup(Native) : NULL;
    if (Native)
        xrtFree(Native);
    char Locator[112];
    snprintf(Locator, sizeof(Locator), "@results/%s", Relative);
    S->Locator = xwork__strdup(Locator);
    return S->Path && S->Locator;
}

void xworkProcessOutputOptionsInit(xwork_process_output_options *Options)
{
    if (!Options)
        return;
    memset(Options, 0, sizeof(*Options));
    Options->Size = sizeof(*Options);
    Options->InlineBytes = XWORK_OUTPUT_INLINE;
    Options->MaxBytes = XWORK_OUTPUT_MAX;
}

xwork_process_output *xworkProcessOutputCreate(const xwork_process_output_options *Options,
                                               xwork_error *Error)
{
    xwork_process_output *O = NULL;
    xworkErrorInit(Error);
    if (!Options || Options->Size != sizeof(*Options) || Options->InlineBytes < 256u ||
        Options->InlineBytes > 1024u * 1024u || !Options->MaxBytes ||
        Options->MaxBytes > XWORK_OUTPUT_STORE_MAX || (Options->MergeStderr && Options->StderrPath))
        goto invalid;
    if (Options->StdoutPath && Options->StderrPath &&
        !strcmp(Options->StdoutPath, Options->StderrPath))
        goto invalid;
    O = calloc(1, sizeof(*O));
    if (!O)
        goto memory;
    O->Lock = xrtMutexCreate();
    if (!O->Lock)
        goto memory;
    O->Root = Options->Directory ? xrtRootOpenIn(Options->Directory, ".") : NULL;
    O->InlineBytes = Options->InlineBytes;
    O->MaxBytes = Options->MaxBytes;
    O->FilesFromStart = Options->FilesFromStart;
    O->MergeStderr = Options->MergeStderr;
    for (size_t i = 0; i < 2u; i++)
    {
        const char *Path = i ? Options->StderrPath : Options->StdoutPath;
        if (Path)
        {
            O->Streams[i].Explicit = true;
            O->Streams[i].Path = xwork__strdup(Path);
            if (!O->Streams[i].Path)
                goto memory;
        }
        if (i && O->MergeStderr)
            continue;
        if ((O->FilesFromStart || Path) && !xwork__output_open(O, i))
            goto io;
    }
    return O;
invalid:
    xwork__set_error(Error, XWORK_ERROR_INVALID_ARGUMENT,
                     "invalid process output options or overlapping destinations");
    return NULL;
memory:
    xwork__set_error(Error, XWORK_ERROR_OUT_OF_MEMORY, "cannot allocate process output storage");
    goto fail;
io:
    xwork__set_error(
        Error, XWORK_ERROR_IO,
        "cannot exclusively create process output files (existing files are preserved)");
fail:
    if (O)
        for (size_t i = 0; i < 2u; i++)
            if (O->Streams[i].Explicit && O->Streams[i].File)
            {
                xrtClose(O->Streams[i].File);
                O->Streams[i].File = NULL;
                (void)xrtFileDelete(O->Streams[i].Path);
            }
    if (O)
        xworkProcessOutputDestroy(O);
    return NULL;
}

bool xworkProcessOutputWrite(xwork_process_output *O, bool Stderr, const void *Data, size_t Size)
{
    if (!O || (!Data && Size) || !xrtMutexLock(O->Lock))
        return false;
    size_t Index = Stderr && !O->MergeStderr ? 1u : 0u;
    xwork_output_stream *S = &O->Streams[Index];
    if (O->Finished)
    {
        xrtMutexUnlock(O->Lock);
        return false;
    }
    S->Received = UINT64_MAX - S->Received < Size ? UINT64_MAX : S->Received + Size;
    size_t Keep = S->Lost ? 0u : Size;
    if ((uint64_t)Keep > O->MaxBytes - O->Stored)
    {
        Keep = (size_t)(O->MaxBytes - O->Stored);
        S->Lost = true;
        xwork__output_error(O, "output_limit");
    }
    if (S->Lost && O->Stored == O->MaxBytes)
        Keep = 0;
    if (!S->File && S->Preview.iLen + Keep > O->InlineBytes / 3u)
    {
        if (!xwork__output_open(O, Index))
        {
            S->Lost = true;
            xwork__output_error(O, "output_open_failed");
            Keep = 0;
        }
        else if (S->Preview.iLen)
        {
            size_t Written = 0;
            bool Ok = xrtWriteAtFull(S->File, 0, S->Preview.pData, S->Preview.iLen, &Written);
            S->FileBytes = Written;
            if (!Ok)
            {
                S->Lost = true;
                xwork__output_error(O, "output_write_failed");
                Keep = 0;
            }
        }
    }
    if (Keep && S->File)
    {
        size_t Written = 0;
        bool Ok = xrtWriteAtFull(S->File, S->Stored, Data, Keep, &Written);
        S->FileBytes += Written;
        if (!Ok)
        {
            S->Lost = true;
            xwork__output_error(O, "output_write_failed");
            Keep = Written;
        }
    }
    if (Keep)
    {
        size_t PreviewKeep =
            O->InlineBytes / 3u > S->Preview.iLen ? O->InlineBytes / 3u - S->Preview.iLen : 0;
        if (PreviewKeep > Keep)
            PreviewKeep = Keep;
        if (PreviewKeep && !xwork__buf_append(&S->Preview, Data, PreviewKeep))
        {
            S->Lost = true;
            xwork__output_error(O, "output_memory_failed");
            if (!S->File)
                Keep = 0;
        }
        S->Stored += Keep;
        O->Stored += Keep;
    }
    xrtMutexUnlock(O->Lock);
    /* Continue draining on quota/IO failure: a stopped reader would deadlock a
     * writer. The snapshot makes any incomplete capture explicit. */
    return true;
}

void xworkProcessOutputFinish(xwork_process_output *O)
{
    if (!O || !O->Lock || !xrtMutexLock(O->Lock))
        return;
    if (!O->Finished)
    {
        O->Finished = true;
        for (size_t i = 0; i < 2u; i++)
            if (O->Streams[i].File)
            {
                if (!xrtClose(O->Streams[i].File))
                {
                    O->Streams[i].Lost = true;
                    xwork__output_error(O, "output_close_failed");
                }
                O->Streams[i].File = NULL;
            }
        if (xwork__output_local_lock())
        {
            if (!xwork__output_receipt(O))
                xwork__output_error(O, "receipt_write_failed");
            xwork_process_output **Link = &xwork__output_active;
            while (*Link && *Link != O)
                Link = &(*Link)->ActiveNext;
            if (*Link == O)
                *Link = O->ActiveNext;
            if (O->Lease)
            {
                xrtClose(O->Lease);
                O->Lease = NULL;
            }
            xrtMutexUnlock(xwork__output_gate);
        }
    }
    xrtMutexUnlock(O->Lock);
}

static xvalue *xwork__output_stream_value(xwork_process_output *O, size_t Index, bool IncludeText)
{
    xwork_output_stream *S = &O->Streams[Index];
    xvalue *V = xrtValueObject();
    if (!V)
        return NULL;
    bool Ok = xwork__output_uint(V, "bytes", S->Received) &&
              xwork__output_uint(V, "saved_bytes", S->Path ? S->FileBytes : S->Stored) &&
              xwork__output_bool(V, "complete", O->Finished && !S->Lost) &&
              xwork__output_bool(V, "preview_omitted",
                                 S->Path ? S->Stored > 1024u : S->Stored > S->Preview.iLen) &&
              xwork__output_put(V, "path", S->Locator ? S->Locator : S->Path) &&
              xwork__output_put(V, "native_path", S->Path);
    if (Ok && IncludeText)
    {
        size_t Head =
            S->Path && S->Stored > 1024u && S->Preview.iLen > 512u ? 512u : S->Preview.iLen;
        xwork_buf Text = {0};
        Ok = xwork__buf_append_process_text(&Text, S->Preview.pData, Head);
        if (Ok && S->Stored > Head && S->Path)
        {
            xfileoptions Opt;
            xrtFileOptionsInit(&Opt);
            Opt.Flags = XFILE_READ | XFILE_NOFOLLOW;
            xfile F = S->Explicit
                          ? xrtFileOpen(S->Path, &Opt)
                          : xrtRootFileOpen(O->Run, Index ? "stderr.log" : "stdout.log", &Opt);
            unsigned char Tail[512];
            size_t N = S->Stored - Head > sizeof(Tail) ? sizeof(Tail) : (size_t)(S->Stored - Head);
            size_t Got = 0;
            if (F && xrtReadAtFull(F, S->Stored - N, Tail, N, &Got))
                Ok = xwork__buf_append_cstr(
                         &Text, "\n...[preview omitted; read file for full output]...\n") &&
                     xwork__buf_append_process_text(&Text, Tail, Got);
            if (F)
                xrtClose(F);
        }
        if (Ok)
            Ok = xwork__output_put(V, "text", Text.pData);
        xwork__buf_unit(&Text);
    }
    if (!Ok)
    {
        xrtValueRelease(V);
        return NULL;
    }
    return V;
}

xvalue *xworkProcessOutputSnapshot(xwork_process_output *O, bool IncludeText)
{
    if (!O || !xrtMutexLock(O->Lock))
        return NULL;
    xvalue *V = xrtValueObject(), *Out = xwork__output_stream_value(O, 0u, IncludeText),
           *Err = xwork__output_stream_value(O, 1u, IncludeText);
    bool Ok = V && Out && Err;
    if (Ok)
    {
        Ok = xrtValueObjectSetNew(V, XRT_STR_LITERAL("stdout"), Out);
        Out = NULL;
    }
    if (Ok)
    {
        Ok = xrtValueObjectSetNew(V, XRT_STR_LITERAL("stderr"), Err);
        Err = NULL;
    }
    if (Ok)
        Ok = xwork__output_bool(V, "output_complete", O->Finished && !O->Error[0]) &&
             xwork__output_bool(V, "stderr_merged", O->MergeStderr) &&
             xwork__output_put(V, "output_error", O->Error) &&
             xwork__output_uint(V, "output_system_code",
                                O->SystemCode < 0 ? 0u : (uint64_t)O->SystemCode);
    xrtMutexUnlock(O->Lock);
    xrtValueRelease(Out);
    xrtValueRelease(Err);
    if (!Ok)
    {
        xrtValueRelease(V);
        V = NULL;
    }
    return V;
}

void xworkProcessOutputDestroy(xwork_process_output *O)
{
    if (!O)
        return;
    xworkProcessOutputFinish(O);
    for (size_t i = 0; i < 2u; i++)
    {
        xwork__buf_unit(&O->Streams[i].Preview);
        free(O->Streams[i].Path);
        free(O->Streams[i].Locator);
    }
    if (O->Lease)
        xrtClose(O->Lease);
    if (O->Run)
        xrtRootClose(O->Run);
    if (O->Root)
        xrtRootClose(O->Root);
    if (O->Lock)
        xrtMutexDestroy(O->Lock);
    free(O);
}

/* Bounded text reads. Byte cursors continue even one very long line without
 * constructing a whole-file tool response or splitting a UTF-8 scalar. */
const char *xwork__result_file_relative(const char *Path)
{
    const char *Relative;
    const char *Slash;
    size_t i;
    if (!Path || strncmp(Path, "@results/", 9u) != 0)
        return NULL;
    Relative = Path + 9u;
    if (!strncmp(Relative, "process/", 8u))
    {
        const char *Id = Relative + 8u;
        const char *End = strchr(Id, '/');
        if (!End || End - Id != 32 || strchr(End + 1u, '/') ||
            (strcmp(End + 1u, "stdout.log") && strcmp(End + 1u, "stderr.log")))
            return NULL;
        for (size_t j = 0; j < 32u; j++)
            if (!((Id[j] >= '0' && Id[j] <= '9') || (Id[j] >= 'a' && Id[j] <= 'f')))
                return NULL;
        return Relative;
    }
    Slash = strchr(Relative, '/');
    if (!Slash || Slash - Relative != 24 || strncmp(Relative, "run-", 4u) != 0 ||
        strlen(Slash + 1u) < 25u || strlen(Relative) > 255u || strchr(Slash + 1u, '/'))
        return NULL;
    for (i = 4u; i < 24u; ++i)
        if (Relative[i] < '0' || Relative[i] > '9')
            return NULL;
    for (i = 0u; i < 20u; ++i)
        if (Slash[1u + i] < '0' || Slash[1u + i] > '9')
            return NULL;
    if (Slash[21] != '-' || strstr(Slash + 1u, ".."))
        return NULL;
    for (i = 1u; Slash[i]; ++i)
    {
        unsigned char c = (unsigned char)Slash[i];
        if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') ||
              c == '-' || c == '_' || c == '.'))
            return NULL;
    }
    return Relative;
}

xfile xwork__result_file_open(xwork_agent *Agent, const char *Path)
{
    const char *Relative = xwork__result_file_relative(Path);
    char Run[25];
    char *Directory = NULL;
    xroot Root = NULL;
    xfile File = NULL;
    xfileinfo Info;
    xfileoptions Options;
    if (!Relative)
        return NULL;
    if (Agent->pArtifactStore)
        Root = xrtRootOpenIn(Agent->pArtifactStore->pDirectory, ".");
    else
    {
        Directory = xwork__resolve_path(Agent, Agent->sArtifactDirectory, NULL);
        if (Directory)
            Root = xrtRootOpen(Directory);
    }
    if (!Root)
        goto done;
    if (!strncmp(Relative, "process/", 8u))
    {
        const char *End = strchr(Relative + 8u, '/');
        char ProcessRun[41];
        memcpy(ProcessRun, Relative, (size_t)(End - Relative));
        ProcessRun[End - Relative] = 0;
        xroot Process = xrtRootOpenIn(Root, ProcessRun);
        xrtRootClose(Root);
        Root = Process;
        if (!Root)
            goto done;
        xrtFileOptionsInit(&Options);
        Options.Flags = XFILE_READ | XFILE_NOFOLLOW;
        File = xrtRootFileOpen(Root, End + 1u, &Options);
        if (File && (!xrtFileStat(File, &Info) || Info.Type != XFILE_TYPE_FILE))
        {
            xrtClose(File);
            File = NULL;
        }
        goto done;
    }
    memcpy(Run, Relative, 24u);
    Run[24] = 0;
    if (!xrtRootStat(Root, Run, false, &Info) || Info.Type != XFILE_TYPE_DIRECTORY)
        goto done;
    xrtFileOptionsInit(&Options);
    Options.Flags = XFILE_READ | XFILE_NOFOLLOW;
    File = xrtRootFileOpen(Root, Relative, &Options);
done:
    if (Root)
        (void)xrtRootClose(Root);
    free(Directory);
    return File;
}

/* Locators are bounded and escaped so malformed model arguments cannot
 * manufacture extra diagnostic lines. Never include file contents here. */
static bool xwork__read_locator(xwork_buf *Text, const char *Label, const char *Path)
{
    int Limit = xwork__diagnostic_prefix(Path, 512u), i;
    if (!xwork__buf_appendf(Text, "%s: \"", Label))
        return false;
    for (i = 0; i < Limit; ++i)
    {
        unsigned char c = (unsigned char)Path[i];
        if (c == '\n' || c == '\r' || c == '\t')
        {
            if (!xwork__buf_appendf(Text, "\\%c", c == '\n' ? 'n' : c == '\r' ? 'r' : 't'))
                return false;
        }
        else if (c == '"' || c == '\\')
        {
            if (!xwork__buf_append_char(Text, '\\') || !xwork__buf_append_char(Text, (char)c))
                return false;
        }
        else if (c < 0x20u || c == 0x7fu)
        {
            if (!xwork__buf_appendf(Text, "\\x%02X", c))
                return false;
        }
        else if (!xwork__buf_append_char(Text, (char)c))
            return false;
    }
    return xwork__buf_appendf(Text, "%s\"\n", Path && strlen(Path) > (size_t)Limit ? "..." : "");
}

static xwork_result xwork__read_fail(xwork_agent *Agent, xwork_tool_output *Output,
                                     const char *Reason, const char *Message, const char *Path,
                                     const char *Resolved, const xerror *Cause)
{
    xwork_buf Text = {0};
    xerror *Saved = Cause ? xrtErrorRef(Cause) : NULL;
    size_t i;
    bool Ok = xwork__buf_appendf(&Text, "read_failure: reason=%s\n%s\n", Reason, Message) &&
              xwork__read_locator(&Text, "path", Path);
    if (Ok && Resolved)
        Ok = xwork__read_locator(&Text, "resolved_path", Resolved);
    if (Ok && Saved)
        Ok = xwork__buf_appendf(&Text, "filesystem_error: kind=%s system_code=%d\n",
                                xwork__process_error_reason(xrtErrorKind(Saved)),
                                (int)xrtErrorSystemCode(Saved));
    if (Ok)
        Ok = xwork__buf_append_cstr(
            &Text, "Relative paths resolve under the workspace; mounted roots require their "
                   "explicit @alias/ prefix.\n"
                   "Do not repeat this unchanged read. Correct the locator using the reported "
                   "reason; if the file is missing or access is denied, report that cause.\n");
    for (i = 0u; Ok && Agent && i < Agent->iPathMountCount; ++i)
    {
        const char *Alias = Agent->PathMounts[i].sAlias;
        if (!strcmp(Alias, "memory-global") || !strcmp(Alias, "memory-project"))
            Ok = xwork__buf_appendf(
                &Text,
                "Memory recovery: read({\"path\":\"@%s/MEMORY.md\"}), then read the topic with "
                "that same prefix. Do not guess physical memory directories.\n",
                Alias);
    }
    if (Saved)
        xrtErrorFree(Saved);
    if (Ok)
        Ok = xworkToolOutputSet(Output, false, Text.pData);
    xwork__buf_unit(&Text);
    return Ok ? XWORK_RESULT_OK : XWORK_RESULT_ERROR;
}

static const char *xwork__read_path_invalid(const char *Path)
{
    size_t i;
    if (!Path || !Path[0])
        return "path must be a non-empty string";
#if defined(_WIN32)
    for (i = 0u; Path[i]; ++i)
    {
        unsigned char c = (unsigned char)Path[i];
        if (c < 0x20u || c == '<' || c == '>' || c == '"' || c == '|' || c == '?' || c == '*')
            return "path contains a Windows-invalid character; remove newlines or tool/XML markup "
                   "and supply a literal file path";
    }
#else
    (void)i;
#endif
    return NULL;
}

static bool xwork__read_path_exists(xwork_agent *Agent, const char *Path)
{
    char *Resolved;
    xfileinfo Info;
    bool Exists;
    /* @results has its own root handle checks and is never rewritten. */
    if (!Path || !Path[0] || !strncmp(Path, "@results/", 9u) || xwork__read_path_invalid(Path))
        return false;
    Resolved = xwork__resolve_path(Agent, Path, NULL);
    Exists = Resolved && xrtPathStat(Resolved, true, &Info) &&
             (Info.Type == XFILE_TYPE_FILE || Info.Type == XFILE_TYPE_DIRECTORY);
    free(Resolved);
    return Exists;
}

/* Only repair common serialization mistakes when the exact original does not
 * exist and the corrected, policy-checked target exists. No fuzzy search,
 * traversal removal, unknown aliases, or substitution after an OS denial. */
char *xwork__read_recover_path(xwork_agent *Agent, const char *Path)
{
    const char *Begin = Path;
    const char *End = Path + strlen(Path);
    const char *Newline;
    char *Candidate;
    char *Match = NULL;
    size_t i;
    if (!strncmp(Path, "@results/", 9u) || xwork__read_path_exists(Agent, Path))
        return NULL;
    while (Begin < End && (*Begin == ' ' || *Begin == '\t' || *Begin == '\r' || *Begin == '\n'))
        ++Begin;
    while (End > Begin && (End[-1] == ' ' || End[-1] == '\t' || End[-1] == '\r' || End[-1] == '\n'))
        --End;
    Newline = memchr(Begin, '\n', (size_t)(End - Begin));
    if (Newline)
    {
        const char *Tail = Newline + 1u;
        while (Tail < End && (*Tail == ' ' || *Tail == '\t' || *Tail == '\r'))
            ++Tail;
        if (!strncmp(Tail, "</parameter", 11u) || !strncmp(Tail, "</function", 10u) ||
            !strncmp(Tail, "</tool_call", 11u))
            End = Newline;
    }
    while (End > Begin && (End[-1] == ' ' || End[-1] == '\t' || End[-1] == '\r'))
        --End;
    if (End - Begin >= 2 && (*Begin == '"' || *Begin == '\'' || *Begin == '`') && End[-1] == *Begin)
    {
        ++Begin;
        --End;
    }
    Candidate = (char *)malloc((size_t)(End - Begin) + 1u);
    if (!Candidate)
        return NULL;
    memcpy(Candidate, Begin, (size_t)(End - Begin));
    Candidate[End - Begin] = 0;
    if (strcmp(Candidate, Path) && xwork__read_path_exists(Agent, Candidate))
        return Candidate;
    /* A missing bare topic (or the obsolete mdo-home/data/memory layout) can
     * be recovered from the explicit host memory mounts only when unique. */
    if (Candidate[0] != '@' && !xwork__read_path_invalid(Candidate) &&
        !xwork__read_path_exists(Agent, Candidate))
    {
        const char *Name = Candidate;
        char *Legacy = NULL;
        if (strchr(Candidate, '/') || strchr(Candidate, '\\') || strchr(Candidate, ':'))
        {
            char *Normal = xwork__strdup(Candidate);
            if (Normal)
            {
                for (i = 0u; Normal[i]; ++i)
                    if (Normal[i] == '\\')
                        Normal[i] = '/';
                Legacy = strstr(Normal, "/mdo-home/data/memory/");
                if (Legacy)
                    Legacy = xwork__strdup(Legacy + strlen("/mdo-home/data/memory/"));
                free(Normal);
            }
            Name = Legacy;
        }
        if (Name && Name[0] && !strchr(Name, '/') && !strchr(Name, '\\') && !strchr(Name, ':') &&
            strcmp(Name, ".") && strcmp(Name, ".."))
        {
            for (i = 0u; i < Agent->iPathMountCount; ++i)
            {
                const char *Alias = Agent->PathMounts[i].sAlias;
                xwork_buf Locator = {0};
                if (strcmp(Alias, "memory-global") && strcmp(Alias, "memory-project"))
                    continue;
                if (xwork__buf_appendf(&Locator, "@%s/%s", Alias, Name) &&
                    xwork__read_path_exists(Agent, Locator.pData))
                {
                    if (Match)
                    {
                        free(Match);
                        Match = NULL;
                        xwork__buf_unit(&Locator);
                        break;
                    }
                    Match = xwork__strdup(Locator.pData);
                }
                xwork__buf_unit(&Locator);
            }
        }
        free(Legacy);
    }
    free(Candidate);
    return Match;
}

static xwork_result xwork__tool_read(void *UserData, const xwork_tool_context *Context,
                                     const char *ArgumentsJson, xwork_tool_output *Output,
                                     xwork_error *Error)
{
    xwork_agent *Agent = (xwork_agent *)UserData;
    xvalue *Args = xwork__json_parse_object(ArgumentsJson);
    const char *Path;
    const char *RequestedPath;
    char *RecoveredPath = NULL;
    char *Resolved = NULL;
    unsigned char *Image = NULL;
    unsigned char Buffer[8196];
    xfile File = NULL;
    xfileoptions Options;
    uint64_t Size = 0u, Position = 0u, StartLine, MaxLines, MaxBytes, Line = 1u;
    size_t Read = 0u, i = 0u, Lines = 0u;
    bool Valid, HasOffset, AtLineStart = true;
    const char *Mime;
    xwork_buf Text = {0};
    xwork_result Result = XWORK_RESULT_ERROR;
    if (!Args)
        return xwork__tool_fail(Output, "invalid arguments: expected a JSON object");
    Path = xwork__json_text(Args, "path");
    RequestedPath = Path;
    {
        xstrview View = {0};
        const char *InvalidPath;
        if (Path && xrtValueGetString(xwork__json_get(Args, "path"), &View) &&
            View.Size != strlen(Path))
        {
            Result = xwork__read_fail(Agent, Output, "invalid_argument",
                                      "path must not contain an embedded NUL", Path, NULL, NULL);
            goto done;
        }
        if (Path && Path[0])
        {
            RecoveredPath = xwork__read_recover_path(Agent, Path);
            if (RecoveredPath)
                Path = RecoveredPath;
        }
        InvalidPath = xwork__read_path_invalid(Path);
        if (InvalidPath)
        {
            xwork__set_error(Error, XWORK_ERROR_INVALID_ARGUMENT, InvalidPath);
            Result =
                xwork__read_fail(Agent, Output, "invalid_argument", InvalidPath, Path, NULL, NULL);
            goto done;
        }
        if (Path[0] == '@' && strncmp(Path, "@results/", 9u) != 0 &&
            !xwork__file_root_for_path(Agent, Path))
        {
            const char *Message =
                "unknown file mount alias; use a mounted @alias/ path with forward slashes";
            xwork__set_error(Error, XWORK_ERROR_INVALID_ARGUMENT, Message);
            Result = xwork__read_fail(Agent, Output, "invalid_argument", Message, Path, NULL, NULL);
            goto done;
        }
    }
    StartLine = xwork__json_u64(Args, "start_line", 1u, &Valid);
    if (!Valid)
        goto invalid;
    if (!StartLine)
        StartLine = 1u;
    MaxLines = xwork__json_u64(Args, "max_lines", 400u, &Valid);
    if (!Valid)
        goto invalid;
    if (!MaxLines)
        MaxLines = 1u;
    if (MaxLines > 10000u)
        MaxLines = 10000u;
    MaxBytes = xwork__json_u64(Args, "max_bytes", 4096u, &Valid);
    if (!Valid)
        goto invalid;
    if (MaxBytes < 256u)
        MaxBytes = 256u;
    if (MaxBytes > 8192u)
        MaxBytes = 8192u;
    HasOffset = xwork__json_get(Args, "offset") != NULL;
    Position = xwork__json_u64(Args, "offset", 0u, &Valid);
    if (!Valid)
        goto invalid;
    if (RecoveredPath &&
        (!xwork__buf_append_cstr(&Text, "read_recovery: using the verified corrected path\n") ||
         !xwork__read_locator(&Text, "requested_path", RequestedPath) ||
         !xwork__read_locator(&Text, "effective_path", Path)))
        goto memory;
    if (strncmp(Path, "@results/", 9u) == 0)
        File = xwork__result_file_open(Agent, Path);
    else
    {
        Resolved = xwork__resolve_path(Agent, Path, Error);
        if (!Resolved)
        {
            const char *Reason = Error && Error->eCode == XWORK_ERROR_POLICY ? "permission_denied"
                                 : Error && Error->eCode == XWORK_ERROR_INVALID_ARGUMENT
                                     ? "invalid_argument"
                                     : "path_resolution_failed";
            Result = xwork__read_fail(Agent, Output, Reason,
                                      Error && Error->sMessage[0] ? Error->sMessage
                                                                  : "path resolution failed",
                                      Path, NULL, NULL);
            goto done;
        }
        if (xrtDirExists(Resolved))
        {
            if (!xwork__buf_appendf(&Text, "dir: %s — this path is a directory, NOT a file:\n",
                                    Path) ||
                !xwork__list_directory(Resolved, false, false, &Text) ||
                !xworkToolOutputSet(Output, true, Text.pData ? Text.pData : ""))
                goto memory;
            Result = XWORK_RESULT_OK;
            goto done;
        }
        xrtFileOptionsInit(&Options);
        Options.Flags = XFILE_READ;
        xrtClearError();
        File = xrtFileOpen(Resolved, &Options);
    }
    if (!File || !xrtFileSize(File, &Size))
    {
        const xerror *Cause = xrtGetError();
        xerrkind Kind = Cause ? xrtErrorKind(Cause) : XERR_NONE;
        const char *Message = Kind == XERR_NOT_FOUND    ? "file not found"
                              : Kind == XERR_PERMISSION ? "file access denied"
                              : Kind == XERR_ARGUMENT   ? "invalid filesystem path"
                                                        : "cannot open or inspect file";
        Result = xwork__read_fail(Agent, Output, xwork__process_error_reason(Kind), Message, Path,
                                  Resolved, Cause);
        goto done;
    }
    if (Position > Size)
        Position = Size;
    if (!xrtReadAt(File, 0u, Buffer, Size < 32u ? (size_t)Size : 32u, &Read))
        goto io;
    Mime = xwork__image_mime(Buffer, Read);
    if (Mime && !HasOffset && StartLine == 1u)
    {
        if (Size > 64u * 1024u * 1024u)
        {
            Result = xwork__tool_fail(Output, "image is larger than the 64 MiB read limit");
            goto done;
        }
        Image = (unsigned char *)malloc((size_t)Size);
        if (!Image)
            goto memory;
        if (!xrtReadAtFull(File, 0u, Image, (size_t)Size, &Read) || Read != Size)
            goto io;
        if (!xwork__buf_appendf(&Text,
                                "image: %s\nsize: %llu bytes\nmime: %s\nattached for viewing", Path,
                                (unsigned long long)Size, Mime) ||
            !xworkToolOutputSet(Output, true, Text.pData) ||
            !xworkToolOutputSetImage(Output, Image, (size_t)Size, Mime))
            goto memory;
        Result = XWORK_RESULT_OK;
        goto done;
    }
    /* Skip to a requested line with bounded IO rather than a whole-file copy. */
    while (!HasOffset && Line < StartLine && Position < Size)
    {
        if (xwork__is_cancelled(Agent))
            goto cancelled;
        if (!xrtReadAt(File, Position, Buffer, sizeof(Buffer), &Read) || !Read)
            goto io;
        for (i = 0u; i < Read && Line < StartLine; ++i)
            if (Buffer[i] == '\n')
                ++Line;
        Position += i;
    }
    if (HasOffset)
    {
        /* A model may estimate a byte offset inside a UTF-8 scalar. Rewind
         * at most three bytes, only after verifying the complete scalar. */
        if (Position < Size && Position > 0u)
        {
            uint64_t Begin = Position > 3u ? Position - 3u : 0u;
            size_t Span = (size_t)(Position - Begin), Count = 0u, Back;
            unsigned char Prefix[7];
            if (!xrtReadAt(File, Begin, Prefix, sizeof(Prefix), &Count))
                goto io;
            if (Span < Count && (Prefix[Span] & 0xc0u) == 0x80u)
            {
                for (Back = 1u; Back <= Span; ++Back)
                {
                    size_t ScalarBytes = 0u;
                    uint32 Scalar;
                    if (xrtUtf8Decode(
                            xrtStrViewN((const char *)Prefix + Span - Back, Count - Span + Back),
                            &Scalar, &ScalarBytes) == XUTF_OK &&
                        ScalarBytes > Back)
                    {
                        if (!xwork__buf_appendf(&Text,
                                                "read_cursor: requested_offset=%llu "
                                                "effective_offset=%llu (UTF-8 boundary)\n",
                                                (unsigned long long)Position,
                                                (unsigned long long)(Position - Back)))
                            goto memory;
                        Position -= Back;
                        break;
                    }
                }
            }
        }
        AtLineStart = false;
    }
    if (!xwork__buf_appendf(&Text, "file: %s (%llu bytes)\n", Path, (unsigned long long)Size))
        goto memory;
    if (Text.iLen + 256u > MaxBytes)
        MaxBytes = Text.iLen + 256u;
    if (MaxBytes > 8192u)
    {
        Result = xwork__tool_fail(Output, "file locator exceeds the read page limit");
        goto done;
    }
    if (!xrtReadAt(File, Position, Buffer, (size_t)MaxBytes + 4u, &Read))
        goto io;
    i = 0u;
    while (i < Read && Lines < MaxLines)
    {
        size_t ScalarBytes = 0u;
        uint32 Scalar;
        if (xwork__is_cancelled(Agent))
            goto cancelled;
        if (AtLineStart)
        {
            if (Text.iLen + 224u >= MaxBytes)
                break;
            if (!xwork__buf_appendf(&Text, "%6llu | ", (unsigned long long)Line))
                goto memory;
            AtLineStart = false;
        }
        if (xrtUtf8Decode(xrtStrViewN((const char *)Buffer + i, Read - i), &Scalar, &ScalarBytes) !=
            XUTF_OK)
        {
            Result = xwork__read_fail(Agent, Output, "invalid_encoding",
                                      "file contains invalid UTF-8 in the requested page; choose "
                                      "the correct text encoding instead of repeating this read",
                                      Path, Resolved, NULL);
            goto done;
        }
        if (Scalar == 0u)
        {
            Result = xwork__read_fail(
                Agent, Output, "binary_data",
                "file contains NUL bytes; read expects UTF-8 text or a supported image", Path,
                Resolved, NULL);
            goto done;
        }
        if (Text.iLen + ScalarBytes + 192u >= MaxBytes)
            break;
        if (!(Scalar == '\r' && i + 1u < Read && Buffer[i + 1u] == '\n') &&
            !xwork__buf_append(&Text, Buffer + i, ScalarBytes))
            goto memory;
        i += ScalarBytes;
        if (Scalar == '\n')
        {
            ++Line;
            ++Lines;
            AtLineStart = true;
        }
    }
    Position += i;
    if (Position < Size)
    {
        if (!xwork__buf_appendf(
                &Text,
                "\n[truncated: more content remains; continue with offset=%llu, max_bytes=%llu]\n",
                (unsigned long long)Position, (unsigned long long)MaxBytes))
            goto memory;
    }
    else if (!xwork__buf_appendf(&Text, "\n[complete: end of file at line %llu; eof=true]\n",
                                 (unsigned long long)(Line - (AtLineStart && Line > 1u ? 1u : 0u))))
        goto memory;
    if (!xworkToolOutputSet(Output, true, Text.pData))
        goto memory;
    Result = XWORK_RESULT_OK;
    goto done;
invalid:
    Result = xwork__tool_fail(
        Output,
        "read needs path, start_line/max_lines, max_bytes (256-8192), or an exclusive byte offset");
    goto done;
cancelled:
    (void)Context;
    xwork__set_error(Error, XWORK_ERROR_CANCELLED, "file read was cancelled");
    Result = XWORK_RESULT_CANCELLED;
    goto done;
io:
    xwork__set_error(Error, XWORK_ERROR_IO, "file read was incomplete");
    goto done;
memory:
    xwork__set_error(Error, XWORK_ERROR_OUT_OF_MEMORY, "cannot construct file page");
done:
    if (File)
        (void)xrtClose(File);
    free(Resolved);
    free(RecoveredPath);
    free(Image);
    xwork__buf_unit(&Text);
    xrtValueRelease(Args);
    return Result;
}

static bool xwork__write_bytes(const char *sPath, const char *sContent, bool bAppend)
{
    size_t iLen = strlen(sContent);
    if (iLen == 0u)
    {
        FILE *pFile = fopen(sPath, bAppend ? "ab" : "wb");
        if (!pFile)
            return false;
        fclose(pFile);
        return true;
    }
    return bAppend ? xrtFileAppend(sPath, (xbytesview){(const uint8 *)sContent, iLen})
                   : xrtFileWriteAtomic(sPath, (xbytesview){(const uint8 *)sContent, iLen});
}

static bool xwork__write_atomic_bytes(const char *sPath, const char *sContent, size_t iLen)
{
    if (!sPath || (!sContent && iLen))
        return false;
    return xrtFileWriteAtomic(sPath, (xbytesview){(const uint8 *)sContent, iLen});
}

/* ------------------------------------------------------------------ */
/* EOL discipline: the model works in LF space; storage converts per   */
/* the agent policy. AUTO keeps each file's dominant ending.           */
/* ------------------------------------------------------------------ */

static bool xwork__file_prefers_crlf(const char *sData, size_t iSize)
{
    size_t iCrlf = 0u;
    size_t iLf = 0u;
    size_t i;
    for (i = 0u; i < iSize; ++i)
    {
        if (sData[i] == '\n')
        {
            if (i > 0u && sData[i - 1u] == '\r')
                ++iCrlf;
            else
                ++iLf;
        }
    }
    return iCrlf > iLf;
}

/* Strip CR from CRLF pairs; returns a malloc'd LF-normalized copy. */
static char *xwork__normalize_to_lf(const char *sData, size_t iSize, size_t *piOut)
{
    char *sOut = (char *)malloc(iSize + 1u);
    size_t i;
    size_t n = 0u;
    if (!sOut)
        return NULL;
    for (i = 0u; i < iSize; ++i)
    {
        if (sData[i] == '\r' && i + 1u < iSize && sData[i + 1u] == '\n')
            continue;
        sOut[n++] = sData[i];
    }
    sOut[n] = '\0';
    if (piOut)
        *piOut = n;
    return sOut;
}

/* Convert LF to the storage ending; returns a malloc'd copy. */
static char *xwork__apply_storage_eol(const char *sLf, size_t iLen, const xwork_agent *pAgent,
                                      bool bExistingPrefersCrlf, size_t *piOut)
{
    bool bCrlf;
    char *sOut;
    size_t i;
    size_t n = 0u;
    if (!pAgent || pAgent->eEolPolicy == XWORK_EOL_PRESERVE)
    {
        bCrlf = false; /* PRESERVE callers pass already-raw text */
    }
    else if (pAgent->eEolPolicy == XWORK_EOL_FORCE_LF)
    {
        bCrlf = false;
    }
    else if (pAgent->eEolPolicy == XWORK_EOL_FORCE_CRLF)
    {
        bCrlf = true;
    }
    else
    {
        bCrlf = bExistingPrefersCrlf;
    }
    if (!bCrlf)
    {
        sOut = (char *)malloc(iLen + 1u);
        if (!sOut)
            return NULL;
        memcpy(sOut, sLf, iLen);
        sOut[iLen] = '\0';
        if (piOut)
            *piOut = iLen;
        return sOut;
    }
    {
        size_t iLf = 0u;
        for (i = 0u; i < iLen; ++i)
        {
            if (sLf[i] == '\n')
                ++iLf;
        }
        sOut = (char *)malloc(iLen + iLf + 1u);
        if (!sOut)
            return NULL;
        for (i = 0u; i < iLen; ++i)
        {
            if (sLf[i] == '\n')
                sOut[n++] = '\r';
            sOut[n++] = sLf[i];
        }
        sOut[n] = '\0';
        if (piOut)
            *piOut = n;
        return sOut;
    }
}

/* Find occurrences of a needle (memmem-free, Windows portable). */
static const char *xwork__find_bytes(const char *sHay, size_t iHay, const char *sNeedle,
                                     size_t iNeedle, size_t iFrom)
{
    if (iNeedle == 0u || iHay < iNeedle)
        return NULL;
    for (; iFrom + iNeedle <= iHay; ++iFrom)
    {
        if (sHay[iFrom] == sNeedle[0] && memcmp(sHay + iFrom, sNeedle, iNeedle) == 0)
        {
            return sHay + iFrom;
        }
    }
    return NULL;
}

static xwork_result xwork__tool_write(void *pUserData, const xwork_tool_context *pContext,
                                      const char *sArgumentsJson, xwork_tool_output *pOutput,
                                      xwork_error *pError)
{
    xwork_agent *pAgent = (xwork_agent *)pUserData;
    xvalue *tArgs = xwork__json_parse_object(sArgumentsJson);
    const char *sPath;
    const char *sContent;
    const char *sMode;
    char *sResolved = NULL;
    char *sStored = NULL;
    char *sExisting = NULL;
    bool bAppend = false;
    bool bCreate = false;
    bool bCreatedDirs = false;
    size_t iStoredLen = 0u;
    xwork_buf tOutput = {0};
    xwork_result eResult = XWORK_RESULT_ERROR;
    (void)pContext;
    if (!tArgs)
        return xwork__tool_fail(pOutput, "invalid arguments: expected a JSON object");
    sPath = xwork__json_text(tArgs, "path");
    sContent = xwork__json_text(tArgs, "content");
    sMode = xwork__json_text(tArgs, "mode");
    if (!sMode || !sMode[0])
        sMode = "overwrite";
    if (!sPath || !sPath[0] || !sContent)
    {
        eResult = xwork__tool_fail(pOutput, "path and content are required");
        goto cleanup;
    }
    if (strcmp(sMode, "append") == 0)
        bAppend = true;
    else if (strcmp(sMode, "create") == 0)
        bCreate = true;
    else if (strcmp(sMode, "overwrite") != 0)
    {
        eResult = xwork__tool_fail(pOutput, "mode must be overwrite, append, or create");
        goto cleanup;
    }
    if (!strncmp(sPath, "@results", 8u) && (sPath[8] == '/' || !sPath[8]))
    {
        eResult = xwork__tool_fail(pOutput, "result paths are read-only");
        goto cleanup;
    }
    sResolved = xwork__resolve_path(pAgent, sPath, pError);
    if (!sResolved)
    {
        eResult = xwork__tool_fail(pOutput, pError && pError->sMessage[0] ? pError->sMessage
                                                                          : "path denied");
        goto cleanup;
    }
    if (bCreate && xrtPathExists((str)sResolved))
    {
        eResult = xwork__tool_fail(pOutput, "create conflict: target already exists");
        goto cleanup;
    }
    /* Parents are always created; the success message reports it so the
     * model notices when it invented structure. */
    bCreatedDirs = !xwork__parent_exists(sResolved);
    if (!xwork__ensure_parent(sResolved))
    {
        eResult = xwork__tool_fail(pOutput, "failed to create parent directories");
        goto cleanup;
    }
    /* EOL discipline: the model's text is normalized to LF first, then
     * storage converts per policy (AUTO keeps the file's dominant ending). */
    {
        size_t iExisting = 0u;
        char *sLf = NULL;
        if (pAgent->eEolPolicy == XWORK_EOL_AUTO && (bAppend || !bCreate))
        {
            sExisting = (char *)xrtFileReadAll(sResolved, &iExisting);
        }
        if (pAgent->eEolPolicy == XWORK_EOL_PRESERVE)
        {
            sStored = xwork__strdup(sContent);
            if (sStored)
            {
                iStoredLen = strlen(sStored);
            }
            if (!sStored)
                goto oom;
        }
        else
        {
            sLf = xwork__normalize_to_lf(sContent, strlen(sContent), NULL);
            if (!sLf)
                goto oom;
            sStored = xwork__apply_storage_eol(
                sLf, strlen(sLf), pAgent,
                sExisting ? xwork__file_prefers_crlf(sExisting, iExisting) : false, &iStoredLen);
            free(sLf);
            if (!sStored)
                goto oom;
        }
    }
    if (!(bAppend ? xwork__write_bytes(sResolved, sStored, true)
                  : xwork__write_atomic_bytes(sResolved, sStored, iStoredLen)))
    {
        eResult = xwork__tool_fail(pOutput, "failed to write file");
        goto cleanup;
    }
    if (!xwork__buf_appendf(&tOutput, "wrote %zu bytes to %s (mode=%s)%s", iStoredLen, sPath, sMode,
                            bCreatedDirs ? " (created parent directories)" : "") ||
        !xworkToolOutputSet(pOutput, true, tOutput.pData))
        goto oom;
    eResult = XWORK_RESULT_OK;
    goto cleanup;
oom:
    xwork__set_error(pError, XWORK_ERROR_OUT_OF_MEMORY, "failed to build write output");
cleanup:
    if (tArgs)
        xrtValueRelease(tArgs);
    free(sResolved);
    free(sStored);
    if (sExisting)
        xrtFree(sExisting);
    xwork__buf_unit(&tOutput);
    return eResult;
}

/* pi-style batch edit: every old_text is matched against the original
 * file (not against earlier edits' output); one atomic write applies the
 * whole batch. 0 or ambiguous matches return candidate context lines for
 * self-correction instead of a bare error. */
#define XWORK_EDIT_MAX_EDITS 64u

typedef struct xwork_edit_span
{
    size_t iStart;
    size_t iLen;
    size_t iNew;
    size_t iNewLen;
} xwork_edit_span;

static int xwork__span_cmp(const void *pA, const void *pB)
{
    const xwork_edit_span *pSA = (const xwork_edit_span *)pA;
    const xwork_edit_span *pSB = (const xwork_edit_span *)pB;
    if (pSA->iStart < pSB->iStart)
        return -1;
    if (pSA->iStart > pSB->iStart)
        return 1;
    return 0;
}

/* Diagnostics only: fixed memory, three bounded windows, no fuzzy writes. */
typedef struct xwork_edit_candidate
{
    size_t iOffset, iLine, iAnchorLine, iAnchorSize;
    unsigned int uScore;
} xwork_edit_candidate;

static size_t xwork__edit_line_key(char *sKey, const char *sLine, size_t iLen)
{
    size_t i, n = 0u;
    /* Limit both scan and comparisons, including adversarially long lines. */
    for (i = 0u; i < iLen && i < 1024u && n < 256u; ++i)
    {
        if (sLine[i] != ' ' && sLine[i] != '\t' && sLine[i] != '\r')
            sKey[n++] = sLine[i];
    }
    return n;
}

static bool xwork__edit_candidate_context(xwork_buf *pOut, const char *sLf, size_t iLen,
                                          size_t iTargetLine)
{
    size_t i = 0u, iStart = 0u, iLine = 1u;
    size_t iFirst = iTargetLine > 2u ? iTargetLine - 2u : 1u;
    for (; i <= iLen && iLine <= iTargetLine + 2u; ++i)
    {
        size_t n, k, iTrailing = 0u;
        if (i != iLen && sLf[i] != '\n')
            continue;
        n = i - iStart;
        if (iLine >= iFirst)
        {
            size_t iShow = n < 240u ? n : 240u;
            while (iShow && iShow < n && ((unsigned char)sLf[iStart + iShow] & 0xC0u) == 0x80u)
                --iShow;
            if (!xwork__buf_appendf(pOut, "%6zu | ", iLine))
                return false;
            for (k = 0u; k < iShow; ++k)
            {
                if (sLf[iStart + k] == '\t')
                {
                    if (!xwork__buf_append_cstr(pOut, "\\t"))
                        return false;
                }
                else if (!xwork__buf_append_char(pOut, sLf[iStart + k]))
                    return false;
            }
            if (n > iShow && !xwork__buf_append_cstr(pOut, " [line truncated]"))
                return false;
            for (k = n; k > 0u && sLf[iStart + k - 1u] == ' '; --k)
                ++iTrailing;
            if (iTrailing && !xwork__buf_appendf(pOut, " [trailing spaces: %zu]", iTrailing))
                return false;
            if (!xwork__buf_append_char(pOut, '\n'))
                return false;
        }
        ++iLine;
        iStart = i + 1u;
    }
    return true;
}

static bool xwork__append_edit_candidates(xwork_buf *pOut, const char *sLf, size_t iLen,
                                          const char *sOld, bool bExact)
{
    xwork_edit_candidate tBest[3] = {{0}};
    char sAnchors[3][256] = {{0}}, sKey[256];
    size_t iAnchorSizes[3] = {0}, iAnchorLines[3] = {0};
    size_t iOld = strlen(sOld), i, j, k, iStart = 0u, iLine = 1u, iHits = 0u;
    if (!xwork__buf_append_cstr(
            pOut, "candidates (context only; no changes applied; tabs shown as \\t):\n"))
        return false;
    if (bExact)
    {
        const char *pMatch;
        size_t iFrom = 0u, iCounted = 0u;
        while (iHits < 3u && (pMatch = xwork__find_bytes(sLf, iLen, sOld, iOld, iFrom)) != NULL)
        {
            size_t iOffset = (size_t)(pMatch - sLf);
            for (; iCounted < iOffset; ++iCounted)
                if (sLf[iCounted] == '\n')
                    ++iLine;
            tBest[iHits].iLine = iLine;
            tBest[iHits++].iOffset = iOffset;
            iFrom = iOffset + iOld;
        }
    }
    else
    {
        /* Long, nonblank anchors avoid suggesting generic braces/file headers.
         * Only the first 64 KiB of old_text participates in diagnostic scoring. */
        for (i = 0u; i <= iOld && i <= 65536u; ++i)
        {
            size_t n;
            if (i != iOld && sOld[i] != '\n')
                continue;
            n = xwork__edit_line_key(sKey, sOld + iStart, i - iStart);
            if (n >= 4u)
                for (j = 0u; j < 3u; ++j)
                    if (n > iAnchorSizes[j])
                    {
                        for (k = 2u; k > j; --k)
                        {
                            memcpy(sAnchors[k], sAnchors[k - 1u], 256u);
                            iAnchorSizes[k] = iAnchorSizes[k - 1u];
                            iAnchorLines[k] = iAnchorLines[k - 1u];
                        }
                        memcpy(sAnchors[j], sKey, n);
                        iAnchorSizes[j] = n;
                        iAnchorLines[j] = iLine;
                        break;
                    }
            iStart = i + 1u;
            ++iLine;
        }
        iStart = 0u;
        iLine = 1u;
        for (i = 0u; i <= iLen; ++i)
        {
            size_t n;
            xwork_edit_candidate t = {0};
            if (i != iLen && sLf[i] != '\n')
                continue;
            n = xwork__edit_line_key(sKey, sLf + iStart, i - iStart);
            for (j = 0u; n && j < 3u && iAnchorSizes[j]; ++j)
            {
                size_t a = iAnchorSizes[j], iPrefix = 0u, iSuffix = 0u, iMax = n > a ? n : a;
                unsigned int uScore;
                while (iPrefix < n && iPrefix < a && sKey[iPrefix] == sAnchors[j][iPrefix])
                    ++iPrefix;
                while (iSuffix + iPrefix < n && iSuffix + iPrefix < a &&
                       sKey[n - iSuffix - 1u] == sAnchors[j][a - iSuffix - 1u])
                    ++iSuffix;
                uScore = (unsigned int)((iPrefix + iSuffix) * 1000u / iMax);
                if (uScore >= 600u && (iPrefix + iSuffix) >= 4u &&
                    (uScore > t.uScore || (uScore == t.uScore && a > t.iAnchorSize)))
                {
                    t.iOffset = iStart;
                    t.iLine = iLine;
                    t.uScore = uScore;
                    t.iAnchorLine = iAnchorLines[j];
                    t.iAnchorSize = a;
                }
            }
            if (t.uScore)
                for (j = 0u; j < 3u; ++j)
                {
                    if (t.uScore > tBest[j].uScore ||
                        (t.uScore == tBest[j].uScore && t.iAnchorSize > tBest[j].iAnchorSize))
                    {
                        for (k = 2u; k > j; --k)
                            tBest[k] = tBest[k - 1u];
                        tBest[j] = t;
                        break;
                    }
                }
            iStart = i + 1u;
            ++iLine;
        }
        for (iHits = 0u; iHits < 3u && tBest[iHits].uScore; ++iHits)
        {
        }
    }
    if (!iHits)
        return xwork__buf_append_cstr(pOut, "(no sufficiently similar anchor found; read the "
                                            "target region and copy exact old_text)\n");
    for (j = 0u; j < iHits; ++j)
    {
        if (!xwork__buf_appendf(pOut, "--- %s at line %zu",
                                bExact ? "exact match" : "similar anchor", tBest[j].iLine))
            return false;
        if (!bExact && !xwork__buf_appendf(pOut, " (old_text line %zu; similarity %u/1000)",
                                           tBest[j].iAnchorLine, tBest[j].uScore))
            return false;
        if (!xwork__buf_append_cstr(pOut, " ---\n") ||
            !xwork__edit_candidate_context(pOut, sLf, iLen, tBest[j].iLine))
            return false;
    }
    return xwork__buf_append_cstr(pOut, "Copy exact text, including indentation and punctuation; "
                                        "add unique context for multiple matches.\n");
}

static xwork_result xwork__tool_edit(void *pUserData, const xwork_tool_context *pContext,
                                     const char *sArgumentsJson, xwork_tool_output *pOutput,
                                     xwork_error *pError)
{
    xwork_agent *pAgent = (xwork_agent *)pUserData;
    xvalue *tArgs = xwork__json_parse_object(sArgumentsJson);
    const char *sPath;
    char *sResolved = NULL;
    char *sRaw = NULL;
    bool bRawFromXrt = false;
    char *sLf = NULL;
    char *sStored = NULL;
    char **psNew = NULL;
    bool *pbAll = NULL;
    xwork_edit_span *pSpans = NULL;
    size_t iSpanCount = 0u;
    size_t iSpanCap = 0u;
    size_t iRaw = 0u;
    size_t iLf = 0u;
    size_t iEditCount = 0u;
    size_t i;
    xvalue *tEdits;
    bool bPrefersCrlf;
    xwork_buf tNext = {0};
    xwork_buf tOutput = {0};
    xwork_result eResult = XWORK_RESULT_ERROR;
    (void)pContext;
    if (!tArgs)
        return xwork__tool_fail(pOutput, "invalid arguments: expected a JSON object");
    sPath = xwork__json_text(tArgs, "path");
    if (!sPath || !sPath[0])
    {
        eResult = xwork__tool_fail(pOutput, "path is required");
        goto cleanup;
    }
    tEdits = xwork__json_get(tArgs, "edits");
    if (!tEdits || xrtValueType(tEdits) != XVALUE_ARRAY ||
        (iEditCount = xrtValueCount(tEdits)) == 0u || iEditCount > XWORK_EDIT_MAX_EDITS)
    {
        eResult = xwork__tool_fail(pOutput, "edits must be an array of 1-64 objects");
        goto cleanup;
    }
    psNew = (char **)calloc(iEditCount, sizeof(char *));
    pbAll = (bool *)calloc(iEditCount, sizeof(bool));
    if (!psNew || !pbAll)
        goto oom;
    for (i = 0u; i < iEditCount; ++i)
    {
        xvalue *tEdit = xrtValueArrayGet(tEdits, i);
        const char *sNew = xwork__json_text(tEdit, "new_text");
        bool bValid;
        if (!tEdit || !xwork__json_text(tEdit, "old_text") ||
            !xwork__json_text(tEdit, "old_text")[0] || !sNew)
        {
            eResult = xwork__tool_fail(pOutput, "each edit needs non-empty old_text and new_text");
            goto cleanup;
        }
        pbAll[i] = xwork__json_bool(tEdit, "replace_all", false, &bValid);
        if (!bValid)
        {
            eResult = xwork__tool_fail(pOutput, "replace_all must be boolean");
            goto cleanup;
        }
        psNew[i] = xwork__strdup(sNew);
        if (!psNew[i])
            goto oom;
    }
    if (!strncmp(sPath, "@results", 8u) && (sPath[8] == '/' || !sPath[8]))
    {
        eResult = xwork__tool_fail(pOutput, "result paths are read-only");
        goto cleanup;
    }
    sResolved = xwork__resolve_path(pAgent, sPath, pError);
    if (!sResolved)
    {
        eResult = xwork__tool_fail(pOutput, pError && pError->sMessage[0] ? pError->sMessage
                                                                          : "path denied");
        goto cleanup;
    }
    if (!xrtFileExists((str)sResolved))
    {
        eResult = xwork__tool_fail(pOutput, "file does not exist");
        goto cleanup;
    }
    sRaw = (char *)xrtFileReadAll(sResolved, &iRaw);
    if (!sRaw && iRaw)
    {
        eResult = xwork__tool_fail(pOutput, "failed to read file");
        goto cleanup;
    }
    bRawFromXrt = sRaw != NULL; /* xrt returns a freeable buffer even when empty */
    if (!sRaw)
        sRaw = xwork__strdup("");
    if (memchr(sRaw, 0, iRaw) != NULL)
    {
        eResult = xwork__tool_fail(pOutput, "binary file; edit supports UTF-8 text only");
        goto cleanup;
    }
    bPrefersCrlf = xwork__file_prefers_crlf(sRaw, iRaw);
    if (pAgent->eEolPolicy == XWORK_EOL_PRESERVE)
    {
        sLf = xwork__strdup(sRaw);
        iLf = iRaw;
    }
    else
    {
        sLf = xwork__normalize_to_lf(sRaw, iRaw, &iLf);
    }
    if (!sLf)
        goto oom;

    /* Collect spans against the ORIGINAL LF text. */
    iSpanCap = iEditCount * 2u;
    pSpans = (xwork_edit_span *)malloc(iSpanCap * sizeof(*pSpans));
    if (!pSpans)
        goto oom;
    for (i = 0u; i < iEditCount; ++i)
    {
        xvalue *tEdit = xrtValueArrayGet(tEdits, i);
        const char *sOld = xwork__json_text(tEdit, "old_text");
        size_t iOld = strlen(sOld);
        size_t iFrom = 0u;
        size_t iMatches = 0u;
        const char *pMatch;
        while ((pMatch = xwork__find_bytes(sLf, iLf, sOld, iOld, iFrom)) != NULL)
        {
            /* replace_all keeps every span; single edits keep only the first
             * (later matches still count toward the ambiguity report). */
            if (pbAll[i] || iMatches == 0u)
            {
                if (iSpanCount == iSpanCap)
                {
                    xwork_edit_span *pNewSpans;
                    iSpanCap *= 2u;
                    pNewSpans = (xwork_edit_span *)realloc(pSpans, iSpanCap * sizeof(*pSpans));
                    if (!pNewSpans)
                        goto oom;
                    pSpans = pNewSpans;
                }
                pSpans[iSpanCount].iStart = (size_t)(pMatch - sLf);
                pSpans[iSpanCount].iLen = iOld;
                pSpans[iSpanCount].iNew = i;
                pSpans[iSpanCount].iNewLen = strlen(psNew[i]);
                ++iSpanCount;
            }
            ++iMatches;
            iFrom = (size_t)(pMatch - sLf) + iOld;
        }
        if (iMatches == 0u)
        {
            if (!xwork__buf_appendf(&tOutput,
                                    "edit %zu failed: old_text was not found (0 matches).\n", i) ||
                !xwork__append_edit_candidates(&tOutput, sLf, iLf, sOld, false))
                goto oom;
            eResult =
                xwork__tool_fail(pOutput, tOutput.pData ? tOutput.pData : "old_text was not found");
            goto cleanup;
        }
        if (iMatches > 1u && !pbAll[i])
        {
            if (!xwork__buf_appendf(
                    &tOutput,
                    "edit %zu failed: old_text occurs %zu times; add context or set replace_all.\n",
                    i, iMatches) ||
                !xwork__append_edit_candidates(&tOutput, sLf, iLf, sOld, true))
                goto oom;
            eResult =
                xwork__tool_fail(pOutput, tOutput.pData ? tOutput.pData : "old_text is ambiguous");
            goto cleanup;
        }
    }

    /* Sort, reject overlaps, splice one atomic result. */
    qsort(pSpans, iSpanCount, sizeof(*pSpans), xwork__span_cmp);
    for (i = 1u; i < iSpanCount; ++i)
    {
        if (pSpans[i].iStart < pSpans[i - 1u].iStart + pSpans[i - 1u].iLen)
        {
            eResult = xwork__tool_fail(pOutput, "edits overlap; merge them into one edit");
            goto cleanup;
        }
    }
    {
        size_t iCursor = 0u;
        for (i = 0u; i < iSpanCount; ++i)
        {
            if (!xwork__buf_append(&tNext, sLf + iCursor, pSpans[i].iStart - iCursor) ||
                !xwork__buf_append(&tNext, psNew[pSpans[i].iNew], pSpans[i].iNewLen))
                goto oom;
            iCursor = pSpans[i].iStart + pSpans[i].iLen;
        }
        if (!xwork__buf_append(&tNext, sLf + iCursor, iLf - iCursor))
            goto oom;
    }
    if (pAgent->eEolPolicy == XWORK_EOL_PRESERVE)
    {
        sStored = xwork__strdup(tNext.pData ? tNext.pData : "");
    }
    else
    {
        sStored = xwork__apply_storage_eol(tNext.pData ? tNext.pData : "", tNext.iLen, pAgent,
                                           bPrefersCrlf, NULL);
    }
    if (!sStored)
        goto oom;
    if (!xwork__write_atomic_bytes(sResolved, sStored, strlen(sStored)))
    {
        eResult = xwork__tool_fail(pOutput, "failed to write edited file");
        goto cleanup;
    }
    if (!xwork__buf_appendf(&tOutput,
                            "applied %zu edit%s (%zu replacement%s) to %s (%zu -> %zu bytes)",
                            iEditCount, iEditCount == 1u ? "" : "s", iSpanCount,
                            iSpanCount == 1u ? "" : "s", sPath, iRaw, strlen(sStored)) ||
        !xworkToolOutputSet(pOutput, true, tOutput.pData))
        goto oom;
    eResult = XWORK_RESULT_OK;
    goto cleanup;
oom:
    xwork__set_error(pError, XWORK_ERROR_OUT_OF_MEMORY, "failed to apply batch edit");
cleanup:
    if (tArgs)
        xrtValueRelease(tArgs);
    free(sResolved);
    if (bRawFromXrt)
    {
        if (sRaw)
            xrtFree(sRaw);
    }
    else
    {
        free(sRaw);
    }
    free(sLf);
    free(sStored);
    if (psNew)
    {
        for (i = 0u; i < iEditCount; ++i)
            free(psNew[i]);
        free(psNew);
    }
    free(pbAll);
    free(pSpans);
    xwork__buf_unit(&tNext);
    xwork__buf_unit(&tOutput);
    return eResult;
}

bool xwork__process_running(const xprocess *pProcess)
{
    return pProcess && xrtProcessState(pProcess) == XPROCESS_RUNNING;
}

static int32 xwork__process_capture_thread(void *pData)
{
    xwork_process_capture_stream *pStream = (xwork_process_capture_stream *)pData;
    uint8_t pChunk[4096];
    for (;;)
    {
        int64 iRead =
            xrtProcessRead(pStream->pOwner->pProcess, pStream->eStream, pChunk, sizeof(pChunk));
        if (iRead <= 0)
            break;
        if (!xrtMutexLock(pStream->pOwner->pLock))
            break;
        if ((size_t)iRead >= pStream->iLimit)
        {
            size_t iKeep = pStream->iLimit;
            pStream->uBaseOffset += pStream->tData.iLen + (uint64_t)iRead - iKeep;
            pStream->tData.iLen = 0u;
            (void)xwork__buf_append(&pStream->tData, pChunk + (size_t)iRead - iKeep, iKeep);
        }
        else
        {
            size_t iDrop = pStream->tData.iLen + (size_t)iRead > pStream->iLimit
                               ? pStream->tData.iLen + (size_t)iRead - pStream->iLimit
                               : 0u;
            if (iDrop)
            {
                memmove(pStream->tData.pData, pStream->tData.pData + iDrop,
                        pStream->tData.iLen - iDrop);
                pStream->tData.iLen -= iDrop;
                pStream->uBaseOffset += iDrop;
            }
            (void)xwork__buf_append(&pStream->tData, pChunk, (size_t)iRead);
        }
        (void)xrtMutexUnlock(pStream->pOwner->pLock);
    }
    if (xrtMutexLock(pStream->pOwner->pLock))
    {
        pStream->bDone = true;
        (void)xrtMutexUnlock(pStream->pOwner->pLock);
    }
    return 0;
}

xwork_process_capture *xwork__process_capture_create(xprocess *pProcess, size_t iLimit,
                                                     bool bCaptureStderr)
{
    xwork_process_capture *pCapture = (xwork_process_capture *)calloc(1u, sizeof(*pCapture));
    if (!pCapture)
        return NULL;
    pCapture->pProcess = pProcess;
    pCapture->pLock = xrtMutexCreate();
    pCapture->tStdout.pOwner = pCapture;
    pCapture->tStdout.eStream = XPROCESS_STDOUT;
    pCapture->tStdout.iLimit = iLimit;
    pCapture->tStderr.pOwner = pCapture;
    pCapture->tStderr.eStream = XPROCESS_STDERR;
    pCapture->tStderr.iLimit = iLimit;
    if (!pCapture->pLock)
        goto fail;
    pCapture->tStdout.pThread =
        xrtThreadCreate(xwork__process_capture_thread, &pCapture->tStdout, 0u);
    if (!pCapture->tStdout.pThread)
        goto fail;
    if (bCaptureStderr)
    {
        pCapture->tStderr.pThread =
            xrtThreadCreate(xwork__process_capture_thread, &pCapture->tStderr, 0u);
        if (!pCapture->tStderr.pThread)
            goto fail;
    }
    else
    {
        pCapture->tStderr.bDone = true;
    }
    return pCapture;
fail:
    (void)xrtProcessKillTree(pProcess);
    if (pCapture->tStdout.pThread)
    {
        (void)xrtProcessClose(pProcess, XPROCESS_STDOUT);
        (void)xrtThreadWait(pCapture->tStdout.pThread);
        xrtThreadDestroy(pCapture->tStdout.pThread);
    }
    if (pCapture->pLock)
        (void)xrtMutexDestroy(pCapture->pLock);
    free(pCapture);
    return NULL;
}

void xwork__process_capture_destroy(xwork_process_capture *pCapture)
{
    if (!pCapture)
        return;
    if (pCapture->tStdout.pThread)
    {
        (void)xrtThreadWait(pCapture->tStdout.pThread);
        xrtThreadDestroy(pCapture->tStdout.pThread);
    }
    if (pCapture->tStderr.pThread)
    {
        (void)xrtThreadWait(pCapture->tStderr.pThread);
        xrtThreadDestroy(pCapture->tStderr.pThread);
    }
    xwork__buf_unit(&pCapture->tStdout.tData);
    xwork__buf_unit(&pCapture->tStderr.tData);
    if (pCapture->pLock)
        (void)xrtMutexDestroy(pCapture->pLock);
    free(pCapture);
}

void *xwork__process_capture_since(xwork_process_capture *pCapture, bool bStderr, uint64_t uOffset,
                                   size_t iMaxBytes, size_t *pSize, uint64_t *pBaseOffset,
                                   uint64_t *pNextOffset)
{
    xwork_process_capture_stream *pStream;
    uint64_t uAvailableEnd;
    size_t iStart;
    size_t iCopy;
    uint8_t *pCopy = NULL;
    if (pSize)
        *pSize = 0u;
    if (!pCapture || !xrtMutexLock(pCapture->pLock))
        return NULL;
    pStream = bStderr ? &pCapture->tStderr : &pCapture->tStdout;
    uAvailableEnd = pStream->uBaseOffset + pStream->tData.iLen;
    if (pBaseOffset)
        *pBaseOffset = pStream->uBaseOffset;
    if (uOffset < pStream->uBaseOffset)
        uOffset = pStream->uBaseOffset;
    if (uOffset > uAvailableEnd)
        uOffset = uAvailableEnd;
    iStart = (size_t)(uOffset - pStream->uBaseOffset);
    iCopy = pStream->tData.iLen - iStart;
    if (iCopy > iMaxBytes)
        iCopy = iMaxBytes;
    if (iCopy)
    {
        pCopy = (uint8_t *)malloc(iCopy);
        if (pCopy)
            memcpy(pCopy, pStream->tData.pData + iStart, iCopy);
        else
            iCopy = 0u;
    }
    if (pSize)
        *pSize = iCopy;
    if (pNextOffset)
        *pNextOffset = uOffset + iCopy;
    (void)xrtMutexUnlock(pCapture->pLock);
    return pCopy;
}

static void xwork__process_log_capture_destroy(struct xwork_process_log_capture *);
static xvalue *xwork__process_status_value(xwork_agent *, xwork_process_entry *);

static void xwork__process_entry_close(xwork_process_entry *pEntry)
{
    if (!pEntry)
        return;
    if (pEntry->eKind == XWORK_TASK_AGENT)
    {
        bool bDone = false;
        if (pEntry->pStateLock)
        {
            (void)xrtMutexLock(pEntry->pStateLock);
            bDone = pEntry->bDone;
            (void)xrtMutexUnlock(pEntry->pStateLock);
        }
        if (pEntry->pChildCancel && !bDone)
        {
            (void)xrtCancelRequest(pEntry->pChildCancel);
        }
        if (pEntry->pThread)
        {
            /* A completed worker may release the last agent/runtime reference
             * from its own thread. xthread keeps a self-reference, so detach
             * that already-completed worker instead of waiting on ourselves. */
            if (!bDone)
                (void)xrtThreadWait(pEntry->pThread);
            xrtThreadDestroy(pEntry->pThread);
        }
        if (pEntry->pChildCancel)
        {
            xrtCancelDestroy(pEntry->pChildCancel);
        }
    }
    else if (pEntry->pProcess)
    {
        /* Completed leaders can leave descendants holding capture pipes. */
        (void)xrtProcessKillTree(pEntry->pProcess);
        if (xwork__process_running(pEntry->pProcess))
        {
            if (xrtProcessWaitFor(pEntry->pProcess, 3000) != XWAIT_OK)
            {
                (void)xrtProcessKill(pEntry->pProcess);
                (void)xrtProcessWait(pEntry->pProcess);
            }
        }
        xwork__process_capture_destroy(pEntry->pCapture);
        xwork__process_log_capture_destroy(pEntry->LogCapture);
        xrtProcessDestroy(pEntry->pProcess);
    }
    if (pEntry->bProcessOwnerHeld)
    {
        pEntry->bProcessOwnerHeld = false;
        pEntry->OnProcessRelease(pEntry->pProcessOwnerData, pEntry->pProcessOwnerLease);
        pEntry->pProcessOwnerLease = NULL;
    }
    if (pEntry->ProcessMonitor)
    {
        if (!pEntry->bDone)
            (void)xrtThreadWait(pEntry->ProcessMonitor);
        xrtThreadDestroy(pEntry->ProcessMonitor);
    }
    xworkProcessOutputDestroy(pEntry->Output);
    free(pEntry->OutputNotice);
    if (pEntry->pStateLock)
    {
        (void)xrtMutexLock(pEntry->pStateLock);
        free(pEntry->sResult);
        pEntry->sResult = NULL;
        (void)xrtMutexUnlock(pEntry->pStateLock);
        (void)xrtMutexDestroy(pEntry->pStateLock);
    }
    free(pEntry->sCommand);
    free(pEntry->sLabel);
    free(pEntry->sNotify);
    free(pEntry->sOwnerSession);
    free(pEntry->sScheduleId);
    memset(pEntry, 0, sizeof(*pEntry));
}

void xwork__task_entry_ref(xwork_process_entry *pEntry)
{
    if (pEntry)
        (void)xwork__atomic_inc(&pEntry->iRefs);
}

void xwork__task_entry_release(xwork_process_entry *pEntry)
{
    if (!pEntry)
        return;
    if (xwork__atomic_dec(&pEntry->iRefs) == 0)
    {
        xwork__process_entry_close(pEntry);
        free(pEntry);
    }
}

void xwork__task_record_event_locked(xwork_process_entry *pEntry, xwork_task_event_kind eKind)
{
    size_t iSlot;
    xwork_task_event *pEvent;
    xwork_event tRuntimeEvent;
    if (!pEntry)
        return;
    ++pEntry->uRevision;
    if (pEntry->iEventCount < sizeof(pEntry->arrEvents) / sizeof(pEntry->arrEvents[0]))
    {
        iSlot = (pEntry->iEventStart + pEntry->iEventCount) %
                (sizeof(pEntry->arrEvents) / sizeof(pEntry->arrEvents[0]));
        ++pEntry->iEventCount;
    }
    else
    {
        iSlot = pEntry->iEventStart;
        pEntry->iEventStart =
            (pEntry->iEventStart + 1u) % (sizeof(pEntry->arrEvents) / sizeof(pEntry->arrEvents[0]));
    }
    pEvent = &pEntry->arrEvents[iSlot];
    pEvent->uTaskId = pEntry->uId;
    pEvent->uRevision = pEntry->uRevision;
    pEvent->eKind = eKind;
    pEvent->eState = pEntry->eState;
    pEvent->iTimestampUs = xllmUnixUs();
    memset(&tRuntimeEvent, 0, sizeof(tRuntimeEvent));
    tRuntimeEvent.eKind = XWORK_EVENT_TASK_UPDATED;
    tRuntimeEvent.iOccurredAtUs = pEvent->iTimestampUs;
    tRuntimeEvent.uAgentId = pEntry->uOwnerAgentId;
    tRuntimeEvent.uRunId = pEntry->uOwnerRunId;
    tRuntimeEvent.uTaskId = pEntry->uId;
    tRuntimeEvent.eTaskEvent = (uint32_t)eKind;
    tRuntimeEvent.eTaskKind = (uint32_t)pEntry->eKind;
    tRuntimeEvent.eTaskState = (uint32_t)pEntry->eState;
    tRuntimeEvent.uTaskRevision = pEntry->uRevision;
    tRuntimeEvent.sText = pEntry->sLabel;
    tRuntimeEvent.iTextLength = pEntry->sLabel ? strlen(pEntry->sLabel) : 0u;
    tRuntimeEvent.bSuccess = pEntry->eState == XWORK_TASK_SUCCEEDED;
    xwork__runtime_record_event(pEntry->pRuntime, &tRuntimeEvent);
}

void xwork__task_set_state_locked(xwork_process_entry *pEntry, xwork_task_state eState)
{
    int64_t iNow;
    if (!pEntry || pEntry->eState == eState)
        return;
    iNow = xllmUnixUs();
    pEntry->eState = eState;
    xwork__task_record_event_locked(pEntry, XWORK_TASK_EVENT_STATE_CHANGED);
    if (eState == XWORK_TASK_RUNNING && pEntry->iStartedAtUs == 0)
    {
        pEntry->iStartedAtUs = iNow;
    }
    if (eState >= XWORK_TASK_SUCCEEDED && pEntry->iEndedAtUs == 0)
    {
        pEntry->iEndedAtUs = iNow;
    }
}

void xwork__task_refresh_locked(xwork_process_entry *pEntry)
{
    xprocessstatus tExit;
    if (pEntry && pEntry->Output)
        return; /* Monitor publishes terminal state after drain/release. */
    if (!pEntry || pEntry->eKind != XWORK_TASK_PROCESS || pEntry->eState != XWORK_TASK_RUNNING ||
        !pEntry->pProcess || xwork__process_running(pEntry->pProcess))
        return;
    memset(&tExit, 0, sizeof(tExit));
    if (xrtProcessStatus(pEntry->pProcess, &tExit))
    {
        pEntry->bExitStatusValid = tExit.Kind != XPROCESS_EXIT_NONE;
        pEntry->iExitCode = tExit.Code;
        pEntry->iExitSignal = tExit.Signal;
        pEntry->iStopReason = tExit.Stop;
    }
    pEntry->bDone = true;
    pEntry->bSuccess =
        pEntry->bExitStatusValid && tExit.Kind == XPROCESS_EXIT_CODE && tExit.Code == 0;
    if (pEntry->bStopRequested)
    {
        xwork__task_set_state_locked(pEntry, XWORK_TASK_CANCELLED);
    }
    else if (tExit.Kind == XPROCESS_EXIT_LOST || !pEntry->bExitStatusValid)
    {
        xwork__task_set_state_locked(pEntry, XWORK_TASK_LOST);
    }
    else
    {
        xwork__task_set_state_locked(pEntry,
                                     pEntry->bSuccess ? XWORK_TASK_SUCCEEDED : XWORK_TASK_FAILED);
    }
    if (pEntry->uExitedUs == 0u)
        pEntry->uExitedUs = xllmClockUs();
}

void xwork__task_mark_stop_locked(xwork_process_entry *pEntry)
{
    if (pEntry->bStopRequested)
        return;
    pEntry->bStopRequested = true;
    xwork__task_record_event_locked(pEntry, XWORK_TASK_EVENT_CANCEL_REQUESTED);
}

/* Process stop calls do not dispatch child cancellation callbacks. Keep the
 * task lock through delivery and publication so a failed delivery cannot
 * expose a stop flag or let another reader misclassify the terminal state.
 * Agent cancellation tokens must still be dispatched outside this lock. */
bool xwork__task_stop_process_locked(xwork_process_entry *pEntry, const char *sMode)
{
    bool bRequested;
    if (!pEntry->pProcess)
        return pEntry->Output != NULL;
    if (pEntry->Output && !xwork__process_running(pEntry->pProcess))
        return true;
    if (strcmp(sMode, "interrupt") == 0)
        bRequested = xrtProcessInterrupt(pEntry->pProcess);
    else if (strcmp(sMode, "terminate") == 0)
        bRequested = xrtProcessTerminate(pEntry->pProcess);
    else if (strcmp(sMode, "kill") == 0)
        bRequested = xrtProcessKill(pEntry->pProcess);
    else
        bRequested = xrtProcessKillTree(pEntry->pProcess);
    if (bRequested)
        xwork__task_mark_stop_locked(pEntry);
    return bRequested;
}

bool xwork__task_running(xwork_process_entry *pEntry)
{
    bool bRunning;
    if (!pEntry)
        return false;
    if (xwork__atomic_load(&pEntry->iReady) == 0)
        return true;
    if (!pEntry->pStateLock || !xrtMutexLock(pEntry->pStateLock))
        return true;
    xwork__task_refresh_locked(pEntry);
    bRunning = pEntry->eState == XWORK_TASK_PENDING || pEntry->eState == XWORK_TASK_RUNNING;
    (void)xrtMutexUnlock(pEntry->pStateLock);
    return bRunning;
}

static bool xwork__task_entry_running(xwork_process_entry *pEntry)
{
    return xwork__task_running(pEntry);
}

xwork_process_entry *xwork__task_add(xwork_agent *pAgent, xwork_task_kind eKind,
                                     const xwork_tool_context *pContext)
{
    return xwork__process_add(pAgent, pContext, eKind);
}

/* Wait up to uWaitMs for a task of either kind to finish. */
static bool xwork__wait_task(xwork_agent *pAgent, xwork_process_entry *pEntry, uint64_t uWaitMs)
{
    uint64_t uDeadline = xllmDeadlineAfterUs(uWaitMs * UINT64_C(1000));
    while (xwork__task_entry_running(pEntry))
    {
        if (xllmDeadlineExpired(uDeadline))
            return false;
        if (xwork__is_cancelled(pAgent))
            return false;
        xrtSleep(5u);
    }
    return true;
}

void xwork__process_remove_entry(xwork_agent *pAgent, xwork_process_entry *pEntry)
{
    xwork_runtime *pRuntime;
    size_t i;
    bool bRemoved = false;
    if (!pAgent || !pEntry || !(pRuntime = pAgent->pRuntime))
        return;
    if (!xrtMutexLock(pRuntime->pLock))
        return;
    for (i = 0u; i < pRuntime->iTaskCount; ++i)
    {
        if (pRuntime->ppTasks[i] != pEntry)
            continue;
        pRuntime->ppTasks[i] = pRuntime->ppTasks[pRuntime->iTaskCount - 1u];
        pRuntime->ppTasks[pRuntime->iTaskCount - 1u] = NULL;
        --pRuntime->iTaskCount;
        bRemoved = true;
        break;
    }
    (void)xrtMutexUnlock(pRuntime->pLock);
    if (bRemoved)
        xwork__task_entry_release(pEntry); /* table reference */
}

void xwork__runtime_tasks_unit(xwork_runtime *pRuntime)
{
    xwork_process_entry **ppTasks;
    size_t iCount;
    size_t i;
    if (!pRuntime || !pRuntime->pLock || !xrtMutexLock(pRuntime->pLock))
        return;
    ppTasks = pRuntime->ppTasks;
    iCount = pRuntime->iTaskCount;
    pRuntime->ppTasks = NULL;
    pRuntime->iTaskCount = 0u;
    pRuntime->iTaskCap = 0u;
    (void)xrtMutexUnlock(pRuntime->pLock);
    for (i = 0u; i < iCount; ++i)
    {
        xwork_process_entry *Entry = ppTasks[i];
        if (Entry->ProcessMonitor)
        {
            xrtMutexLock(Entry->pStateLock);
            if (Entry->pProcess)
                (void)xwork__task_stop_process_locked(Entry, "kill_tree");
            xrtMutexUnlock(Entry->pStateLock);
            (void)xrtThreadWait(Entry->ProcessMonitor);
        }
        xwork__task_entry_release(Entry);
    }
    free(ppTasks);
}

xwork_process_entry *xwork__process_find(xwork_agent *pAgent, uint64_t uId, size_t *piIndex)
{
    xwork_runtime *pRuntime;
    size_t i;
    if (piIndex)
        *piIndex = (size_t)-1;
    if (!pAgent || !uId || !(pRuntime = pAgent->pRuntime) || !xrtMutexLock(pRuntime->pLock))
        return NULL;
    for (i = 0u; i < pRuntime->iTaskCount; ++i)
    {
        if (pRuntime->ppTasks[i]->uId == uId &&
            xwork__atomic_load(&pRuntime->ppTasks[i]->iReady) != 0)
        {
            xwork_process_entry *pFound = pRuntime->ppTasks[i];
            if (piIndex)
                *piIndex = i;
            xwork__task_entry_ref(pFound);
            (void)xrtMutexUnlock(pRuntime->pLock);
            return pFound;
        }
    }
    (void)xrtMutexUnlock(pRuntime->pLock);
    return NULL;
}

xwork_process_entry *xwork__process_add(xwork_agent *pAgent, const xwork_tool_context *pContext,
                                        xwork_task_kind eKind)
{
    xwork_runtime *pRuntime;
    xwork_process_entry *pNew = NULL;
    xwork_process_entry **ppNew;
    xwork_process_entry **ppReclaimed = NULL;
    size_t i;
    size_t iCap;
    size_t iOwned = 0u;
    size_t iReclaimed = 0u;
    if (!pAgent || !(pRuntime = pAgent->pRuntime) || !xrtMutexLock(pRuntime->pLock))
        return NULL;
    if (pRuntime->iTaskCount)
    {
        ppReclaimed = (xwork_process_entry **)calloc(pRuntime->iTaskCount, sizeof(*ppReclaimed));
        if (!ppReclaimed)
        {
            (void)xrtMutexUnlock(pRuntime->pLock);
            return NULL;
        }
    }
    for (i = 0u; i < pRuntime->iTaskCount; ++i)
    {
        if (pRuntime->ppTasks[i]->uOwnerAgentId == pAgent->uAgentId)
            ++iOwned;
    }
    for (i = pRuntime->iTaskCount; i > 0u && iOwned >= pAgent->uMaxManagedProcesses; --i)
    {
        xwork_process_entry *pCandidate = pRuntime->ppTasks[i - 1u];
        /* Only reclaim finished tasks whose completion notice was consumed;
         * an unclaimed notice is still owed to the host/model. */
        if (pCandidate->uOwnerAgentId == pAgent->uAgentId &&
            xwork__atomic_load(&pCandidate->iReady) != 0 &&
            !xwork__task_entry_running(pCandidate) && pCandidate->bNoticeTaken)
        {
            pRuntime->ppTasks[i - 1u] = pRuntime->ppTasks[pRuntime->iTaskCount - 1u];
            pRuntime->ppTasks[pRuntime->iTaskCount - 1u] = NULL;
            --pRuntime->iTaskCount;
            --iOwned;
            ppReclaimed[iReclaimed++] = pCandidate;
        }
    }
    if (iOwned >= pAgent->uMaxManagedProcesses || pRuntime->iTaskCount >= pRuntime->uMaxTasks)
    {
        goto done_locked;
    }
    if (pRuntime->iTaskCount == pRuntime->iTaskCap)
    {
        iCap = pRuntime->iTaskCap ? pRuntime->iTaskCap * 2u : 8u;
        if (iCap > pRuntime->uMaxTasks)
            iCap = pRuntime->uMaxTasks;
        ppNew = (xwork_process_entry **)realloc(pRuntime->ppTasks, iCap * sizeof(*ppNew));
        if (!ppNew)
            goto done_locked;
        pRuntime->ppTasks = ppNew;
        pRuntime->iTaskCap = iCap;
    }
    pNew = (xwork_process_entry *)calloc(1u, sizeof(*pNew));
    if (!pNew)
        goto done_locked;
    pNew->pStateLock = xrtMutexCreate();
    pNew->sOwnerSession = pAgent->sSessionPath ? xwork__strdup(pAgent->sSessionPath) : NULL;
    if (!pNew->pStateLock || (pAgent->sSessionPath && !pNew->sOwnerSession))
    {
        if (pNew->pStateLock)
            (void)xrtMutexDestroy(pNew->pStateLock);
        free(pNew->sOwnerSession);
        free(pNew);
        pNew = NULL;
        goto done_locked;
    }
    pNew->iRefs = 2; /* table reference plus unpublished caller reference */
    pNew->pRuntime = pRuntime;
    pNew->uId = ++pRuntime->uNextTaskId;
    if (pNew->uId == 0u)
        pNew->uId = ++pRuntime->uNextTaskId;
    pNew->uOwnerAgentId = pAgent->uAgentId;
    pNew->uOwnerRunId = pContext ? pContext->uRunId : 0u;
    pNew->uParentTaskId = pAgent->uParentTaskId;
    pNew->eKind = eKind;
    pNew->eState = XWORK_TASK_RUNNING;
    pNew->uRevision = 0u;
    pNew->iCreatedAtUs = xllmUnixUs();
    pNew->iStartedAtUs = pNew->iCreatedAtUs;
    pNew->uStartedUs = xllmClockUs();
    xwork__task_record_event_locked(pNew, XWORK_TASK_EVENT_CREATED);
    pRuntime->ppTasks[pRuntime->iTaskCount++] = pNew;
done_locked:
    (void)xrtMutexUnlock(pRuntime->pLock);
    for (i = 0u; i < iReclaimed; ++i)
    {
        xwork__task_entry_release(ppReclaimed[i]);
    }
    free(ppReclaimed);
    return pNew;
}

static const char *xwork__task_state_name(xwork_task_state eState)
{
    switch (eState)
    {
    case XWORK_TASK_PENDING:
        return "pending";
    case XWORK_TASK_RUNNING:
        return "running";
    case XWORK_TASK_SUCCEEDED:
        return "succeeded";
    case XWORK_TASK_FAILED:
        return "failed";
    case XWORK_TASK_CANCELLED:
        return "cancelled";
    case XWORK_TASK_TIMED_OUT:
        return "timed_out";
    case XWORK_TASK_LOST:
        return "lost";
    default:
        return "unknown";
    }
}

static bool xwork__append_process_status(xwork_agent *Agent, xwork_buf *Buffer,
                                         xwork_process_entry *Entry, uint32_t WaitMs,
                                         size_t MaxBytes)
{
    (void)MaxBytes;
    if (WaitMs)
        (void)xwork__wait_task(Agent, Entry, WaitMs);
    xvalue *V = xwork__process_status_value(Agent, Entry);
    char *Json = V ? xrtJsonStringify(V, false, NULL) : NULL;
    bool Ok = Json && xwork__buf_append_cstr(Buffer, Json) && xwork__buf_append_char(Buffer, '\n');
    if (Json)
        xrtFree(Json);
    xrtValueRelease(V);
    return Ok;
}

/* ------------------------------------------------------------------ */
/* Task-family helpers: argv/env parsing shared by exec and spawn.     */
/* ------------------------------------------------------------------ */

static void xwork__free_string_array(char **psItems, size_t iCount)
{
    size_t i;
    if (!psItems)
        return;
    for (i = 0u; i < iCount; ++i)
        free(psItems[i]);
    free(psItems);
}

/* Duplicate a JSON string array into owned C strings; 1..iMax entries.
 * Returns false for absent or malformed keys; nothing leaks on failure. */
static bool xwork__parse_string_array(xvalue *tArgs, const char *sKey, char ***ppsItems,
                                      size_t *piCount, size_t iMax)
{
    xvalue *tArray = xwork__json_get(tArgs, sKey);
    size_t i;
    *ppsItems = NULL;
    *piCount = 0u;
    if (!tArray || xrtValueType(tArray) != XVALUE_ARRAY)
        return false;
    *piCount = xrtValueCount(tArray);
    if (*piCount == 0u || *piCount > iMax)
    {
        *piCount = 0u;
        return false;
    }
    *ppsItems = (char **)calloc(*piCount, sizeof(char *));
    if (!*ppsItems)
    {
        *piCount = 0u;
        return false;
    }
    for (i = 0u; i < *piCount; ++i)
    {
        xstrview tText;
        xvalue *pItem = xrtValueArrayGet(tArray, i);
        if (!pItem || !xrtValueGetString(pItem, &tText) ||
            (!tText.Size && (i == 0u || strcmp(sKey, "argv") != 0)) ||
            (tText.Size && (!tText.Data || memchr(tText.Data, 0, tText.Size))))
        {
            xwork__free_string_array(*ppsItems, *piCount);
            *ppsItems = NULL;
            *piCount = 0u;
            return false;
        }
        (*ppsItems)[i] = tText.Size ? xwork__strndup(tText.Data, tText.Size) : xwork__strdup("");
        if (!(*ppsItems)[i])
        {
            xwork__free_string_array(*ppsItems, *piCount);
            *ppsItems = NULL;
            *piCount = 0u;
            return false;
        }
    }
    return true;
}

/* Parse "K=V" entries and append the PAGER/TERM defaults; env storage is
 * owned by the caller, the xprocessenv array borrows it plus literals. */
static bool xwork__build_env(xvalue *tArgs, xprocessenv **ppEnv, size_t *piEnvCount,
                             char ***ppsEnvStorage, size_t *piStorageCount)
{
    size_t iModel = 0u;
    size_t iTotal;
    xprocessenv *pEnv = NULL;
    char **psStorage = NULL;
    *ppEnv = NULL;
    *piEnvCount = 0u;
    *ppsEnvStorage = NULL;
    *piStorageCount = 0u;
    if (xwork__json_get(tArgs, "env"))
    {
        if (!xwork__parse_string_array(tArgs, "env", &psStorage, &iModel, 128u))
            return false;
    }
    iTotal = iModel + 2u;
    pEnv = (xprocessenv *)calloc(iTotal, sizeof(*pEnv));
    if (!pEnv)
    {
        xwork__free_string_array(psStorage, iModel);
        return false;
    }
    {
        size_t n = 0u;
        size_t i;
        for (i = 0u; i < iModel; ++i)
        {
            char *sEq = strchr(psStorage[i], '=');
            if (!sEq)
                continue; /* malformed entries are skipped */
            *sEq = '\0';
            pEnv[n].Name = psStorage[i];
            pEnv[n].Value = sEq + 1;
            ++n;
        }
        pEnv[n].Name = "PAGER";
        pEnv[n].Value = "cat";
        ++n;
        pEnv[n].Name = "TERM";
        pEnv[n].Value = "dumb";
        ++n;
        *piEnvCount = n;
    }
    *ppEnv = pEnv;
    *ppsEnvStorage = psStorage;
    *piStorageCount = iModel;
    return true;
}

/* argv[0] is the program; xprocessconfig.Args excludes it. */
static void xwork__apply_argv_config(xprocessconfig *pConfig, char **psArgv, size_t iArgvCount)
{
    pConfig->Target = XPROCESS_EXEC;
    pConfig->Program = psArgv[0];
    pConfig->Args = (const cstr *)(iArgvCount > 1u ? psArgv + 1 : NULL);
    pConfig->ArgCount = iArgvCount - 1u;
}

/* Space-joined preview for status display. */
static char *xwork__argv_preview(char **psArgv, size_t iArgvCount)
{
    xwork_buf tBuf = {0};
    size_t i;
    for (i = 0u; i < iArgvCount; ++i)
    {
        if (i && !xwork__buf_append_char(&tBuf, ' '))
        {
            xwork__buf_unit(&tBuf);
            return NULL;
        }
        if (!xwork__buf_append_cstr(&tBuf, psArgv[i][0] ? psArgv[i] : "\"\""))
        {
            xwork__buf_unit(&tBuf);
            return NULL;
        }
        if (tBuf.iLen > 2048u)
            break;
    }
    return xwork__buf_detach(&tBuf);
}

static bool xwork__process_wait_all_ready(xwork_agent *pAgent, const uint64_t *puIds, size_t iCount,
                                          bool bAll, uint32_t uTimeoutMs)
{
    uint64_t uDeadline = xllmDeadlineAfterUs((uint64_t)uTimeoutMs * UINT64_C(1000));
    for (;;)
    {
        xwork_process_entry *pEntry;
        size_t iReady = 0u;
        size_t i;
        for (i = 0u; i < iCount; ++i)
        {
            pEntry = xwork__process_find(pAgent, puIds[i], NULL);
            if (pEntry && !xwork__task_entry_running(pEntry))
                ++iReady;
            xwork__task_entry_release(pEntry);
        }
        if (bAll ? iReady == iCount : iReady > 0u)
            return true;
        if (xllmDeadlineExpired(uDeadline))
            return false;
        if (xwork__is_cancelled(pAgent))
            return false;
        xrtSleep(10u);
    }
}

size_t xworkAgentTakeTaskNotices(xwork_agent *pAgent, xwork_task_notice *pNotices, size_t iCapacity)
{
    xwork_runtime *pRuntime;
    size_t iTaken = 0u;
    size_t i;
    if (!pAgent || !(pRuntime = pAgent->pRuntime) || !xrtMutexLock(pRuntime->pLock))
    {
        return 0u;
    }
    for (i = 0u; i < pRuntime->iTaskCount && iTaken < iCapacity; ++i)
    {
        xwork_process_entry *pEntry = pRuntime->ppTasks[i];
        if (xwork__atomic_load(&pEntry->iReady) == 0)
            continue;
        if (pEntry->uOwnerAgentId != 0u && pEntry->uOwnerAgentId != pAgent->uAgentId)
            continue;
        if (pEntry->bNoticeTaken || xwork__task_entry_running(pEntry))
            continue;
        if (pEntry->uExitedUs == 0u)
            pEntry->uExitedUs = xllmClockUs();
        pNotices[iTaken].uTaskId = pEntry->uId;
        pNotices[iTaken].eKind = pEntry->eKind;
        if (pEntry->eKind == XWORK_TASK_AGENT || pEntry->eKind == XWORK_TASK_SCHEDULED)
        {
            /* In-process tasks: success flag from their worker/host; the
             * final report rides the preview slot (borrowed). */
            pNotices[iTaken].iExitCode = pEntry->bSuccess ? 0 : 1;
            pNotices[iTaken].bExitedCleanly = pEntry->bSuccess;
            pNotices[iTaken].sPreview =
                pEntry->sResult && pEntry->sResult[0] ? pEntry->sResult : pEntry->sCommand;
        }
        else
        {
            pNotices[iTaken].iExitCode = pEntry->iExitCode;
            pNotices[iTaken].bExitedCleanly = pEntry->bSuccess;
            pNotices[iTaken].sPreview =
                pEntry->OutputNotice ? pEntry->OutputNotice : pEntry->sCommand;
        }
        pNotices[iTaken].sNotify = pEntry->sNotify; /* borrowed */
        if (pEntry->pStateLock && xrtMutexLock(pEntry->pStateLock))
        {
            pEntry->bNoticeTaken = true;
            xwork__task_record_event_locked(pEntry, XWORK_TASK_EVENT_NOTICE_TAKEN);
            (void)xrtMutexUnlock(pEntry->pStateLock);
        }
        else
        {
            pEntry->bNoticeTaken = true;
        }
        ++iTaken;
    }
    (void)xrtMutexUnlock(pRuntime->pLock);
    return iTaken;
}

bool xworkTaskWatchdog(xwork_agent *pAgent, xwork_watchdog_digest *pDigest)
{
    xwork_runtime *pRuntime;
    static const uint64_t uUncollectedNudgeMs = 60000u;
    uint64_t uNow = xllmClockUs();
    uint64_t uNextWakeUs = 0u;
    size_t i;
    if (!pDigest)
    {
        return false;
    }
    memset(pDigest, 0, sizeof(*pDigest));
    if (!pAgent || !(pRuntime = pAgent->pRuntime) || !xrtMutexLock(pRuntime->pLock))
    {
        return false;
    }
    for (i = 0u; i < pRuntime->iTaskCount; ++i)
    {
        xwork_process_entry *pEntry = pRuntime->ppTasks[i];
        if (xwork__atomic_load(&pEntry->iReady) == 0)
            continue;
        if (pEntry->uOwnerAgentId != 0u && pEntry->uOwnerAgentId != pAgent->uAgentId)
            continue;
        bool bRunning = xwork__task_entry_running(pEntry);
        if (bRunning)
        {
            ++pDigest->iRunningTasks;
            if (pEntry->uRemindAfterMs != 0u)
            {
                uint64_t uElapsedUs = uNow - pEntry->uStartedUs;
                uint64_t uRemindUs = pEntry->uRemindAfterMs * UINT64_C(1000);
                if (uElapsedUs >= uRemindUs)
                {
                    ++pDigest->iStalledTasks;
                    pDigest->bShouldWake = true;
                }
                else
                {
                    uint64_t uDue = uRemindUs - uElapsedUs;
                    if (uNextWakeUs == 0u || uDue < uNextWakeUs)
                        uNextWakeUs = uDue;
                }
            }
        }
        else
        {
            if (pEntry->uExitedUs == 0u)
                pEntry->uExitedUs = uNow;
            if (!pEntry->bNoticeTaken)
            {
                ++pDigest->iUncollectedNotices;
                /* One gentle nudge per task; the model decides after that. */
                if (!pEntry->bNudged &&
                    uNow - pEntry->uExitedUs >= uUncollectedNudgeMs * UINT64_C(1000))
                {
                    pEntry->bNudged = true;
                    pDigest->bShouldWake = true;
                }
                else if (!pEntry->bNudged)
                {
                    uint64_t uDue =
                        uUncollectedNudgeMs * UINT64_C(1000) - (uNow - pEntry->uExitedUs);
                    if (uNextWakeUs == 0u || uDue < uNextWakeUs)
                        uNextWakeUs = uDue;
                }
            }
        }
    }
    pDigest->uNextWakeMs = uNextWakeUs != 0u ? (uNextWakeUs + 999u) / 1000u : 0u;
    (void)xrtMutexUnlock(pRuntime->pLock);
    return true;
}

/* Process execution owns byte capture; file tools own output consumption.
 * Included after the task-table helpers and argv/env parser in xwork_tools.c. */
typedef struct xwork_process_log_capture_stream
{
    struct xwork_process_log_capture *pOwner;
    xprocessstream eStream;
    xthread *pThread;
} xwork_process_log_capture_stream;
typedef struct xwork_process_log_capture
{
    xprocess *pProcess;
    xwork_process_output *Output;
    xwork_process_log_capture_stream tStdout, tStderr;
} xwork_process_log_capture;

char *xwork__process_destination(xwork_agent *Agent, xvalue *Args, const char *Key)
{
    xvalue *V = xwork__json_get(Args, Key);
    if (!V)
        return NULL;
    xstrview Text = {0};
    if (!xrtValueGetString(V, &Text) || !Text.Size || Text.Size > 8192u ||
        memchr(Text.Data, 0, Text.Size) || !xrtUtf8Valid(Text, NULL))
        return NULL;
    const char *Cwd = xwork__json_text(Args, "cwd");
    if (!Cwd || !Cwd[0])
        Cwd = ".";
    char *Joined = (xrtPathIsAbs(Text.Data) || Text.Data[0] == '@') ? xrtStrDup(Text.Data)
                                                                    : xrtPathJoin(Cwd, Text.Data);
    char *Result = Joined ? xwork__strdup(Joined) : NULL;
    if (Joined)
        xrtFree(Joined);
    (void)Agent;
    return Result;
}

static xwork_process_output *xwork__process_output_create(xwork_agent *Agent, xvalue *Args,
                                                          bool Background, bool Merge,
                                                          xwork_error *Error)
{
    xwork_process_output_options Opt;
    xworkProcessOutputOptionsInit(&Opt);
    xroot Root = NULL;
    char *Dest[2] = {0};
    bool Valid = true;
    Opt.FilesFromStart = Background;
    Opt.MergeStderr = Merge;
    Opt.InlineBytes = Agent->iMaxInlineToolBytes;
    if (Opt.InlineBytes < 256u)
        Opt.InlineBytes = 256u;
    if (Opt.InlineBytes > XWORK_OUTPUT_INLINE)
        Opt.InlineBytes = XWORK_OUTPUT_INLINE;
    Opt.MaxBytes = xwork__json_u64(Args, "max_output_bytes", XWORK_OUTPUT_MAX, &Valid);
    if (!Valid || !Opt.MaxBytes || Opt.MaxBytes > XWORK_OUTPUT_STORE_MAX)
        goto invalid;
    for (size_t i = 0; i < 2u; i++)
    {
        const char *Key = i ? "stderr_path" : "stdout_path";
        if (!xwork__json_get(Args, Key))
            continue;
        char *Logical = xwork__process_destination(Agent, Args, Key);
        if (!Logical || !strncmp(Logical, "@results", 8u))
        {
            free(Logical);
            goto invalid;
        }
        Dest[i] = xwork__resolve_path(Agent, Logical, Error);
        free(Logical);
        if (!Dest[i])
            goto done;
    }
    Opt.StdoutPath = Dest[0];
    Opt.StderrPath = Dest[1];
    if (Agent->bAllowArtifactWrites)
    {
        if (Agent->pArtifactStore)
            Root = xrtRootOpenIn(Agent->pArtifactStore->pDirectory, ".");
        else
        {
            char *Absolute = xwork__resolve_path(Agent, Agent->sArtifactDirectory, Error);
            if (Absolute)
            {
                Root = xwork__artifact_open_directory(Agent, Absolute, ".", Error);
                free(Absolute);
            }
        }
    }
    Opt.Directory = Root;
    {
        xwork_process_output *Output = xworkProcessOutputCreate(&Opt, Error);
        if (Root)
            xrtRootClose(Root);
        free(Dest[0]);
        free(Dest[1]);
        return Output;
    }
invalid:
    xwork__set_error(Error, XWORK_ERROR_INVALID_ARGUMENT,
                     "invalid output paths, merge mode, or max_output_bytes (1..1073741824)");
done:
    if (Root)
        xrtRootClose(Root);
    free(Dest[0]);
    free(Dest[1]);
    return NULL;
}

static bool xwork__process_run_output(xprocessstream Stream, xbytesview Bytes, void *User)
{
    return xworkProcessOutputWrite(User, Stream == XPROCESS_STDERR, Bytes.Data, Bytes.Size);
}

static int32 xwork__process_log_capture_thread(void *User)
{
    xwork_process_log_capture_stream *Stream = User;
    unsigned char Chunk[16384];
    for (;;)
    {
        int64 Read =
            xrtProcessRead(Stream->pOwner->pProcess, Stream->eStream, Chunk, sizeof(Chunk));
        if (Read < 0)
        {
            xwork_process_output *O = Stream->pOwner->Output;
            if (xrtMutexLock(O->Lock))
            {
                O->Streams[Stream->eStream == XPROCESS_STDERR ? 1u : 0u].Lost = true;
                xwork__output_error(O, "output_read_failed");
                xrtMutexUnlock(O->Lock);
            }
            break;
        }
        if (!Read)
            break;
        if (!xworkProcessOutputWrite(Stream->pOwner->Output, Stream->eStream == XPROCESS_STDERR,
                                     Chunk, (size_t)Read))
            break;
    }
    return 0;
}

static void xwork__process_log_capture_destroy(xwork_process_log_capture *Capture)
{
    if (!Capture)
        return;
    if (Capture->tStdout.pThread)
    {
        xrtThreadWait(Capture->tStdout.pThread);
        xrtThreadDestroy(Capture->tStdout.pThread);
    }
    if (Capture->tStderr.pThread)
    {
        xrtThreadWait(Capture->tStderr.pThread);
        xrtThreadDestroy(Capture->tStderr.pThread);
    }
    free(Capture);
}

static xwork_process_log_capture *
xwork__process_log_capture_create(xprocess *Process, xwork_process_output *Output, bool Stderr)
{
    xwork_process_log_capture *C = calloc(1, sizeof(*C));
    if (!C)
        return NULL;
    C->pProcess = Process;
    C->Output = Output;
    C->tStdout.pOwner = C;
    C->tStdout.eStream = XPROCESS_STDOUT;
    C->tStderr.pOwner = C;
    C->tStderr.eStream = XPROCESS_STDERR;
    C->tStdout.pThread = xrtThreadCreate(xwork__process_log_capture_thread, &C->tStdout, 0u);
    if (C->tStdout.pThread && Stderr)
        C->tStderr.pThread = xrtThreadCreate(xwork__process_log_capture_thread, &C->tStderr, 0u);
    if (!C->tStdout.pThread || (Stderr && !C->tStderr.pThread))
    {
        (void)xrtProcessKillTree(Process);
        xwork__process_log_capture_destroy(C);
        return NULL;
    }
    return C;
}

bool xwork__process_output_read(xwork_process_output *O, bool Err, uint64_t Offset, size_t Max,
                                unsigned char **Bytes, size_t *Size, uint64_t *Next, bool *Dropped)
{
    *Bytes = NULL;
    *Size = 0;
    *Next = Offset;
    *Dropped = false;
    if (!O || !xrtMutexLock(O->Lock))
        return false;
    xwork_output_stream *S = &O->Streams[Err ? 1u : 0u];
    uint64_t Available = S->Path ? S->FileBytes : S->Stored;
    if (Offset > Available)
        Offset = Available;
    *Next = Offset;
    *Dropped = S->Lost;
    size_t N = Available - Offset > Max ? Max : (size_t)(Available - Offset);
    bool Ok = true;
    if (N)
    {
        *Bytes = malloc(N);
        if (!*Bytes)
        {
            Ok = false;
            goto done;
        }
        if (S->Path)
        {
            xfileoptions Opt;
            xrtFileOptionsInit(&Opt);
            Opt.Flags = XFILE_READ | XFILE_NOFOLLOW;
            xfile F = S->Explicit
                          ? xrtFileOpen(S->Path, &Opt)
                          : xrtRootFileOpen(O->Run, Err ? "stderr.log" : "stdout.log", &Opt);
            size_t Got = 0;
            Ok = F && xrtReadAtFull(F, Offset, *Bytes, N, &Got) && Got == N;
            if (F)
                xrtClose(F);
        }
        else
            memcpy(*Bytes, S->Preview.pData + (size_t)Offset, N);
        if (Ok)
        {
            *Size = N;
            *Next = Offset + N;
        }
    }
done:
    if (!Ok)
    {
        free(*Bytes);
        *Bytes = NULL;
    }
    xrtMutexUnlock(O->Lock);
    return Ok;
}

static xvalue *xwork__process_status_value(xwork_agent *Agent, xwork_process_entry *Entry)
{
    if (!xrtMutexLock(Entry->pStateLock))
        return NULL;
    xwork__task_refresh_locked(Entry);
    bool Complete = Entry->eState >= XWORK_TASK_SUCCEEDED;
    xvalue *V = Entry->Output ? xworkProcessOutputSnapshot(Entry->Output, false) : xrtValueObject();
    bool Ok = V && xwork__output_uint(V, "task_id", Entry->uId) &&
              xwork__output_put(V, "state", xwork__task_state_name(Entry->eState)) &&
              xwork__output_put(V, "kind",
                                Entry->eKind == XWORK_TASK_PROCESS ? "process"
                                : Entry->eKind == XWORK_TASK_AGENT ? "agent"
                                                                   : "scheduled") &&
              xwork__output_bool(V, "finished", Complete);
    if (Ok && Entry->bExitStatusValid)
        Ok = xrtValueObjectSetNew(V, XRT_STR_LITERAL("exit_code"), xrtValueInt(Entry->iExitCode)) &&
             xrtValueObjectSetNew(V, XRT_STR_LITERAL("signal"), xrtValueInt(Entry->iExitSignal)) &&
             xrtValueObjectSetNew(V, XRT_STR_LITERAL("stop_reason"),
                                  xrtValueInt(Entry->iStopReason));
    if (Ok && Complete && Entry->eKind != XWORK_TASK_PROCESS && Entry->sResult)
    {
        Ok = xwork__output_bool(V, "result_truncated", Entry->bResultDropped) &&
             xwork__output_uint(V, "result_offset", Entry->uResultBaseOffset);
        size_t Len = strlen(Entry->sResult);
        if (Len > Agent->iMaxInlineToolBytes && Agent->bAllowArtifactWrites && !Entry->Output)
        {
            xvalue *Args = xrtValueObject();
            Entry->Output = xwork__process_output_create(Agent, Args, true, false, NULL);
            xrtValueRelease(Args);
            if (Entry->Output)
            {
                xworkProcessOutputWrite(Entry->Output, false, Entry->sResult, Len);
                xworkProcessOutputFinish(Entry->Output);
            }
        }
        if (Entry->Output)
        {
            xvalue *Output = xworkProcessOutputSnapshot(Entry->Output, false);
            if (Output)
                Ok = xrtValueObjectSetNew(V, XRT_STR_LITERAL("result"), Output);
            else
                Ok = false;
        }
        else
            Ok = xwork__output_put(V, "result", Entry->sResult);
    }
    xrtMutexUnlock(Entry->pStateLock);
    if (!Ok)
    {
        xrtValueRelease(V);
        V = NULL;
    }
    return V;
}

static int32 xwork__process_monitor(void *User)
{
    xwork_process_entry *Entry = User;
    xprocessstatus Exit = {0};
    (void)xrtProcessWait(Entry->pProcess);
    (void)xrtProcessStatus(Entry->pProcess, &Exit);
    /* A finished leader must not leave descendants holding its output pipes. */
    (void)xrtProcessKillTree(Entry->pProcess);
    xwork__process_log_capture_destroy(Entry->LogCapture);
    Entry->LogCapture = NULL;
    xworkProcessOutputFinish(Entry->Output);
    xvalue *Summary = xworkProcessOutputSnapshot(Entry->Output, false);
    if (Summary)
    {
        char *Json = xrtJsonStringify(Summary, false, NULL);
        Entry->OutputNotice = Json ? xwork__strdup(Json) : NULL;
        if (Json)
            xrtFree(Json);
        xrtValueRelease(Summary);
    }
    xrtMutexLock(Entry->pStateLock);
    xrtProcessDestroy(Entry->pProcess);
    Entry->pProcess = NULL;
    bool Held = Entry->bProcessOwnerHeld;
    Entry->bProcessOwnerHeld = false;
    xrtMutexUnlock(Entry->pStateLock);
    if (Held)
        Entry->OnProcessRelease(Entry->pProcessOwnerData, Entry->pProcessOwnerLease);
    xrtMutexLock(Entry->pStateLock);
    Entry->pProcessOwnerLease = NULL;
    Entry->bExitStatusValid = Exit.Kind != XPROCESS_EXIT_NONE;
    Entry->iExitCode = Exit.Code;
    Entry->iExitSignal = Exit.Signal;
    Entry->iStopReason = Exit.Stop;
    Entry->bDone = true;
    Entry->bSuccess = Exit.Kind == XPROCESS_EXIT_CODE && !Exit.Code;
    xwork_task_state State = Entry->bStopRequested ? XWORK_TASK_CANCELLED
                             : Exit.Kind == XPROCESS_EXIT_LOST || !Entry->bExitStatusValid
                                 ? XWORK_TASK_LOST
                             : Entry->bSuccess ? XWORK_TASK_SUCCEEDED
                                               : XWORK_TASK_FAILED;
    xwork__task_set_state_locked(Entry, State);
    Entry->uExitedUs = xllmClockUs();
    xrtMutexUnlock(Entry->pStateLock);
    xwork__task_entry_release(Entry);
    return 0;
}

xwork_result xwork__tool_spawn(void *User, const xwork_tool_context *Context, const char *Arguments,
                               xwork_tool_output *Output, xwork_error *Error)
{
    xwork_agent *Agent = User;
    xvalue *Args = xwork__json_parse_object(Arguments);
    char **Argv = NULL, **EnvStorage = NULL;
    size_t Argc = 0, EnvCount = 0, EnvStorageCount = 0;
    xprocessenv *Env = NULL;
    char *Cwd = NULL;
    xwork_process_entry *Entry = NULL;
    xwork_result Result = XWORK_RESULT_ERROR;
    bool Valid = true, Merge = false, Started = false;
    uint64_t Remind = 0;
    if (!Args)
        return xwork__tool_fail(Output, "invalid arguments: expected a JSON object");
    if (!xwork__parse_string_array(Args, "argv", &Argv, &Argc, 256u) || !Argv)
    {
        Result = xwork__tool_fail(Output, "argv must contain 1-256 strings");
        goto done;
    }
    Merge = xwork__json_bool(Args, "merge_stderr", false, &Valid);
    if (!Valid)
        goto invalid;
    const char *Notify = xwork__json_text(Args, "notify");
    if (Notify && strlen(Notify) > 500u)
        goto invalid;
    Remind = xwork__json_u64(Args, "remind_after_ms", 0u, &Valid);
    if (!Valid || Remind > 3600000u)
        goto invalid;
    const char *Logical = xwork__json_text(Args, "cwd");
    if (!Logical || !Logical[0])
        Logical = ".";
    Cwd = xwork__resolve_path(Agent, Logical, Error);
    if (!Cwd)
        goto fail;
    xrtClearError();
    if (!xrtDirExists(Cwd))
    {
        if (!xrtGetError())
            xrtSetErrorInfo(XERR_NOT_FOUND, "xwork.process", 0, "working directory does not exist");
        Result = xwork__tool_process_fail(Output, "spawn/cwd", Argv[0], Cwd, xrtGetError());
        goto done;
    }
    if (!xwork__build_env(Args, &Env, &EnvCount, &EnvStorage, &EnvStorageCount))
        goto done;
    Entry = xwork__process_add(Agent, Context, XWORK_TASK_PROCESS);
    if (!Entry)
    {
        Result = xwork__tool_fail(Output, "managed task limit reached");
        goto done;
    }
    Entry->Output = xwork__process_output_create(Agent, Args, true, Merge, Error);
    if (!Entry->Output)
        goto fail;
    if (Agent->OnProcessAcquire)
    {
        if (!Agent->OnProcessAcquire(Agent->pProcessOwnerData, &Entry->pProcessOwnerLease))
        {
            Result =
                xwork__tool_process_fail(Output, "spawn/resources", Argv[0], Cwd, xrtGetError());
            goto done;
        }
        Entry->OnProcessRelease = Agent->OnProcessRelease;
        Entry->pProcessOwnerData = Agent->pProcessOwnerData;
        Entry->bProcessOwnerHeld = true;
    }
    Entry->sCommand = xwork__argv_preview(Argv, Argc);
    Entry->sLabel = Entry->sCommand ? xwork__strdup(Entry->sCommand) : NULL;
    Entry->sNotify = Notify ? xwork__strdup(Notify) : NULL;
    Entry->uRemindAfterMs = Remind;
    if (!Entry->sCommand || !Entry->sLabel || (Notify && !Entry->sNotify))
        goto done;
    /* Allocate the complete response before starting a process with effects. */
    xvalue *V = xwork__process_status_value(Agent, Entry);
    char *Json = NULL;
    if (V)
    {
        xwork__output_put(V, "state", "running");
        Json = xrtJsonStringify(V, false, NULL);
    }
    bool Prepared = Json && xworkToolOutputSet(Output, true, Json);
    if (Json)
        xrtFree(Json);
    xrtValueRelease(V);
    if (!Prepared)
        goto done;
    xprocessconfig Config;
    xrtProcessConfigInit(&Config);
    xwork__apply_argv_config(&Config, Argv, Argc);
    Config.WorkDir = Cwd;
    Config.InheritEnv = true;
    Config.Env = Env;
    Config.EnvCount = EnvCount;
    Config.NewGroup = true;
    Config.HideWindow = true;
    Config.Stdin.Mode = XPROCESS_IO_PIPE;
    Config.Stdout.Mode = XPROCESS_IO_PIPE;
    Config.Stderr.Mode = Merge ? XPROCESS_IO_MERGE : XPROCESS_IO_PIPE;
    xrtClearError();
    Entry->pProcess = xrtProcessSpawn(&Config);
    if (!Entry->pProcess)
    {
        Result = xwork__tool_process_fail(Output, "spawn", Argv[0], Cwd, xrtGetError());
        goto done;
    }
    Entry->LogCapture = xwork__process_log_capture_create(Entry->pProcess, Entry->Output, !Merge);
    if (!Entry->LogCapture)
        goto done;
    xwork__atomic_store(&Entry->iReady, 1);
    xwork__task_entry_ref(Entry);
    Entry->ProcessMonitor = xrtThreadCreate(xwork__process_monitor, Entry, 0u);
    if (!Entry->ProcessMonitor)
    {
        xwork__task_entry_release(Entry);
        goto done;
    }
    Started = true;
    Result = XWORK_RESULT_OK;
    goto done;
invalid:
    Result = xwork__tool_fail(Output, "invalid merge_stderr, notify or remind_after_ms");
    goto done;
fail:
    Result = xwork__tool_fail(Output, Error && Error->sMessage[0]
                                          ? Error->sMessage
                                          : "process output storage unavailable");
done:
    if (Entry && !Started)
        xwork__process_remove_entry(Agent, Entry);
    xwork__task_entry_release(Entry);
    xwork__free_string_array(Argv, Argc);
    xwork__free_string_array(EnvStorage, EnvStorageCount);
    free(Env);
    free(Cwd);
    xrtValueRelease(Args);
    return Result;
}

static xwork_result xwork__tool_poll(void *User, const xwork_tool_context *Context,
                                     const char *Arguments, xwork_tool_output *Output,
                                     xwork_error *Error)
{
    xwork_agent *Agent = User;
    xvalue *Args = xwork__json_parse_object(Arguments);
    bool Valid = false;
    uint64_t Id = Args ? xwork__json_u64(Args, "task_id", 0u, &Valid) : 0;
    (void)Context;
    if (!Valid || !Id)
    {
        xrtValueRelease(Args);
        return xwork__tool_fail(Output, "positive task_id is required");
    }
    xwork_process_entry *Entry = xwork__process_find(Agent, Id, NULL);
    xrtValueRelease(Args);
    if (!Entry)
        return xwork__tool_fail(Output, "unknown task_id");
    xvalue *V = xwork__process_status_value(Agent, Entry);
    char *Json = V ? xrtJsonStringify(V, false, NULL) : NULL;
    bool Ok = Json && xworkToolOutputSet(Output, true, Json);
    if (Json)
        xrtFree(Json);
    xrtValueRelease(V);
    xwork__task_entry_release(Entry);
    if (!Ok)
        xwork__set_error(Error, XWORK_ERROR_OUT_OF_MEMORY, "cannot build task status");
    return Ok ? XWORK_RESULT_OK : XWORK_RESULT_ERROR;
}

static xwork_result xwork__tool_exec(void *User, const xwork_tool_context *Context,
                                     const char *Arguments, xwork_tool_output *Output,
                                     xwork_error *Error)
{
    xwork_agent *Agent = User;
    xvalue *Args = xwork__json_parse_object(Arguments);
    char **Argv = NULL, **EnvStorage = NULL;
    size_t Argc = 0, EnvCount = 0, EnvStorageCount = 0;
    xprocessenv *Env = NULL;
    char *Cwd = NULL;
    xwork_process_output *Capture = NULL;
    xwork_result Result = XWORK_RESULT_ERROR;
    bool Valid = true, Merge = false, Expected = false;
    void *Lease = NULL;
    bool Held = false;
    xprocessresult Process = {0};
    if (!Args)
        return xwork__tool_fail(Output, "invalid arguments: expected a JSON object");
    if (!xwork__parse_string_array(Args, "argv", &Argv, &Argc, 256u) || !Argv)
    {
        Result = xwork__tool_fail(Output, "argv must contain 1-256 strings");
        goto done;
    }
    uint64_t Timeout = xwork__json_u64(Args, "timeout_ms", Agent->uCommandTimeoutMs, &Valid);
    if (!Valid || !Timeout || Timeout > 3600000u)
    {
        Result = xwork__tool_fail(Output, "timeout_ms must be between 1 and 3600000");
        goto done;
    }
    Merge = xwork__json_bool(Args, "merge_stderr", false, &Valid);
    if (!Valid)
    {
        Result = xwork__tool_fail(Output, "merge_stderr must be boolean");
        goto done;
    }
    xvalue *Codes = xwork__json_get(Args, "expected_exit_codes");
    if (Codes)
    {
        if (xrtValueType(Codes) != XVALUE_ARRAY || !xrtValueCount(Codes) ||
            xrtValueCount(Codes) > 32u)
            goto invalid_codes;
        for (size_t i = 0; i < xrtValueCount(Codes); i++)
        {
            int64_t Code = 0;
            if (!xrtValueGetInt(xrtValueArrayGet(Codes, i), &Code) || Code < INT32_MIN ||
                Code > INT32_MAX)
                goto invalid_codes;
        }
    }
    const char *Logical = xwork__json_text(Args, "cwd");
    if (!Logical || !Logical[0])
        Logical = ".";
    Cwd = xwork__resolve_path(Agent, Logical, Error);
    if (!Cwd)
        goto storage_error;
    xrtClearError();
    if (!xrtDirExists(Cwd))
    {
        if (!xrtGetError())
            xrtSetErrorInfo(XERR_NOT_FOUND, "xwork.process", 0, "working directory does not exist");
        Result = xwork__tool_process_fail(Output, "exec/cwd", Argv[0], Cwd, xrtGetError());
        goto done;
    }
    if (!xwork__build_env(Args, &Env, &EnvCount, &EnvStorage, &EnvStorageCount))
        goto done;
    Capture = xwork__process_output_create(Agent, Args, false, Merge, Error);
    if (!Capture)
        goto storage_error;
    xprocessconfig Config;
    xrtProcessConfigInit(&Config);
    xwork__apply_argv_config(&Config, Argv, Argc);
    Config.WorkDir = Cwd;
    Config.InheritEnv = true;
    Config.Env = Env;
    Config.EnvCount = EnvCount;
    Config.NewGroup = true;
    Config.HideWindow = true;
    Config.Stdin.Mode = XPROCESS_IO_NULL;
    Config.Stdout.Mode = XPROCESS_IO_PIPE;
    Config.Stderr.Mode = Merge ? XPROCESS_IO_MERGE : XPROCESS_IO_PIPE;
    xprocessrunoptions Options;
    xrtProcessRunOptionsInit(&Options);
    uint64_t Scope =
        Context && Context->uDeadline ? Context->uDeadline : xwork__active_deadline(Agent);
    uint64_t Deadline = xllmDeadlineAfterUs(Timeout * UINT64_C(1000));
    if (Scope != XLLM_DEADLINE_NEVER && Scope < Deadline)
        Deadline = Scope;
    Options.Timeout = xllmDeadlineWaitMs(Deadline);
    Options.Cancel = Context && Context->pCancel ? Context->pCancel : xwork__active_cancel(Agent);
    Options.StdoutLimit = Options.StderrLimit = 0;
    Options.Overflow = XPROCESS_OVERFLOW_KEEP_FIRST;
    Options.Output = xwork__process_run_output;
    Options.UserData = Capture;
    if (Agent->OnProcessAcquire)
    {
        if (!Agent->OnProcessAcquire(Agent->pProcessOwnerData, &Lease))
        {
            Result =
                xwork__tool_process_fail(Output, "exec/resources", Argv[0], Cwd, xrtGetError());
            goto done;
        }
        Held = true;
    }
    xrtClearError();
    bool Ran = xrtProcessRun(&Config, &Options, &Process);
    /* Format the native cause before closing output files can replace the
     * thread's error. Retaining partial logs must not hide the launch error. */
    if (!Ran)
        Result =
            xwork__tool_process_fail(Output, "exec/start-or-capture", Argv[0], Cwd, xrtGetError());
    xworkProcessOutputFinish(Capture);
    if (!Ran)
    {
        if (xrtMutexLock(Capture->Lock))
        {
            xwork__output_error(Capture, "process_capture_failed");
            xrtMutexUnlock(Capture->Lock);
        }
        xvalue *Partial = xworkProcessOutputSnapshot(Capture, true);
        char *Json = Partial ? xrtJsonStringify(Partial, false, NULL) : NULL;
        if (Json)
        {
            xwork_buf Detail = {0};
            if (xwork__buf_append_cstr(&Detail, Output->sContent ? Output->sContent : "") &&
                xwork__buf_append_char(&Detail, '\n') && xwork__buf_append_cstr(&Detail, Json))
                (void)xworkToolOutputSet(Output, false, Detail.pData);
            xwork__buf_unit(&Detail);
            xrtFree(Json);
        }
        xrtValueRelease(Partial);
        goto done;
    }
    Expected = Process.Wait == XWAIT_OK && Process.Status.Kind == XPROCESS_EXIT_CODE;
    if (Expected && Codes)
    {
        Expected = false;
        for (size_t i = 0; i < xrtValueCount(Codes); i++)
        {
            int64_t Code = 0;
            xrtValueGetInt(xrtValueArrayGet(Codes, i), &Code);
            if (Code == Process.Status.Code)
                Expected = true;
        }
    }
    else if (Expected)
        Expected = Process.Status.Code == 0;
    xvalue *V = xworkProcessOutputSnapshot(Capture, true);
    char *Json = NULL;
    bool Built =
        V &&
        xrtValueObjectSetNew(V, XRT_STR_LITERAL("exit_code"), xrtValueInt(Process.Status.Code)) &&
        xrtValueObjectSetNew(V, XRT_STR_LITERAL("signal"), xrtValueInt(Process.Status.Signal)) &&
        xwork__output_uint(V, "duration_ms", Process.Duration) &&
        xwork__output_bool(V, "exit_expected", Expected) &&
        xwork__output_put(V, "state",
                          Process.Wait == XWAIT_CANCELLED ? "cancelled"
                          : Process.Wait == XWAIT_TIMEOUT ? "timed_out"
                          : Expected                      ? "succeeded"
                                                          : "failed");
    if (Built)
        Json = xrtJsonStringify(V, false, NULL);
    bool Complete = false;
    if (V)
        xrtValueGetBool(xrtValueObjectGet(V, XRT_STR_LITERAL("output_complete")), &Complete);
    if (Json && xworkToolOutputSet(Output, Expected && Complete, Json))
        Result = XWORK_RESULT_OK;
    if (Json)
        xrtFree(Json);
    xrtValueRelease(V);
    /* Preserve partial output in the tool receipt even on scope cancellation. */
    if (Process.Wait == XWAIT_CANCELLED)
    {
        xwork__set_error(Error, XWORK_ERROR_CANCELLED,
                         "agent operation cancelled during execution");
        Result = XWORK_RESULT_CANCELLED;
    }
    else if (Process.Wait == XWAIT_TIMEOUT && Scope != XLLM_DEADLINE_NEVER &&
             xllmDeadlineExpired(Scope))
    {
        xwork__set_error(Error, XWORK_ERROR_TIMEOUT,
                         "agent operation deadline exceeded during execution");
        Result = XWORK_RESULT_TIMEOUT;
    }
    goto done;
invalid_codes:
    Result =
        xwork__tool_fail(Output, "expected_exit_codes must contain 1-32 signed 32-bit integers");
    goto done;
storage_error:
    Result = xwork__tool_fail(Output, Error && Error->sMessage[0]
                                          ? Error->sMessage
                                          : "process output storage unavailable");
done:
    if (Held)
        Agent->OnProcessRelease(Agent->pProcessOwnerData, Lease);
    xrtProcessResultUnit(&Process);
    xworkProcessOutputDestroy(Capture);
    xwork__free_string_array(Argv, Argc);
    xwork__free_string_array(EnvStorage, EnvStorageCount);
    free(Env);
    free(Cwd);
    xrtValueRelease(Args);
    return Result;
}

static xwork_result xwork__tool_stdin(void *pUserData, const xwork_tool_context *pContext,
                                      const char *sArgumentsJson, xwork_tool_output *pOutput,
                                      xwork_error *pError)
{
    xwork_agent *pAgent = (xwork_agent *)pUserData;
    xvalue *tArgs = xwork__json_parse_object(sArgumentsJson);
    uint64_t uId;
    const char *sInput;
    bool bValid;
    bool bNewline;
    bool bClose;
    xwork_process_entry *pEntry = NULL;
    xprocess *Process = NULL;
    xwork_buf tInput = {0};
    xwork_buf tOutput = {0};
    int64_t iWritten = 0;
    xwork_result eResult = XWORK_RESULT_ERROR;
    (void)pContext;
    if (!tArgs)
        return xwork__tool_fail(pOutput, "invalid arguments: expected a JSON object");
    uId = xwork__json_u64(tArgs, "task_id", 0u, &bValid);
    if (!bValid || !uId)
    {
        eResult = xwork__tool_fail(pOutput, "positive task_id is required");
        goto cleanup;
    }
    sInput = xwork__json_text(tArgs, "input");
    bNewline = xwork__json_bool(tArgs, "append_newline", false, &bValid);
    if (!bValid)
    {
        eResult = xwork__tool_fail(pOutput, "append_newline must be boolean");
        goto cleanup;
    }
    bClose = xwork__json_bool(tArgs, "close_stdin", false, &bValid);
    if (!bValid)
    {
        eResult = xwork__tool_fail(pOutput, "close_stdin must be boolean");
        goto cleanup;
    }
    if (!sInput && !bClose)
    {
        eResult = xwork__tool_fail(pOutput, "input or close_stdin=true is required");
        goto cleanup;
    }
    pEntry = xwork__process_find(pAgent, uId, NULL);
    if (!pEntry)
    {
        eResult = xwork__tool_fail(pOutput, "unknown or released task_id");
        goto cleanup;
    }
    if (pEntry->eKind != XWORK_TASK_PROCESS)
    {
        eResult = xwork__tool_fail(pOutput, "stdin applies to process tasks only");
        goto cleanup;
    }
    xrtMutexLock(pEntry->pStateLock);
    Process = pEntry->pProcess ? xrtProcessRef(pEntry->pProcess) : NULL;
    xrtMutexUnlock(pEntry->pStateLock);
    if (!xwork__process_running(Process))
    {
        eResult = xwork__tool_fail(pOutput, "process has already exited");
        goto cleanup;
    }
    if (pEntry->bStdinClosed)
    {
        eResult = xwork__tool_fail(pOutput, "process stdin is already closed");
        goto cleanup;
    }
    if (sInput && (sInput[0] || bNewline))
    {
        if (!xwork__buf_append_cstr(&tInput, sInput) ||
            (bNewline && !xwork__buf_append_char(&tInput, '\n')))
            goto oom;
        iWritten = xrtProcessWrite(Process, tInput.pData, tInput.iLen);
        if (iWritten < 0 || (size_t)iWritten != tInput.iLen)
        {
            eResult = xwork__tool_fail(pOutput, "failed to write complete input to process");
            goto cleanup;
        }
    }
    if (bClose)
    {
        if (!xrtProcessClose(Process, XPROCESS_STDIN))
        {
            eResult = xwork__tool_fail(pOutput, "failed to close process stdin");
            goto cleanup;
        }
        pEntry->bStdinClosed = true;
    }
    if (!xwork__buf_appendf(&tOutput, "task_id: %llu\nwrote: %lld bytes\nstdin: %s\n",
                            (unsigned long long)uId, (long long)iWritten,
                            pEntry->bStdinClosed ? "closed" : "open") ||
        !xworkToolOutputSet(pOutput, true, tOutput.pData))
        goto oom;
    eResult = XWORK_RESULT_OK;
    goto cleanup;
oom:
    xwork__set_error(pError, XWORK_ERROR_OUT_OF_MEMORY, "failed to prepare managed process input");
cleanup:
    if (Process)
        xrtProcessDestroy(Process);
    xwork__task_entry_release(pEntry);
    if (tArgs)
        xrtValueRelease(tArgs);
    xwork__buf_unit(&tInput);
    xwork__buf_unit(&tOutput);
    return eResult;
}

static xwork_result xwork__tool_stop(void *pUserData, const xwork_tool_context *pContext,
                                     const char *sArgumentsJson, xwork_tool_output *pOutput,
                                     xwork_error *pError)
{
    xwork_agent *pAgent = (xwork_agent *)pUserData;
    xvalue *tArgs = xwork__json_parse_object(sArgumentsJson);
    uint64_t uId;
    uint64_t uWaitMs;
    const char *sMode;
    bool bValid;
    bool bRelease;
    bool bRequested = true;
    bool bRequestAgent = false;
    bool bRunning;
    xerror *pStopError = NULL;
    xwork_process_entry *pEntry = NULL;
    xwork_buf tOutput = {0};
    xwork_result eResult = XWORK_RESULT_ERROR;
    (void)pContext;
    if (!tArgs)
        return xwork__tool_fail(pOutput, "invalid arguments: expected a JSON object");
    uId = xwork__json_u64(tArgs, "task_id", 0u, &bValid);
    if (!bValid || !uId)
    {
        eResult = xwork__tool_fail(pOutput, "positive task_id is required");
        goto cleanup;
    }
    sMode = xwork__json_text(tArgs, "mode");
    if (!sMode || !sMode[0])
        sMode = "terminate";
    if (strcmp(sMode, "interrupt") != 0 && strcmp(sMode, "terminate") != 0 &&
        strcmp(sMode, "kill") != 0 && strcmp(sMode, "kill_tree") != 0)
    {
        eResult =
            xwork__tool_fail(pOutput, "mode must be interrupt, terminate, kill, or kill_tree");
        goto cleanup;
    }
    uWaitMs = xwork__json_u64(tArgs, "wait_ms", 3000u, &bValid);
    if (!bValid || uWaitMs > 30000u)
    {
        eResult = xwork__tool_fail(pOutput, "wait_ms must be between 0 and 30000");
        goto cleanup;
    }
    bRelease = xwork__json_bool(tArgs, "release", false, &bValid);
    if (!bValid)
    {
        eResult = xwork__tool_fail(pOutput, "release must be boolean");
        goto cleanup;
    }
    pEntry = xwork__process_find(pAgent, uId, NULL);
    if (!pEntry)
    {
        eResult = xwork__tool_fail(pOutput, "unknown or released task_id");
        goto cleanup;
    }
    if (!pEntry->pStateLock || !xrtMutexLock(pEntry->pStateLock))
    {
        eResult = xwork__tool_fail(pOutput, "task state is unavailable");
        goto cleanup;
    }
    xwork__task_refresh_locked(pEntry);
    if (pEntry->eState == XWORK_TASK_PENDING || pEntry->eState == XWORK_TASK_RUNNING)
    {
        if (pEntry->eKind == XWORK_TASK_PROCESS)
        {
            xrtClearError();
            bRequested = xwork__task_stop_process_locked(pEntry, sMode);
            if (!bRequested)
            {
                pStopError = xrtErrorRef(xrtGetError());
                xwork__task_refresh_locked(pEntry);
                bRequested =
                    pEntry->eState >= XWORK_TASK_SUCCEEDED && pEntry->eState <= XWORK_TASK_LOST;
            }
        }
        else
        {
            xwork__task_mark_stop_locked(pEntry);
            bRequestAgent = pEntry->eKind == XWORK_TASK_AGENT;
            if (pEntry->eKind == XWORK_TASK_SCHEDULED)
            {
                pEntry->bDone = true;
                xwork__task_set_state_locked(pEntry, XWORK_TASK_CANCELLED);
            }
        }
    }
    (void)xrtMutexUnlock(pEntry->pStateLock);
    if (bRequestAgent && pEntry->pChildCancel)
        (void)xrtCancelRequest(pEntry->pChildCancel);
    if (!bRequested)
    {
        eResult = xwork__tool_process_fail(pOutput, sMode, NULL, NULL, pStopError);
        goto cleanup;
    }
    if (uWaitMs && xwork__task_entry_running(pEntry))
    {
        (void)xwork__wait_task(pAgent, pEntry, uWaitMs);
    }
    if (!xwork__append_process_status(pAgent, &tOutput, pEntry, (uint32_t)uWaitMs, 64u * 1024u))
        goto oom;
    bRunning = xwork__task_entry_running(pEntry);
    if (!xworkToolOutputSet(pOutput, !bRunning, tOutput.pData))
        goto oom;
    if (bRelease && !bRunning)
        xwork__process_remove_entry(pAgent, pEntry);
    eResult = XWORK_RESULT_OK;
    goto cleanup;
oom:
    xwork__set_error(pError, XWORK_ERROR_OUT_OF_MEMORY, "failed to stop or report managed process");
cleanup:
    xrtErrorFree(pStopError);
    xwork__task_entry_release(pEntry);
    if (tArgs)
        xrtValueRelease(tArgs);
    xwork__buf_unit(&tOutput);
    return eResult;
}

static xwork_result xwork__tool_wait(void *pUserData, const xwork_tool_context *pContext,
                                     const char *sArgumentsJson, xwork_tool_output *pOutput,
                                     xwork_error *pError)
{
    xwork_agent *pAgent = (xwork_agent *)pUserData;
    xvalue *tArgs = xwork__json_parse_object(sArgumentsJson);
    const char *sMode;
    xvalue *tIds;
    bool bValid;
    bool bAll;
    uint64_t uTimeoutMs;
    uint64_t *puIds = NULL;
    size_t iIdCount = 0u;
    size_t i;
    xwork_buf tOutput = {0};
    xwork_result eResult = XWORK_RESULT_ERROR;
    (void)pContext;
    if (!tArgs)
        return xwork__tool_fail(pOutput, "invalid arguments: expected a JSON object");
    tIds = xwork__json_get(tArgs, "task_ids");
    if (!tIds || xrtValueType(tIds) != XVALUE_ARRAY || (iIdCount = xrtValueCount(tIds)) == 0u ||
        iIdCount > 32u)
    {
        eResult = xwork__tool_fail(pOutput, "task_ids must be an array of 1-32 integers");
        goto cleanup;
    }
    puIds = (uint64_t *)calloc(iIdCount, sizeof(*puIds));
    if (!puIds)
        goto oom;
    for (i = 0u; i < iIdCount; ++i)
    {
        int64_t iId = 0;
        xvalue *pItem = xrtValueArrayGet(tIds, i);
        if (!pItem || !xrtValueGetInt(pItem, &iId) || iId < 1)
        {
            eResult = xwork__tool_fail(pOutput, "task_ids must contain positive integers");
            goto cleanup;
        }
        puIds[i] = (uint64_t)iId;
        {
            xwork_process_entry *pCheck = xwork__process_find(pAgent, puIds[i], NULL);
            if (pCheck)
                xwork__task_entry_release(pCheck);
            if (!pCheck)
            {
                xwork_buf tBad = {0};
                if (xwork__buf_appendf(&tBad, "unknown or released task_id %llu",
                                       (unsigned long long)puIds[i]) &&
                    tBad.pData)
                {
                    eResult = xwork__tool_fail(pOutput, tBad.pData);
                }
                else
                {
                    eResult = xwork__tool_fail(pOutput, "unknown or released task_id");
                }
                xwork__buf_unit(&tBad);
                goto cleanup;
            }
        }
    }
    sMode = xwork__json_text(tArgs, "mode");
    if (!sMode || !sMode[0])
        sMode = "any";
    if (strcmp(sMode, "any") != 0 && strcmp(sMode, "all") != 0)
    {
        eResult = xwork__tool_fail(pOutput, "mode must be any or all");
        goto cleanup;
    }
    bAll = strcmp(sMode, "all") == 0;
    uTimeoutMs = xwork__json_u64(tArgs, "timeout_ms", 120000u, &bValid);
    if (!bValid || uTimeoutMs > 600000u)
    {
        eResult = xwork__tool_fail(pOutput, "timeout_ms must be between 0 and 600000");
        goto cleanup;
    }
    bool Ready = xwork__process_wait_all_ready(pAgent, puIds, iIdCount, bAll, (uint32_t)uTimeoutMs);
    xvalue *Envelope = xrtValueObject();
    xvalue *Items = xrtValueArray();
    if (!Envelope || !Items)
    {
        xrtValueRelease(Envelope);
        xrtValueRelease(Items);
        goto oom;
    }
    bool Built = true;
    for (i = 0; i < iIdCount; i++)
    {
        xwork_process_entry *Entry = xwork__process_find(pAgent, puIds[i], NULL);
        xvalue *Item = Entry ? xwork__process_status_value(pAgent, Entry) : NULL;
        xwork__task_entry_release(Entry);
        if (!Item || !xrtValueArrayAppendNew(Items, Item))
        {
            Built = false;
            break;
        }
    }
    if (Built)
    {
        Built = xrtValueObjectSetNew(Envelope, XRT_STR_LITERAL("tasks"), Items);
        Items = NULL;
    }
    if (Built)
        Built = xwork__output_bool(Envelope, "ready", Ready);
    char *Json = Built ? xrtJsonStringify(Envelope, false, NULL) : NULL;
    bool Written = Json && xworkToolOutputSet(pOutput, true, Json);
    if (Json)
        xrtFree(Json);
    xrtValueRelease(Items);
    xrtValueRelease(Envelope);
    if (!Written)
        goto oom;
    eResult = XWORK_RESULT_OK;
    goto cleanup;
oom:
    xwork__set_error(pError, XWORK_ERROR_OUT_OF_MEMORY, "failed to build wait output");
cleanup:
    free(puIds);
    if (tArgs)
        xrtValueRelease(tArgs);
    xwork__buf_unit(&tOutput);
    return eResult;
}

bool xworkAgentRegisterBuiltinReadOnlyTools(xwork_agent *pAgent, xwork_error *pError)
{
    static const xwork_tool_definition arrTools[] = {
        {"read",
         "Read files within the host-selected filesystem scope, explicit mounted @alias/ paths, or "
         "read-only @results/ files. Full access permits native paths outside the workspace. A "
         "relative path resolves under the workspace. Use mounted memory paths exactly as supplied "
         "in the system prompt. Common path formatting mistakes and uniquely matching missing "
         "memory topics are corrected only after verifying the target exists. Text is paged by "
         "start_line/max_lines and max_bytes (default 4096, clamped to 256-8192). Continue from "
         "the exact offset in a truncated result; offset takes precedence over start_line. Images "
         "are attached for viewing. Never assume a partial page is the whole file.",
         "{\"type\":\"object\",\"properties\":{\"path\":{\"type\":\"string\"},\"start_line\":{"
         "\"type\":\"integer\",\"minimum\":1},\"max_lines\":{\"type\":\"integer\",\"minimum\":1,"
         "\"maximum\":10000},\"max_bytes\":{\"type\":\"integer\",\"minimum\":256,\"maximum\":8192},"
         "\"offset\":{\"type\":\"integer\",\"minimum\":0}},\"required\":[\"path\"],"
         "\"additionalProperties\":false}",
         true, XWORK_TOOL_EFFECT_READ_ONLY, xwork__tool_read, NULL, NULL, NULL, NULL, 0u, NULL,
         NULL, false, NULL},
    };
    size_t i;
    xwork_tool_definition tTool;
    if (!pAgent)
    {
        xwork__set_error(pError, XWORK_ERROR_INVALID_ARGUMENT, "agent is null");
        return false;
    }
    for (i = 0u; i < sizeof(arrTools) / sizeof(arrTools[0]); ++i)
    {
        tTool = arrTools[i];
        tTool.pUserData = pAgent;
        tTool.sSource = "builtin";
        tTool.OnDescribePermissions = xwork__describe_builtin_permissions;
        if (!xworkAgentRegisterTool(pAgent, &tTool, pError))
            return false;
        {
            xwork_tool_concurrency_config tConcurrency;
            xworkToolConcurrencyConfigInit(&tConcurrency);
            tConcurrency.sToolName = tTool.sName;
            tConcurrency.bParallelSafe = true;
            if (!xworkAgentConfigureToolConcurrency(pAgent, &tConcurrency, pError))
                return false;
        }
    }
    return xwork__register_explore_tools(pAgent, pError);
}

bool xworkAgentRegisterBuiltinTools(xwork_agent *pAgent, xwork_error *pError)
{
    static const xwork_tool_definition arrTools[] = {
        {"write",
         "Create, overwrite, or append a UTF-8 workspace file. Parent directories are created "
         "automatically and reported. Prefer edit for small changes.",
         "{\"type\":\"object\",\"properties\":{\"path\":{\"type\":\"string\"},\"content\":{"
         "\"type\":\"string\"},\"mode\":{\"type\":\"string\",\"enum\":[\"overwrite\",\"create\","
         "\"append\"]}},\"required\":[\"path\",\"content\"],\"additionalProperties\":false}",
         true, XWORK_TOOL_EFFECT_WORKSPACE_WRITE, xwork__tool_write, NULL, NULL, NULL, NULL, 0u,
         NULL, NULL, false, NULL},
        {"edit",
         "Apply exact text edits to one file in a single atomic pass. Each old_text must match the "
         "original file uniquely (or set replace_all); 0 or multiple matches return candidate "
         "context lines for self-correction.",
         "{\"type\":\"object\",\"properties\":{\"path\":{\"type\":\"string\"},\"edits\":{\"type\":"
         "\"array\",\"minItems\":1,\"maxItems\":64,\"items\":{\"type\":\"object\",\"properties\":{"
         "\"old_text\":{\"type\":\"string\"},\"new_text\":{\"type\":\"string\"},\"replace_all\":{"
         "\"type\":\"boolean\"}},\"required\":[\"old_text\",\"new_text\"],\"additionalProperties\":"
         "false}}},\"required\":[\"path\",\"edits\"],\"additionalProperties\":false}",
         true, XWORK_TOOL_EFFECT_WORKSPACE_WRITE, xwork__tool_edit, NULL, NULL, NULL, NULL, 0u,
         NULL, NULL, false, NULL},
        {"spawn",
         "Start a background process. Returns task_id and complete stdout/stderr log paths "
         "immediately. Use status/wait for completion and read/grep for output; resources are "
         "released automatically.",
         "{\"type\":\"object\",\"properties\":{\"argv\":{\"type\":\"array\",\"minItems\":1,"
         "\"maxItems\":256,\"items\":{\"type\":\"string\"}},\"cwd\":{\"type\":\"string\"},\"env\":{"
         "\"type\":\"array\",\"maxItems\":128,\"items\":{\"type\":\"string\"}},\"merge_stderr\":{"
         "\"type\":\"boolean\"},\"notify\":{\"type\":\"string\",\"maxLength\":500},\"remind_after_"
         "ms\":{\"type\":\"integer\",\"minimum\":0,\"maximum\":3600000},\"stdout_path\":{\"type\":"
         "\"string\"},\"stderr_path\":{\"type\":\"string\"},\"max_output_bytes\":{\"type\":"
         "\"integer\",\"minimum\":1,\"maximum\":1073741824}},\"required\":[\"argv\"],"
         "\"additionalProperties\":false}",
         true, XWORK_TOOL_EFFECT_PROCESS, xwork__tool_spawn, NULL, NULL, NULL, NULL, 0u, NULL, NULL,
         false, NULL},
        {"status",
         "Get task status and output file paths. Read/grep the files; output reads do not consume "
         "it.",
         "{\"type\":\"object\",\"properties\":{\"task_id\":{\"type\":\"integer\",\"minimum\":1}},"
         "\"required\":[\"task_id\"],\"additionalProperties\":false}",
         true, XWORK_TOOL_EFFECT_READ_ONLY, xwork__tool_poll, NULL, NULL, NULL, NULL, 0u, NULL,
         NULL, false, NULL},
        {"wait",
         "Block until any (default) or all of the given tasks exit, returning their new output and "
         "exit status. An expired timeout returns early with still-running states.",
         "{\"type\":\"object\",\"properties\":{\"task_ids\":{\"type\":\"array\",\"minItems\":1,"
         "\"maxItems\":32,\"items\":{\"type\":\"integer\",\"minimum\":1}},\"mode\":{\"type\":"
         "\"string\",\"enum\":[\"any\",\"all\"]},\"timeout_ms\":{\"type\":\"integer\",\"minimum\":"
         "0,\"maximum\":600000}},\"required\":[\"task_ids\"],\"additionalProperties\":false}",
         true, XWORK_TOOL_EFFECT_READ_ONLY, xwork__tool_wait, NULL, NULL, NULL, NULL, 0u, NULL,
         NULL, false, NULL},
        {"stdin",
         "Write text to a process task's stdin, optionally appending a newline and/or closing "
         "stdin.",
         "{\"type\":\"object\",\"properties\":{\"task_id\":{\"type\":\"integer\",\"minimum\":1},"
         "\"input\":{\"type\":\"string\"},\"append_newline\":{\"type\":\"boolean\"},\"close_"
         "stdin\":{\"type\":\"boolean\"}},\"required\":[\"task_id\"],\"additionalProperties\":"
         "false}",
         true, XWORK_TOOL_EFFECT_PROCESS, xwork__tool_stdin, NULL, NULL, NULL, NULL, 0u, NULL, NULL,
         false, NULL},
        {"stop",
         "Stop a task. Modes interrupt/terminate/kill/kill_tree apply to processes; a subagent is "
         "cancelled cooperatively. Success means the task actually stopped.",
         "{\"type\":\"object\",\"properties\":{\"task_id\":{\"type\":\"integer\",\"minimum\":1},"
         "\"mode\":{\"type\":\"string\",\"enum\":[\"interrupt\",\"terminate\",\"kill\",\"kill_"
         "tree\"]},\"wait_ms\":{\"type\":\"integer\",\"minimum\":0,\"maximum\":30000}},"
         "\"required\":[\"task_id\"],\"additionalProperties\":false}",
         true, XWORK_TOOL_EFFECT_PROCESS, xwork__tool_stop, NULL, NULL, NULL, NULL, 0u, NULL, NULL,
         false, NULL},
        {"exec",
         "Run argv directly to completion. Returns exit status and short stdout/stderr; long "
         "output is saved to files for read/grep. Optional output paths are exclusive, relative to "
         "cwd. Capture failure does not mean the command did not run.",
         "{\"type\":\"object\",\"properties\":{\"argv\":{\"type\":\"array\",\"minItems\":1,"
         "\"maxItems\":256,\"items\":{\"type\":\"string\"}},\"cwd\":{\"type\":\"string\"},\"env\":{"
         "\"type\":\"array\",\"maxItems\":128,\"items\":{\"type\":\"string\"}},\"timeout_ms\":{"
         "\"type\":\"integer\",\"minimum\":1,\"maximum\":3600000},\"merge_stderr\":{\"type\":"
         "\"boolean\"},\"expected_exit_codes\":{\"type\":\"array\",\"minItems\":1,\"maxItems\":32,"
         "\"items\":{\"type\":\"integer\",\"minimum\":-2147483648,\"maximum\":2147483647}},"
         "\"stdout_path\":{\"type\":\"string\"},\"stderr_path\":{\"type\":\"string\"},\"max_output_"
         "bytes\":{\"type\":\"integer\",\"minimum\":1,\"maximum\":1073741824}},\"required\":["
         "\"argv\"],\"additionalProperties\":false}",
         true, XWORK_TOOL_EFFECT_PROCESS, xwork__tool_exec, NULL, NULL, NULL, NULL, 0u, NULL, NULL,
         false, NULL}};
    size_t i;
    xwork_tool_definition tTool;
    if (!pAgent)
    {
        xwork__set_error(pError, XWORK_ERROR_INVALID_ARGUMENT, "agent is null");
        return false;
    }
    if (!xworkAgentRegisterBuiltinReadOnlyTools(pAgent, pError))
        return false;
    for (i = 0u; i < sizeof(arrTools) / sizeof(arrTools[0]); ++i)
    {
        tTool = arrTools[i];
        tTool.pUserData = pAgent;
        tTool.sSource = "builtin";
        tTool.OnDescribePermissions = xwork__describe_builtin_permissions;
        if (!xworkAgentRegisterTool(pAgent, &tTool, pError))
            return false;
        {
            xwork_tool_concurrency_config tConcurrency;
            xworkToolConcurrencyConfigInit(&tConcurrency);
            tConcurrency.sToolName = tTool.sName;
            tConcurrency.bParallelSafe = true;
            if (!xworkAgentConfigureToolConcurrency(pAgent, &tConcurrency, pError))
                return false;
        }
    }
    if (xwork__runtime_mcp_enabled_count(pAgent->pRuntime) != 0u &&
        !xworkAgentRegisterMcpDiscoveryTools(pAgent, pError))
        return false;
    return true;
}
