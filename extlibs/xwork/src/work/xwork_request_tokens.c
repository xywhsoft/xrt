#include "../internal/xwork_internal.h"

/* Request identity survives the interval before a callback's result exists.
 * It shares the host-selected receipt scope, never the model's workspace.
 * Atomic non-replacing publication decides races before any external I/O. */
#define XWORK_REQUEST_RECORD_BYTES 1024u
#define XWORK_REQUEST_SCOPE_BYTES 256u

void xworkToolRequestTokenInit(xwork_tool_request_token *Token)
{
    if (!Token)
        return;
    memset(Token, 0, sizeof(*Token));
    Token->uSize = sizeof(*Token);
    Token->uAbiVersion = XWORK_ABI_VERSION;
}

static bool xwork__request_scope_valid(const char *Scope)
{
    size_t i, Size = Scope ? strlen(Scope) : 0u;
    if (!Size || Size > XWORK_REQUEST_SCOPE_BYTES || !xrtUtf8Valid(xrtStrView(Scope), NULL))
        return false;
    for (i = 0u; i < Size; ++i)
        if ((unsigned char)Scope[i] < 0x20u || (unsigned char)Scope[i] == 0x7fu)
            return false;
    return true;
}

/* 0: absent; 1: valid; -1: damaged, inaccessible or bound to another scope. */
static int xwork__request_load(xwork_agent *Agent, const char *Key, const char *Scope,
                               xwork_tool_request_token *Token, xwork_error *Error)
{
    char Name[96], Text[XWORK_REQUEST_RECORD_BYTES + 1u];
    xfileoptions Options;
    xfile File = NULL;
    xvalue *Object = NULL;
    xjsonreadconfig Config;
    uint64_t Size = 0u, Version, Created;
    size_t Read = 0u, i;
    const char *StoredKey;
    const char *StoredScope;
    const char *Id;
    bool Valid;
    int Result = -1;
    snprintf(Name, sizeof(Name), "requests/%s.json", Key);
    xrtFileOptionsInit(&Options);
    Options.Flags = XFILE_READ | XFILE_NOFOLLOW;
    File = xrtRootFileOpen(Agent->pArtifactStore->pDirectory, Name, &Options);
    if (!File && xrtErrorKind(xrtGetError()) == XERR_NOT_FOUND)
    {
        xrtClearError();
        return 0;
    }
    if (!File || !xrtFileSize(File, &Size) || !Size || Size > XWORK_REQUEST_RECORD_BYTES ||
        !xrtReadFull(File, Text, (size_t)Size, &Read) || Read != Size)
        goto done;
    Text[Size] = 0;
    xrtJsonReadConfigInit(&Config);
    Config.MaxInputBytes = XWORK_REQUEST_RECORD_BYTES;
    Config.MaxDepth = 3u;
    Config.MaxValues = 20u;
    Object = xrtJsonRead(xrtStrViewN(Text, (size_t)Size), &Config);
    Version = xwork__json_u64(Object, "version", 0u, &Valid);
    if (!Valid || Version != 1u)
        goto done;
    Created = xwork__json_u64(Object, "created_at", 0u, &Valid);
    if (!Valid || !Created || Created > INT64_MAX)
        goto done;
    StoredKey = xwork__json_text(Object, "key");
    StoredScope = xwork__json_text(Object, "scope");
    Id = xwork__json_text(Object, "id");
    if (!StoredKey || strcmp(StoredKey, Key) || !StoredScope || !Id || strlen(Id) != 32u)
        goto done;
    for (i = 0u; i < 32u; ++i)
        if (!(Id[i] >= '0' && Id[i] <= '9') && !(Id[i] >= 'a' && Id[i] <= 'f'))
            goto done;
    if (strcmp(StoredScope, Scope))
    {
        xwork__set_error(
            Error, XWORK_ERROR_POLICY,
            "saved request belongs to another service or account; no repeated request was sent");
        goto done;
    }
    xworkToolRequestTokenInit(Token);
    memcpy(Token->sId, Id, 33u);
    Token->iCreatedAt = (int64_t)Created;
    Token->bPersistent = true;
    Token->bRecovered = true;
    Result = 1;
done:
    if (File)
        (void)xrtClose(File);
    xrtValueRelease(Object);
    if (Result < 0 && (!Error || Error->eCode == XWORK_ERROR_NONE))
        xwork__set_error(
            Error, XWORK_ERROR_IO,
            "saved request identity is damaged or unavailable; no replacement request was created");
    return Result;
}

bool xworkToolResultWriterPrepareRequest(xwork_tool_result_writer *Writer,
                                         const xwork_tool_context *Context, const char *Scope,
                                         xwork_tool_request_token *Token, xwork_error *Error)
{
    xwork_tool_request_token Prepared;
    unsigned char Random[16];
    char Key[65], Name[80], Temporary[64];
    xroot Directory = NULL;
    xfile File = NULL;
    xvalue *Object = NULL;
    char *Json = NULL;
    size_t Size = 0u, i;
    bool Persistent, TemporaryOwned = false, Ok = false;
    int Loaded;
    xworkErrorInit(Error);
    if (!Token || Token->uSize < sizeof(*Token) || Token->uAbiVersion != XWORK_ABI_VERSION)
    {
        xwork__set_error(Error, XWORK_ERROR_INVALID_ARGUMENT, "invalid request token output");
        return false;
    }
    xworkToolRequestTokenInit(Token);
    if (!Writer || Writer->uMagic != XWORK_RESULT_WRITER_MAGIC || Writer->bFailed || !Context ||
        Context != Writer->pContext || Context->pAgent != Writer->pAgent || !Writer->pAgent ||
        !Writer->pTool || !Writer->pCall || !xwork__request_scope_valid(Scope))
    {
        xwork__set_error(Error, XWORK_ERROR_INVALID_ARGUMENT,
                         "invalid callback request identity preparation");
        return false;
    }
    Persistent = Writer->pTool->bRetrySafeRead && Writer->pAgent->pArtifactStore &&
                 Writer->pAgent->bAllowArtifactWrites && !Writer->pAgent->uParentTaskId;
    if (Persistent)
    {
        if (!xwork__receipt_key(Writer->pTool, Writer->pCall, Context->uAgentTurn, Key))
            goto io;
        Loaded = xwork__request_load(Writer->pAgent, Key, Scope, &Prepared, Error);
        if (Loaded < 0)
            goto done;
        if (Loaded > 0)
        {
            *Token = Prepared;
            xrtClearError();
            return true;
        }
    }
    xworkToolRequestTokenInit(&Prepared);
    Prepared.iCreatedAt = (int64_t)time(NULL);
    if (Prepared.iCreatedAt <= 0 || !xrtSecureRandom(Random, sizeof(Random)))
        goto io;
    for (i = 0u; i < sizeof(Random); ++i)
    {
        static const char Hex[] = "0123456789abcdef";
        Prepared.sId[2u * i] = Hex[Random[i] >> 4];
        Prepared.sId[2u * i + 1u] = Hex[Random[i] & 15u];
    }
    xrtSecureZero(Random, sizeof(Random));
    if (!Persistent)
    {
        *Token = Prepared;
        return true;
    }
    Object = xrtValueObject();
    if (!Object || !xwork__receipt_value(Object, "version", xrtValueUInt(1u)) ||
        !xwork__receipt_value(Object, "key", xrtValueString(xrtStrView(Key))) ||
        !xwork__receipt_value(Object, "scope", xrtValueString(xrtStrView(Scope))) ||
        !xwork__receipt_value(Object, "id", xrtValueString(xrtStrView(Prepared.sId))) ||
        !xwork__receipt_value(Object, "created_at", xrtValueUInt((uint64_t)Prepared.iCreatedAt)))
        goto io;
    Json = xrtJsonStringify(Object, false, &Size);
    if (!Json || Size > XWORK_REQUEST_RECORD_BYTES)
        goto io;
    Directory = xwork__artifact_open_directory(Writer->pAgent, "requests", "requests", Error);
    if (!Directory)
        goto done;
    File = xwork__artifact_temp(Directory, Temporary);
    if (!File)
        goto io;
    TemporaryOwned = true;
    if (!xrtWriteFull(File, Json, Size, NULL) || !xrtFlush(File))
        goto io;
    if (!xrtClose(File))
    {
        File = NULL;
        goto io;
    }
    File = NULL;
    snprintf(Name, sizeof(Name), "%s.json", Key);
    if (!xrtRootLinkHard(Directory, Temporary, Name))
    {
        if (xrtErrorKind(xrtGetError()) != XERR_EXISTS)
            goto io;
        Loaded = xwork__request_load(Writer->pAgent, Key, Scope, &Prepared, Error);
        if (Loaded != 1)
        {
            if (!Loaded)
                goto io;
            goto done;
        }
    }
    else
    {
        Prepared.bPersistent = true; /* unlink temporary name during cleanup */
    }
    *Token = Prepared;
    Ok = true;
    goto done;
io:
    xwork__set_error(
        Error, XWORK_ERROR_IO,
        "cannot retain request identity before submission; no external request was sent");
done:
    if (File)
        (void)xrtClose(File);
    if (TemporaryOwned)
        (void)xrtRootRemove(Directory, Temporary);
    if (Directory)
        (void)xrtRootClose(Directory);
    xrtFree(Json);
    xrtValueRelease(Object);
    if (Ok)
        xrtClearError();
    return Ok;
}
