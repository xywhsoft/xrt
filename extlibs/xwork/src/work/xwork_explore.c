#include "../internal/xwork_internal.h"

#include <inttypes.h>

/*
 * Native, dependency-free workspace discovery tools.
 *
 * The result contract is identical on every platform: paths use '/', results
 * are sorted by raw UTF-8 bytes, glob matching is case-sensitive unless
 * requested otherwise, links are never traversed, resource use is bounded,
 * and cancellation/deadlines are observed between bounded operations.
 */

#define XWORK_EXPLORE_MAX_DEPTH 64u
#define XWORK_EXPLORE_MAX_ENTRIES 100000u
#define XWORK_EXPLORE_MAX_FILES 20000u
#define XWORK_EXPLORE_MAX_FILE_BYTES (2u * 1024u * 1024u)
#define XWORK_EXPLORE_MAX_TOTAL_BYTES (64u * 1024u * 1024u)
#define XWORK_EXPLORE_LINE_PREVIEW 400u
#define XWORK_EXPLORE_MAX_PATTERN 4096u

typedef enum xwork_explore_status
{
    XWORK_EXPLORE_OK = 0,
    XWORK_EXPLORE_OUT_OF_MEMORY,
    XWORK_EXPLORE_IO,
    XWORK_EXPLORE_CANCELLED,
    XWORK_EXPLORE_TIMED_OUT,
    XWORK_EXPLORE_REGEX_ERROR,
    XWORK_EXPLORE_LIMIT
} xwork_explore_status;

typedef struct xwork_explore_entry
{
    char *sName;
    xfileinfo tInfo;
} xwork_explore_entry;

typedef struct xwork_explore_entries
{
    xwork_explore_entry *pItems;
    size_t iCount;
    size_t iCapacity;
    size_t iInvalidUtf8;
} xwork_explore_entries;

typedef struct xwork_explore_file
{
    char *sAbsolute;
    char *sSearchRelative;
    char *sWorkspaceRelative;
    xfileinfo tInfo;
} xwork_explore_file;

typedef struct xwork_explore_files
{
    xwork_explore_file *pItems;
    size_t iCount;
    size_t iCapacity;
    size_t iEntriesVisited;
    size_t iDirectoriesVisited;
    size_t iInvalidUtf8;
    size_t iLinksSkipped;
    size_t iSpecialSkipped;
    size_t iIgnoredDirectories;
    size_t iDepthSkipped;
    bool bScanLimited;
    bool bIncludeHidden;
    uint32_t uMaxDepth;
    const xwork_tool_context *pContext;
} xwork_explore_files;

static const char *const c_xwork_explore_ignored_directories[] = {
    ".git", ".svn", ".hg",         "node_modules",  "__pycache__", ".venv",
    "venv", ".tox", ".mypy_cache", ".pytest_cache", "target",      NULL};

static const char *const c_xwork_explore_binary_extensions[] = {
    ".png",   ".jpg", ".jpeg",   ".gif",     ".bmp", ".webp", ".ico",   ".icns",  ".zip",
    ".7z",    ".gz",  ".xz",     ".bz2",     ".tar", ".rar",  ".exe",   ".dll",   ".so",
    ".dylib", ".a",   ".lib",    ".obj",     ".o",   ".pdf",  ".woff",  ".woff2", ".ttf",
    ".otf",   ".eot", ".mp3",    ".mp4",     ".avi", ".mov",  ".wav",   ".flac",  ".ogg",
    ".webm",  ".db",  ".sqlite", ".sqlite3", ".pyc", ".pyd",  ".class", ".wasm",  NULL};

static xwork_explore_status xwork__explore_scope_status(const xwork_tool_context *pContext)
{
    if (pContext && pContext->pCancel && xrtCancelRequested(pContext->pCancel))
        return XWORK_EXPLORE_CANCELLED;
    if (pContext && pContext->uDeadline && pContext->uDeadline != XLLM_DEADLINE_NEVER &&
        xllmDeadlineExpired(pContext->uDeadline))
        return XWORK_EXPLORE_TIMED_OUT;
    return XWORK_EXPLORE_OK;
}

static xwork_result xwork__explore_status_result(xwork_explore_status eStatus,
                                                 const char *sOperation, xwork_error *pError)
{
    switch (eStatus)
    {
    case XWORK_EXPLORE_CANCELLED:
        xwork__set_error(pError, XWORK_ERROR_CANCELLED, "filesystem discovery was cancelled");
        return XWORK_RESULT_CANCELLED;
    case XWORK_EXPLORE_TIMED_OUT:
        xwork__set_error(pError, XWORK_ERROR_TIMEOUT, "filesystem discovery exceeded its deadline");
        return XWORK_RESULT_TIMEOUT;
    case XWORK_EXPLORE_OUT_OF_MEMORY:
        xwork__set_error(pError, XWORK_ERROR_OUT_OF_MEMORY,
                         "filesystem discovery ran out of memory");
        return XWORK_RESULT_ERROR;
    case XWORK_EXPLORE_REGEX_ERROR:
        xwork__set_error(pError, XWORK_ERROR_TOOL, "regular expression execution failed");
        return XWORK_RESULT_ERROR;
    case XWORK_EXPLORE_LIMIT:
        xwork__set_error(pError, XWORK_ERROR_LIMIT,
                         "filesystem discovery exceeded its internal resource limit");
        return XWORK_RESULT_LIMIT;
    case XWORK_EXPLORE_IO:
    default:
        xwork__set_error(pError, XWORK_ERROR_IO,
                         sOperation ? sOperation : "filesystem discovery failed");
        return XWORK_RESULT_ERROR;
    }
}

static bool xwork__explore_path_char_equal(char a, char b)
{
#if defined(_WIN32)
    if (a >= 'A' && a <= 'Z')
        a = (char)(a + ('a' - 'A'));
    if (b >= 'A' && b <= 'Z')
        b = (char)(b + ('a' - 'A'));
#endif
    return a == b;
}

static bool xwork__explore_path_inside(const char *sRoot, const char *sPath)
{
    size_t i;
    size_t iRootLength;
    if (!sRoot || !sPath)
        return false;
    iRootLength = strlen(sRoot);
    for (i = 0u; i < iRootLength; ++i)
    {
        if (!sPath[i] || !xwork__explore_path_char_equal(sRoot[i], sPath[i]))
            return false;
    }
    if (iRootLength && (sRoot[iRootLength - 1u] == '/' || sRoot[iRootLength - 1u] == '\\'))
        return true;
    return sPath[iRootLength] == '\0' || sPath[iRootLength] == '/' || sPath[iRootLength] == '\\';
}

/* This physical check blocks an existing path that already resolves outside
 * the workspace. Recursive traversal also skips links. It is a read boundary,
 * not an OS sandbox against a hostile concurrent filesystem mutation. */
static char *xwork__explore_resolve_existing(xwork_agent *pAgent, const char *sPath,
                                             xwork_error *pError)
{
    char *sResolved = xwork__resolve_path(pAgent, sPath, pError);
    char *sRootReal;
    char *sTargetReal;
    bool bInside;
    if (!sResolved)
        return NULL;
    if (xwork__external_paths(pAgent) && sPath[0] != '@')
        return sResolved;
    sRootReal = xrtPathReal(xwork__file_root_for_path(pAgent, sPath));
    sTargetReal = xrtPathReal(sResolved);
    if (!sRootReal || !sTargetReal)
    {
        xrtFree(sRootReal);
        xrtFree(sTargetReal);
        free(sResolved);
        xwork__set_error(pError, XWORK_ERROR_IO, "failed to resolve the physical discovery path");
        return NULL;
    }
    bInside = xwork__explore_path_inside(sRootReal, sTargetReal);
    xrtFree(sRootReal);
    xrtFree(sTargetReal);
    if (!bInside)
    {
        free(sResolved);
        xwork__set_error(pError, XWORK_ERROR_POLICY,
                         "physical discovery path escapes the configured workspace");
        return NULL;
    }
    return sResolved;
}

static bool xwork__explore_ignored_directory(const char *sName)
{
    size_t i;
    for (i = 0u; c_xwork_explore_ignored_directories[i]; ++i)
    {
        if (strcmp(sName, c_xwork_explore_ignored_directories[i]) == 0)
            return true;
    }
    return false;
}

static bool xwork__explore_ascii_text_equal(const char *sLeft, const char *sRight)
{
    while (*sLeft && *sRight)
    {
        char a = *sLeft++;
        char b = *sRight++;
        if (a >= 'A' && a <= 'Z')
            a = (char)(a + ('a' - 'A'));
        if (b >= 'A' && b <= 'Z')
            b = (char)(b + ('a' - 'A'));
        if (a != b)
            return false;
    }
    return *sLeft == '\0' && *sRight == '\0';
}

static bool xwork__explore_binary_extension(const char *sName)
{
    const char *sDot = strrchr(sName, '.');
    size_t i;
    if (!sDot)
        return false;
    for (i = 0u; c_xwork_explore_binary_extensions[i]; ++i)
    {
        if (xwork__explore_ascii_text_equal(sDot, c_xwork_explore_binary_extensions[i]))
            return true;
    }
    return false;
}

static char *xwork__explore_join(const char *sLeft, const char *sRight)
{
    size_t iLeft;
    size_t iRight;
    char *sResult;
    if (!sLeft || !sRight)
        return NULL;
    iLeft = strlen(sLeft);
    iRight = strlen(sRight);
    if (iLeft > SIZE_MAX - iRight - 2u)
        return NULL;
    sResult = (char *)malloc(iLeft + iRight + 2u);
    if (!sResult)
        return NULL;
    if (iLeft)
    {
        memcpy(sResult, sLeft, iLeft);
        sResult[iLeft] = '/';
        memcpy(sResult + iLeft + 1u, sRight, iRight + 1u);
    }
    else
    {
        memcpy(sResult, sRight, iRight + 1u);
    }
    return sResult;
}

static char *xwork__explore_workspace_relative(const xwork_agent *pAgent, const char *sResolved)
{
    const char *ResultRoot = xwork__file_root_for_path(pAgent, "@results");
    if (ResultRoot && xwork__path_is_inside(ResultRoot, sResolved))
    {
        const char *Tail = sResolved + strlen(ResultRoot);
        if (*Tail == '/' || *Tail == '\\')
            Tail++;
        size_t Size = strlen(Tail) + 10u;
        char *Locator = malloc(Size);
        if (Locator)
        {
            snprintf(Locator, Size, "@results/%s", Tail);
            for (char *C = Locator; *C; C++)
                if (*C == '\\')
                    *C = '/';
        }
        return Locator;
    }
    char *sNative = NULL;
    char *sMounted = xwork__relative_path(pAgent, sResolved);
    if (sMounted && sMounted[0] == '@')
        return sMounted;
    free(sMounted);
    sNative = xrtPathRel(pAgent->sWorkspaceRoot, sResolved);
    char *sResult;
    size_t i;
    if (!sNative)
        sNative = xrtStrDup(sResolved);
    if (!sNative)
        return NULL;
    if (strcmp(sNative, ".") == 0)
    {
        xrtFree(sNative);
        return xwork__strdup("");
    }
    sResult = xwork__strdup(sNative);
    xrtFree(sNative);
    if (!sResult)
        return NULL;
    for (i = 0u; sResult[i]; ++i)
    {
        if (sResult[i] == '\\')
            sResult[i] = '/';
    }
    while (sResult[0] == '.' && sResult[1] == '/')
        memmove(sResult, sResult + 2u, strlen(sResult + 2u) + 1u);
    return sResult;
}

static int xwork__explore_entry_compare(const void *pLeft, const void *pRight)
{
    const xwork_explore_entry *pA = (const xwork_explore_entry *)pLeft;
    const xwork_explore_entry *pB = (const xwork_explore_entry *)pRight;
    return strcmp(pA->sName, pB->sName);
}

static void xwork__explore_entries_unit(xwork_explore_entries *pEntries)
{
    size_t i;
    if (!pEntries)
        return;
    for (i = 0u; i < pEntries->iCount; ++i)
        free(pEntries->pItems[i].sName);
    free(pEntries->pItems);
    memset(pEntries, 0, sizeof(*pEntries));
}

static xwork_explore_status xwork__explore_entries_add(xwork_explore_entries *pEntries,
                                                       const xdirentry *pEntry)
{
    xwork_explore_entry *pNew;
    char *sName;
    size_t iCapacity;
    if (!pEntries || !pEntry || !pEntry->Name.Data || !pEntry->Name.Size ||
        memchr(pEntry->Name.Data, '\0', pEntry->Name.Size) || !xrtUtf8Valid(pEntry->Name, NULL))
    {
        if (pEntries)
            ++pEntries->iInvalidUtf8;
        return XWORK_EXPLORE_OK;
    }
    if (pEntries->iCount == pEntries->iCapacity)
    {
        iCapacity = pEntries->iCapacity ? pEntries->iCapacity * 2u : 16u;
        if (iCapacity > XWORK_EXPLORE_MAX_ENTRIES)
            iCapacity = XWORK_EXPLORE_MAX_ENTRIES;
        if (iCapacity <= pEntries->iCapacity)
            return XWORK_EXPLORE_LIMIT;
        pNew = (xwork_explore_entry *)realloc(pEntries->pItems, iCapacity * sizeof(*pNew));
        if (!pNew)
            return XWORK_EXPLORE_OUT_OF_MEMORY;
        pEntries->pItems = pNew;
        pEntries->iCapacity = iCapacity;
    }
    sName = (char *)malloc(pEntry->Name.Size + 1u);
    if (!sName)
        return XWORK_EXPLORE_OUT_OF_MEMORY;
    memcpy(sName, pEntry->Name.Data, pEntry->Name.Size);
    sName[pEntry->Name.Size] = '\0';
    pEntries->pItems[pEntries->iCount].sName = sName;
    pEntries->pItems[pEntries->iCount].tInfo = pEntry->Info;
    ++pEntries->iCount;
    return XWORK_EXPLORE_OK;
}

static xwork_explore_status xwork__explore_collect_directory(const char *sDirectory,
                                                             const xwork_tool_context *pContext,
                                                             xwork_explore_entries *pEntries)
{
    xdir hDirectory = xrtDirOpen(sDirectory, XDIR_STAT);
    xdirentry tEntry;
    xdirnext eNext;
    xwork_explore_status eStatus = XWORK_EXPLORE_OK;
    if (!hDirectory)
        return XWORK_EXPLORE_IO;
    for (;;)
    {
        eStatus = xwork__explore_scope_status(pContext);
        if (eStatus != XWORK_EXPLORE_OK)
            break;
        eNext = xrtDirNext(hDirectory, &tEntry);
        if (eNext == XDIR_NEXT_END)
            break;
        if (eNext == XDIR_NEXT_ERROR)
        {
            eStatus = XWORK_EXPLORE_IO;
            break;
        }
        eStatus = xwork__explore_entries_add(pEntries, &tEntry);
        if (eStatus != XWORK_EXPLORE_OK)
            break;
    }
    if (!xrtDirClose(hDirectory) && eStatus == XWORK_EXPLORE_OK)
        eStatus = XWORK_EXPLORE_IO;
    if (eStatus == XWORK_EXPLORE_OK && pEntries->iCount > 1u)
        qsort(pEntries->pItems, pEntries->iCount, sizeof(*pEntries->pItems),
              xwork__explore_entry_compare);
    return eStatus;
}

static int xwork__explore_file_compare(const void *pLeft, const void *pRight)
{
    const xwork_explore_file *pA = (const xwork_explore_file *)pLeft;
    const xwork_explore_file *pB = (const xwork_explore_file *)pRight;
    return strcmp(pA->sWorkspaceRelative, pB->sWorkspaceRelative);
}

static void xwork__explore_files_unit(xwork_explore_files *pFiles)
{
    size_t i;
    if (!pFiles)
        return;
    for (i = 0u; i < pFiles->iCount; ++i)
    {
        free(pFiles->pItems[i].sAbsolute);
        free(pFiles->pItems[i].sSearchRelative);
        free(pFiles->pItems[i].sWorkspaceRelative);
    }
    free(pFiles->pItems);
    memset(pFiles, 0, sizeof(*pFiles));
}

static xwork_explore_status xwork__explore_files_add(xwork_explore_files *pFiles,
                                                     const char *sAbsolute,
                                                     const char *sSearchRelative,
                                                     const char *sWorkspaceRelative,
                                                     const xfileinfo *pInfo)
{
    xwork_explore_file *pNew;
    size_t iCapacity;
    if (pFiles->iCount >= XWORK_EXPLORE_MAX_FILES)
    {
        pFiles->bScanLimited = true;
        return XWORK_EXPLORE_LIMIT;
    }
    if (pFiles->iCount == pFiles->iCapacity)
    {
        iCapacity = pFiles->iCapacity ? pFiles->iCapacity * 2u : 64u;
        if (iCapacity > XWORK_EXPLORE_MAX_FILES)
            iCapacity = XWORK_EXPLORE_MAX_FILES;
        pNew = (xwork_explore_file *)realloc(pFiles->pItems, iCapacity * sizeof(*pNew));
        if (!pNew)
            return XWORK_EXPLORE_OUT_OF_MEMORY;
        pFiles->pItems = pNew;
        pFiles->iCapacity = iCapacity;
    }
    pNew = &pFiles->pItems[pFiles->iCount];
    memset(pNew, 0, sizeof(*pNew));
    pNew->sAbsolute = xwork__strdup(sAbsolute);
    pNew->sSearchRelative = xwork__strdup(sSearchRelative);
    pNew->sWorkspaceRelative = xwork__strdup(sWorkspaceRelative);
    if (!pNew->sAbsolute || !pNew->sSearchRelative || !pNew->sWorkspaceRelative)
    {
        free(pNew->sAbsolute);
        free(pNew->sSearchRelative);
        free(pNew->sWorkspaceRelative);
        memset(pNew, 0, sizeof(*pNew));
        return XWORK_EXPLORE_OUT_OF_MEMORY;
    }
    pNew->tInfo = *pInfo;
    ++pFiles->iCount;
    return XWORK_EXPLORE_OK;
}

static xwork_explore_status xwork__explore_walk(xwork_explore_files *pFiles,
                                                const char *sAbsoluteDirectory,
                                                const char *sSearchPrefix,
                                                const char *sWorkspacePrefix, uint32_t uDepth)
{
    xwork_explore_entries tEntries = {0};
    xwork_explore_status eStatus;
    size_t i;
    if (pFiles->bScanLimited)
        return XWORK_EXPLORE_LIMIT;
    eStatus = xwork__explore_scope_status(pFiles->pContext);
    if (eStatus != XWORK_EXPLORE_OK)
        return eStatus;
    if (pFiles->iDirectoriesVisited >= 4096u)
    {
        pFiles->bScanLimited = true;
        return XWORK_EXPLORE_LIMIT;
    }
    ++pFiles->iDirectoriesVisited;
    eStatus = xwork__explore_collect_directory(sAbsoluteDirectory, pFiles->pContext, &tEntries);
    pFiles->iInvalidUtf8 += tEntries.iInvalidUtf8;
    if (eStatus != XWORK_EXPLORE_OK)
    {
        xwork__explore_entries_unit(&tEntries);
        return eStatus;
    }
    for (i = 0u; i < tEntries.iCount; ++i)
    {
        xwork_explore_entry *pEntry = &tEntries.pItems[i];
        char *sAbsolute = NULL;
        char *sSearchRelative = NULL;
        char *sWorkspaceRelative = NULL;
        if (++pFiles->iEntriesVisited > XWORK_EXPLORE_MAX_ENTRIES)
        {
            pFiles->bScanLimited = true;
            eStatus = XWORK_EXPLORE_LIMIT;
            break;
        }
        eStatus = xwork__explore_scope_status(pFiles->pContext);
        if (eStatus != XWORK_EXPLORE_OK)
            break;
        if (!pFiles->bIncludeHidden && pEntry->sName[0] == '.')
            continue;
        sAbsolute = xwork__explore_join(sAbsoluteDirectory, pEntry->sName);
#if defined(_WIN32)
        /* Canonical mount roots may use the extended-length prefix. Windows
         * does not normalize '/' in those native paths. Keep display paths
         * portable, but use native separators when opening enumerated files. */
        if (sAbsolute)
        {
            size_t j;
            for (j = 0u; sAbsolute[j]; ++j)
                if (sAbsolute[j] == '/')
                    sAbsolute[j] = '\\';
        }
#endif
        sSearchRelative = xwork__explore_join(sSearchPrefix, pEntry->sName);
        sWorkspaceRelative = xwork__explore_join(sWorkspacePrefix, pEntry->sName);
        if (!sAbsolute || !sSearchRelative || !sWorkspaceRelative)
        {
            eStatus = XWORK_EXPLORE_OUT_OF_MEMORY;
        }
        else if (pEntry->tInfo.Type == XFILE_TYPE_DIRECTORY)
        {
            if (xwork__explore_ignored_directory(pEntry->sName))
                ++pFiles->iIgnoredDirectories;
            else if (uDepth >= pFiles->uMaxDepth)
                ++pFiles->iDepthSkipped;
            else
                eStatus = xwork__explore_walk(pFiles, sAbsolute, sSearchRelative,
                                              sWorkspaceRelative, uDepth + 1u);
        }
        else if (pEntry->tInfo.Type == XFILE_TYPE_FILE)
        {
            eStatus = xwork__explore_files_add(pFiles, sAbsolute, sSearchRelative,
                                               sWorkspaceRelative, &pEntry->tInfo);
        }
        else if (pEntry->tInfo.Type == XFILE_TYPE_LINK)
        {
            ++pFiles->iLinksSkipped;
        }
        else
        {
            ++pFiles->iSpecialSkipped;
        }
        free(sAbsolute);
        free(sSearchRelative);
        free(sWorkspaceRelative);
        if (eStatus != XWORK_EXPLORE_OK)
            break;
    }
    xwork__explore_entries_unit(&tEntries);
    return eStatus;
}

static xwork_explore_status xwork__explore_collect_files(xwork_explore_files *pFiles,
                                                         const char *sResolved,
                                                         const char *sWorkspacePrefix)
{
    xwork_explore_status eStatus = xwork__explore_walk(pFiles, sResolved, "", sWorkspacePrefix, 0u);
    if (eStatus == XWORK_EXPLORE_LIMIT && pFiles->bScanLimited)
        eStatus = XWORK_EXPLORE_OK;
    if (eStatus == XWORK_EXPLORE_OK && pFiles->iCount > 1u)
        qsort(pFiles->pItems, pFiles->iCount, sizeof(*pFiles->pItems), xwork__explore_file_compare);
    return eStatus;
}

static const char *xwork__explore_type_name(xfiletype eType)
{
    switch (eType)
    {
    case XFILE_TYPE_FILE:
        return "file";
    case XFILE_TYPE_DIRECTORY:
        return "dir";
    case XFILE_TYPE_LINK:
        return "link";
    case XFILE_TYPE_FIFO:
        return "fifo";
    case XFILE_TYPE_SOCKET:
        return "socket";
    case XFILE_TYPE_DEVICE:
        return "device";
    default:
        return "other";
    }
}

bool xwork__list_directory(const char *sDirectory, bool bLong, bool bAll, xwork_buf *pOutput)
{
    xwork_explore_entries tEntries = {0};
    xwork_explore_status eStatus;
    size_t i;
    size_t iShown = 0u;
    eStatus = xwork__explore_collect_directory(sDirectory, NULL, &tEntries);
    if (eStatus != XWORK_EXPLORE_OK)
    {
        xwork__explore_entries_unit(&tEntries);
        return false;
    }
    for (i = 0u; i < tEntries.iCount; ++i)
    {
        xwork_explore_entry *pEntry = &tEntries.pItems[i];
        const char *sSuffix = pEntry->tInfo.Type == XFILE_TYPE_DIRECTORY ? "/"
                              : pEntry->tInfo.Type == XFILE_TYPE_LINK
                                  ? "@"
                                  : (pEntry->tInfo.Type == XFILE_TYPE_FILE ? "" : "?");
        if (!bAll && pEntry->sName[0] == '.')
            continue;
        if (bLong)
        {
            if (!xwork__buf_appendf(pOutput, "%s\t",
                                    xwork__explore_type_name(pEntry->tInfo.Type)) ||
                ((pEntry->tInfo.Available & XFILE_INFO_SIZE) != 0u
                     ? !xwork__buf_appendf(pOutput, "%" PRIu64 "\t", pEntry->tInfo.Size)
                     : !xwork__buf_append_cstr(pOutput, "-\t")) ||
                ((pEntry->tInfo.Available & XFILE_INFO_MODIFY_TIME) != 0u
                     ? !xwork__buf_appendf(pOutput, "%" PRId64 "\t",
                                           xllmTimeToUnixUs(pEntry->tInfo.Modified))
                     : !xwork__buf_append_cstr(pOutput, "-\t")) ||
                !xwork__buf_appendf(pOutput, "%s%s\n", pEntry->sName, sSuffix))
            {
                xwork__explore_entries_unit(&tEntries);
                return false;
            }
        }
        else if (!xwork__buf_appendf(pOutput, "%s%s\n", pEntry->sName, sSuffix))
        {
            xwork__explore_entries_unit(&tEntries);
            return false;
        }
        ++iShown;
    }
    if (!iShown && !xwork__buf_append_cstr(pOutput, "(empty directory)\n"))
    {
        xwork__explore_entries_unit(&tEntries);
        return false;
    }
    xwork__explore_entries_unit(&tEntries);
    return true;
}

static xwork_result xwork__tool_ls(void *pUserData, const xwork_tool_context *pContext,
                                   const char *sArgumentsJson, xwork_tool_output *pOutput,
                                   xwork_error *pError)
{
    xwork_agent *pAgent = (xwork_agent *)pUserData;
    xvalue *tArgs = xwork__json_parse_object(sArgumentsJson);
    const char *sPath;
    bool bAll;
    bool bLong;
    bool bValid;
    uint64_t uMax;
    char *sResolved = NULL;
    xwork_explore_entries tEntries = {0};
    xwork_buf tOutput = {0};
    xwork_explore_status eStatus;
    xwork_result eResult = XWORK_RESULT_ERROR;
    size_t i;
    size_t iShown = 0u;
    if (!tArgs)
        return xwork__tool_fail(pOutput, "invalid arguments: expected a JSON object");
    sPath = xwork__json_text(tArgs, "path");
    if (!sPath || !sPath[0])
        sPath = ".";
    bAll = xwork__json_bool(tArgs, "all", false, &bValid);
    if (!bValid)
    {
        eResult = xwork__tool_fail(pOutput, "all must be boolean");
        goto cleanup;
    }
    bLong = xwork__json_bool(tArgs, "long", false, &bValid);
    if (!bValid)
    {
        eResult = xwork__tool_fail(pOutput, "long must be boolean");
        goto cleanup;
    }
    uMax = xwork__json_u64(tArgs, "max_results", 500u, &bValid);
    if (!bValid || !uMax || uMax > 2000u)
    {
        eResult = xwork__tool_fail(pOutput, "max_results must be between 1 and 2000");
        goto cleanup;
    }
    sResolved = xwork__explore_resolve_existing(pAgent, sPath, pError);
    if (!sResolved)
    {
        eResult = xwork__tool_fail(pOutput, pError && pError->sMessage[0] ? pError->sMessage
                                                                          : "path denied");
        goto cleanup;
    }
    {
        xfileinfo tInfo;
        if (!xrtPathStat(sResolved, false, &tInfo) || tInfo.Type != XFILE_TYPE_DIRECTORY)
        {
            eResult = xwork__tool_fail(pOutput, "path is not a directory or is a symbolic link");
            goto cleanup;
        }
    }
    eStatus = xwork__explore_collect_directory(sResolved, pContext, &tEntries);
    if (eStatus != XWORK_EXPLORE_OK)
    {
        eResult = xwork__explore_status_result(eStatus, "failed to enumerate directory", pError);
        goto cleanup;
    }
    for (i = 0u; i < tEntries.iCount && iShown < (size_t)uMax; ++i)
    {
        xwork_explore_entry *pEntry = &tEntries.pItems[i];
        const char *sSuffix;
        if (!bAll && pEntry->sName[0] == '.')
            continue;
        sSuffix = pEntry->tInfo.Type == XFILE_TYPE_DIRECTORY ? "/"
                  : pEntry->tInfo.Type == XFILE_TYPE_LINK
                      ? "@"
                      : (pEntry->tInfo.Type == XFILE_TYPE_FILE ? "" : "?");
        if (bLong)
        {
            if (!xwork__buf_appendf(&tOutput, "%s\t",
                                    xwork__explore_type_name(pEntry->tInfo.Type)) ||
                ((pEntry->tInfo.Available & XFILE_INFO_SIZE) != 0u
                     ? !xwork__buf_appendf(&tOutput, "%" PRIu64 "\t", pEntry->tInfo.Size)
                     : !xwork__buf_append_cstr(&tOutput, "-\t")) ||
                ((pEntry->tInfo.Available & XFILE_INFO_MODIFY_TIME) != 0u
                     ? !xwork__buf_appendf(&tOutput, "%" PRId64 "\t",
                                           xllmTimeToUnixUs(pEntry->tInfo.Modified))
                     : !xwork__buf_append_cstr(&tOutput, "-\t")) ||
                !xwork__buf_appendf(&tOutput, "%s%s\n", pEntry->sName, sSuffix))
            {
                eResult = xwork__explore_status_result(XWORK_EXPLORE_OUT_OF_MEMORY, NULL, pError);
                goto cleanup;
            }
        }
        else if (!xwork__buf_appendf(&tOutput, "%s%s\n", pEntry->sName, sSuffix))
        {
            eResult = xwork__explore_status_result(XWORK_EXPLORE_OUT_OF_MEMORY, NULL, pError);
            goto cleanup;
        }
        ++iShown;
    }
    if (!iShown && !xwork__buf_append_cstr(&tOutput, "(empty directory)\n"))
    {
        eResult = xwork__explore_status_result(XWORK_EXPLORE_OUT_OF_MEMORY, NULL, pError);
        goto cleanup;
    }
    if (i < tEntries.iCount &&
        !xwork__buf_appendf(&tOutput, "[truncated: shown=%" PRIu64 " entries]\n", uMax))
    {
        eResult = xwork__explore_status_result(XWORK_EXPLORE_OUT_OF_MEMORY, NULL, pError);
        goto cleanup;
    }
    if (tEntries.iInvalidUtf8 &&
        !xwork__buf_appendf(&tOutput, "[skipped: invalid_utf8_names=%zu]\n", tEntries.iInvalidUtf8))
    {
        eResult = xwork__explore_status_result(XWORK_EXPLORE_OUT_OF_MEMORY, NULL, pError);
        goto cleanup;
    }
    if (!xworkToolOutputSet(pOutput, true, tOutput.pData ? tOutput.pData : "(empty directory)\n"))
    {
        eResult = xwork__explore_status_result(XWORK_EXPLORE_OUT_OF_MEMORY, NULL, pError);
        goto cleanup;
    }
    eResult = XWORK_RESULT_OK;
cleanup:
    xwork__explore_entries_unit(&tEntries);
    xwork__buf_unit(&tOutput);
    free(sResolved);
    if (tArgs)
        xrtValueRelease(tArgs);
    return eResult;
}

typedef struct xwork_glob_segments
{
    const char *pText[65];
    size_t iLength[65];
    size_t iCount;
} xwork_glob_segments;

static size_t xwork__explore_utf8_scalar_size(const char *pText, size_t iLength, size_t iOffset)
{
    unsigned char c;
    if (iOffset >= iLength)
        return 0u;
    c = (unsigned char)pText[iOffset];
    if (c < 0x80u)
        return 1u;
    if (c < 0xe0u && iOffset + 2u <= iLength)
        return 2u;
    if (c < 0xf0u && iOffset + 3u <= iLength)
        return 3u;
    if (iOffset + 4u <= iLength)
        return 4u;
    return 1u;
}

static bool xwork__explore_glob_byte_equal(char a, char b, bool bIgnoreCase)
{
    if (bIgnoreCase)
    {
        if (a >= 'A' && a <= 'Z')
            a = (char)(a + ('a' - 'A'));
        if (b >= 'A' && b <= 'Z')
            b = (char)(b + ('a' - 'A'));
    }
    return a == b;
}

static bool xwork__explore_glob_segment(const char *pPattern, size_t iPatternLength,
                                        const char *pText, size_t iTextLength, bool bIgnoreCase)
{
    size_t iPattern = 0u;
    size_t iText = 0u;
    size_t iStar = SIZE_MAX;
    size_t iStarText = 0u;
    while (iText < iTextLength)
    {
        if (iPattern < iPatternLength && pPattern[iPattern] == '?')
        {
            iText += xwork__explore_utf8_scalar_size(pText, iTextLength, iText);
            ++iPattern;
        }
        else if (iPattern < iPatternLength && pPattern[iPattern] != '*' &&
                 xwork__explore_glob_byte_equal(pPattern[iPattern], pText[iText], bIgnoreCase))
        {
            ++iPattern;
            ++iText;
        }
        else if (iPattern < iPatternLength && pPattern[iPattern] == '*')
        {
            iStar = iPattern++;
            iStarText = iText;
        }
        else if (iStar != SIZE_MAX && iStarText < iTextLength)
        {
            iStarText += xwork__explore_utf8_scalar_size(pText, iTextLength, iStarText);
            iText = iStarText;
            iPattern = iStar + 1u;
        }
        else
        {
            return false;
        }
    }
    while (iPattern < iPatternLength && pPattern[iPattern] == '*')
        ++iPattern;
    return iPattern == iPatternLength;
}

static bool xwork__explore_split_segments(const char *sText, xwork_glob_segments *pSegments)
{
    const char *pStart = sText;
    const char *p = sText;
    memset(pSegments, 0, sizeof(*pSegments));
    for (;; ++p)
    {
        if (*p == '/' || *p == '\0')
        {
            if (p == pStart || pSegments->iCount >= 64u)
                return false;
            pSegments->pText[pSegments->iCount] = pStart;
            pSegments->iLength[pSegments->iCount] = (size_t)(p - pStart);
            ++pSegments->iCount;
            if (*p == '\0')
                return true;
            pStart = p + 1u;
        }
    }
}

static bool xwork__explore_glob_match(const char *sPattern, const char *sRelative, bool bIgnoreCase)
{
    xwork_glob_segments tPattern;
    xwork_glob_segments tPath;
    unsigned char arrReach[65][65];
    size_t iPattern;
    size_t iPath;
    if (!strchr(sPattern, '/'))
    {
        const char *sBase = strrchr(sRelative, '/');
        sBase = sBase ? sBase + 1u : sRelative;
        return xwork__explore_glob_segment(sPattern, strlen(sPattern), sBase, strlen(sBase),
                                           bIgnoreCase);
    }
    if (!xwork__explore_split_segments(sPattern, &tPattern) ||
        !xwork__explore_split_segments(sRelative, &tPath))
        return false;
    memset(arrReach, 0, sizeof(arrReach));
    arrReach[0][0] = 1u;
    for (iPattern = 0u; iPattern < tPattern.iCount; ++iPattern)
    {
        bool bDoubleStar = tPattern.iLength[iPattern] == 2u && tPattern.pText[iPattern][0] == '*' &&
                           tPattern.pText[iPattern][1] == '*';
        for (iPath = 0u; iPath <= tPath.iCount; ++iPath)
        {
            if (!arrReach[iPattern][iPath])
                continue;
            if (bDoubleStar)
            {
                arrReach[iPattern + 1u][iPath] = 1u;
                if (iPath < tPath.iCount)
                    arrReach[iPattern][iPath + 1u] = 1u;
            }
            else if (iPath < tPath.iCount &&
                     xwork__explore_glob_segment(tPattern.pText[iPattern],
                                                 tPattern.iLength[iPattern], tPath.pText[iPath],
                                                 tPath.iLength[iPath], bIgnoreCase))
            {
                arrReach[iPattern + 1u][iPath + 1u] = 1u;
            }
        }
    }
    return arrReach[tPattern.iCount][tPath.iCount] != 0u;
}

static xwork_explore_status xwork__explore_normalize_pattern(const char *sPattern,
                                                             char **psNormalized)
{
    char *sCopy;
    size_t i;
    size_t iLength;
    if (!psNormalized)
        return XWORK_EXPLORE_IO;
    *psNormalized = NULL;
    if (!sPattern)
        return XWORK_EXPLORE_IO;
    iLength = strlen(sPattern);
    if (!iLength || iLength > XWORK_EXPLORE_MAX_PATTERN)
        return XWORK_EXPLORE_IO;
    sCopy = xwork__strdup(sPattern);
    if (!sCopy)
        return XWORK_EXPLORE_OUT_OF_MEMORY;
    for (i = 0u; i < iLength; ++i)
    {
        if (sCopy[i] == '\\')
            sCopy[i] = '/';
    }
    if (sCopy[0] == '/' || sCopy[iLength - 1u] == '/' || strstr(sCopy, "//"))
    {
        free(sCopy);
        return XWORK_EXPLORE_IO;
    }
    *psNormalized = sCopy;
    return XWORK_EXPLORE_OK;
}

static bool xwork__explore_append_scan_summary(xwork_buf *pOutput,
                                               const xwork_explore_files *pFiles)
{
    if (pFiles->bScanLimited || pFiles->iInvalidUtf8 || pFiles->iLinksSkipped ||
        pFiles->iSpecialSkipped || pFiles->iIgnoredDirectories || pFiles->iDepthSkipped)
    {
        return xwork__buf_appendf(pOutput,
                                  "[scan: limited=%s invalid_utf8_names=%zu links=%zu "
                                  "special=%zu ignored_dirs=%zu depth_limited=%zu]\n",
                                  pFiles->bScanLimited ? "true" : "false", pFiles->iInvalidUtf8,
                                  pFiles->iLinksSkipped, pFiles->iSpecialSkipped,
                                  pFiles->iIgnoredDirectories, pFiles->iDepthSkipped);
    }
    return true;
}

static xwork_result xwork__tool_glob(void *pUserData, const xwork_tool_context *pContext,
                                     const char *sArgumentsJson, xwork_tool_output *pOutput,
                                     xwork_error *pError)
{
    xwork_agent *pAgent = (xwork_agent *)pUserData;
    xvalue *tArgs = xwork__json_parse_object(sArgumentsJson);
    const char *sPatternInput;
    const char *sPath;
    char *sPattern = NULL;
    char *sResolved = NULL;
    char *sWorkspacePrefix = NULL;
    uint64_t uMax;
    uint64_t uMaxDepth;
    bool bIgnoreCase;
    bool bHidden;
    bool bValid;
    size_t i;
    size_t iMatches = 0u;
    size_t iShown = 0u;
    xwork_explore_files tFiles = {0};
    xwork_buf tOutput = {0};
    xwork_explore_status eStatus;
    xwork_result eResult = XWORK_RESULT_ERROR;
    if (!tArgs)
        return xwork__tool_fail(pOutput, "invalid arguments: expected a JSON object");
    sPatternInput = xwork__json_text(tArgs, "pattern");
    eStatus = xwork__explore_normalize_pattern(sPatternInput, &sPattern);
    if (eStatus == XWORK_EXPLORE_OUT_OF_MEMORY)
    {
        eResult = xwork__explore_status_result(eStatus, NULL, pError);
        goto cleanup;
    }
    if (eStatus != XWORK_EXPLORE_OK)
    {
        eResult =
            xwork__tool_fail(pOutput, "pattern must be 1-4096 bytes with non-empty '/' segments");
        goto cleanup;
    }
    sPath = xwork__json_text(tArgs, "path");
    if (!sPath || !sPath[0])
        sPath = ".";
    uMax = xwork__json_u64(tArgs, "max_results", 200u, &bValid);
    if (!bValid || !uMax || uMax > 1000u)
    {
        eResult = xwork__tool_fail(pOutput, "max_results must be between 1 and 1000");
        goto cleanup;
    }
    uMaxDepth = xwork__json_u64(tArgs, "max_depth", 32u, &bValid);
    if (!bValid || !uMaxDepth || uMaxDepth > XWORK_EXPLORE_MAX_DEPTH)
    {
        eResult = xwork__tool_fail(pOutput, "max_depth must be between 1 and 64");
        goto cleanup;
    }
    bIgnoreCase = xwork__json_bool(tArgs, "ignore_case", false, &bValid);
    if (!bValid)
    {
        eResult = xwork__tool_fail(pOutput, "ignore_case must be boolean");
        goto cleanup;
    }
    bHidden = xwork__json_bool(tArgs, "hidden", false, &bValid);
    if (!bValid)
    {
        eResult = xwork__tool_fail(pOutput, "hidden must be boolean");
        goto cleanup;
    }
    sResolved = xwork__explore_resolve_existing(pAgent, sPath, pError);
    if (!sResolved)
    {
        eResult = xwork__tool_fail(pOutput, pError && pError->sMessage[0] ? pError->sMessage
                                                                          : "path denied");
        goto cleanup;
    }
    {
        xfileinfo tInfo;
        if (!xrtPathStat(sResolved, false, &tInfo) || tInfo.Type != XFILE_TYPE_DIRECTORY)
        {
            eResult = xwork__tool_fail(pOutput, "path is not a directory or is a symbolic link");
            goto cleanup;
        }
    }
    sWorkspacePrefix = xwork__explore_workspace_relative(pAgent, sResolved);
    if (!sWorkspacePrefix)
    {
        eResult = xwork__explore_status_result(XWORK_EXPLORE_OUT_OF_MEMORY, NULL, pError);
        goto cleanup;
    }
    tFiles.bIncludeHidden = bHidden;
    tFiles.uMaxDepth = (uint32_t)uMaxDepth;
    tFiles.pContext = pContext;
    eStatus = xwork__explore_collect_files(&tFiles, sResolved, sWorkspacePrefix);
    if (eStatus != XWORK_EXPLORE_OK)
    {
        eResult =
            xwork__explore_status_result(eStatus, "failed to enumerate files for glob", pError);
        goto cleanup;
    }
    for (i = 0u; i < tFiles.iCount; ++i)
    {
        if (xwork__explore_glob_match(sPattern, tFiles.pItems[i].sSearchRelative, bIgnoreCase))
        {
            ++iMatches;
            if (iShown < (size_t)uMax)
            {
                if (!xwork__buf_appendf(&tOutput, "%s\n", tFiles.pItems[i].sWorkspaceRelative))
                {
                    eResult =
                        xwork__explore_status_result(XWORK_EXPLORE_OUT_OF_MEMORY, NULL, pError);
                    goto cleanup;
                }
                ++iShown;
            }
        }
    }
    if (!iMatches && !xwork__buf_append_cstr(&tOutput, "(no matches)\n"))
    {
        eResult = xwork__explore_status_result(XWORK_EXPLORE_OUT_OF_MEMORY, NULL, pError);
        goto cleanup;
    }
    if (iMatches > iShown &&
        !xwork__buf_appendf(&tOutput, "[truncated: shown=%zu matches=%zu]\n", iShown, iMatches))
    {
        eResult = xwork__explore_status_result(XWORK_EXPLORE_OUT_OF_MEMORY, NULL, pError);
        goto cleanup;
    }
    if (!xwork__explore_append_scan_summary(&tOutput, &tFiles))
    {
        eResult = xwork__explore_status_result(XWORK_EXPLORE_OUT_OF_MEMORY, NULL, pError);
        goto cleanup;
    }
    if (!xworkToolOutputSet(pOutput, true, tOutput.pData ? tOutput.pData : "(no matches)\n"))
    {
        eResult = xwork__explore_status_result(XWORK_EXPLORE_OUT_OF_MEMORY, NULL, pError);
        goto cleanup;
    }
    eResult = XWORK_RESULT_OK;
cleanup:
    xwork__explore_files_unit(&tFiles);
    xwork__buf_unit(&tOutput);
    free(sWorkspacePrefix);
    free(sResolved);
    free(sPattern);
    if (tArgs)
        xrtValueRelease(tArgs);
    return eResult;
}

typedef struct xwork_grep_state
{
    xregex *pRegex;
    xregexmatcher *pMatcher;
    const char *sLiteral;
    size_t iLiteralLength;
    bool bIgnoreCase;
    size_t iMaxResults;
    size_t iMatches;
    size_t iShown;
    size_t iBytesScanned;
    size_t iBinarySkipped;
    size_t iOversizedSkipped;
    bool bTruncated;
    bool bScanLimited;
    bool ResultLogs;
    const xwork_tool_context *pContext;
    xwork_buf *pOutput;
} xwork_grep_state;

static const char *xwork__explore_memmem_ascii(const char *pHaystack, size_t iHaystack,
                                               const char *pNeedle, size_t iNeedle,
                                               bool bIgnoreCase)
{
    size_t i;
    size_t j;
    if (!iNeedle)
        return pHaystack;
    if (iNeedle > iHaystack)
        return NULL;
    for (i = 0u; i <= iHaystack - iNeedle; ++i)
    {
        for (j = 0u; j < iNeedle; ++j)
        {
            if (!xwork__explore_glob_byte_equal(pHaystack[i + j], pNeedle[j], bIgnoreCase))
                break;
        }
        if (j == iNeedle)
            return pHaystack + i;
    }
    return NULL;
}

static xwork_explore_status xwork__explore_line_match(xwork_grep_state *pState, const char *pLine,
                                                      size_t iLineLength, size_t *piBegin,
                                                      size_t *piEnd, bool *pbMatch)
{
    *pbMatch = false;
    *piBegin = 0u;
    *piEnd = 0u;
    if (pState->pMatcher)
    {
        xregexresult eRegex =
            xrtRegexMatcherFind(pState->pMatcher, (xstrview){pLine, iLineLength}, 0u);
        if (eRegex == XREGEX_ERROR)
            return XWORK_EXPLORE_REGEX_ERROR;
        if (eRegex == XREGEX_MATCH)
        {
            xregexcapture tCapture;
            if (!xrtRegexMatcherCapture(pState->pMatcher, 0u, &tCapture) || !tCapture.Matched)
                return XWORK_EXPLORE_REGEX_ERROR;
            *piBegin = tCapture.Span.Begin;
            *piEnd = tCapture.Span.End;
            *pbMatch = true;
        }
    }
    else
    {
        const char *pFound = xwork__explore_memmem_ascii(
            pLine, iLineLength, pState->sLiteral, pState->iLiteralLength, pState->bIgnoreCase);
        if (pFound)
        {
            *piBegin = (size_t)(pFound - pLine);
            *piEnd = *piBegin + pState->iLiteralLength;
            *pbMatch = true;
        }
    }
    return XWORK_EXPLORE_OK;
}

static bool xwork__explore_append_preview(xwork_buf *pOutput, const char *pLine, size_t iLineLength,
                                          size_t iMatchBegin, size_t iMatchEnd)
{
    size_t iStart;
    size_t iEnd;
    size_t i;
    if (iLineLength <= XWORK_EXPLORE_LINE_PREVIEW)
    {
        iStart = 0u;
        iEnd = iLineLength;
    }
    else
    {
        size_t iCenter = iMatchBegin + (iMatchEnd - iMatchBegin) / 2u;
        iStart = iCenter > XWORK_EXPLORE_LINE_PREVIEW / 2u
                     ? iCenter - XWORK_EXPLORE_LINE_PREVIEW / 2u
                     : 0u;
        if (iStart > iLineLength - XWORK_EXPLORE_LINE_PREVIEW)
            iStart = iLineLength - XWORK_EXPLORE_LINE_PREVIEW;
        while (iStart < iLineLength && (((unsigned char)pLine[iStart] & 0xc0u) == 0x80u))
            ++iStart;
        iEnd = iStart + XWORK_EXPLORE_LINE_PREVIEW;
        if (iEnd > iLineLength)
            iEnd = iLineLength;
        while (iEnd > iStart && iEnd < iLineLength &&
               (((unsigned char)pLine[iEnd] & 0xc0u) == 0x80u))
            --iEnd;
    }
    if (iStart && !xwork__buf_append_cstr(pOutput, "..."))
        return false;
    for (i = iStart; i < iEnd; ++i)
    {
        unsigned char c = (unsigned char)pLine[i];
        if (c == '\t')
        {
            if (!xwork__buf_append_cstr(pOutput, "\\t"))
                return false;
        }
        else if (c < 0x20u || c == 0x7fu)
        {
            if (!xwork__buf_appendf(pOutput, "\\x%02X", (unsigned)c))
                return false;
        }
        else if (!xwork__buf_append(pOutput, (const char *)&pLine[i], 1u))
            return false;
    }
    if (iEnd < iLineLength && !xwork__buf_append_cstr(pOutput, "..."))
        return false;
    return true;
}

static xwork_explore_status xwork__explore_read_file(const xwork_explore_file *pFile,
                                                     xwork_grep_state *pState,
                                                     unsigned char **ppData, size_t *piSize)
{
    xfileoptions tOptions;
    xfile hFile;
    xfileinfo tInfo;
    unsigned char *pData;
    size_t iUsed = 0u;
    size_t iRead;
    xwork_explore_status eStatus;
    *ppData = NULL;
    *piSize = 0u;
    if (xwork__explore_binary_extension(pFile->sWorkspaceRelative))
    {
        ++pState->iBinarySkipped;
        return XWORK_EXPLORE_OK;
    }
    xrtFileOptionsInit(&tOptions);
    hFile = xrtFileOpen(pFile->sAbsolute, &tOptions);
    if (!hFile || !xrtFileStat(hFile, &tInfo) || tInfo.Type != XFILE_TYPE_FILE)
    {
        if (hFile)
            (void)xrtClose(hFile);
        return XWORK_EXPLORE_IO;
    }
    if (tInfo.Size > XWORK_EXPLORE_MAX_FILE_BYTES)
    {
        ++pState->iOversizedSkipped;
        (void)xrtClose(hFile);
        return XWORK_EXPLORE_OK;
    }
    if (tInfo.Size > XWORK_EXPLORE_MAX_TOTAL_BYTES - pState->iBytesScanned)
    {
        pState->bScanLimited = true;
        (void)xrtClose(hFile);
        return XWORK_EXPLORE_LIMIT;
    }
    if (tInfo.Size > SIZE_MAX - 1u)
    {
        (void)xrtClose(hFile);
        return XWORK_EXPLORE_LIMIT;
    }
    pData = (unsigned char *)malloc((size_t)tInfo.Size + 1u);
    if (!pData)
    {
        (void)xrtClose(hFile);
        return XWORK_EXPLORE_OUT_OF_MEMORY;
    }
    while (iUsed < (size_t)tInfo.Size)
    {
        size_t iChunk = (size_t)tInfo.Size - iUsed;
        if (iChunk > 64u * 1024u)
            iChunk = 64u * 1024u;
        eStatus = xwork__explore_scope_status(pState->pContext);
        if (eStatus != XWORK_EXPLORE_OK)
        {
            free(pData);
            (void)xrtClose(hFile);
            return eStatus;
        }
        if (!xrtRead(hFile, pData + iUsed, iChunk, &iRead))
        {
            free(pData);
            (void)xrtClose(hFile);
            return XWORK_EXPLORE_IO;
        }
        if (!iRead)
            break;
        iUsed += iRead;
    }
    if (!xrtClose(hFile))
    {
        free(pData);
        return XWORK_EXPLORE_IO;
    }
    pData[iUsed] = 0u;
    pState->iBytesScanned += iUsed;
    if (memchr(pData, 0, iUsed) || !xrtUtf8Valid((xstrview){(const char *)pData, iUsed}, NULL))
    {
        ++pState->iBinarySkipped;
        free(pData);
        return XWORK_EXPLORE_OK;
    }
    *ppData = pData;
    *piSize = iUsed;
    return XWORK_EXPLORE_OK;
}

/* Search large result files with bounded memory. Ordinary workspace limits
 * stay unchanged. A snapshot ends at the size observed when the file opens. */
static xwork_explore_status xwork__explore_grep_log_line(const xwork_explore_file *File,
                                                         xwork_grep_state *State, const char *Text,
                                                         size_t Size, size_t Line)
{
    if (Size && Text[Size - 1u] == '\r')
        Size--;
    if (memchr(Text, 0, Size) || !xrtUtf8Valid((xstrview){Text, Size}, NULL))
        return XWORK_EXPLORE_IO;
    if (State->bTruncated)
        return XWORK_EXPLORE_OK;
    size_t Begin = 0, End = 0;
    bool Match = false;
    xwork_explore_status Result =
        xwork__explore_line_match(State, Text, Size, &Begin, &End, &Match);
    if (Result != XWORK_EXPLORE_OK || !Match)
        return Result;
    State->iMatches++;
    if (State->iShown >= State->iMaxResults)
    {
        State->bTruncated = true;
        return XWORK_EXPLORE_OK;
    }
    if (!xwork__buf_appendf(State->pOutput, "%s:%zu: ", File->sWorkspaceRelative, Line) ||
        !xwork__explore_append_preview(State->pOutput, Text, Size, Begin, End) ||
        !xwork__buf_append_char(State->pOutput, '\n'))
        return XWORK_EXPLORE_OUT_OF_MEMORY;
    State->iShown++;
    return XWORK_EXPLORE_OK;
}

static xwork_explore_status xwork__explore_grep_log(const xwork_explore_file *File,
                                                    xwork_grep_state *State)
{
    if (xwork__explore_binary_extension(File->sWorkspaceRelative))
    {
        State->iBinarySkipped++;
        return XWORK_EXPLORE_OK;
    }
    xfileoptions Options;
    xrtFileOptionsInit(&Options);
    Options.Flags = XFILE_READ | XFILE_NOFOLLOW;
    xfile Handle = xrtFileOpen(File->sAbsolute, &Options);
    xfileinfo Info;
    if (!Handle || !xrtFileStat(Handle, &Info) || Info.Type != XFILE_TYPE_FILE)
    {
        if (Handle)
            xrtClose(Handle);
        return XWORK_EXPLORE_IO;
    }
    if (Info.Size > XWORK_EXPLORE_MAX_TOTAL_BYTES - State->iBytesScanned)
    {
        State->bScanLimited = true;
        xrtClose(Handle);
        return XWORK_EXPLORE_LIMIT;
    }
    size_t BeforeSize = State->pOutput->iLen, BeforeShown = State->iShown,
           BeforeMatches = State->iMatches;
    bool BeforeTruncated = State->bTruncated, Binary = false;
    xwork_buf Line = {0};
    size_t LineNumber = 0;
    uint64_t Remaining = Info.Size;
    unsigned char Chunk[32768];
    xwork_explore_status Result = XWORK_EXPLORE_OK;
    while (Remaining)
    {
        Result = xwork__explore_scope_status(State->pContext);
        if (Result != XWORK_EXPLORE_OK)
            break;
        size_t Want = Remaining > sizeof(Chunk) ? sizeof(Chunk) : (size_t)Remaining, Got = 0;
        if (!xrtRead(Handle, Chunk, Want, &Got))
        {
            Result = XWORK_EXPLORE_IO;
            break;
        }
        if (!Got)
        {
            State->bScanLimited = true;
            Result = XWORK_EXPLORE_LIMIT;
            break;
        }
        Remaining -= Got;
        State->iBytesScanned += Got;
        size_t At = 0;
        while (At < Got)
        {
            const unsigned char *Newline = memchr(Chunk + At, '\n', Got - At);
            size_t N = Newline ? (size_t)(Newline - (Chunk + At)) : Got - At;
            if (N > XWORK_EXPLORE_MAX_FILE_BYTES - Line.iLen)
            {
                State->bScanLimited = true;
                Result = XWORK_EXPLORE_LIMIT;
                break;
            }
            if (!xwork__buf_append(&Line, Chunk + At, N))
            {
                Result = XWORK_EXPLORE_OUT_OF_MEMORY;
                break;
            }
            At += N + (Newline ? 1u : 0u);
            if (Newline)
            {
                Result = xwork__explore_grep_log_line(File, State, Line.pData ? Line.pData : "",
                                                      Line.iLen, ++LineNumber);
                if (Result == XWORK_EXPLORE_IO)
                {
                    Binary = true;
                    break;
                }
                if (Result != XWORK_EXPLORE_OK)
                    break;
                Line.iLen = 0;
                if (Line.pData)
                    Line.pData[0] = 0;
            }
        }
        if (Binary || Result != XWORK_EXPLORE_OK)
            break;
    }
    if (!Binary && Result == XWORK_EXPLORE_OK && Line.iLen)
    {
        Result = xwork__explore_grep_log_line(File, State, Line.pData, Line.iLen, ++LineNumber);
        if (Result == XWORK_EXPLORE_IO)
            Binary = true;
    }
    if (Binary)
    {
        State->pOutput->iLen = BeforeSize;
        if (State->pOutput->pData)
            State->pOutput->pData[BeforeSize] = 0;
        State->iShown = BeforeShown;
        State->iMatches = BeforeMatches;
        State->bTruncated = BeforeTruncated;
        State->iBinarySkipped++;
        Result = XWORK_EXPLORE_OK;
    }
    xwork__buf_unit(&Line);
    if (!xrtClose(Handle) && Result == XWORK_EXPLORE_OK)
        Result = XWORK_EXPLORE_IO;
    return Result;
}

static xwork_explore_status xwork__explore_grep_file(const xwork_explore_file *pFile,
                                                     xwork_grep_state *pState)
{
    if (pState->ResultLogs)
        return xwork__explore_grep_log(pFile, pState);
    unsigned char *pData = NULL;
    size_t iSize = 0u;
    size_t iOffset = 0u;
    size_t iLine = 0u;
    xwork_explore_status eStatus = xwork__explore_read_file(pFile, pState, &pData, &iSize);
    if (eStatus != XWORK_EXPLORE_OK || !pData)
        return eStatus;
    while (iOffset < iSize)
    {
        const char *pLine = (const char *)pData + iOffset;
        const char *pNewline = (const char *)memchr(pLine, '\n', iSize - iOffset);
        size_t iLength = pNewline ? (size_t)(pNewline - pLine) : iSize - iOffset;
        size_t iBegin;
        size_t iEnd;
        bool bMatch;
        ++iLine;
        if (iLength && pLine[iLength - 1u] == '\r')
            --iLength;
        eStatus = xwork__explore_scope_status(pState->pContext);
        if (eStatus != XWORK_EXPLORE_OK)
            break;
        eStatus = xwork__explore_line_match(pState, pLine, iLength, &iBegin, &iEnd, &bMatch);
        if (eStatus != XWORK_EXPLORE_OK)
            break;
        if (bMatch)
        {
            ++pState->iMatches;
            if (pState->iShown >= pState->iMaxResults)
            {
                pState->bTruncated = true;
                break;
            }
            if (!xwork__buf_appendf(pState->pOutput, "%s:%zu: ", pFile->sWorkspaceRelative,
                                    iLine) ||
                !xwork__explore_append_preview(pState->pOutput, pLine, iLength, iBegin, iEnd) ||
                !xwork__buf_append_cstr(pState->pOutput, "\n"))
            {
                eStatus = XWORK_EXPLORE_OUT_OF_MEMORY;
                break;
            }
            ++pState->iShown;
        }
        iOffset = pNewline ? (size_t)(pNewline - (const char *)pData) + 1u : iSize;
    }
    free(pData);
    return eStatus;
}

static xwork_result xwork__tool_grep(void *pUserData, const xwork_tool_context *pContext,
                                     const char *sArgumentsJson, xwork_tool_output *pOutput,
                                     xwork_error *pError)
{
    xwork_agent *pAgent = (xwork_agent *)pUserData;
    xvalue *tArgs = xwork__json_parse_object(sArgumentsJson);
    const char *sPattern;
    const char *sPath;
    char *sResolved = NULL;
    char *sWorkspacePrefix = NULL;
    bool bRegex;
    bool bIgnoreCase;
    bool bHidden;
    bool bValid;
    uint64_t uMax;
    uint64_t uMaxDepth;
    xfileinfo tRootInfo;
    xwork_explore_files tFiles = {0};
    xwork_grep_state tGrep;
    xwork_buf tOutput = {0};
    xwork_explore_status eStatus = XWORK_EXPLORE_OK;
    xwork_result eResult = XWORK_RESULT_ERROR;
    size_t i;
    memset(&tGrep, 0, sizeof(tGrep));
    if (!tArgs)
        return xwork__tool_fail(pOutput, "invalid arguments: expected a JSON object");
    sPattern = xwork__json_text(tArgs, "pattern");
    if (!sPattern || !sPattern[0] || strlen(sPattern) > XWORK_EXPLORE_MAX_PATTERN)
    {
        eResult = xwork__tool_fail(pOutput, "pattern must be between 1 and 4096 bytes");
        goto cleanup;
    }
    sPath = xwork__json_text(tArgs, "path");
    if (!sPath || !sPath[0])
        sPath = ".";
    bRegex = xwork__json_bool(tArgs, "regex", true, &bValid);
    if (!bValid)
    {
        eResult = xwork__tool_fail(pOutput, "regex must be boolean");
        goto cleanup;
    }
    bIgnoreCase = xwork__json_bool(tArgs, "ignore_case", false, &bValid);
    if (!bValid)
    {
        eResult = xwork__tool_fail(pOutput, "ignore_case must be boolean");
        goto cleanup;
    }
    bHidden = xwork__json_bool(tArgs, "hidden", false, &bValid);
    if (!bValid)
    {
        eResult = xwork__tool_fail(pOutput, "hidden must be boolean");
        goto cleanup;
    }
    uMax = xwork__json_u64(tArgs, "max_results", 100u, &bValid);
    if (!bValid || !uMax || uMax > 500u)
    {
        eResult = xwork__tool_fail(pOutput, "max_results must be between 1 and 500");
        goto cleanup;
    }
    uMaxDepth = xwork__json_u64(tArgs, "max_depth", 32u, &bValid);
    if (!bValid || !uMaxDepth || uMaxDepth > XWORK_EXPLORE_MAX_DEPTH)
    {
        eResult = xwork__tool_fail(pOutput, "max_depth must be between 1 and 64");
        goto cleanup;
    }
    sResolved = xwork__explore_resolve_existing(pAgent, sPath, pError);
    if (!sResolved)
    {
        eResult = xwork__tool_fail(pOutput, pError && pError->sMessage[0] ? pError->sMessage
                                                                          : "path denied");
        goto cleanup;
    }
    if (!xrtPathStat(sResolved, false, &tRootInfo) ||
        (tRootInfo.Type != XFILE_TYPE_DIRECTORY && tRootInfo.Type != XFILE_TYPE_FILE))
    {
        eResult = xwork__tool_fail(pOutput,
                                   "path must be a regular file or directory, not a symbolic link");
        goto cleanup;
    }
    sWorkspacePrefix = xwork__explore_workspace_relative(pAgent, sResolved);
    if (!sWorkspacePrefix)
    {
        eResult = xwork__explore_status_result(XWORK_EXPLORE_OUT_OF_MEMORY, NULL, pError);
        goto cleanup;
    }
    tFiles.bIncludeHidden = bHidden;
    tFiles.uMaxDepth = (uint32_t)uMaxDepth;
    tFiles.pContext = pContext;
    if (tRootInfo.Type == XFILE_TYPE_FILE)
    {
        const char *sLeaf = strrchr(sWorkspacePrefix, '/');
        sLeaf = sLeaf ? sLeaf + 1u : sWorkspacePrefix;
        eStatus = xwork__explore_files_add(&tFiles, sResolved, sLeaf, sWorkspacePrefix, &tRootInfo);
    }
    else
    {
        eStatus = xwork__explore_collect_files(&tFiles, sResolved, sWorkspacePrefix);
    }
    if (eStatus != XWORK_EXPLORE_OK)
    {
        eResult =
            xwork__explore_status_result(eStatus, "failed to enumerate files for grep", pError);
        goto cleanup;
    }
    tGrep.ResultLogs = !strncmp(sWorkspacePrefix, "@results/", 9u);
    tGrep.sLiteral = sPattern;
    tGrep.iLiteralLength = strlen(sPattern);
    tGrep.bIgnoreCase = bIgnoreCase;
    tGrep.iMaxResults = (size_t)uMax;
    tGrep.pContext = pContext;
    tGrep.pOutput = &tOutput;
    if (bRegex)
    {
        xregexconfig tRegexConfig;
        xrtRegexConfigInit(&tRegexConfig);
        tRegexConfig.Flags = bIgnoreCase ? XREGEX_IGNORE_CASE : 0u;
        tRegexConfig.MaxPatternBytes = XWORK_EXPLORE_MAX_PATTERN;
        tRegexConfig.MaxCaptures = 128u;
        tGrep.pRegex = xrtRegexCompileConfig((xstrview){sPattern, strlen(sPattern)}, &tRegexConfig);
        if (!tGrep.pRegex)
        {
            eResult = xwork__tool_fail(pOutput, "invalid or over-budget regex pattern");
            goto cleanup;
        }
        tGrep.pMatcher = xrtRegexMatcherCreate(tGrep.pRegex);
        if (!tGrep.pMatcher)
        {
            eResult = xwork__explore_status_result(XWORK_EXPLORE_OUT_OF_MEMORY, NULL, pError);
            goto cleanup;
        }
    }
    for (i = 0u; i < tFiles.iCount && !tGrep.bTruncated; ++i)
    {
        eStatus = xwork__explore_grep_file(&tFiles.pItems[i], &tGrep);
        if (eStatus == XWORK_EXPLORE_LIMIT && tGrep.bScanLimited)
        {
            eStatus = XWORK_EXPLORE_OK;
            break;
        }
        if (eStatus != XWORK_EXPLORE_OK)
        {
            eResult = xwork__explore_status_result(eStatus, "failed to search a file", pError);
            goto cleanup;
        }
    }
    if (tGrep.ResultLogs && tGrep.bScanLimited &&
        !xwork__buf_append_cstr(&tOutput,
                                "[partial search: byte or line limit reached; read in pages or use "
                                "an external tool for the remaining data]\n"))
        goto cleanup;
    if (!tGrep.iMatches &&
        !xwork__buf_append_cstr(&tOutput, tGrep.bScanLimited ? "(no matches in scanned portion)\n"
                                                             : "(no matches)\n"))
    {
        eResult = xwork__explore_status_result(XWORK_EXPLORE_OUT_OF_MEMORY, NULL, pError);
        goto cleanup;
    }
    if (tGrep.bTruncated && !xwork__buf_appendf(&tOutput, "[truncated: shown=%zu matches>=%zu]\n",
                                                tGrep.iShown, tGrep.iMatches))
    {
        eResult = xwork__explore_status_result(XWORK_EXPLORE_OUT_OF_MEMORY, NULL, pError);
        goto cleanup;
    }
    tFiles.bScanLimited = tFiles.bScanLimited || tGrep.bScanLimited;
    if ((tGrep.iBinarySkipped || tGrep.iOversizedSkipped) &&
        !xwork__buf_appendf(&tOutput, "[skipped: binary_or_invalid_utf8=%zu oversized=%zu]\n",
                            tGrep.iBinarySkipped, tGrep.iOversizedSkipped))
    {
        eResult = xwork__explore_status_result(XWORK_EXPLORE_OUT_OF_MEMORY, NULL, pError);
        goto cleanup;
    }
    if (!xwork__explore_append_scan_summary(&tOutput, &tFiles))
    {
        eResult = xwork__explore_status_result(XWORK_EXPLORE_OUT_OF_MEMORY, NULL, pError);
        goto cleanup;
    }
    if (!xworkToolOutputSet(pOutput, true, tOutput.pData ? tOutput.pData : "(no matches)\n"))
    {
        eResult = xwork__explore_status_result(XWORK_EXPLORE_OUT_OF_MEMORY, NULL, pError);
        goto cleanup;
    }
    eResult = XWORK_RESULT_OK;
cleanup:
    if (tGrep.pMatcher)
        xrtRegexMatcherFree(tGrep.pMatcher);
    if (tGrep.pRegex)
        xrtRegexRelease(tGrep.pRegex);
    xwork__explore_files_unit(&tFiles);
    xwork__buf_unit(&tOutput);
    free(sWorkspacePrefix);
    free(sResolved);
    if (tArgs)
        xrtValueRelease(tArgs);
    return eResult;
}

bool xwork__register_explore_tools(xwork_agent *pAgent, xwork_error *pError)
{
    static const xwork_tool_definition arrTools[] = {
        {"ls",
         "List one workspace directory in deterministic UTF-8 order. "
         "Directories end with '/', links with '@', and special entries "
         "with '?'. Long mode adds stable type, byte-size, and Unix-epoch-"
         "microsecond columns.",
         "{\"type\":\"object\",\"properties\":{\"path\":{\"type\":\"string\"},"
         "\"all\":{\"type\":\"boolean\"},\"long\":{\"type\":\"boolean\"},"
         "\"max_results\":{\"type\":\"integer\",\"minimum\":1,\"maximum\":2000}},"
         "\"additionalProperties\":false}",
         true, XWORK_TOOL_EFFECT_READ_ONLY, xwork__tool_ls, NULL, NULL, NULL, NULL, 0u, NULL, NULL,
         false, NULL},
        {"glob",
         "Find regular workspace files with deterministic native globbing. "
         "'*' and '?' stay within one path segment; a complete '**' segment "
         "crosses directories. Matching is case-sensitive unless requested.",
         "{\"type\":\"object\",\"properties\":{\"pattern\":{\"type\":\"string\","
         "\"minLength\":1,\"maxLength\":4096},\"path\":{\"type\":\"string\"},"
         "\"ignore_case\":{\"type\":\"boolean\"},\"hidden\":{\"type\":\"boolean\"},"
         "\"max_depth\":{\"type\":\"integer\",\"minimum\":1,\"maximum\":64},"
         "\"max_results\":{\"type\":\"integer\",\"minimum\":1,\"maximum\":1000}},"
         "\"required\":[\"pattern\"],\"additionalProperties\":false}",
         true, XWORK_TOOL_EFFECT_READ_ONLY, xwork__tool_glob, NULL, NULL, NULL, NULL, 0u, NULL,
         NULL, false, NULL},
        {"grep",
         "Search UTF-8 text in regular workspace files without external "
         "programs. Results are deterministic workspace-relative "
         "file:line previews; links, binary files, and oversized files "
         "are skipped and reported.",
         "{\"type\":\"object\",\"properties\":{\"pattern\":{\"type\":\"string\","
         "\"minLength\":1,\"maxLength\":4096},\"path\":{\"type\":\"string\"},"
         "\"regex\":{\"type\":\"boolean\"},\"ignore_case\":{\"type\":\"boolean\"},"
         "\"hidden\":{\"type\":\"boolean\"},\"max_depth\":{\"type\":\"integer\","
         "\"minimum\":1,\"maximum\":64},\"max_results\":{\"type\":\"integer\","
         "\"minimum\":1,\"maximum\":500}},\"required\":[\"pattern\"],"
         "\"additionalProperties\":false}",
         true, XWORK_TOOL_EFFECT_READ_ONLY, xwork__tool_grep, NULL, NULL, NULL, NULL, 0u, NULL,
         NULL, false, NULL}};
    xwork_tool_definition arrBound[sizeof(arrTools) / sizeof(arrTools[0])];
    size_t i;
    size_t iRemoved = 0u;
    if (!pAgent)
    {
        xwork__set_error(pError, XWORK_ERROR_INVALID_ARGUMENT, "agent is null");
        return false;
    }
    for (i = 0u; i < sizeof(arrTools) / sizeof(arrTools[0]); ++i)
    {
        arrBound[i] = arrTools[i];
        arrBound[i].pUserData = pAgent;
        arrBound[i].sSource = "builtin-explore";
        arrBound[i].OnDescribePermissions = xwork__describe_builtin_permissions;
    }
    if (!xworkAgentReplaceToolsBySource(pAgent, "builtin-explore", arrBound,
                                        sizeof(arrBound) / sizeof(arrBound[0]), &iRemoved, pError))
        return false;
    for (i = 0u; i < sizeof(arrTools) / sizeof(arrTools[0]); ++i)
    {
        xwork_tool_concurrency_config tConcurrency;
        xworkToolConcurrencyConfigInit(&tConcurrency);
        tConcurrency.sToolName = arrTools[i].sName;
        tConcurrency.bParallelSafe = true;
        if (!xworkAgentConfigureToolConcurrency(pAgent, &tConcurrency, pError))
            return false;
    }
    return true;
}
