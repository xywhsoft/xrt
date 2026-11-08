#include "../internal/xwork_internal.h"

/* Small immutable completion receipts for explicitly recoverable read tools.
 * They bridge the window between callback completion and model-ledger append.
 * A receipt is scoped to a host-selected session store and exact call inputs;
 * ordinary workspace files and arbitrary orphan artifacts are never trusted. */
bool xwork__receipt_value(xvalue *Object, const char *Key, xvalue *Value)
{
    bool Ok = Value && xrtValueObjectSetTake(Object, xrtStrView(Key), &Value);
    xrtValueRelease(Value);
    return Ok;
}

bool xwork__receipt_key(const xwork_tool_entry *Tool, const xllm_tool_call *Call, uint64_t Turn,
                        char Key[65])
{
    xwork_buf Input = {0};
    uint8 Digest[32];
    size_t i;
    bool Ok = xwork__buf_appendf(&Input, "xwork-read-receipt-1\n%llu\n", (unsigned long long)Turn);
    const char *Fields[4] = {Tool->sSource, Call->sName, Call->sId, Call->sArgumentsJson};
    for (i = 0u; Ok && i < 4u; ++i)
    {
        const char *Field = Fields[i] ? Fields[i] : "";
        Ok = xwork__buf_appendf(&Input, "%zu:", strlen(Field)) &&
             xwork__buf_append_cstr(&Input, Field);
    }
    Ok = Ok && xrtSha256(Input.pData, Input.iLen, Digest);
    xwork__buf_unit(&Input);
    if (!Ok)
        return false;
    for (i = 0u; i < 32u; ++i)
    {
        static const char Hex[] = "0123456789abcdef";
        Key[2u * i] = Hex[Digest[i] >> 4];
        Key[2u * i + 1u] = Hex[Digest[i] & 15u];
    }
    Key[64] = 0;
    return true;
}

char *xwork__result_locator(xwork_agent *Agent, const char *Path)
{
    char *Relative;
    char *Result;
    size_t i;
    if (!Path || !Agent->pArtifactStore)
        return xwork__strdup("");
    Relative = xrtPathRel(Agent->pArtifactStore->sDirectory, Path);
    if (!Relative)
        return NULL;
    for (i = 0u; Relative[i]; ++i)
        if (Relative[i] == '\\')
            Relative[i] = '/';
    Result = (char *)malloc(strlen(Relative) + 10u);
    if (Result)
    {
        strcpy(Result, "@results/");
        strcat(Result, Relative);
    }
    xrtFree(Relative);
    if (Result && !xwork__result_file_relative(Result))
    {
        free(Result);
        Result = NULL;
    }
    return Result;
}

static bool xwork__receipt_file_digest(xfile File, char Hex[65], uint64_t *Size)
{
    xsha256 Hash;
    uint8 Digest[32];
    unsigned char Buffer[4096];
    uint64_t Position = 0u;
    size_t Read, i;
    xrtSha256Init(&Hash);
    if (!xrtFileSize(File, Size) || *Size > 64u * 1024u * 1024u)
        return false;
    while (Position < *Size)
    {
        if (!xrtReadAt(File, Position, Buffer, sizeof(Buffer), &Read) || !Read ||
            !xrtSha256Update(&Hash, Buffer, Read))
            return false;
        Position += Read;
    }
    if (Position != *Size || !xrtSha256Final(&Hash, Digest))
        return false;
    for (i = 0u; i < 32u; ++i)
    {
        static const char Digits[] = "0123456789abcdef";
        Hex[2u * i] = Digits[Digest[i] >> 4];
        Hex[2u * i + 1u] = Digits[Digest[i] & 15u];
    }
    Hex[64] = 0;
    return true;
}

/* 0: no committed result; 1: reusable result; -1: damaged receipt. */
int xwork__receipt_load(xwork_agent *Agent, const xwork_tool_entry *Tool,
                        const xllm_tool_call *Call, uint64_t Turn, xwork_tool_output *Output,
                        char **Artifact, xwork_error *Error)
{
    char Key[65], Name[96];
    xfileoptions Options;
    xfile File = NULL, ResultFile = NULL;
    xvalue *Object = NULL;
    xjsonreadconfig Config;
    char *Text = NULL;
    uint64_t Size;
    size_t Read = 0u;
    const char *StoredKey;
    const char *StoredText;
    const char *Locator;
    bool Success;
    int Result = -1;
    *Artifact = NULL;
    if (!Tool->bRetrySafeRead || !Agent->pArtifactStore || Agent->uParentTaskId)
        return 0;
    if (!xwork__receipt_key(Tool, Call, Turn, Key))
        goto done;
    snprintf(Name, sizeof(Name), "completed/%s.json", Key);
    xrtFileOptionsInit(&Options);
    Options.Flags = XFILE_READ | XFILE_NOFOLLOW;
    File = xrtRootFileOpen(Agent->pArtifactStore->pDirectory, Name, &Options);
    if (!File && xrtErrorKind(xrtGetError()) == XERR_NOT_FOUND)
    {
        xrtClearError();
        return 0;
    }
    if (!File || !xrtFileSize(File, &Size) || Size > 64u * 1024u)
        goto done;
    Text = (char *)malloc((size_t)Size + 1u);
    if (!Text || !xrtReadFull(File, Text, (size_t)Size, &Read) || Read != Size)
        goto done;
    Text[Size] = 0;
    xrtJsonReadConfigInit(&Config);
    Config.MaxInputBytes = 64u * 1024u;
    Config.MaxDepth = 3u;
    Config.MaxValues = 32u;
    Object = xrtJsonRead(xrtStrViewN(Text, (size_t)Size), &Config);
    StoredKey = xwork__json_text(Object, "key");
    StoredText = xwork__json_text(Object, "text");
    Locator = xwork__json_text(Object, "file");
    if (!StoredKey || strcmp(StoredKey, Key) || !StoredText || !Locator ||
        !xrtUtf8Valid(xrtStrView(StoredText), NULL) ||
        !xrtValueGetBool(xrtValueObjectGet(Object, xrtStrView("success")), &Success))
        goto done;
    if (Locator[0])
    {
        char Digest[65];
        uint64_t FileSize, ExpectedSize;
        bool Valid;
        const char *ExpectedDigest = xwork__json_text(Object, "sha256");
        const char *Relative = xwork__result_file_relative(Locator);
        ExpectedSize = xwork__json_u64(Object, "bytes", 0u, &Valid);
        ResultFile = Relative ? xwork__result_file_open(Agent, Locator) : NULL;
        if (!ResultFile || !Valid || !ExpectedDigest ||
            !xwork__receipt_file_digest(ResultFile, Digest, &FileSize) ||
            FileSize != ExpectedSize || strcmp(Digest, ExpectedDigest))
            goto done;
        {
            char *Joined = xrtPathJoin(Agent->pArtifactStore->sDirectory, Relative);
            *Artifact = Joined ? xwork__strdup(Joined) : NULL;
            xrtFree(Joined);
        }
        if (!*Artifact)
            goto done;
    }
    if (!xworkToolOutputSet(Output, Success, StoredText))
        goto done;
    Result = 1;
done:
    if (File)
        (void)xrtClose(File);
    if (ResultFile)
        (void)xrtClose(ResultFile);
    free(Text);
    xrtValueRelease(Object);
    if (Result < 0)
    {
        free(*Artifact);
        *Artifact = NULL;
        xwork__set_error(
            Error, XWORK_ERROR_IO,
            "saved tool result is damaged or unavailable; no repeated request was sent");
    }
    return Result;
}

bool xwork__receipt_save(xwork_agent *Agent, const xwork_tool_entry *Tool,
                         const xllm_tool_call *Call, uint64_t Turn, const char *Text, bool Success,
                         const char *Artifact, xwork_error *Error)
{
    char Key[65], Name[80], Temporary[64], Digest[65] = "";
    char *Locator = NULL;
    char *Json = NULL;
    uint64_t Size = 0u;
    size_t JsonSize;
    xroot Directory = NULL;
    xfile File = NULL, ResultFile = NULL;
    xvalue *Object = NULL;
    bool TemporaryOwned = false, Ok = false;
    if (!Tool || !Tool->bRetrySafeRead || !Agent->pArtifactStore || Agent->uParentTaskId ||
        !Agent->bAllowArtifactWrites)
        return true;
    if (!xwork__receipt_key(Tool, Call, Turn, Key))
        goto done;
    Locator = xwork__result_locator(Agent, Artifact);
    if (!Locator)
        goto done;
    if (Locator[0])
    {
        ResultFile = xwork__result_file_open(Agent, Locator);
        if (!ResultFile || !xwork__receipt_file_digest(ResultFile, Digest, &Size))
            goto done;
    }
    Object = xrtValueObject();
    if (!Object || !xwork__receipt_value(Object, "key", xrtValueString(xrtStrView(Key))) ||
        !xwork__receipt_value(Object, "text", xrtValueString(xrtStrView(Text))) ||
        !xwork__receipt_value(Object, "success", xrtValueBool(Success)) ||
        !xwork__receipt_value(Object, "file", xrtValueString(xrtStrView(Locator))) ||
        !xwork__receipt_value(Object, "bytes", xrtValueUInt(Size)) ||
        !xwork__receipt_value(Object, "sha256", xrtValueString(xrtStrView(Digest))))
        goto done;
    Json = xrtJsonStringify(Object, false, &JsonSize);
    if (!Json || JsonSize > 64u * 1024u)
        goto done;
    Directory = xwork__artifact_open_directory(Agent, "completed", "completed", Error);
    if (!Directory)
        goto done;
    File = xwork__artifact_temp(Directory, Temporary);
    if (!File)
        goto done;
    TemporaryOwned = true;
    if (!xrtWriteFull(File, Json, JsonSize, NULL) || !xrtFlush(File))
        goto done;
    if (!xrtClose(File))
    {
        File = NULL;
        goto done;
    }
    File = NULL;
    snprintf(Name, sizeof(Name), "%s.json", Key);
    /* Publish the flushed immutable inode without replacing an existing receipt.
     * The root-relative hard link is atomic and never exposes partial JSON. */
    if (!xrtRootLinkHard(Directory, Temporary, Name))
        goto done;
    Ok = true; /* cleanup unlinks the private temporary name */
done:
    if (File)
        (void)xrtClose(File);
    if (ResultFile)
        (void)xrtClose(ResultFile);
    if (TemporaryOwned)
        (void)xrtRootRemove(Directory, Temporary);
    if (Directory)
        (void)xrtRootClose(Directory);
    free(Locator);
    xrtFree(Json);
    xrtValueRelease(Object);
    if (!Ok)
        xwork__set_error(Error, XWORK_ERROR_IO,
                         "cannot save the completed tool result for recovery");
    return Ok;
}
