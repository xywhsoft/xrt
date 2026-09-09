#include "../xwork_core/xwork_internal.h"
#include "../../lib/xllm-session.h"
#include "../../lib/xrt.h"

#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

#ifdef _WIN32
#include <direct.h>
#define XWORK__HOST_POPEN _popen
#define XWORK__HOST_PCLOSE _pclose
#else
#include <dirent.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#define XWORK__HOST_POPEN popen
#define XWORK__HOST_PCLOSE pclose
#endif

#define XWORK__LOCAL_HOST_DEFAULT_READ_BYTES (64u * 1024u)
#define XWORK__LOCAL_HOST_DEFAULT_PROCESS_INPUT_BYTES (64u * 1024u)
#define XWORK__LOCAL_HOST_DEFAULT_PROCESS_ENV_ENTRIES 16u
#define XWORK__LOCAL_HOST_DEFAULT_PROCESS_OUTPUT_BYTES (64u * 1024u)
#define XWORK__LOCAL_HOST_DEFAULT_TERMINAL_COLS 120u
#define XWORK__LOCAL_HOST_DEFAULT_TERMINAL_ROWS 30u
#define XWORK__LOCAL_HOST_PROCESS_TIMEOUT_GRACE_MS 100u

typedef struct xwork__local_editor_buffer {
    char *sBufferId;
    char *sPath;
    char *sResolvedPath;
    char *sText;
    bool bDirty;
    size_t iSelectionStart;
    size_t iSelectionEnd;
    struct xwork__local_editor_buffer *pNext;
} xwork__local_editor_buffer;

static const xwork_host_service *xwork__runtime_get_host_service_slot(
    const xwork_runtime *pRuntime,
    xwork_host_service_kind eKind
)
{
    if ( !pRuntime ) {
        return NULL;
    }

    switch ( eKind ) {
        case XWORK_HOST_NONE:
            return NULL;
        case XWORK_HOST_FILESYSTEM:
            return &pRuntime->tHostServices.tFilesystem;
        case XWORK_HOST_PROCESS:
            return &pRuntime->tHostServices.tProcess;
        case XWORK_HOST_VCS:
            return &pRuntime->tHostServices.tVcs;
        case XWORK_HOST_NETWORK:
            return &pRuntime->tHostServices.tNetwork;
        case XWORK_HOST_DIAGNOSTICS:
            return &pRuntime->tHostServices.tDiagnostics;
        case XWORK_HOST_EDITOR:
            return &pRuntime->tHostServices.tEditor;
        default:
            return NULL;
    }
}

static xwork_replay_entry_kind xwork__host_replay_entry_kind(
    xwork_host_service_kind eKind
)
{
    switch ( eKind ) {
        case XWORK_HOST_FILESYSTEM:
            return XWORK_REPLAY_ENTRY_FILESYSTEM;
        case XWORK_HOST_PROCESS:
            return XWORK_REPLAY_ENTRY_PROCESS;
        case XWORK_HOST_VCS:
        case XWORK_HOST_NETWORK:
        case XWORK_HOST_DIAGNOSTICS:
        case XWORK_HOST_EDITOR:
            return XWORK_REPLAY_ENTRY_HOST_TOOL;
        default:
            return XWORK_REPLAY_ENTRY_HOST_TOOL;
    }
}

static xwork_status xwork__runtime_replay_host_result(
    const xwork_runtime *pRuntime,
    const xwork_replay_entry_summary *pActual,
    xwork_tool_result *pResult
)
{
    xwork_runtime *pMutableRuntime;
    xwork_status iStatus;

    if ( !pRuntime || !pActual || !pResult ) {
        return XWORK_ERROR_INVALID_ARGUMENT;
    }
    if ( !pActual->sResultJson ) {
        return XWORK_ERROR_EXTERNAL_FAILURE;
    }

    pMutableRuntime = (xwork_runtime *)pRuntime;
    iStatus = xwork__replace_cstr(
        &pMutableRuntime->sLastReplayHostOutputText,
        pActual->sResultJson
    );
    if ( iStatus != XWORK_OK ) {
        return iStatus;
    }
    iStatus = xwork__replace_cstr(
        &pMutableRuntime->sLastReplayHostVisibleSummary,
        "replayed host service result"
    );
    if ( iStatus != XWORK_OK ) {
        return iStatus;
    }

    xwork_tool_result_init(pResult);
    pResult->sOutputText = pMutableRuntime->sLastReplayHostOutputText;
    pResult->sVisibleSummary = pMutableRuntime->sLastReplayHostVisibleSummary;
    pResult->bRetryable = false;
    return pActual->iStatus;
}

static bool xwork__local_host_is_absolute_path(const char *sPath)
{
    if ( !sPath || !sPath[0] ) {
        return false;
    }

#ifdef _WIN32
    if ( ((unsigned char)sPath[0] != 0u) &&
         isalpha((unsigned char)sPath[0]) &&
         sPath[1] == ':' &&
         (sPath[2] == '/' || sPath[2] == '\\') ) {
        return true;
    }
    if ( sPath[0] == '\\' && sPath[1] == '\\' ) {
        return true;
    }
#endif

    return sPath[0] == '/';
}

static char *xwork__local_host_join_path(const char *sBasePath, const char *sPath)
{
    size_t iBaseLength;

    if ( !sBasePath || !sBasePath[0] ) {
        return xwork__dup_cstr(sPath);
    }
    if ( !sPath || !sPath[0] ) {
        return xwork__dup_cstr(sBasePath);
    }

    iBaseLength = strlen(sBasePath);
    if ( sBasePath[iBaseLength - 1u] == '/' || sBasePath[iBaseLength - 1u] == '\\' ) {
        return xwork__dup_printf("%s%s", sBasePath, sPath);
    }
    return xwork__dup_printf("%s/%s", sBasePath, sPath);
}

static char *xwork__local_host_resolve_path(
    const xwork_local_host *pHost,
    const char *sPath
)
{
    if ( !sPath || !sPath[0] ) {
        return NULL;
    }
    if ( xwork__local_host_is_absolute_path(sPath) ) {
        return xwork__dup_cstr(sPath);
    }
    return xwork__local_host_join_path(
        pHost ? pHost->sDefaultWorkingDirectory : NULL,
        sPath
    );
}

static int xwork__local_host_path_char_cmp(char cLeft, char cRight)
{
#ifdef _WIN32
    cLeft = (char)tolower((unsigned char)cLeft);
    cRight = (char)tolower((unsigned char)cRight);
#endif
    return (int)((unsigned char)cLeft) - (int)((unsigned char)cRight);
}

static bool xwork__local_host_path_is_normalized_absolute(const char *sPath)
{
    if ( !sPath || !sPath[0] ) {
        return false;
    }
#ifdef _WIN32
    if ( isalpha((unsigned char)sPath[0]) && sPath[1] == ':' && sPath[2] == '/' ) {
        return true;
    }
    if ( sPath[0] == '/' && sPath[1] == '/' ) {
        return true;
    }
#endif
    return sPath[0] == '/';
}

static bool xwork__local_host_path_starts_with_parent(const char *sPath)
{
    return sPath &&
        sPath[0] == '.' &&
        sPath[1] == '.' &&
        (sPath[2] == '\0' || sPath[2] == '/');
}

static char *xwork__local_host_normalize_path(const char *sPath)
{
    char *sWorking;
    char **psComponents;
    size_t iLength;
    size_t iPrefixLength = 0u;
    size_t iComponentCount = 0u;
    size_t i;
    size_t iOutputLength;
    char *sOutput;
    char *sOut;

    if ( !sPath || !sPath[0] ) {
        return NULL;
    }

    iLength = strlen(sPath);
    sWorking = xwork__dup_cstr(sPath);
    if ( !sWorking ) {
        return NULL;
    }
    psComponents = (char **)calloc(iLength + 1u, sizeof(*psComponents));
    if ( !psComponents ) {
        free(sWorking);
        return NULL;
    }

    for ( i = 0u; sWorking[i] != '\0'; ++i ) {
        if ( sWorking[i] == '\\' ) {
            sWorking[i] = '/';
        }
    }

#ifdef _WIN32
    if ( isalpha((unsigned char)sWorking[0]) && sWorking[1] == ':' ) {
        iPrefixLength = 2u;
        if ( sWorking[2] == '/' ) {
            iPrefixLength = 3u;
        }
    } else if ( sWorking[0] == '/' && sWorking[1] == '/' ) {
        iPrefixLength = 2u;
    } else
#endif
    if ( sWorking[0] == '/' ) {
        iPrefixLength = 1u;
    }

    i = iPrefixLength;
    while ( i <= iLength ) {
        char *sPart = sWorking + i;
        size_t iPartLength = 0u;

        while ( i + iPartLength < iLength && sWorking[i + iPartLength] != '/' ) {
            ++iPartLength;
        }
        if ( i + iPartLength < iLength ) {
            sWorking[i + iPartLength] = '\0';
        }

        if ( iPartLength > 0u && strcmp(sPart, ".") != 0 ) {
            if ( strcmp(sPart, "..") == 0 ) {
                if ( iComponentCount > 0u &&
                     strcmp(psComponents[iComponentCount - 1u], "..") != 0 ) {
                    --iComponentCount;
                } else if ( iPrefixLength == 0u ) {
                    psComponents[iComponentCount++] = sPart;
                }
            } else {
                psComponents[iComponentCount++] = sPart;
            }
        }

        i += iPartLength + 1u;
        if ( i > iLength ) {
            break;
        }
    }

    iOutputLength = iPrefixLength;
    for ( i = 0u; i < iComponentCount; ++i ) {
        if ( iOutputLength > 0u && sWorking[iOutputLength - 1u] != '/' ) {
            ++iOutputLength;
        } else if ( iOutputLength == 0u && i > 0u ) {
            ++iOutputLength;
        }
        iOutputLength += strlen(psComponents[i]);
    }
    if ( iOutputLength == 0u ) {
        iOutputLength = 1u;
    }

    sOutput = (char *)calloc(iOutputLength + 1u, sizeof(char));
    if ( !sOutput ) {
        free(psComponents);
        free(sWorking);
        return NULL;
    }

    sOut = sOutput;
    if ( iPrefixLength > 0u ) {
        memcpy(sOut, sWorking, iPrefixLength);
        sOut += iPrefixLength;
    }
    for ( i = 0u; i < iComponentCount; ++i ) {
        if ( sOut > sOutput && sOut[-1] != '/' ) {
            *sOut++ = '/';
        } else if ( sOut == sOutput && i > 0u ) {
            *sOut++ = '/';
        }
        memcpy(sOut, psComponents[i], strlen(psComponents[i]));
        sOut += strlen(psComponents[i]);
    }
    if ( sOut == sOutput ) {
        *sOut++ = '.';
    }
    *sOut = '\0';

    free(psComponents);
    free(sWorking);
    return sOutput;
}

static bool xwork__local_host_path_has_prefix(const char *sPath, const char *sPrefix)
{
    size_t i;
    size_t iPathLength;
    size_t iPrefixLength;

    if ( !sPath || !sPrefix || !sPrefix[0] ) {
        return false;
    }

    iPathLength = strlen(sPath);
    iPrefixLength = strlen(sPrefix);
    if ( iPrefixLength > 1u && sPrefix[iPrefixLength - 1u] == '/' ) {
        --iPrefixLength;
    }
    if ( iPathLength < iPrefixLength ) {
        return false;
    }

    for ( i = 0u; i < iPrefixLength; ++i ) {
        if ( xwork__local_host_path_char_cmp(sPath[i], sPrefix[i]) != 0 ) {
            return false;
        }
    }

    return iPathLength == iPrefixLength || sPath[iPrefixLength] == '/';
}

static xwork_status xwork__local_host_copy_path_prefixes(
    char ***ppsTarget,
    size_t *piTargetCount,
    const char **psSource,
    size_t iSourceCount
)
{
    char **psTarget;
    size_t i;

    if ( !ppsTarget || !piTargetCount ) {
        return XWORK_ERROR_INVALID_ARGUMENT;
    }
    *ppsTarget = NULL;
    *piTargetCount = 0u;
    if ( !psSource || iSourceCount == 0u ) {
        return XWORK_OK;
    }

    psTarget = (char **)calloc(iSourceCount, sizeof(*psTarget));
    if ( !psTarget ) {
        return XWORK_ERROR_NO_MEMORY;
    }
    for ( i = 0u; i < iSourceCount; ++i ) {
        if ( psSource[i] && psSource[i][0] ) {
            psTarget[i] = xwork__dup_cstr(psSource[i]);
            if ( !psTarget[i] ) {
                xwork__free_str_array(&psTarget, &iSourceCount);
                return XWORK_ERROR_NO_MEMORY;
            }
        }
    }
    *ppsTarget = psTarget;
    *piTargetCount = iSourceCount;
    return XWORK_OK;
}

static void xwork__local_host_free_editor_buffers(xwork_local_host *pHost)
{
    xwork__local_editor_buffer *pBuffer;

    if ( !pHost ) {
        return;
    }

    pBuffer = (xwork__local_editor_buffer *)pHost->pEditorBuffers;
    while ( pBuffer ) {
        xwork__local_editor_buffer *pNext = pBuffer->pNext;

        free(pBuffer->sBufferId);
        free(pBuffer->sPath);
        free(pBuffer->sResolvedPath);
        free(pBuffer->sText);
        free(pBuffer);
        pBuffer = pNext;
    }
    pHost->pEditorBuffers = NULL;
}

static xwork__local_editor_buffer *xwork__local_host_find_editor_buffer(
    xwork_local_host *pHost,
    const char *sBufferId,
    const char *sResolvedPath
)
{
    xwork__local_editor_buffer *pBuffer;

    if ( !pHost ) {
        return NULL;
    }

    for ( pBuffer = (xwork__local_editor_buffer *)pHost->pEditorBuffers;
          pBuffer;
          pBuffer = pBuffer->pNext ) {
        if ( sBufferId &&
             sBufferId[0] &&
             pBuffer->sBufferId &&
             strcmp(pBuffer->sBufferId, sBufferId) == 0 ) {
            return pBuffer;
        }
        if ( sResolvedPath &&
             sResolvedPath[0] &&
             pBuffer->sResolvedPath &&
             strcmp(pBuffer->sResolvedPath, sResolvedPath) == 0 ) {
            return pBuffer;
        }
    }
    return NULL;
}

static bool xwork__local_host_path_prefix_matches(
    const xwork_local_host *pHost,
    const char *sNormalizedPath,
    const char *sPrefix
)
{
    char *sResolvedPrefix = NULL;
    char *sNormalizedPrefix = NULL;
    bool bMatches = false;

    if ( !sNormalizedPath || !sPrefix || !sPrefix[0] ) {
        return false;
    }

    sResolvedPrefix = xwork__local_host_resolve_path(pHost, sPrefix);
    if ( sResolvedPrefix ) {
        sNormalizedPrefix = xwork__local_host_normalize_path(sResolvedPrefix);
    }
    if ( sNormalizedPrefix ) {
        bMatches = xwork__local_host_path_has_prefix(
            sNormalizedPath,
            sNormalizedPrefix
        );
    }
    free(sResolvedPrefix);
    free(sNormalizedPrefix);
    return bMatches;
}

static xwork_status xwork__local_host_check_filesystem_path(
    const xwork_local_host *pHost,
    const char *sResolvedPath,
    const char **psFailureMessage
)
{
    char *sNormalizedPath = NULL;
    char *sNormalizedRootPath = NULL;
    bool bAllowed;
    size_t i;

    if ( psFailureMessage ) {
        *psFailureMessage = NULL;
    }
    if ( !pHost || !sResolvedPath || !sResolvedPath[0] ) {
        return XWORK_ERROR_INVALID_ARGUMENT;
    }

    sNormalizedPath = xwork__local_host_normalize_path(sResolvedPath);
    if ( !sNormalizedPath ) {
        return XWORK_ERROR_NO_MEMORY;
    }

    if ( pHost->bEnforceFilesystemRoot &&
         pHost->sDefaultWorkingDirectory &&
         pHost->sDefaultWorkingDirectory[0] ) {
        sNormalizedRootPath =
            xwork__local_host_normalize_path(pHost->sDefaultWorkingDirectory);
        if ( !sNormalizedRootPath ) {
            free(sNormalizedPath);
            return XWORK_ERROR_NO_MEMORY;
        }

        if ( strcmp(sNormalizedRootPath, ".") == 0 ) {
            if ( xwork__local_host_path_is_normalized_absolute(sNormalizedPath) ||
                 xwork__local_host_path_starts_with_parent(sNormalizedPath) ) {
                if ( psFailureMessage ) {
                    *psFailureMessage = "path is outside filesystem root";
                }
                free(sNormalizedRootPath);
                free(sNormalizedPath);
                return XWORK_ERROR_INVALID_STATE;
            }
        } else if ( !xwork__local_host_path_has_prefix(
                        sNormalizedPath,
                        sNormalizedRootPath
                    ) ) {
            if ( psFailureMessage ) {
                *psFailureMessage = "path is outside filesystem root";
            }
            free(sNormalizedRootPath);
            free(sNormalizedPath);
            return XWORK_ERROR_INVALID_STATE;
        }
    }

    bAllowed = pHost->iFilesystemAllowPathPrefixCount == 0u;
    for ( i = 0u; !bAllowed && i < pHost->iFilesystemAllowPathPrefixCount; ++i ) {
        bAllowed = xwork__local_host_path_prefix_matches(
            pHost,
            sNormalizedPath,
            pHost->psFilesystemAllowPathPrefixes[i]
        );
    }
    if ( !bAllowed ) {
        if ( psFailureMessage ) {
            *psFailureMessage = "path is not in allowed filesystem prefixes";
        }
        free(sNormalizedRootPath);
        free(sNormalizedPath);
        return XWORK_ERROR_INVALID_STATE;
    }

    for ( i = 0u; i < pHost->iFilesystemDenyPathPrefixCount; ++i ) {
        if ( xwork__local_host_path_prefix_matches(
                 pHost,
                 sNormalizedPath,
                 pHost->psFilesystemDenyPathPrefixes[i]
             ) ) {
            if ( psFailureMessage ) {
                *psFailureMessage = "path is denied by filesystem prefixes";
            }
            free(sNormalizedRootPath);
            free(sNormalizedPath);
            return XWORK_ERROR_INVALID_STATE;
        }
    }

    free(sNormalizedRootPath);
    free(sNormalizedPath);
    return XWORK_OK;
}

static bool xwork__local_host_ascii_contains_ci(
    const char *sText,
    const char *sPattern
)
{
    size_t i;
    size_t j;
    size_t iTextLength;
    size_t iPatternLength;

    if ( !sText || !sPattern || !sPattern[0] ) {
        return false;
    }

    iTextLength = strlen(sText);
    iPatternLength = strlen(sPattern);
    if ( iPatternLength > iTextLength ) {
        return false;
    }

    for ( i = 0u; i <= iTextLength - iPatternLength; ++i ) {
        bool bMatches = true;

        for ( j = 0u; j < iPatternLength; ++j ) {
            if ( tolower((unsigned char)sText[i + j]) !=
                 tolower((unsigned char)sPattern[j]) ) {
                bMatches = false;
                break;
            }
        }
        if ( bMatches ) {
            return true;
        }
    }
    return false;
}

static bool xwork__local_host_command_has_any_pattern(
    const char *sCommand,
    char **psPatterns,
    size_t iPatternCount
)
{
    size_t i;

    if ( !sCommand || !psPatterns || iPatternCount == 0u ) {
        return false;
    }

    for ( i = 0u; i < iPatternCount; ++i ) {
        if ( psPatterns[i] &&
             psPatterns[i][0] &&
             xwork__local_host_ascii_contains_ci(sCommand, psPatterns[i]) ) {
            return true;
        }
    }
    return false;
}

static bool xwork__local_host_command_looks_destructive(const char *sCommand)
{
    static const char *const psDestructivePatterns[] = {
        "rm -",
        "rm /",
        "del ",
        "erase ",
        "rd ",
        "rmdir ",
        "remove-item",
        "git clean",
        "git reset --hard",
        "git checkout --"
    };
    size_t i;

    if ( !sCommand || !sCommand[0] ) {
        return false;
    }
    for ( i = 0u; i < sizeof(psDestructivePatterns) / sizeof(psDestructivePatterns[0]); ++i ) {
        if ( xwork__local_host_ascii_contains_ci(sCommand, psDestructivePatterns[i]) ) {
            return true;
        }
    }
    return false;
}

static xwork_status xwork__local_host_check_command_policy(
    const xwork_local_host *pHost,
    const char *sCommand,
    const char **psFailureKind,
    const char **psFailureMessage
)
{
    bool bAllowed;

    if ( psFailureKind ) {
        *psFailureKind = NULL;
    }
    if ( psFailureMessage ) {
        *psFailureMessage = NULL;
    }
    if ( !pHost || !sCommand || !sCommand[0] ) {
        return XWORK_ERROR_INVALID_ARGUMENT;
    }

    bAllowed = pHost->iCommandAllowPatternCount == 0u ||
        xwork__local_host_command_has_any_pattern(
            sCommand,
            pHost->psCommandAllowPatterns,
            pHost->iCommandAllowPatternCount
        );
    if ( !bAllowed ) {
        if ( psFailureKind ) {
            *psFailureKind = "command_denied";
        }
        if ( psFailureMessage ) {
            *psFailureMessage = "command is not in allowed command patterns";
        }
        return XWORK_ERROR_INVALID_STATE;
    }

    if ( xwork__local_host_command_has_any_pattern(
             sCommand,
             pHost->psCommandDenyPatterns,
             pHost->iCommandDenyPatternCount
         ) ) {
        if ( psFailureKind ) {
            *psFailureKind = "command_denied";
        }
        if ( psFailureMessage ) {
            *psFailureMessage = "command matches denied command patterns";
        }
        return XWORK_ERROR_INVALID_STATE;
    }

    if ( pHost->bDenyDestructiveCommands &&
         xwork__local_host_command_looks_destructive(sCommand) ) {
        if ( psFailureKind ) {
            *psFailureKind = "destructive_command";
        }
        if ( psFailureMessage ) {
            *psFailureMessage = "command is classified as destructive";
        }
        return XWORK_ERROR_INVALID_STATE;
    }

    return XWORK_OK;
}

static xwork_status xwork__local_host_json_escape(
    const char *sText,
    char **ppsEscaped
)
{
    char *sEscaped;
    size_t iLength = 0u;
    size_t i;

    if ( !ppsEscaped ) {
        return XWORK_ERROR_INVALID_ARGUMENT;
    }

    *ppsEscaped = NULL;
    if ( !sText ) {
        sText = "";
    }

    for ( i = 0u; sText[i] != '\0'; ++i ) {
        unsigned char c = (unsigned char)sText[i];

        switch ( c ) {
            case '"':
            case '\\':
            case '\b':
            case '\f':
            case '\n':
            case '\r':
            case '\t':
                iLength += 2u;
                break;
            default:
                if ( c < 0x20u ) {
                    iLength += 6u;
                } else {
                    ++iLength;
                }
                break;
        }
    }

    sEscaped = (char *)malloc(iLength + 1u);
    if ( !sEscaped ) {
        return XWORK_ERROR_NO_MEMORY;
    }

    iLength = 0u;
    for ( i = 0u; sText[i] != '\0'; ++i ) {
        unsigned char c = (unsigned char)sText[i];

        switch ( c ) {
            case '"':
                sEscaped[iLength++] = '\\';
                sEscaped[iLength++] = '"';
                break;
            case '\\':
                sEscaped[iLength++] = '\\';
                sEscaped[iLength++] = '\\';
                break;
            case '\b':
                sEscaped[iLength++] = '\\';
                sEscaped[iLength++] = 'b';
                break;
            case '\f':
                sEscaped[iLength++] = '\\';
                sEscaped[iLength++] = 'f';
                break;
            case '\n':
                sEscaped[iLength++] = '\\';
                sEscaped[iLength++] = 'n';
                break;
            case '\r':
                sEscaped[iLength++] = '\\';
                sEscaped[iLength++] = 'r';
                break;
            case '\t':
                sEscaped[iLength++] = '\\';
                sEscaped[iLength++] = 't';
                break;
            default:
                if ( c < 0x20u ) {
                    (void)snprintf(
                        &sEscaped[iLength],
                        7u,
                        "\\u%04x",
                        (unsigned)c
                    );
                    iLength += 6u;
                } else {
                    sEscaped[iLength++] = (char)c;
                }
                break;
        }
    }

    sEscaped[iLength] = '\0';
    *ppsEscaped = sEscaped;
    return XWORK_OK;
}

static char *xwork__local_host_dup_prefix(const char *sText, size_t iLength);

static xwork_status xwork__local_host_string_append(
    char **ppsText,
    size_t *piLength,
    const char *sChunk
)
{
    char *sNextText;
    size_t iChunkLength;

    if ( !ppsText || !piLength || !sChunk ) {
        return XWORK_ERROR_INVALID_ARGUMENT;
    }

    iChunkLength = strlen(sChunk);
    sNextText = (char *)realloc(*ppsText, *piLength + iChunkLength + 1u);
    if ( !sNextText ) {
        return XWORK_ERROR_NO_MEMORY;
    }

    *ppsText = sNextText;
    memcpy(*ppsText + *piLength, sChunk, iChunkLength + 1u);
    *piLength += iChunkLength;
    return XWORK_OK;
}

static xwork_status xwork__local_host_append_patch_lines(
    char **ppsText,
    size_t *piLength,
    char cPrefix,
    const char *sText
)
{
    const char *sCursor;
    xwork_status iStatus;

    if ( !ppsText || !piLength ) {
        return XWORK_ERROR_INVALID_ARGUMENT;
    }
    if ( !sText ) {
        sText = "";
    }

    sCursor = sText;
    while ( *sCursor ) {
        const char *sLineEnd = strchr(sCursor, '\n');
        char sPrefix[2];
        char *sLine = NULL;
        size_t iLineLength;

        sPrefix[0] = cPrefix;
        sPrefix[1] = '\0';
        iStatus = xwork__local_host_string_append(ppsText, piLength, sPrefix);
        if ( iStatus != XWORK_OK ) {
            return iStatus;
        }

        iLineLength = sLineEnd ? (size_t)(sLineEnd - sCursor) + 1u : strlen(sCursor);
        sLine = xwork__local_host_dup_prefix(sCursor, iLineLength);
        if ( !sLine ) {
            return XWORK_ERROR_NO_MEMORY;
        }
        iStatus = xwork__local_host_string_append(ppsText, piLength, sLine);
        free(sLine);
        if ( iStatus != XWORK_OK ) {
            return iStatus;
        }
        if ( !sLineEnd ) {
            iStatus = xwork__local_host_string_append(ppsText, piLength, "\n");
            if ( iStatus != XWORK_OK ) {
                return iStatus;
            }
            break;
        }
        sCursor = sLineEnd + 1;
    }

    if ( !sText[0] ) {
        char sEmptyLine[3];

        sEmptyLine[0] = cPrefix;
        sEmptyLine[1] = '\n';
        sEmptyLine[2] = '\0';
        return xwork__local_host_string_append(ppsText, piLength, sEmptyLine);
    }
    return XWORK_OK;
}

static xwork_status xwork__local_host_build_replacement_patch_text(
    const char *sPath,
    const char *sOldText,
    const char *sNewText,
    char **ppsPatchText
)
{
    char *sPatchText = NULL;
    size_t iPatchTextLength = 0u;
    xwork_status iStatus;

    if ( !ppsPatchText ) {
        return XWORK_ERROR_INVALID_ARGUMENT;
    }
    *ppsPatchText = NULL;

    iStatus = xwork__local_host_string_append(&sPatchText, &iPatchTextLength, "--- a/");
    if ( iStatus != XWORK_OK ) goto cleanup;
    iStatus = xwork__local_host_string_append(&sPatchText, &iPatchTextLength, sPath ? sPath : "");
    if ( iStatus != XWORK_OK ) goto cleanup;
    iStatus = xwork__local_host_string_append(&sPatchText, &iPatchTextLength, "\n+++ b/");
    if ( iStatus != XWORK_OK ) goto cleanup;
    iStatus = xwork__local_host_string_append(&sPatchText, &iPatchTextLength, sPath ? sPath : "");
    if ( iStatus != XWORK_OK ) goto cleanup;
    iStatus = xwork__local_host_string_append(&sPatchText, &iPatchTextLength, "\n@@\n");
    if ( iStatus != XWORK_OK ) goto cleanup;
    iStatus = xwork__local_host_append_patch_lines(&sPatchText, &iPatchTextLength, '-', sOldText);
    if ( iStatus != XWORK_OK ) goto cleanup;
    iStatus = xwork__local_host_append_patch_lines(&sPatchText, &iPatchTextLength, '+', sNewText);
    if ( iStatus != XWORK_OK ) goto cleanup;

    *ppsPatchText = sPatchText;
    return XWORK_OK;

cleanup:
    free(sPatchText);
    return iStatus;
}

static xwork_status xwork__local_host_parse_request_json(
    const char *sRequestJson,
    xvalue *ptRequest
)
{
    xvalue tRequest;

    if ( !ptRequest ) {
        return XWORK_ERROR_INVALID_ARGUMENT;
    }

    *ptRequest = NULL;
    if ( !sRequestJson || !sRequestJson[0] ) {
        return XWORK_OK;
    }

    tRequest = xrtParseJSON((str)sRequestJson, strlen(sRequestJson));
    if ( !tRequest || xvoType(tRequest) != XVO_DT_TABLE ) {
        if ( tRequest ) {
            xvoUnref(tRequest);
        }
        return XWORK_ERROR_INVALID_ARGUMENT;
    }

    *ptRequest = tRequest;
    return XWORK_OK;
}

static const char *xwork__local_host_request_get_text(
    xvalue tRequest,
    const char *sKey
)
{
    xvalue tValue;

    if ( !tRequest || !sKey ) {
        return NULL;
    }

    tValue = xvoTableGetValue(tRequest, sKey, (uint32)strlen(sKey));
    if ( !tValue || xvoType(tValue) == XVO_DT_NULL ) {
        return NULL;
    }
    if ( xvoType(tValue) != XVO_DT_TEXT ) {
        return NULL;
    }
    return (const char *)xvoGetText(tValue);
}

static xwork_status xwork__local_host_request_get_list(
    xvalue tRequest,
    const char *sKey,
    xvalue *ptValue
)
{
    xvalue tValue;

    if ( !ptValue ) {
        return XWORK_ERROR_INVALID_ARGUMENT;
    }

    *ptValue = NULL;
    if ( !tRequest || !sKey ) {
        return XWORK_OK;
    }

    tValue = xvoTableGetValue(tRequest, sKey, (uint32)strlen(sKey));
    if ( !tValue || xvoType(tValue) == XVO_DT_NULL ) {
        return XWORK_OK;
    }
    if ( xvoType(tValue) != XVO_DT_LIST && xvoType(tValue) != XVO_DT_ARRAY ) {
        return XWORK_ERROR_INVALID_ARGUMENT;
    }

    *ptValue = tValue;
    return XWORK_OK;
}

static bool xwork__local_host_request_get_size(
    xvalue tRequest,
    const char *sKey,
    size_t *piValue
)
{
    xvalue tValue;
    int64 iValue;

    if ( !piValue ) {
        return false;
    }

    *piValue = 0u;
    if ( !tRequest || !sKey ) {
        return false;
    }

    tValue = xvoTableGetValue(tRequest, sKey, (uint32)strlen(sKey));
    if ( !tValue || xvoType(tValue) == XVO_DT_NULL ) {
        return false;
    }
    if ( xvoType(tValue) != XVO_DT_INT ) {
        return false;
    }

    iValue = xvoGetInt(tValue);
    if ( iValue <= 0 ) {
        return false;
    }

    *piValue = (size_t)iValue;
    return true;
}

static xwork_status xwork__local_host_request_get_positive_size_strict(
    xvalue tRequest,
    const char *sKey,
    bool *pbHasValue,
    size_t *piValue
)
{
    xvalue tValue;
    int64 iValue;

    if ( !pbHasValue || !piValue ) {
        return XWORK_ERROR_INVALID_ARGUMENT;
    }

    *pbHasValue = false;
    *piValue = 0u;
    if ( !tRequest || !sKey ) {
        return XWORK_OK;
    }

    tValue = xvoTableGetValue(tRequest, sKey, (uint32)strlen(sKey));
    if ( !tValue || xvoType(tValue) == XVO_DT_NULL ) {
        return XWORK_OK;
    }
    if ( xvoType(tValue) != XVO_DT_INT ) {
        return XWORK_ERROR_INVALID_ARGUMENT;
    }

    iValue = xvoGetInt(tValue);
    if ( iValue <= 0 ) {
        return XWORK_ERROR_INVALID_ARGUMENT;
    }

    *pbHasValue = true;
    *piValue = (size_t)iValue;
    return XWORK_OK;
}

static xwork_status xwork__local_host_request_get_bool(
    xvalue tRequest,
    const char *sKey,
    bool *pbHasValue,
    bool *pbValue
)
{
    xvalue tValue;

    if ( !pbHasValue || !pbValue ) {
        return XWORK_ERROR_INVALID_ARGUMENT;
    }

    *pbHasValue = false;
    if ( !tRequest || !sKey ) {
        return XWORK_OK;
    }

    tValue = xvoTableGetValue(tRequest, sKey, (uint32)strlen(sKey));
    if ( !tValue || xvoType(tValue) == XVO_DT_NULL ) {
        return XWORK_OK;
    }
    if ( xvoType(tValue) != XVO_DT_BOOL ) {
        return XWORK_ERROR_INVALID_ARGUMENT;
    }

    *pbHasValue = true;
    *pbValue = xvoGetBool(tValue);
    return XWORK_OK;
}

static size_t xwork__local_host_effective_limit(size_t iConfigured, size_t iRequested)
{
    size_t iLimit;

    iLimit = iConfigured;
    if ( iLimit == 0u ) {
        iLimit = iRequested;
    } else if ( iRequested > 0u && iRequested < iLimit ) {
        iLimit = iRequested;
    }
    if ( iLimit == 0u ) {
        iLimit = XWORK__LOCAL_HOST_DEFAULT_PROCESS_OUTPUT_BYTES;
    }
    return iLimit;
}

static xwork_status xwork__local_host_read_text_file(
    const char *sPath,
    size_t iOffsetBytes,
    size_t iMaxBytes,
    char **ppsText,
    size_t *piFileSizeBytes,
    size_t *piBytesRead,
    bool *pbTruncated,
    bool *pbEof
)
{
    FILE *pFile = NULL;
    char *sText = NULL;
    long long iFileSizeLong;
    size_t iFileSize;
    size_t iReadableBytes;
    size_t iReadBytes;

    if ( !sPath || !sPath[0] || !ppsText || !piFileSizeBytes || !piBytesRead ||
         !pbTruncated || !pbEof ) {
        return XWORK_ERROR_INVALID_ARGUMENT;
    }

    *ppsText = NULL;
    *piFileSizeBytes = 0u;
    *piBytesRead = 0u;
    *pbTruncated = false;
    *pbEof = false;

    pFile = fopen(sPath, "rb");
    if ( !pFile ) {
        return (errno == ENOENT) ? XWORK_ERROR_NOT_FOUND : XWORK_ERROR_EXTERNAL_FAILURE;
    }

    if ( fseek(pFile, 0, SEEK_END) != 0 ) {
        fclose(pFile);
        return XWORK_ERROR_EXTERNAL_FAILURE;
    }
    iFileSizeLong = ftell(pFile);
    if ( iFileSizeLong < 0 ) {
        fclose(pFile);
        return XWORK_ERROR_EXTERNAL_FAILURE;
    }
    iFileSize = (size_t)iFileSizeLong;
    if ( iOffsetBytes > iFileSize ) {
        iOffsetBytes = iFileSize;
    }
    if ( fseek(pFile, (long)iOffsetBytes, SEEK_SET) != 0 ) {
        fclose(pFile);
        return XWORK_ERROR_EXTERNAL_FAILURE;
    }

    iReadableBytes = iFileSize - iOffsetBytes;
    iReadBytes = iReadableBytes;
    if ( iReadBytes > iMaxBytes ) {
        iReadBytes = iMaxBytes;
        *pbTruncated = true;
    }

    sText = (char *)malloc(iReadBytes + 1u);
    if ( !sText ) {
        fclose(pFile);
        return XWORK_ERROR_NO_MEMORY;
    }

    if ( iReadBytes > 0u && fread(sText, 1u, iReadBytes, pFile) != iReadBytes ) {
        fclose(pFile);
        free(sText);
        return XWORK_ERROR_EXTERNAL_FAILURE;
    }

    sText[iReadBytes] = '\0';
    fclose(pFile);
    *ppsText = sText;
    *piFileSizeBytes = iFileSize;
    *piBytesRead = iReadBytes;
    *pbEof = ((iOffsetBytes + iReadBytes) >= iFileSize);
    return XWORK_OK;
}

static xwork_status xwork__local_host_write_text_file(
    const char *sPath,
    const char *sText,
    bool bAppend,
    size_t *piBytesWritten
);

static bool xwork__local_host_text_file_exists(const char *sPath)
{
    FILE *pFile;

    if ( !sPath || !sPath[0] ) {
        return false;
    }

    pFile = fopen(sPath, "rb");
    if ( !pFile ) {
        return false;
    }

    (void)fclose(pFile);
    return true;
}

static bool xwork__local_host_directory_exists(const char *sPath)
{
#ifdef _WIN32
    struct _stat tStat;

    if ( !sPath || !sPath[0] ) {
        return false;
    }
    if ( _stat(sPath, &tStat) != 0 ) {
        return false;
    }
    return (tStat.st_mode & _S_IFDIR) != 0;
#else
    struct stat tStat;

    if ( !sPath || !sPath[0] ) {
        return false;
    }
    if ( stat(sPath, &tStat) != 0 ) {
        return false;
    }
    return S_ISDIR(tStat.st_mode);
#endif
}

static const char *xwork__local_host_path_leaf(const char *sPath)
{
    const char *sSlash;
    const char *sBackslash;

    if ( !sPath || !sPath[0] ) {
        return "";
    }

    sSlash = strrchr(sPath, '/');
    sBackslash = strrchr(sPath, '\\');
    if ( sBackslash && (!sSlash || sBackslash > sSlash) ) {
        sSlash = sBackslash;
    }
    return (sSlash && sSlash[1]) ? (sSlash + 1) : sPath;
}

static bool xwork__local_host_path_is_hidden(const char *sPath)
{
    const char *sLeaf = xwork__local_host_path_leaf(sPath);

    return sLeaf && sLeaf[0] == '.' && sLeaf[1] != '\0';
}

static const char *xwork__local_host_path_relative(
    const char *sRootPath,
    const char *sPath
)
{
    size_t iRootLength;
    const char *sRelative;

    if ( !sRootPath || !sRootPath[0] || !sPath || !sPath[0] ) {
        return sPath ? sPath : "";
    }

    iRootLength = strlen(sRootPath);
    if ( strncmp(sRootPath, sPath, iRootLength) != 0 ) {
        return sPath;
    }

    sRelative = sPath + iRootLength;
    if ( sRelative[0] == '/' || sRelative[0] == '\\' ) {
        ++sRelative;
    }
    return sRelative;
}

static xwork_status xwork__local_host_stat_path(
    const char *sPath,
    const char **psType,
    size_t *piSizeBytes,
    long long *piMtimeUnix
)
{
#ifdef _WIN32
    struct _stat tStat;
#else
    struct stat tStat;
#endif

    if ( !sPath || !sPath[0] || !psType || !piSizeBytes || !piMtimeUnix ) {
        return XWORK_ERROR_INVALID_ARGUMENT;
    }

#ifdef _WIN32
    if ( _stat(sPath, &tStat) != 0 ) {
#else
    if ( stat(sPath, &tStat) != 0 ) {
#endif
        return errno == ENOENT ? XWORK_ERROR_NOT_FOUND : XWORK_ERROR_EXTERNAL_FAILURE;
    }

#ifdef _WIN32
    if ( (tStat.st_mode & _S_IFDIR) != 0 ) {
        *psType = "directory";
    } else if ( (tStat.st_mode & _S_IFREG) != 0 ) {
        *psType = "file";
    } else {
        *psType = "other";
    }
#else
    if ( S_ISDIR(tStat.st_mode) ) {
        *psType = "directory";
    } else if ( S_ISREG(tStat.st_mode) ) {
        *psType = "file";
    } else {
        *psType = "other";
    }
#endif
    *piSizeBytes = (tStat.st_size > 0) ? (size_t)tStat.st_size : 0u;
    *piMtimeUnix = (long long)tStat.st_mtime;
    return XWORK_OK;
}

static bool xwork__local_host_glob_match(const char *sPattern, const char *sText)
{
    char cPattern;
    char cText;

    if ( !sPattern || !sText ) {
        return false;
    }

    while ( *sPattern ) {
        cPattern = *sPattern;
        if ( cPattern == '*' ) {
            while ( sPattern[1] == '*' ) {
                ++sPattern;
            }
            if ( sPattern[1] == '\0' ) {
                return true;
            }
            ++sPattern;
            while ( *sText ) {
                if ( xwork__local_host_glob_match(sPattern, sText) ) {
                    return true;
                }
                ++sText;
            }
            return xwork__local_host_glob_match(sPattern, sText);
        }
        if ( cPattern == '?' ) {
            if ( !*sText ) {
                return false;
            }
            ++sPattern;
            ++sText;
            continue;
        }

        cText = *sText;
#ifdef _WIN32
        cPattern = (char)tolower((unsigned char)cPattern);
        cText = (char)tolower((unsigned char)cText);
#endif
        if ( cPattern != cText ) {
            return false;
        }
        ++sPattern;
        ++sText;
    }
    return *sText == '\0';
}

typedef struct {
    const char *sRootPath;
    const char *sPattern;
    bool bUsePattern;
    bool bIncludeHidden;
    size_t iLimit;
    size_t iTotalCount;
    size_t iItemCount;
    bool bHasMore;
    char *sEntriesJson;
    size_t iEntriesJsonLength;
    xwork_status iStatus;
} xwork__local_host_scan_context;

static xwork_status xwork__local_host_append_scan_entry(
    xwork__local_host_scan_context *pContext,
    const char *sPath,
    const char *sType
)
{
    const char *sRelativePath;
    const char *sName;
    const char *sEntryType;
    char *sEscapedPath = NULL;
    char *sEscapedResolvedPath = NULL;
    char *sEscapedName = NULL;
    char *sEscapedType = NULL;
    char *sEntryJson = NULL;
    size_t iSizeBytes = 0u;
    long long iMtimeUnix = 0;
    xwork_status iStatus;

    if ( !pContext || !sPath ) {
        return XWORK_ERROR_INVALID_ARGUMENT;
    }

    sRelativePath = xwork__local_host_path_relative(pContext->sRootPath, sPath);
    sName = xwork__local_host_path_leaf(sPath);
    if ( !pContext->bIncludeHidden && xwork__local_host_path_is_hidden(sName) ) {
        return XWORK_OK;
    }
    if ( pContext->bUsePattern &&
         !xwork__local_host_glob_match(pContext->sPattern, sRelativePath) &&
         !xwork__local_host_glob_match(pContext->sPattern, sName) ) {
        return XWORK_OK;
    }

    ++pContext->iTotalCount;
    if ( pContext->iLimit > 0u && pContext->iItemCount >= pContext->iLimit ) {
        pContext->bHasMore = true;
        return XWORK_OK;
    }

    sEntryType = sType;
    iStatus = xwork__local_host_stat_path(
        sPath,
        &sEntryType,
        &iSizeBytes,
        &iMtimeUnix
    );
    if ( iStatus != XWORK_OK ) {
        return iStatus;
    }

    iStatus = xwork__local_host_json_escape(sRelativePath, &sEscapedPath);
    if ( iStatus != XWORK_OK ) goto cleanup;
    iStatus = xwork__local_host_json_escape(sPath, &sEscapedResolvedPath);
    if ( iStatus != XWORK_OK ) goto cleanup;
    iStatus = xwork__local_host_json_escape(sName, &sEscapedName);
    if ( iStatus != XWORK_OK ) goto cleanup;
    iStatus = xwork__local_host_json_escape(sEntryType, &sEscapedType);
    if ( iStatus != XWORK_OK ) goto cleanup;

    sEntryJson = xwork__dup_printf(
        "%s{\"path\":\"%s\",\"resolved_path\":\"%s\",\"name\":\"%s\","
        "\"type\":\"%s\",\"size_bytes\":%llu,\"mtime_unix\":%lld}",
        pContext->iItemCount > 0u ? "," : "",
        sEscapedPath,
        sEscapedResolvedPath,
        sEscapedName,
        sEscapedType,
        (unsigned long long)iSizeBytes,
        iMtimeUnix
    );
    if ( !sEntryJson ) {
        iStatus = XWORK_ERROR_NO_MEMORY;
        goto cleanup;
    }

    iStatus = xwork__local_host_string_append(
        &pContext->sEntriesJson,
        &pContext->iEntriesJsonLength,
        sEntryJson
    );
    if ( iStatus == XWORK_OK ) {
        ++pContext->iItemCount;
    }

cleanup:
    free(sEscapedPath);
    free(sEscapedResolvedPath);
    free(sEscapedName);
    free(sEscapedType);
    free(sEntryJson);
    return iStatus;
}

typedef struct {
    char *sText;
    size_t iLength;
    size_t iCapacity;
    size_t iLimit;
    bool bTruncated;
    xwork_status iStatus;
} xwork__local_host_process_capture;

typedef struct {
    xwork__local_host_process_capture *pStdoutCapture;
    xwork__local_host_process_capture *pStderrCapture;
} xwork__local_host_process_capture_set;

typedef struct xwork__local_host_terminal_session {
    char *sSessionId;
    char *sSessionName;
    char *sCommand;
    char *sResolvedCwd;
    xprocess *pProcess;
    size_t iSessionIndex;
    size_t iTerminalCols;
    size_t iTerminalRows;
    bool bStdinClosed;
    struct xwork__local_host_terminal_session *pNext;
} xwork__local_host_terminal_session;

static int xwork__local_host_process_stop_best_effort(
    xprocess *pProcess,
    int iInitialStopReason
);

static void xwork__local_host_process_capture_init(
    xwork__local_host_process_capture *pCapture,
    size_t iLimit
)
{
    if ( !pCapture ) {
        return;
    }

    memset(pCapture, 0, sizeof(*pCapture));
    pCapture->iLimit = iLimit;
    pCapture->iStatus = XWORK_OK;
}

static void xwork__local_host_process_capture_reset(
    xwork__local_host_process_capture *pCapture
)
{
    if ( !pCapture ) {
        return;
    }

    free(pCapture->sText);
    memset(pCapture, 0, sizeof(*pCapture));
}

static xwork_status xwork__local_host_process_capture_append(
    xwork__local_host_process_capture *pCapture,
    const void *pData,
    size_t iSize
)
{
    size_t iWritable;

    if ( !pCapture || (iSize > 0u && !pData) ) {
        return XWORK_ERROR_INVALID_ARGUMENT;
    }
    if ( pCapture->iStatus != XWORK_OK || iSize == 0u ) {
        return pCapture->iStatus;
    }

    if ( pCapture->iLength >= pCapture->iLimit ) {
        pCapture->bTruncated = true;
        return XWORK_OK;
    }

    iWritable = iSize;
    if ( pCapture->iLength + iWritable > pCapture->iLimit ) {
        iWritable = pCapture->iLimit - pCapture->iLength;
        pCapture->bTruncated = true;
    }

    if ( iWritable > 0u ) {
        if ( pCapture->iLength + iWritable + 1u > pCapture->iCapacity ) {
            char *sNextText;
            size_t iNextCapacity = pCapture->iLength + iWritable + 1u;

            sNextText = (char *)realloc(pCapture->sText, iNextCapacity);
            if ( !sNextText ) {
                pCapture->iStatus = XWORK_ERROR_NO_MEMORY;
                return pCapture->iStatus;
            }
            pCapture->sText = sNextText;
            pCapture->iCapacity = iNextCapacity;
        }

        memcpy(
            pCapture->sText + pCapture->iLength,
            pData,
            iWritable
        );
        pCapture->iLength += iWritable;
        pCapture->sText[pCapture->iLength] = '\0';
    }

    if ( iWritable < iSize ) {
        pCapture->bTruncated = true;
    }

    return XWORK_OK;
}

static char *xwork__local_host_process_capture_take_text(
    xwork__local_host_process_capture *pCapture
)
{
    char *sText;

    if ( !pCapture ) {
        return NULL;
    }
    if ( !pCapture->sText ) {
        return xwork__dup_cstr("");
    }

    sText = pCapture->sText;
    pCapture->sText = NULL;
    pCapture->iLength = 0u;
    pCapture->iCapacity = 0u;
    return sText;
}

static char *xwork__local_host_copy_xrt_text(const void *pData, size_t iSize)
{
    char *sText;

    sText = (char *)malloc(iSize + 1u);
    if ( !sText ) {
        return NULL;
    }
    if ( pData && iSize > 0u ) {
        memcpy(sText, pData, iSize);
    }
    sText[iSize] = '\0';
    return sText;
}

static xwork__local_host_terminal_session *xwork__local_host_find_terminal_session(
    xwork_local_host *pHost,
    const char *sSessionId
)
{
    xwork__local_host_terminal_session *pSession;

    if ( !pHost || !sSessionId || !sSessionId[0] ) {
        return NULL;
    }

    for ( pSession = (xwork__local_host_terminal_session *)pHost->pTerminalSessions;
          pSession;
          pSession = pSession->pNext ) {
        if ( pSession->sSessionId &&
             strcmp(pSession->sSessionId, sSessionId) == 0 ) {
            return pSession;
        }
    }
    return NULL;
}

static size_t xwork__local_host_terminal_session_count(const xwork_local_host *pHost)
{
    const xwork__local_host_terminal_session *pSession;
    size_t iCount = 0u;

    if ( !pHost ) {
        return 0u;
    }

    for ( pSession = (const xwork__local_host_terminal_session *)pHost->pTerminalSessions;
          pSession;
          pSession = pSession->pNext ) {
        ++iCount;
    }
    return iCount;
}

static int xwork__local_host_compare_terminal_session_index(
    const void *pLeft,
    const void *pRight
)
{
    const xwork__local_host_terminal_session *const *ppLeft =
        (const xwork__local_host_terminal_session *const *)pLeft;
    const xwork__local_host_terminal_session *const *ppRight =
        (const xwork__local_host_terminal_session *const *)pRight;
    const xwork__local_host_terminal_session *pSessionLeft =
        ppLeft ? *ppLeft : NULL;
    const xwork__local_host_terminal_session *pSessionRight =
        ppRight ? *ppRight : NULL;

    if ( pSessionLeft == pSessionRight ) {
        return 0;
    }
    if ( !pSessionLeft ) {
        return -1;
    }
    if ( !pSessionRight ) {
        return 1;
    }
    if ( pSessionLeft->iSessionIndex < pSessionRight->iSessionIndex ) {
        return -1;
    }
    if ( pSessionLeft->iSessionIndex > pSessionRight->iSessionIndex ) {
        return 1;
    }
    return 0;
}

static void xwork__local_host_free_terminal_session(
    xwork__local_host_terminal_session *pSession
)
{
    if ( !pSession ) {
        return;
    }

    if ( pSession->pProcess ) {
        if ( xrtProcessIsRunning(pSession->pProcess) ) {
            (void)xwork__local_host_process_stop_best_effort(
                pSession->pProcess,
                XPROC_STOP_INTERRUPT
            );
        }
        xrtProcessDestroy(pSession->pProcess);
    }
    free(pSession->sSessionId);
    free(pSession->sSessionName);
    free(pSession->sCommand);
    free(pSession->sResolvedCwd);
    free(pSession);
}

static void xwork__local_host_reset_terminal_sessions(xwork_local_host *pHost)
{
    xwork__local_host_terminal_session *pSession;

    if ( !pHost ) {
        return;
    }

    pSession = (xwork__local_host_terminal_session *)pHost->pTerminalSessions;
    while ( pSession ) {
        xwork__local_host_terminal_session *pNext = pSession->pNext;
        xwork__local_host_free_terminal_session(pSession);
        pSession = pNext;
    }
    pHost->pTerminalSessions = NULL;
    pHost->iNextTerminalSessionId = 0u;
}

static void xwork__local_host_remove_terminal_session(
    xwork_local_host *pHost,
    xwork__local_host_terminal_session *pSession
)
{
    xwork__local_host_terminal_session **ppCursor;

    if ( !pHost || !pSession ) {
        return;
    }

    ppCursor = (xwork__local_host_terminal_session **)&pHost->pTerminalSessions;
    while ( *ppCursor ) {
        if ( *ppCursor == pSession ) {
            *ppCursor = pSession->pNext;
            xwork__local_host_free_terminal_session(pSession);
            return;
        }
        ppCursor = &(*ppCursor)->pNext;
    }
}

static void xwork__local_host_process_stdout_cb(
    xprocess *pProcess,
    const void *pData,
    size_t iSize,
    ptr pUserData
)
{
    xwork__local_host_process_capture_set *pCaptureSet =
        (xwork__local_host_process_capture_set *)pUserData;
    xwork__local_host_process_capture *pCapture;

    (void)pProcess;
    if ( !pCaptureSet || !pCaptureSet->pStdoutCapture ) {
        return;
    }

    pCapture = pCaptureSet->pStdoutCapture;
    (void)xwork__local_host_process_capture_append(pCapture, pData, iSize);
}

static void xwork__local_host_process_stderr_cb(
    xprocess *pProcess,
    const void *pData,
    size_t iSize,
    ptr pUserData
)
{
    xwork__local_host_process_capture_set *pCaptureSet =
        (xwork__local_host_process_capture_set *)pUserData;
    xwork__local_host_process_capture *pCapture;

    (void)pProcess;
    if ( !pCaptureSet || !pCaptureSet->pStderrCapture ) {
        return;
    }

    pCapture = pCaptureSet->pStderrCapture;
    (void)xwork__local_host_process_capture_append(pCapture, pData, iSize);
}

static const char *xwork__local_host_process_stop_reason_name(int iStopReason)
{
    switch ( iStopReason ) {
        case XPROC_STOP_INTERRUPT:
            return "interrupt";
        case XPROC_STOP_TERMINATE:
            return "terminate";
        case XPROC_STOP_KILL:
            return "kill";
        case XPROC_STOP_KILL_TREE:
            return "kill_tree";
        case XPROC_STOP_NONE:
        default:
            return "none";
    }
}

static const char *xwork__local_host_process_event_kind_name(int iKind)
{
    switch ( iKind ) {
        case XPROC_EVENT_START:
            return "start";
        case XPROC_EVENT_OUTPUT:
            return "output";
        case XPROC_EVENT_EXIT:
            return "exit";
        case XPROC_EVENT_NONE:
        default:
            return "none";
    }
}

static const char *xwork__local_host_process_stream_name(int iStream)
{
    switch ( iStream ) {
        case XPROC_STREAM_STDOUT:
            return "stdout";
        case XPROC_STREAM_STDERR:
            return "stderr";
        case XPROC_STREAM_TERMINAL:
            return "terminal";
        case XPROC_STREAM_NONE:
        default:
            return "none";
    }
}

static const char *xwork__local_host_process_exit_kind_name(int iKind)
{
    switch ( iKind ) {
        case XPROC_EXIT_NORMAL:
            return "normal";
        case XPROC_EXIT_SIGNAL:
            return "signal";
        case XPROC_EXIT_SPAWN_FAILED:
            return "spawn_failed";
        case XPROC_EXIT_WAIT_FAILED:
            return "wait_failed";
        case XPROC_EXIT_NONE:
        default:
            return "none";
    }
}

static const char *xwork__local_host_process_exit_stage_name(int iStage)
{
    switch ( iStage ) {
        case XPROC_STAGE_SPAWN:
            return "spawn";
        case XPROC_STAGE_WORKDIR:
            return "workdir";
        case XPROC_STAGE_ENV:
            return "env";
        case XPROC_STAGE_STDIN:
            return "stdin";
        case XPROC_STAGE_STDOUT:
            return "stdout";
        case XPROC_STAGE_STDERR:
            return "stderr";
        case XPROC_STAGE_EXEC:
            return "exec";
        case XPROC_STAGE_WAIT:
            return "wait";
        case XPROC_STAGE_NONE:
        default:
            return "none";
    }
}

static xwork_status xwork__local_host_build_process_events_json(
    const xprocessevent *pEvents,
    uint32 iEventCount,
    const char *sStdout,
    const char *sStderr,
    char **ppsEventsJson
)
{
    char *sEventsJson = NULL;
    size_t iEventsJsonLength = 0u;
    char *sItemJson = NULL;
    char *sEventText = NULL;
    char *sEscapedEventText = NULL;
    uint32 i;
    xwork_status iStatus;

    if ( !ppsEventsJson ) {
        return XWORK_ERROR_INVALID_ARGUMENT;
    }

    *ppsEventsJson = NULL;
    if ( iEventCount > 0u && !pEvents ) {
        return XWORK_ERROR_INVALID_ARGUMENT;
    }

    iStatus = xwork__local_host_string_append(
        &sEventsJson,
        &iEventsJsonLength,
        "["
    );
    if ( iStatus != XWORK_OK ) {
        goto cleanup;
    }

    for ( i = 0u; i < iEventCount; ++i ) {
        const xprocessevent *pEvent = &pEvents[i];
        const char *sKind = xwork__local_host_process_event_kind_name(pEvent->iKind);
        const char *sStream = xwork__local_host_process_stream_name(pEvent->iStream);

        if ( i > 0u ) {
            iStatus = xwork__local_host_string_append(
                &sEventsJson,
                &iEventsJsonLength,
                ","
            );
            if ( iStatus != XWORK_OK ) {
                goto cleanup;
            }
        }

        if ( pEvent->iKind == XPROC_EVENT_OUTPUT ) {
            const char *sSource =
                (pEvent->iStream == XPROC_STREAM_STDERR)
                    ? (sStderr ? sStderr : "")
                    : (sStdout ? sStdout : "");
            size_t iSourceLength = strlen(sSource);
            size_t iOffsetBytes =
                (pEvent->iOffset > (uint64)SIZE_MAX)
                    ? SIZE_MAX
                    : (size_t)pEvent->iOffset;
            size_t iRequestedSize =
                (pEvent->iSize > (uint64)SIZE_MAX)
                    ? SIZE_MAX
                    : (size_t)pEvent->iSize;
            size_t iAvailableBytes = 0u;
            bool bTextTruncated = false;

            if ( iOffsetBytes < iSourceLength ) {
                iAvailableBytes = iSourceLength - iOffsetBytes;
                if ( iAvailableBytes > iRequestedSize ) {
                    iAvailableBytes = iRequestedSize;
                }
            }
            if ( iAvailableBytes < iRequestedSize ) {
                bTextTruncated = true;
            }

            sEventText = (char *)malloc(iAvailableBytes + 1u);
            if ( !sEventText ) {
                iStatus = XWORK_ERROR_NO_MEMORY;
                goto cleanup;
            }
            if ( iAvailableBytes > 0u ) {
                memcpy(sEventText, sSource + iOffsetBytes, iAvailableBytes);
            }
            sEventText[iAvailableBytes] = '\0';

            iStatus = xwork__local_host_json_escape(
                sEventText,
                &sEscapedEventText
            );
            if ( iStatus != XWORK_OK ) {
                goto cleanup;
            }

            sItemJson = xwork__dup_printf(
                "{\"seq\":%llu,\"kind\":\"%s\",\"stream\":\"%s\","
                "\"offset_bytes\":%llu,\"size_bytes\":%llu,\"time_ms\":%llu,"
                "\"text\":\"%s\",\"text_truncated\":%s}",
                (unsigned long long)pEvent->iSeq,
                sKind,
                sStream,
                (unsigned long long)pEvent->iOffset,
                (unsigned long long)pEvent->iSize,
                (unsigned long long)pEvent->tTimeMs,
                sEscapedEventText ? sEscapedEventText : "",
                bTextTruncated ? "true" : "false"
            );
        } else if ( pEvent->iKind == XPROC_EVENT_EXIT ) {
            sItemJson = xwork__dup_printf(
                "{\"seq\":%llu,\"kind\":\"%s\",\"stream\":\"%s\","
                "\"offset_bytes\":%llu,\"size_bytes\":%llu,\"time_ms\":%llu,"
                "\"exit_kind\":\"%s\",\"exit_code\":%d,\"signal\":%d,"
                "\"stage\":\"%s\",\"os_error\":%d,"
                "\"stop_reason\":\"%s\",\"timed_out\":%s,\"cancelled\":%s}",
                (unsigned long long)pEvent->iSeq,
                sKind,
                sStream,
                (unsigned long long)pEvent->iOffset,
                (unsigned long long)pEvent->iSize,
                (unsigned long long)pEvent->tTimeMs,
                xwork__local_host_process_exit_kind_name(pEvent->ExitInfo.iKind),
                pEvent->ExitInfo.iExitCode,
                pEvent->ExitInfo.iSignal,
                xwork__local_host_process_exit_stage_name(pEvent->ExitInfo.iStage),
                pEvent->ExitInfo.iOsError,
                xwork__local_host_process_stop_reason_name(
                    pEvent->ExitInfo.iStopReason
                ),
                pEvent->ExitInfo.bTimedOut ? "true" : "false",
                pEvent->ExitInfo.bCancelled ? "true" : "false"
            );
        } else {
            sItemJson = xwork__dup_printf(
                "{\"seq\":%llu,\"kind\":\"%s\",\"stream\":\"%s\","
                "\"offset_bytes\":%llu,\"size_bytes\":%llu,\"time_ms\":%llu}",
                (unsigned long long)pEvent->iSeq,
                sKind,
                sStream,
                (unsigned long long)pEvent->iOffset,
                (unsigned long long)pEvent->iSize,
                (unsigned long long)pEvent->tTimeMs
            );
        }
        if ( !sItemJson ) {
            iStatus = XWORK_ERROR_NO_MEMORY;
            goto cleanup;
        }

        iStatus = xwork__local_host_string_append(
            &sEventsJson,
            &iEventsJsonLength,
            sItemJson
        );
        if ( iStatus != XWORK_OK ) {
            goto cleanup;
        }

        free(sItemJson);
        sItemJson = NULL;
        free(sEventText);
        sEventText = NULL;
        free(sEscapedEventText);
        sEscapedEventText = NULL;
    }

    iStatus = xwork__local_host_string_append(
        &sEventsJson,
        &iEventsJsonLength,
        "]"
    );
    if ( iStatus != XWORK_OK ) {
        goto cleanup;
    }

    *ppsEventsJson = sEventsJson;
    sEventsJson = NULL;
    iStatus = XWORK_OK;

cleanup:
    free(sItemJson);
    free(sEventText);
    free(sEscapedEventText);
    free(sEventsJson);
    return iStatus;
}

static xwork_status xwork__local_host_build_process_events_output_text(
    const xprocessevent *pEvents,
    uint32 iEventCount,
    const char *sStdout,
    const char *sStderr,
    char **ppsOutputText,
    size_t *piOutputBytes
)
{
    char *sOutputText = NULL;
    size_t iOutputBytes = 0u;
    uint32 i;

    if ( ppsOutputText ) {
        *ppsOutputText = NULL;
    }
    if ( piOutputBytes ) {
        *piOutputBytes = 0u;
    }
    if ( !ppsOutputText || !piOutputBytes ) {
        return XWORK_ERROR_INVALID_ARGUMENT;
    }

    for ( i = 0u; i < iEventCount; ++i ) {
        const xprocessevent *pEvent = &pEvents[i];
        const char *sSource;
        size_t iSourceLength;
        size_t iOffsetBytes;
        size_t iRequestedSize;
        size_t iWritableBytes = 0u;
        char *sNext;

        if ( pEvent->iKind != XPROC_EVENT_OUTPUT ) {
            continue;
        }

        sSource =
            (pEvent->iStream == XPROC_STREAM_STDERR)
                ? (sStderr ? sStderr : "")
                : (sStdout ? sStdout : "");
        iSourceLength = strlen(sSource);
        iOffsetBytes =
            (pEvent->iOffset > (uint64)SIZE_MAX)
                ? SIZE_MAX
                : (size_t)pEvent->iOffset;
        iRequestedSize =
            (pEvent->iSize > (uint64)SIZE_MAX)
                ? SIZE_MAX
                : (size_t)pEvent->iSize;
        if ( iOffsetBytes < iSourceLength ) {
            iWritableBytes = iSourceLength - iOffsetBytes;
            if ( iWritableBytes > iRequestedSize ) {
                iWritableBytes = iRequestedSize;
            }
        }
        if ( iWritableBytes == 0u ) {
            continue;
        }

        sNext = (char *)realloc(sOutputText, iOutputBytes + iWritableBytes + 1u);
        if ( !sNext ) {
            free(sOutputText);
            return XWORK_ERROR_NO_MEMORY;
        }
        sOutputText = sNext;
        memcpy(sOutputText + iOutputBytes, sSource + iOffsetBytes, iWritableBytes);
        iOutputBytes += iWritableBytes;
        sOutputText[iOutputBytes] = '\0';
    }

    *ppsOutputText = sOutputText;
    *piOutputBytes = iOutputBytes;
    return XWORK_OK;
}

static xwork_status xwork__local_host_parse_process_stop_reason(
    const char *sText,
    int *piStopReason
)
{
    if ( !piStopReason ) {
        return XWORK_ERROR_INVALID_ARGUMENT;
    }

    if ( !sText || !sText[0] || strcmp(sText, "interrupt") == 0 ) {
        *piStopReason = XPROC_STOP_INTERRUPT;
        return XWORK_OK;
    }
    if ( strcmp(sText, "terminate") == 0 ) {
        *piStopReason = XPROC_STOP_TERMINATE;
        return XWORK_OK;
    }
    if ( strcmp(sText, "kill") == 0 ) {
        *piStopReason = XPROC_STOP_KILL;
        return XWORK_OK;
    }
    if ( strcmp(sText, "kill_tree") == 0 ) {
        *piStopReason = XPROC_STOP_KILL_TREE;
        return XWORK_OK;
    }
    return XWORK_ERROR_INVALID_ARGUMENT;
}

static bool xwork__local_host_process_request_stop(
    xprocess *pProcess,
    int iStopReason
)
{
    switch ( iStopReason ) {
        case XPROC_STOP_INTERRUPT:
            return xrtProcessInterrupt(pProcess);
        case XPROC_STOP_TERMINATE:
            return xrtProcessTerminate(pProcess);
        case XPROC_STOP_KILL:
            return xrtProcessKill(pProcess);
        case XPROC_STOP_KILL_TREE:
            return xrtProcessKillTree(pProcess);
        default:
            return false;
    }
}

static int xwork__local_host_process_stop_best_effort(
    xprocess *pProcess,
    int iInitialStopReason
)
{
    if ( !pProcess ) {
        return XPROC_STOP_NONE;
    }

    if ( iInitialStopReason == XPROC_STOP_NONE ) {
        iInitialStopReason = XPROC_STOP_INTERRUPT;
    }

    (void)xwork__local_host_process_request_stop(pProcess, iInitialStopReason);
    if ( xrtProcessWaitTimeout(pProcess, XWORK__LOCAL_HOST_PROCESS_TIMEOUT_GRACE_MS) ==
         XRT_WAIT_OK ) {
        return iInitialStopReason;
    }

    if ( iInitialStopReason != XPROC_STOP_TERMINATE &&
         iInitialStopReason != XPROC_STOP_KILL &&
         iInitialStopReason != XPROC_STOP_KILL_TREE ) {
        (void)xrtProcessTerminate(pProcess);
        if ( xrtProcessWaitTimeout(pProcess, XWORK__LOCAL_HOST_PROCESS_TIMEOUT_GRACE_MS) ==
             XRT_WAIT_OK ) {
            return XPROC_STOP_TERMINATE;
        }
    }

    if ( iInitialStopReason == XPROC_STOP_KILL ) {
        (void)xrtProcessWait(pProcess);
        return XPROC_STOP_KILL;
    }

    (void)xrtProcessKillTree(pProcess);
    if ( xrtProcessWaitTimeout(pProcess, XWORK__LOCAL_HOST_PROCESS_TIMEOUT_GRACE_MS) ==
         XRT_WAIT_OK ) {
        return XPROC_STOP_KILL_TREE;
    }

    (void)xrtProcessKill(pProcess);
    (void)xrtProcessWait(pProcess);
    return XPROC_STOP_KILL;
}

bool xwork_host_invoke_context_should_cancel(
    const xwork_host_invoke_context *pContext,
    const char *sPhase
)
{
    if ( !pContext ) {
        return false;
    }
    if ( pContext->pCancelToken &&
         xllm_cancel_token_is_cancelled(pContext->pCancelToken) ) {
        return true;
    }
    if ( pContext->pfnShouldInterrupt &&
         pContext->pfnShouldInterrupt(
             pContext->pRun,
             sPhase && sPhase[0]
                 ? sPhase
                 : (pContext->sPhase ? pContext->sPhase : "host_invoke"),
             pContext->pInterruptUserData
         ) ) {
        return true;
    }
    return false;
}

static int xwork__local_host_process_wait_cancellable(
    xprocess *pProcess,
    bool bHasTimeoutMs,
    size_t iTimeoutMs,
    const xwork_host_invoke_context *pContext,
    bool *pbCancelled
)
{
    const uint32 iPollMs = 50u;
    double dStartSeconds = 0.0;

    if ( pbCancelled ) {
        *pbCancelled = false;
    }
    if ( !pProcess ) {
        return XRT_WAIT_ERROR;
    }

    if ( bHasTimeoutMs ) {
        dStartSeconds = xrtTimer();
    }

    for ( ;; ) {
        uint32 iWaitMs = iPollMs;
        int iWaitResult;

        if ( xwork_host_invoke_context_should_cancel(pContext, "process_wait") ) {
            if ( pbCancelled ) {
                *pbCancelled = true;
            }
            return XRT_WAIT_TIMEOUT;
        }

        if ( bHasTimeoutMs ) {
            double dElapsedMs = (xrtTimer() - dStartSeconds) * 1000.0;
            double dRemainMs;

            if ( dElapsedMs >= (double)iTimeoutMs ) {
                return XRT_WAIT_TIMEOUT;
            }
            dRemainMs = (double)iTimeoutMs - dElapsedMs;
            if ( dRemainMs < (double)iWaitMs ) {
                iWaitMs = dRemainMs <= 1.0 ? 1u : (uint32)dRemainMs;
            }
        }

        iWaitResult = xrtProcessWaitTimeout(pProcess, iWaitMs);
        if ( iWaitResult != XRT_WAIT_TIMEOUT ) {
            return iWaitResult;
        }

        if ( !bHasTimeoutMs ) {
            continue;
        }
    }
}

static xwork_status xwork__local_host_write_text_file(
    const char *sPath,
    const char *sText,
    bool bAppend,
    size_t *piBytesWritten
)
{
    FILE *pFile = NULL;
    size_t iLength;

    if ( !sPath || !sPath[0] || !sText || !piBytesWritten ) {
        return XWORK_ERROR_INVALID_ARGUMENT;
    }

    *piBytesWritten = 0u;
    iLength = strlen(sText);

    pFile = fopen(sPath, bAppend ? "ab" : "wb");
    if ( !pFile ) {
        return XWORK_ERROR_EXTERNAL_FAILURE;
    }

    if ( iLength > 0u && fwrite(sText, 1u, iLength, pFile) != iLength ) {
        fclose(pFile);
        return XWORK_ERROR_EXTERNAL_FAILURE;
    }
    if ( fclose(pFile) != 0 ) {
        return XWORK_ERROR_EXTERNAL_FAILURE;
    }

    *piBytesWritten = iLength;
    return XWORK_OK;
}

static bool xwork__local_host_is_path_separator(char c)
{
    return c == '/' || c == '\\';
}

static char *xwork__local_host_dup_prefix(const char *sText, size_t iLength)
{
    char *sCopy;

    if ( !sText ) {
        return NULL;
    }

    sCopy = (char *)malloc(iLength + 1u);
    if ( !sCopy ) {
        return NULL;
    }

    if ( iLength > 0u ) {
        memcpy(sCopy, sText, iLength);
    }
    sCopy[iLength] = '\0';
    return sCopy;
}

static char *xwork__local_host_parent_directory(const char *sPath)
{
    size_t iLength;
    const char *sLastSeparator = NULL;
    size_t i;

    if ( !sPath || !sPath[0] ) {
        return NULL;
    }

    iLength = strlen(sPath);
    for ( i = 0u; i < iLength; ++i ) {
        if ( xwork__local_host_is_path_separator(sPath[i]) ) {
            sLastSeparator = &sPath[i];
        }
    }
    if ( !sLastSeparator ) {
        return NULL;
    }

#ifdef _WIN32
    if ( sLastSeparator == sPath + 2u &&
         isalpha((unsigned char)sPath[0]) &&
         sPath[1] == ':' &&
         xwork__local_host_is_path_separator(sPath[2]) ) {
        return xwork__local_host_dup_prefix(sPath, 3u);
    }
#endif

    if ( sLastSeparator == sPath ) {
        return xwork__local_host_dup_prefix(sPath, 1u);
    }

    return xwork__local_host_dup_prefix(
        sPath,
        (size_t)(sLastSeparator - sPath)
    );
}

static xwork_status xwork__local_host_create_directory(const char *sPath)
{
    if ( !sPath || !sPath[0] ) {
        return XWORK_OK;
    }

    errno = 0;
#ifdef _WIN32
    if ( _mkdir(sPath) == 0 ) {
        return XWORK_OK;
    }
#else
    if ( mkdir(sPath, 0777) == 0 ) {
        return XWORK_OK;
    }
#endif
    if ( errno == EEXIST ) {
        return XWORK_OK;
    }
    return XWORK_ERROR_EXTERNAL_FAILURE;
}

static xwork_status xwork__local_host_ensure_directory_tree(const char *sDirectoryPath)
{
    char *sMutablePath;
    size_t iStart = 0u;
    size_t i;
    xwork_status iStatus = XWORK_OK;

    if ( !sDirectoryPath || !sDirectoryPath[0] ) {
        return XWORK_OK;
    }

    sMutablePath = xwork__dup_cstr(sDirectoryPath);
    if ( !sMutablePath ) {
        return XWORK_ERROR_NO_MEMORY;
    }

#ifdef _WIN32
    if ( isalpha((unsigned char)sMutablePath[0]) &&
         sMutablePath[1] == ':' ) {
        iStart = 2u;
        if ( xwork__local_host_is_path_separator(sMutablePath[2]) ) {
            iStart = 3u;
        }
    } else if ( xwork__local_host_is_path_separator(sMutablePath[0]) &&
                xwork__local_host_is_path_separator(sMutablePath[1]) ) {
        iStart = 2u;
    }
#else
    if ( xwork__local_host_is_path_separator(sMutablePath[0]) ) {
        iStart = 1u;
    }
#endif

    for ( i = iStart; sMutablePath[i] != '\0'; ++i ) {
        if ( !xwork__local_host_is_path_separator(sMutablePath[i]) ) {
            continue;
        }

        sMutablePath[i] = '\0';
        if ( sMutablePath[0] != '\0' ) {
            iStatus = xwork__local_host_create_directory(sMutablePath);
            if ( iStatus != XWORK_OK ) {
                free(sMutablePath);
                return iStatus;
            }
        }
        sMutablePath[i] = '/';
    }

    iStatus = xwork__local_host_create_directory(sMutablePath);
    free(sMutablePath);
    return iStatus;
}

static xwork_status xwork__local_host_ensure_parent_directories(const char *sPath)
{
    char *sParentDirectory;
    xwork_status iStatus;

    sParentDirectory = xwork__local_host_parent_directory(sPath);
    if ( !sParentDirectory ) {
        return XWORK_OK;
    }

    iStatus = xwork__local_host_ensure_directory_tree(sParentDirectory);
    free(sParentDirectory);
    return iStatus;
}

static xwork_status xwork__local_host_remove_empty_directory(const char *sPath)
{
    if ( !sPath || !sPath[0] ) {
        return XWORK_ERROR_INVALID_ARGUMENT;
    }

    errno = 0;
#ifdef _WIN32
    if ( _rmdir(sPath) == 0 ) {
        return XWORK_OK;
    }
#else
    if ( rmdir(sPath) == 0 ) {
        return XWORK_OK;
    }
#endif
    if ( errno == ENOENT ) {
        return XWORK_ERROR_NOT_FOUND;
    }
    if ( errno == EEXIST || errno == ENOTEMPTY ) {
        return XWORK_ERROR_INVALID_STATE;
    }
    return XWORK_ERROR_EXTERNAL_FAILURE;
}

static xwork_status xwork__local_host_delete_path(
    const char *sPath,
    bool bRecursive,
    const char **psDeletedType
);

static xwork_status xwork__local_host_delete_directory_contents(const char *sPath)
{
    if ( !sPath || !sPath[0] ) {
        return XWORK_ERROR_INVALID_ARGUMENT;
    }

#ifdef _WIN32
    {
        WIN32_FIND_DATAA tFindData;
        HANDLE hFind;
        char *sFindPath;
        xwork_status iStatus = XWORK_OK;

        sFindPath = xwork__local_host_join_path(sPath, "*");
        if ( !sFindPath ) {
            return XWORK_ERROR_NO_MEMORY;
        }

        hFind = FindFirstFileA(sFindPath, &tFindData);
        free(sFindPath);
        if ( hFind == INVALID_HANDLE_VALUE ) {
            return XWORK_ERROR_EXTERNAL_FAILURE;
        }

        do {
            char *sChildPath;

            if ( strcmp(tFindData.cFileName, ".") == 0 ||
                 strcmp(tFindData.cFileName, "..") == 0 ) {
                continue;
            }

            sChildPath = xwork__local_host_join_path(sPath, tFindData.cFileName);
            if ( !sChildPath ) {
                iStatus = XWORK_ERROR_NO_MEMORY;
                break;
            }
            iStatus = xwork__local_host_delete_path(sChildPath, true, NULL);
            free(sChildPath);
            if ( iStatus != XWORK_OK ) {
                break;
            }
        } while ( FindNextFileA(hFind, &tFindData) );

        FindClose(hFind);
        return iStatus;
    }
#else
    {
        DIR *pDir;
        struct dirent *pEntry;
        xwork_status iStatus = XWORK_OK;

        pDir = opendir(sPath);
        if ( !pDir ) {
            return errno == ENOENT ? XWORK_ERROR_NOT_FOUND : XWORK_ERROR_EXTERNAL_FAILURE;
        }

        while ( (pEntry = readdir(pDir)) != NULL ) {
            char *sChildPath;

            if ( strcmp(pEntry->d_name, ".") == 0 ||
                 strcmp(pEntry->d_name, "..") == 0 ) {
                continue;
            }

            sChildPath = xwork__local_host_join_path(sPath, pEntry->d_name);
            if ( !sChildPath ) {
                iStatus = XWORK_ERROR_NO_MEMORY;
                break;
            }
            iStatus = xwork__local_host_delete_path(sChildPath, true, NULL);
            free(sChildPath);
            if ( iStatus != XWORK_OK ) {
                break;
            }
        }

        closedir(pDir);
        return iStatus;
    }
#endif
}

static xwork_status xwork__local_host_delete_path(
    const char *sPath,
    bool bRecursive,
    const char **psDeletedType
)
{
    const char *sType = "other";
    size_t iSizeBytes = 0u;
    long long iMtimeUnix = 0;
    xwork_status iStatus;

    if ( !sPath || !sPath[0] ) {
        return XWORK_ERROR_INVALID_ARGUMENT;
    }
    if ( psDeletedType ) {
        *psDeletedType = "other";
    }

    iStatus = xwork__local_host_stat_path(sPath, &sType, &iSizeBytes, &iMtimeUnix);
    (void)iSizeBytes;
    (void)iMtimeUnix;
    if ( iStatus != XWORK_OK ) {
        return iStatus;
    }
    if ( psDeletedType ) {
        *psDeletedType = sType;
    }

    if ( strcmp(sType, "directory") == 0 ) {
        if ( bRecursive ) {
            iStatus = xwork__local_host_delete_directory_contents(sPath);
            if ( iStatus != XWORK_OK ) {
                return iStatus;
            }
        }
        return xwork__local_host_remove_empty_directory(sPath);
    }

    errno = 0;
    if ( remove(sPath) == 0 ) {
        return XWORK_OK;
    }
    if ( errno == ENOENT ) {
        return XWORK_ERROR_NOT_FOUND;
    }
    return XWORK_ERROR_EXTERNAL_FAILURE;
}

static char *xwork__local_host_quote_shell_arg(const char *sText)
{
    if ( !sText ) {
        return NULL;
    }

#ifdef _WIN32
    if ( strchr(sText, '"') != NULL ) {
        return NULL;
    }
    return xwork__dup_printf("\"%s\"", sText);
#else
    char *sEscaped = NULL;
    char *sQuoted = NULL;
    size_t i;
    size_t iExtra = 0u;
    size_t iPos = 0u;

    for ( i = 0u; sText[i] != '\0'; ++i ) {
        if ( sText[i] == '\'' ) {
            iExtra += 3u;
        }
    }
    sEscaped = (char *)malloc(strlen(sText) + iExtra + 1u);
    if ( !sEscaped ) {
        return NULL;
    }
    for ( i = 0u; sText[i] != '\0'; ++i ) {
        if ( sText[i] == '\'' ) {
            sEscaped[iPos++] = '\'';
            sEscaped[iPos++] = '\\';
            sEscaped[iPos++] = '\'';
            sEscaped[iPos++] = '\'';
        } else {
            sEscaped[iPos++] = sText[i];
        }
    }
    sEscaped[iPos] = '\0';
    sQuoted = xwork__dup_printf("'%s'", sEscaped);
    free(sEscaped);
    return sQuoted;
#endif
}

static bool xwork__local_host_is_valid_env_name(
    const char *sName,
    size_t iLength
)
{
    size_t i;

    if ( !sName || iLength == 0u ) {
        return false;
    }
    if ( !(isalpha((unsigned char)sName[0]) || sName[0] == '_') ) {
        return false;
    }
    for ( i = 1u; i < iLength; ++i ) {
        if ( !(isalnum((unsigned char)sName[i]) || sName[i] == '_') ) {
            return false;
        }
    }

    return true;
}

static void xwork__local_host_free_process_env_entries(
    char ***ppsEntries,
    size_t iEntryCount
)
{
    size_t i;
    char **psEntries;

    if ( !ppsEntries || !*ppsEntries ) {
        return;
    }

    psEntries = *ppsEntries;
    for ( i = 0u; i < iEntryCount; ++i ) {
        free(psEntries[i]);
    }
    free(psEntries);
    *ppsEntries = NULL;
}

static xwork_status xwork__local_host_build_process_env_entries(
    xvalue tEnvList,
    size_t iMaxEnvEntries,
    char ***ppsEntries,
    size_t *piEnvCount
)
{
    char **psEntries = NULL;
    uint32 iCount = 0u;
    uint32 i;

    if ( !ppsEntries ) {
        return XWORK_ERROR_INVALID_ARGUMENT;
    }

    *ppsEntries = NULL;
    if ( piEnvCount ) {
        *piEnvCount = 0u;
    }
    if ( !tEnvList ) {
        return XWORK_OK;
    }

    if ( xvoType(tEnvList) == XVO_DT_ARRAY ) {
        iCount = xvoArrayItemCount(tEnvList);
    } else {
        iCount = xvoListItemCount(tEnvList);
    }
    if ( iMaxEnvEntries > 0u && (size_t)iCount > iMaxEnvEntries ) {
        return XWORK_ERROR_INVALID_ARGUMENT;
    }
    if ( iCount == 0u ) {
        return XWORK_OK;
    }

    psEntries = (char **)calloc((size_t)iCount, sizeof(*psEntries));
    if ( !psEntries ) {
        return XWORK_ERROR_NO_MEMORY;
    }

    for ( i = 0u; i < iCount; ++i ) {
        xvalue tItem = (xvoType(tEnvList) == XVO_DT_ARRAY)
            ? xvoArrayGetValue(tEnvList, i)
            : xvoListGetValue(tEnvList, (int64)i);
        const char *sEntry;
        const char *sEquals;
        size_t iNameLength;

        if ( !tItem || xvoType(tItem) != XVO_DT_TEXT ) {
            xwork__local_host_free_process_env_entries(&psEntries, (size_t)iCount);
            return XWORK_ERROR_INVALID_ARGUMENT;
        }

        sEntry = (const char *)xvoGetText(tItem);
        if ( !sEntry ) {
            xwork__local_host_free_process_env_entries(&psEntries, (size_t)iCount);
            return XWORK_ERROR_INVALID_ARGUMENT;
        }

        sEquals = strchr(sEntry, '=');
        if ( !sEquals || sEquals == sEntry ) {
            xwork__local_host_free_process_env_entries(&psEntries, (size_t)iCount);
            return XWORK_ERROR_INVALID_ARGUMENT;
        }

        iNameLength = (size_t)(sEquals - sEntry);
        if ( !xwork__local_host_is_valid_env_name(sEntry, iNameLength) ) {
            xwork__local_host_free_process_env_entries(&psEntries, (size_t)iCount);
            return XWORK_ERROR_INVALID_ARGUMENT;
        }

        psEntries[i] = xwork__dup_cstr(sEntry);
        if ( !psEntries[i] ) {
            xwork__local_host_free_process_env_entries(&psEntries, (size_t)iCount);
            return XWORK_ERROR_NO_MEMORY;
        }
    }

    *ppsEntries = psEntries;
    if ( piEnvCount ) {
        *piEnvCount = (size_t)iCount;
    }
    return XWORK_OK;
}

static xwork_status xwork__local_host_capture_command_output(
    const char *sShellCommand,
    size_t iMaxBytes,
    char **ppsOutput,
    int *piExitCode,
    bool *pbTruncated
)
{
    FILE *pPipe = NULL;
    char *sOutput = NULL;
    size_t iCapacity = 0u;
    size_t iLength = 0u;
    int iCloseStatus;

    if ( !sShellCommand || !sShellCommand[0] || !ppsOutput || !piExitCode || !pbTruncated ) {
        return XWORK_ERROR_INVALID_ARGUMENT;
    }

    *ppsOutput = NULL;
    *piExitCode = -1;
    *pbTruncated = false;

    pPipe = XWORK__HOST_POPEN(sShellCommand, "r");
    if ( !pPipe ) {
        return XWORK_ERROR_EXTERNAL_FAILURE;
    }

    iCapacity = (iMaxBytes > 0u && iMaxBytes < 4096u) ? iMaxBytes + 1u : 4097u;
    sOutput = (char *)malloc(iCapacity);
    if ( !sOutput ) {
        (void)XWORK__HOST_PCLOSE(pPipe);
        return XWORK_ERROR_NO_MEMORY;
    }

    for (;;) {
        char sChunk[1024];
        size_t iChunkBytes;

        iChunkBytes = fread(sChunk, 1u, sizeof(sChunk), pPipe);
        if ( iChunkBytes == 0u ) {
            break;
        }

        if ( iLength < iMaxBytes ) {
            size_t iWritable = iChunkBytes;

            if ( iLength + iWritable > iMaxBytes ) {
                iWritable = iMaxBytes - iLength;
                *pbTruncated = true;
            }
            if ( iLength + iWritable + 1u > iCapacity ) {
                char *sNextOutput;
                size_t iNextCapacity = iLength + iWritable + 1u;

                sNextOutput = (char *)realloc(sOutput, iNextCapacity);
                if ( !sNextOutput ) {
                    free(sOutput);
                    (void)XWORK__HOST_PCLOSE(pPipe);
                    return XWORK_ERROR_NO_MEMORY;
                }
                sOutput = sNextOutput;
                iCapacity = iNextCapacity;
            }
            if ( iWritable > 0u ) {
                memcpy(sOutput + iLength, sChunk, iWritable);
                iLength += iWritable;
            }
        } else {
            *pbTruncated = true;
        }
    }

    sOutput[iLength] = '\0';
    iCloseStatus = XWORK__HOST_PCLOSE(pPipe);

#ifdef _WIN32
    *piExitCode = iCloseStatus;
#else
    if ( WIFEXITED(iCloseStatus) ) {
        *piExitCode = WEXITSTATUS(iCloseStatus);
    } else {
        *piExitCode = iCloseStatus;
    }
#endif

    *ppsOutput = sOutput;
    return XWORK_OK;
}

static xwork_status xwork__local_host_set_result(
    xwork_local_host *pHost,
    const char *sOutputText,
    const char *sVisibleSummary,
    xwork_tool_result *pResult
)
{
    xwork_status iStatus;

    if ( !pHost || !pResult ) {
        return XWORK_ERROR_INVALID_ARGUMENT;
    }

    iStatus = xwork__replace_cstr(&pHost->sLastOutputText, sOutputText);
    if ( iStatus != XWORK_OK ) {
        return iStatus;
    }
    iStatus = xwork__replace_cstr(&pHost->sLastVisibleSummary, sVisibleSummary);
    if ( iStatus != XWORK_OK ) {
        return iStatus;
    }

    pResult->sOutputText = pHost->sLastOutputText;
    pResult->sVisibleSummary = pHost->sLastVisibleSummary;
    pResult->bRetryable = false;
    return XWORK_OK;
}

static size_t xwork__local_host_list_count(xvalue tList)
{
    if ( !tList ) {
        return 0u;
    }
    if ( xvoType(tList) == XVO_DT_ARRAY ) {
        return (size_t)xvoArrayItemCount(tList);
    }
    return (size_t)xvoListItemCount(tList);
}

static xwork_status xwork__local_host_set_process_result(
    xwork_local_host *pHost,
    const char *sCommand,
    const char *sResolvedCwd,
    const char *sStdout,
    const char *sStderr,
    int iExitCode,
    bool bTruncated,
    bool bStdoutTruncated,
    bool bStderrTruncated,
    bool bMergeStderr,
    bool bUseTerminal,
    size_t iTerminalCols,
    size_t iTerminalRows,
    size_t iEnvCount,
    size_t iStdinBytes,
    size_t iTimeoutMs,
    bool bTimedOut,
    bool bCancelled,
    int iTimeoutStopReason,
    int iStopReason,
    bool bAllowNonZeroExit,
    bool bIncludeEvents,
    uint32 iEventCount,
    const char *sEventsJson,
    bool bOk,
    const char *sVisibleSummary,
    const char *sErrorKind,
    const char *sErrorMessage,
    xwork_tool_result *pResult
)
{
    char *sEscapedCommand = NULL;
    char *sEscapedStdout = NULL;
    char *sEscapedStderr = NULL;
    char *sEscapedCwd = NULL;
    char *sEscapedErrorKind = NULL;
    char *sEscapedErrorMessage = NULL;
    char *sEventsFragment = NULL;
    char *sOutputText = NULL;
    const char *sTimeoutStopName;
    const char *sStopReasonName;
    bool bTerminalOutputCaptured = false;
    xwork_status iStatus;

    if ( !pHost || !pResult ) {
        return XWORK_ERROR_INVALID_ARGUMENT;
    }

    sTimeoutStopName = xwork__local_host_process_stop_reason_name(iTimeoutStopReason);
    sStopReasonName = xwork__local_host_process_stop_reason_name(iStopReason);
    bTerminalOutputCaptured = bUseTerminal &&
        ((sStdout && sStdout[0]) ||
         (sStderr && sStderr[0]) ||
         (sEventsJson && strstr(sEventsJson, "\"stream\":\"terminal\"") != NULL));

    iStatus = xwork__local_host_json_escape(sCommand ? sCommand : "", &sEscapedCommand);
    if ( iStatus != XWORK_OK ) goto cleanup;
    iStatus = xwork__local_host_json_escape(sStdout ? sStdout : "", &sEscapedStdout);
    if ( iStatus != XWORK_OK ) goto cleanup;
    iStatus = xwork__local_host_json_escape(sStderr ? sStderr : "", &sEscapedStderr);
    if ( iStatus != XWORK_OK ) goto cleanup;
    iStatus = xwork__local_host_json_escape(
        sResolvedCwd ? sResolvedCwd : "",
        &sEscapedCwd
    );
    if ( iStatus != XWORK_OK ) goto cleanup;
    if ( bIncludeEvents ) {
        sEventsFragment = xwork__dup_printf(
            ",\"include_events\":true,\"event_count\":%u,\"events\":%s",
            (unsigned)iEventCount,
            sEventsJson ? sEventsJson : "[]"
        );
        if ( !sEventsFragment ) {
            iStatus = XWORK_ERROR_NO_MEMORY;
            goto cleanup;
        }
    }

    if ( bOk ) {
        sOutputText = xwork__dup_printf(
            "{\"ok\":true,\"command\":\"%s\",\"cwd\":\"%s\",\"merge_stderr\":%s,"
            "\"use_terminal\":%s,\"terminal_cols\":%zu,\"terminal_rows\":%zu,"
            "\"terminal_output_captured\":%s,"
            "\"stdout\":\"%s\",\"stderr\":\"%s\"%s,\"exit_code\":%d,"
            "\"truncated\":%s,\"stdout_truncated\":%s,\"stderr_truncated\":%s,"
            "\"env_count\":%zu,\"stdin_bytes\":%zu,"
            "\"timeout_ms\":%zu,\"timed_out\":%s,\"cancelled\":%s,"
            "\"timeout_stop\":\"%s\","
            "\"stop_reason\":\"%s\",\"allow_nonzero_exit\":%s}",
            sEscapedCommand,
            sEscapedCwd,
            bMergeStderr ? "true" : "false",
            bUseTerminal ? "true" : "false",
            iTerminalCols,
            iTerminalRows,
            bTerminalOutputCaptured ? "true" : "false",
            sEscapedStdout,
            sEscapedStderr,
            sEventsFragment ? sEventsFragment : "",
            iExitCode,
            bTruncated ? "true" : "false",
            bStdoutTruncated ? "true" : "false",
            bStderrTruncated ? "true" : "false",
            iEnvCount,
            iStdinBytes,
            iTimeoutMs,
            bTimedOut ? "true" : "false",
            bCancelled ? "true" : "false",
            sTimeoutStopName,
            sStopReasonName,
            bAllowNonZeroExit ? "true" : "false"
        );
    } else {
        iStatus = xwork__local_host_json_escape(
            sErrorKind ? sErrorKind : "external_failure",
            &sEscapedErrorKind
        );
        if ( iStatus != XWORK_OK ) goto cleanup;
        iStatus = xwork__local_host_json_escape(
            sErrorMessage ? sErrorMessage : "process.exec failed",
            &sEscapedErrorMessage
        );
        if ( iStatus != XWORK_OK ) goto cleanup;
        sOutputText = xwork__dup_printf(
            "{\"ok\":false,\"command\":\"%s\",\"cwd\":\"%s\",\"merge_stderr\":%s,"
            "\"use_terminal\":%s,\"terminal_cols\":%zu,\"terminal_rows\":%zu,"
            "\"terminal_output_captured\":%s,"
            "\"stdout\":\"%s\",\"stderr\":\"%s\"%s,\"exit_code\":%d,"
            "\"truncated\":%s,\"stdout_truncated\":%s,\"stderr_truncated\":%s,"
            "\"env_count\":%zu,\"stdin_bytes\":%zu,"
            "\"timeout_ms\":%zu,\"timed_out\":%s,\"cancelled\":%s,"
            "\"timeout_stop\":\"%s\","
            "\"stop_reason\":\"%s\",\"allow_nonzero_exit\":%s,"
            "\"error_kind\":\"%s\",\"error\":\"%s\"}",
            sEscapedCommand,
            sEscapedCwd,
            bMergeStderr ? "true" : "false",
            bUseTerminal ? "true" : "false",
            iTerminalCols,
            iTerminalRows,
            bTerminalOutputCaptured ? "true" : "false",
            sEscapedStdout,
            sEscapedStderr,
            sEventsFragment ? sEventsFragment : "",
            iExitCode,
            bTruncated ? "true" : "false",
            bStdoutTruncated ? "true" : "false",
            bStderrTruncated ? "true" : "false",
            iEnvCount,
            iStdinBytes,
            iTimeoutMs,
            bTimedOut ? "true" : "false",
            bCancelled ? "true" : "false",
            sTimeoutStopName,
            sStopReasonName,
            bAllowNonZeroExit ? "true" : "false",
            sEscapedErrorKind,
            sEscapedErrorMessage
        );
    }
    if ( !sOutputText ) {
        iStatus = XWORK_ERROR_NO_MEMORY;
        goto cleanup;
    }

    iStatus = xwork__local_host_set_result(
        pHost,
        sOutputText,
        sVisibleSummary ? sVisibleSummary : (bOk ? "process.exec ok" : "process.exec failed"),
        pResult
    );

cleanup:
    free(sEscapedCommand);
    free(sEscapedStdout);
    free(sEscapedStderr);
    free(sEscapedCwd);
    free(sEscapedErrorKind);
    free(sEscapedErrorMessage);
    free(sEventsFragment);
    free(sOutputText);
    return iStatus;
}

static xwork_status xwork__local_host_set_terminal_state_result(
    xwork_local_host *pHost,
    const char *sSessionId,
    const char *sSessionName,
    const char *sCommand,
    const char *sResolvedCwd,
    size_t iSessionIndex,
    const char *sOutputText,
    size_t iOutputBytes,
    size_t iTerminalCols,
    size_t iTerminalRows,
    bool bStdinClosed,
    bool bTerminalOutputCaptured,
    bool bRunning,
    bool bEventStreamDone,
    bool bRemoved,
    uint64 iNextAfterSeq,
    uint64 iEventEndSeq,
    uint32 iEventCount,
    const char *sEventsJson,
    const char *sExtraJsonFields,
    const xprocessexitinfo *pExitInfo,
    bool bOk,
    const char *sVisibleSummary,
    const char *sErrorKind,
    const char *sErrorMessage,
    xwork_tool_result *pResult
)
{
    xprocessexitinfo tExitInfo;
    char *sEscapedSessionId = NULL;
    char *sEscapedSessionName = NULL;
    char *sEscapedCommand = NULL;
    char *sEscapedCwd = NULL;
    char *sEscapedOutputText = NULL;
    char *sEscapedErrorKind = NULL;
    char *sEscapedErrorMessage = NULL;
    char *sResultText = NULL;
    xwork_status iStatus;

    if ( !pHost || !pResult ) {
        return XWORK_ERROR_INVALID_ARGUMENT;
    }

    memset(&tExitInfo, 0, sizeof(tExitInfo));
    if ( pExitInfo ) {
        tExitInfo = *pExitInfo;
    }

    iStatus = xwork__local_host_json_escape(
        sSessionId ? sSessionId : "",
        &sEscapedSessionId
    );
    if ( iStatus != XWORK_OK ) goto cleanup;
    iStatus = xwork__local_host_json_escape(
        sSessionName ? sSessionName : "",
        &sEscapedSessionName
    );
    if ( iStatus != XWORK_OK ) goto cleanup;
    iStatus = xwork__local_host_json_escape(
        sCommand ? sCommand : "",
        &sEscapedCommand
    );
    if ( iStatus != XWORK_OK ) goto cleanup;
    iStatus = xwork__local_host_json_escape(
        sResolvedCwd ? sResolvedCwd : "",
        &sEscapedCwd
    );
    if ( iStatus != XWORK_OK ) goto cleanup;
    iStatus = xwork__local_host_json_escape(
        sOutputText ? sOutputText : "",
        &sEscapedOutputText
    );
    if ( iStatus != XWORK_OK ) goto cleanup;

    if ( bOk ) {
        sResultText = xwork__dup_printf(
            "{\"schema\":\"" XWORK_TERMINAL_STATE_SCHEMA_V1 "\","
            "\"ok\":true,\"session_id\":\"%s\",\"session_name\":\"%s\",\"command\":\"%s\",\"cwd\":\"%s\","
            "\"session_index\":%zu,\"stdin_closed\":%s,"
            "\"running\":%s,\"done\":%s,\"removed\":%s,"
            "\"terminal_cols\":%zu,\"terminal_rows\":%zu,"
            "\"terminal_output_captured\":%s,"
            "\"output_text\":\"%s\",\"output_bytes\":%zu,"
            "\"event_count\":%u,\"events\":%s,\"next_after_seq\":%llu,"
            "\"event_end_seq\":%llu,\"has_more_events\":%s,\"event_stream_done\":%s,%s"
            "\"exit_kind\":\"%s\",\"exit_code\":%d,\"signal\":%d,"
            "\"stage\":\"%s\",\"os_error\":%d,"
            "\"stop_reason\":\"%s\",\"timed_out\":%s,\"cancelled\":%s}",
            sEscapedSessionId,
            sEscapedSessionName,
            sEscapedCommand,
            sEscapedCwd,
            iSessionIndex,
            bStdinClosed ? "true" : "false",
            bRunning ? "true" : "false",
            bRunning ? "false" : "true",
            bRemoved ? "true" : "false",
            iTerminalCols,
            iTerminalRows,
            bTerminalOutputCaptured ? "true" : "false",
            sEscapedOutputText ? sEscapedOutputText : "",
            iOutputBytes,
            (unsigned)iEventCount,
            sEventsJson ? sEventsJson : "[]",
            (unsigned long long)iNextAfterSeq,
            (unsigned long long)iEventEndSeq,
            (iNextAfterSeq < iEventEndSeq) ? "true" : "false",
            bEventStreamDone ? "true" : "false",
            sExtraJsonFields ? sExtraJsonFields : "",
            xwork__local_host_process_exit_kind_name(tExitInfo.iKind),
            tExitInfo.iExitCode,
            tExitInfo.iSignal,
            xwork__local_host_process_exit_stage_name(tExitInfo.iStage),
            tExitInfo.iOsError,
            xwork__local_host_process_stop_reason_name(tExitInfo.iStopReason),
            tExitInfo.bTimedOut ? "true" : "false",
            tExitInfo.bCancelled ? "true" : "false"
        );
    } else {
        iStatus = xwork__local_host_json_escape(
            sErrorKind ? sErrorKind : "external_failure",
            &sEscapedErrorKind
        );
        if ( iStatus != XWORK_OK ) goto cleanup;
        iStatus = xwork__local_host_json_escape(
            sErrorMessage ? sErrorMessage : "terminal session operation failed",
            &sEscapedErrorMessage
        );
        if ( iStatus != XWORK_OK ) goto cleanup;
        sResultText = xwork__dup_printf(
            "{\"schema\":\"" XWORK_TERMINAL_STATE_SCHEMA_V1 "\","
            "\"ok\":false,\"session_id\":\"%s\",\"session_name\":\"%s\",\"command\":\"%s\",\"cwd\":\"%s\","
            "\"session_index\":%zu,\"stdin_closed\":%s,"
            "\"running\":%s,\"done\":%s,\"removed\":%s,"
            "\"terminal_cols\":%zu,\"terminal_rows\":%zu,"
            "\"terminal_output_captured\":%s,"
            "\"output_text\":\"%s\",\"output_bytes\":%zu,"
            "\"event_count\":%u,\"events\":%s,\"next_after_seq\":%llu,"
            "\"event_end_seq\":%llu,\"has_more_events\":%s,\"event_stream_done\":%s,%s"
            "\"exit_kind\":\"%s\",\"exit_code\":%d,\"signal\":%d,"
            "\"stage\":\"%s\",\"os_error\":%d,"
            "\"stop_reason\":\"%s\",\"timed_out\":%s,\"cancelled\":%s,"
            "\"error_kind\":\"%s\",\"error\":\"%s\"}",
            sEscapedSessionId,
            sEscapedSessionName,
            sEscapedCommand,
            sEscapedCwd,
            iSessionIndex,
            bStdinClosed ? "true" : "false",
            bRunning ? "true" : "false",
            bRunning ? "false" : "true",
            bRemoved ? "true" : "false",
            iTerminalCols,
            iTerminalRows,
            bTerminalOutputCaptured ? "true" : "false",
            sEscapedOutputText ? sEscapedOutputText : "",
            iOutputBytes,
            (unsigned)iEventCount,
            sEventsJson ? sEventsJson : "[]",
            (unsigned long long)iNextAfterSeq,
            (unsigned long long)iEventEndSeq,
            (iNextAfterSeq < iEventEndSeq) ? "true" : "false",
            bEventStreamDone ? "true" : "false",
            sExtraJsonFields ? sExtraJsonFields : "",
            xwork__local_host_process_exit_kind_name(tExitInfo.iKind),
            tExitInfo.iExitCode,
            tExitInfo.iSignal,
            xwork__local_host_process_exit_stage_name(tExitInfo.iStage),
            tExitInfo.iOsError,
            xwork__local_host_process_stop_reason_name(tExitInfo.iStopReason),
            tExitInfo.bTimedOut ? "true" : "false",
            tExitInfo.bCancelled ? "true" : "false",
            sEscapedErrorKind,
            sEscapedErrorMessage
        );
    }
    if ( !sResultText ) {
        iStatus = XWORK_ERROR_NO_MEMORY;
        goto cleanup;
    }

    iStatus = xwork__local_host_set_result(
        pHost,
        sResultText,
        sVisibleSummary
            ? sVisibleSummary
            : (bOk ? "process.terminal session ok" : "process.terminal session failed"),
        pResult
    );

cleanup:
    free(sEscapedSessionId);
    free(sEscapedSessionName);
    free(sEscapedCommand);
    free(sEscapedCwd);
    free(sEscapedOutputText);
    free(sEscapedErrorKind);
    free(sEscapedErrorMessage);
    free(sResultText);
    return iStatus;
}

static xwork_status xwork__local_host_set_terminal_write_result(
    xwork_local_host *pHost,
    const char *sSessionId,
    const char *sSessionName,
    size_t iSessionIndex,
    size_t iBytesWritten,
    bool bWriteEof,
    bool bStdinClosed,
    bool bHasStateSnapshot,
    const char *sCommand,
    const char *sResolvedCwd,
    const char *sOutputText,
    size_t iOutputBytes,
    size_t iTerminalCols,
    size_t iTerminalRows,
    bool bTerminalOutputCaptured,
    bool bRunning,
    bool bEventStreamDone,
    uint64 iNextAfterSeq,
    uint64 iEventEndSeq,
    uint32 iEventCount,
    const char *sEventsJson,
    bool bOk,
    const char *sVisibleSummary,
    const char *sErrorKind,
    const char *sErrorMessage,
    xwork_tool_result *pResult
)
{
    char sExtraJson[128];
    char *sEscapedSessionId = NULL;
    char *sEscapedSessionName = NULL;
    char *sEscapedErrorKind = NULL;
    char *sEscapedErrorMessage = NULL;
    char *sResultText = NULL;
    xwork_status iStatus;

    if ( !pHost || !pResult ) {
        return XWORK_ERROR_INVALID_ARGUMENT;
    }

    iStatus = xwork__local_host_json_escape(
        sSessionId ? sSessionId : "",
        &sEscapedSessionId
    );
    if ( iStatus != XWORK_OK ) goto cleanup;
    iStatus = xwork__local_host_json_escape(
        sSessionName ? sSessionName : "",
        &sEscapedSessionName
    );
    if ( iStatus != XWORK_OK ) goto cleanup;

    if ( bHasStateSnapshot ) {
        snprintf(
            sExtraJson,
            sizeof(sExtraJson),
            "\"bytes_written\":%zu,\"write_eof\":%s,\"stdin_closed\":%s,",
            iBytesWritten,
            bWriteEof ? "true" : "false",
            bStdinClosed ? "true" : "false"
        );
        iStatus = xwork__local_host_set_terminal_state_result(
            pHost,
            sSessionId,
            sSessionName,
            sCommand,
            sResolvedCwd,
            iSessionIndex,
            sOutputText,
            iOutputBytes,
            iTerminalCols,
            iTerminalRows,
            bStdinClosed,
            bTerminalOutputCaptured,
            bRunning,
            bEventStreamDone,
            false,
            iNextAfterSeq,
            iEventEndSeq,
            iEventCount,
            sEventsJson,
            sExtraJson,
            NULL,
            bOk,
            sVisibleSummary,
            sErrorKind,
            sErrorMessage,
            pResult
        );
        goto cleanup;
    }

    if ( bOk ) {
        sResultText = xwork__dup_printf(
            "{\"ok\":true,\"session_id\":\"%s\",\"session_name\":\"%s\",\"session_index\":%zu,\"bytes_written\":%zu,"
            "\"write_eof\":%s,\"stdin_closed\":%s}",
            sEscapedSessionId,
            sEscapedSessionName,
            iSessionIndex,
            iBytesWritten,
            bWriteEof ? "true" : "false",
            bStdinClosed ? "true" : "false"
        );
    } else {
        iStatus = xwork__local_host_json_escape(
            sErrorKind ? sErrorKind : "external_failure",
            &sEscapedErrorKind
        );
        if ( iStatus != XWORK_OK ) goto cleanup;
        iStatus = xwork__local_host_json_escape(
            sErrorMessage ? sErrorMessage : "terminal write failed",
            &sEscapedErrorMessage
        );
        if ( iStatus != XWORK_OK ) goto cleanup;
        sResultText = xwork__dup_printf(
            "{\"ok\":false,\"session_id\":\"%s\",\"session_name\":\"%s\",\"session_index\":%zu,\"bytes_written\":%zu,"
            "\"write_eof\":%s,\"stdin_closed\":%s,"
            "\"error_kind\":\"%s\",\"error\":\"%s\"}",
            sEscapedSessionId,
            sEscapedSessionName,
            iSessionIndex,
            iBytesWritten,
            bWriteEof ? "true" : "false",
            bStdinClosed ? "true" : "false",
            sEscapedErrorKind,
            sEscapedErrorMessage
        );
    }
    if ( !sResultText ) {
        iStatus = XWORK_ERROR_NO_MEMORY;
        goto cleanup;
    }

    iStatus = xwork__local_host_set_result(
        pHost,
        sResultText,
        sVisibleSummary
            ? sVisibleSummary
            : (bOk ? "process.terminal_write ok" : "process.terminal_write failed"),
        pResult
    );

cleanup:
    free(sEscapedSessionId);
    free(sEscapedSessionName);
    free(sEscapedErrorKind);
    free(sEscapedErrorMessage);
    free(sResultText);
    return iStatus;
}

static xwork_status xwork__local_host_set_terminal_list_result(
    xwork_local_host *pHost,
    size_t iSessionCount,
    size_t iTotalSessionCount,
    const char *sSessionsJson,
    const char *sExtraJsonFields,
    bool bOk,
    const char *sVisibleSummary,
    const char *sErrorKind,
    const char *sErrorMessage,
    xwork_tool_result *pResult
)
{
    char *sEscapedErrorKind = NULL;
    char *sEscapedErrorMessage = NULL;
    char *sResultText = NULL;
    xwork_status iStatus;

    if ( !pHost || !pResult ) {
        return XWORK_ERROR_INVALID_ARGUMENT;
    }

    if ( bOk ) {
        sResultText = xwork__dup_printf(
            "{\"schema\":\"" XWORK_TERMINAL_INVENTORY_SCHEMA_V1 "\","
            "\"ok\":true,\"session_count\":%zu,\"total_session_count\":%zu,%s\"sessions\":%s}",
            iSessionCount,
            iTotalSessionCount,
            sExtraJsonFields ? sExtraJsonFields : "",
            sSessionsJson ? sSessionsJson : "[]"
        );
    } else {
        iStatus = xwork__local_host_json_escape(
            sErrorKind ? sErrorKind : "external_failure",
            &sEscapedErrorKind
        );
        if ( iStatus != XWORK_OK ) goto cleanup;
        iStatus = xwork__local_host_json_escape(
            sErrorMessage ? sErrorMessage : "failed to list terminal sessions",
            &sEscapedErrorMessage
        );
        if ( iStatus != XWORK_OK ) goto cleanup;
        sResultText = xwork__dup_printf(
            "{\"schema\":\"" XWORK_TERMINAL_INVENTORY_SCHEMA_V1 "\","
            "\"ok\":false,\"session_count\":%zu,\"total_session_count\":%zu,%s\"sessions\":%s,"
            "\"error_kind\":\"%s\",\"error\":\"%s\"}",
            iSessionCount,
            iTotalSessionCount,
            sExtraJsonFields ? sExtraJsonFields : "",
            sSessionsJson ? sSessionsJson : "[]",
            sEscapedErrorKind,
            sEscapedErrorMessage
        );
    }
    if ( !sResultText ) {
        iStatus = XWORK_ERROR_NO_MEMORY;
        goto cleanup;
    }

    iStatus = xwork__local_host_set_result(
        pHost,
        sResultText,
        sVisibleSummary
            ? sVisibleSummary
            : (bOk ? "process.list_terminals ok" : "process.list_terminals failed"),
        pResult
    );

cleanup:
    free(sEscapedErrorKind);
    free(sEscapedErrorMessage);
    free(sResultText);
    return iStatus;
}

static xwork_status xwork__local_host_set_filesystem_read_result(
    xwork_local_host *pHost,
    const char *sPath,
    const char *sResolvedPath,
    const char *sText,
    size_t iOffsetBytes,
    size_t iFileSizeBytes,
    size_t iBytesRead,
    bool bTruncated,
    bool bEof,
    bool bOk,
    const char *sVisibleSummary,
    const char *sErrorKind,
    const char *sErrorMessage,
    xwork_tool_result *pResult
)
{
    char *sEscapedPath = NULL;
    char *sEscapedResolvedPath = NULL;
    char *sEscapedText = NULL;
    char *sEscapedErrorKind = NULL;
    char *sEscapedErrorMessage = NULL;
    char *sOutputText = NULL;
    xwork_status iStatus;

    if ( !pHost || !pResult ) {
        return XWORK_ERROR_INVALID_ARGUMENT;
    }

    iStatus = xwork__local_host_json_escape(sPath ? sPath : "", &sEscapedPath);
    if ( iStatus != XWORK_OK ) goto cleanup;
    iStatus = xwork__local_host_json_escape(
        sResolvedPath ? sResolvedPath : "",
        &sEscapedResolvedPath
    );
    if ( iStatus != XWORK_OK ) goto cleanup;
    iStatus = xwork__local_host_json_escape(sText ? sText : "", &sEscapedText);
    if ( iStatus != XWORK_OK ) goto cleanup;

    if ( bOk ) {
        sOutputText = xwork__dup_printf(
            "{\"ok\":true,\"path\":\"%s\",\"resolved_path\":\"%s\",\"text\":\"%s\","
            "\"offset_bytes\":%zu,\"file_size_bytes\":%zu,\"bytes_read\":%zu,"
            "\"next_offset_bytes\":%zu,\"remaining_bytes\":%zu,"
            "\"truncated\":%s,\"eof\":%s}",
            sEscapedPath,
            sEscapedResolvedPath,
            sEscapedText,
            iOffsetBytes,
            iFileSizeBytes,
            iBytesRead,
            iOffsetBytes + iBytesRead,
            (iOffsetBytes + iBytesRead <= iFileSizeBytes)
                ? (iFileSizeBytes - (iOffsetBytes + iBytesRead))
                : 0u,
            bTruncated ? "true" : "false",
            bEof ? "true" : "false"
        );
    } else {
        iStatus = xwork__local_host_json_escape(
            sErrorKind ? sErrorKind : "external_failure",
            &sEscapedErrorKind
        );
        if ( iStatus != XWORK_OK ) goto cleanup;
        iStatus = xwork__local_host_json_escape(
            sErrorMessage ? sErrorMessage : "filesystem.read_text failed",
            &sEscapedErrorMessage
        );
        if ( iStatus != XWORK_OK ) goto cleanup;
        sOutputText = xwork__dup_printf(
            "{\"ok\":false,\"path\":\"%s\",\"resolved_path\":\"%s\",\"text\":\"%s\","
            "\"offset_bytes\":%zu,\"file_size_bytes\":%zu,\"bytes_read\":%zu,"
            "\"next_offset_bytes\":%zu,\"remaining_bytes\":%zu,"
            "\"truncated\":%s,\"eof\":%s,"
            "\"error_kind\":\"%s\",\"error\":\"%s\"}",
            sEscapedPath,
            sEscapedResolvedPath,
            sEscapedText,
            iOffsetBytes,
            iFileSizeBytes,
            iBytesRead,
            iOffsetBytes + iBytesRead,
            (iOffsetBytes + iBytesRead <= iFileSizeBytes)
                ? (iFileSizeBytes - (iOffsetBytes + iBytesRead))
                : 0u,
            bTruncated ? "true" : "false",
            bEof ? "true" : "false",
            sEscapedErrorKind,
            sEscapedErrorMessage
        );
    }
    if ( !sOutputText ) {
        iStatus = XWORK_ERROR_NO_MEMORY;
        goto cleanup;
    }

    iStatus = xwork__local_host_set_result(
        pHost,
        sOutputText,
        sVisibleSummary ? sVisibleSummary : (bOk ? "filesystem.read_text ok" : "filesystem.read_text failed"),
        pResult
    );

cleanup:
    free(sEscapedPath);
    free(sEscapedResolvedPath);
    free(sEscapedText);
    free(sEscapedErrorKind);
    free(sEscapedErrorMessage);
    free(sOutputText);
    return iStatus;
}

static xwork_status xwork__local_host_set_editor_buffer_result(
    xwork_local_host *pHost,
    const char *sOperation,
    const xwork__local_editor_buffer *pBuffer,
    bool bChanged,
    const char *sVisibleSummary,
    xwork_tool_result *pResult
)
{
    char *sEscapedOperation = NULL;
    char *sEscapedBufferId = NULL;
    char *sEscapedPath = NULL;
    char *sEscapedResolvedPath = NULL;
    char *sEscapedText = NULL;
    char *sOutputText = NULL;
    xwork_status iStatus;

    if ( !pHost || !pBuffer || !pResult ) {
        return XWORK_ERROR_INVALID_ARGUMENT;
    }

    iStatus = xwork__local_host_json_escape(sOperation ? sOperation : "", &sEscapedOperation);
    if ( iStatus != XWORK_OK ) goto cleanup;
    iStatus = xwork__local_host_json_escape(pBuffer->sBufferId ? pBuffer->sBufferId : "", &sEscapedBufferId);
    if ( iStatus != XWORK_OK ) goto cleanup;
    iStatus = xwork__local_host_json_escape(pBuffer->sPath ? pBuffer->sPath : "", &sEscapedPath);
    if ( iStatus != XWORK_OK ) goto cleanup;
    iStatus = xwork__local_host_json_escape(pBuffer->sResolvedPath ? pBuffer->sResolvedPath : "", &sEscapedResolvedPath);
    if ( iStatus != XWORK_OK ) goto cleanup;
    iStatus = xwork__local_host_json_escape(pBuffer->sText ? pBuffer->sText : "", &sEscapedText);
    if ( iStatus != XWORK_OK ) goto cleanup;

    sOutputText = xwork__dup_printf(
        "{\"ok\":true,\"operation\":\"%s\",\"buffer_id\":\"%s\","
        "\"path\":\"%s\",\"resolved_path\":\"%s\",\"dirty\":%s,"
        "\"selection_start\":%llu,\"selection_end\":%llu,"
        "\"changed\":%s,\"text_size_bytes\":%llu,\"text\":\"%s\"}",
        sEscapedOperation,
        sEscapedBufferId,
        sEscapedPath,
        sEscapedResolvedPath,
        pBuffer->bDirty ? "true" : "false",
        (unsigned long long)pBuffer->iSelectionStart,
        (unsigned long long)pBuffer->iSelectionEnd,
        bChanged ? "true" : "false",
        (unsigned long long)(pBuffer->sText ? strlen(pBuffer->sText) : 0u),
        sEscapedText
    );
    if ( !sOutputText ) {
        iStatus = XWORK_ERROR_NO_MEMORY;
        goto cleanup;
    }

    iStatus = xwork__local_host_set_result(
        pHost,
        sOutputText,
        sVisibleSummary ? sVisibleSummary : "editor buffer operation ok",
        pResult
    );

cleanup:
    free(sEscapedOperation);
    free(sEscapedBufferId);
    free(sEscapedPath);
    free(sEscapedResolvedPath);
    free(sEscapedText);
    free(sOutputText);
    return iStatus;
}

static xwork_status xwork__local_host_set_editor_error_result(
    xwork_local_host *pHost,
    const char *sOperation,
    const char *sBufferId,
    const char *sPath,
    const char *sResolvedPath,
    const char *sErrorKind,
    const char *sErrorMessage,
    const char *sVisibleSummary,
    xwork_tool_result *pResult
)
{
    char *sEscapedBufferId = NULL;
    char *sEscapedPath = NULL;
    char *sEscapedResolvedPath = NULL;
    char *sEscapedErrorKind = NULL;
    char *sEscapedErrorMessage = NULL;
    char *sOutputText = NULL;
    xwork_status iStatus;

    if ( !pHost || !pResult ) {
        return XWORK_ERROR_INVALID_ARGUMENT;
    }

    iStatus = xwork__local_host_json_escape(sBufferId ? sBufferId : "", &sEscapedBufferId);
    if ( iStatus != XWORK_OK ) goto cleanup;
    iStatus = xwork__local_host_json_escape(sPath ? sPath : "", &sEscapedPath);
    if ( iStatus != XWORK_OK ) goto cleanup;
    iStatus = xwork__local_host_json_escape(sResolvedPath ? sResolvedPath : "", &sEscapedResolvedPath);
    if ( iStatus != XWORK_OK ) goto cleanup;
    iStatus = xwork__local_host_json_escape(sErrorKind ? sErrorKind : "external_failure", &sEscapedErrorKind);
    if ( iStatus != XWORK_OK ) goto cleanup;
    iStatus = xwork__local_host_json_escape(sErrorMessage ? sErrorMessage : "editor buffer operation failed", &sEscapedErrorMessage);
    if ( iStatus != XWORK_OK ) goto cleanup;

    sOutputText = xwork__dup_printf(
        "{\"ok\":false,\"operation\":\"%s\",\"buffer_id\":\"%s\","
        "\"path\":\"%s\",\"resolved_path\":\"%s\",\"error_kind\":\"%s\","
        "\"error\":\"%s\"}",
        sOperation ? sOperation : "",
        sEscapedBufferId,
        sEscapedPath,
        sEscapedResolvedPath,
        sEscapedErrorKind,
        sEscapedErrorMessage
    );
    if ( !sOutputText ) {
        iStatus = XWORK_ERROR_NO_MEMORY;
        goto cleanup;
    }

    iStatus = xwork__local_host_set_result(
        pHost,
        sOutputText,
        sVisibleSummary ? sVisibleSummary : "editor buffer operation failed",
        pResult
    );

cleanup:
    free(sEscapedBufferId);
    free(sEscapedPath);
    free(sEscapedResolvedPath);
    free(sEscapedErrorKind);
    free(sEscapedErrorMessage);
    free(sOutputText);
    return iStatus;
}

static xwork_status xwork__local_host_set_filesystem_write_result(
    xwork_local_host *pHost,
    const char *sPath,
    const char *sResolvedPath,
    const char *sMode,
    bool bCreateDirs,
    size_t iBytesWritten,
    bool bOk,
    const char *sVisibleSummary,
    const char *sErrorKind,
    const char *sErrorMessage,
    xwork_tool_result *pResult
)
{
    char *sEscapedPath = NULL;
    char *sEscapedResolvedPath = NULL;
    char *sEscapedMode = NULL;
    char *sEscapedErrorKind = NULL;
    char *sEscapedErrorMessage = NULL;
    char *sOutputText = NULL;
    xwork_status iStatus;

    if ( !pHost || !pResult ) {
        return XWORK_ERROR_INVALID_ARGUMENT;
    }

    iStatus = xwork__local_host_json_escape(sPath ? sPath : "", &sEscapedPath);
    if ( iStatus != XWORK_OK ) goto cleanup;
    iStatus = xwork__local_host_json_escape(
        sResolvedPath ? sResolvedPath : "",
        &sEscapedResolvedPath
    );
    if ( iStatus != XWORK_OK ) goto cleanup;
    iStatus = xwork__local_host_json_escape(
        sMode ? sMode : "overwrite",
        &sEscapedMode
    );
    if ( iStatus != XWORK_OK ) goto cleanup;

    if ( bOk ) {
        sOutputText = xwork__dup_printf(
            "{\"ok\":true,\"path\":\"%s\",\"resolved_path\":\"%s\",\"mode\":\"%s\","
            "\"create_dirs\":%s,\"bytes_written\":%llu}",
            sEscapedPath,
            sEscapedResolvedPath,
            sEscapedMode,
            bCreateDirs ? "true" : "false",
            (unsigned long long)iBytesWritten
        );
    } else {
        iStatus = xwork__local_host_json_escape(
            sErrorKind ? sErrorKind : "external_failure",
            &sEscapedErrorKind
        );
        if ( iStatus != XWORK_OK ) goto cleanup;
        iStatus = xwork__local_host_json_escape(
            sErrorMessage ? sErrorMessage : "filesystem.write_text failed",
            &sEscapedErrorMessage
        );
        if ( iStatus != XWORK_OK ) goto cleanup;
        sOutputText = xwork__dup_printf(
            "{\"ok\":false,\"path\":\"%s\",\"resolved_path\":\"%s\",\"mode\":\"%s\","
            "\"create_dirs\":%s,\"bytes_written\":%llu,"
            "\"error_kind\":\"%s\",\"error\":\"%s\"}",
            sEscapedPath,
            sEscapedResolvedPath,
            sEscapedMode,
            bCreateDirs ? "true" : "false",
            (unsigned long long)iBytesWritten,
            sEscapedErrorKind,
            sEscapedErrorMessage
        );
    }
    if ( !sOutputText ) {
        iStatus = XWORK_ERROR_NO_MEMORY;
        goto cleanup;
    }

    iStatus = xwork__local_host_set_result(
        pHost,
        sOutputText,
        sVisibleSummary ? sVisibleSummary : (bOk ? "filesystem.write_text ok" : "filesystem.write_text failed"),
        pResult
    );

cleanup:
    free(sEscapedPath);
    free(sEscapedResolvedPath);
    free(sEscapedMode);
    free(sEscapedErrorKind);
    free(sEscapedErrorMessage);
    free(sOutputText);
    return iStatus;
}

static xwork_status xwork__local_host_set_filesystem_stat_result(
    xwork_local_host *pHost,
    const char *sPath,
    const char *sResolvedPath,
    const char *sType,
    size_t iSizeBytes,
    long long iMtimeUnix,
    bool bOk,
    const char *sVisibleSummary,
    const char *sErrorKind,
    const char *sErrorMessage,
    xwork_tool_result *pResult
)
{
    char *sEscapedPath = NULL;
    char *sEscapedResolvedPath = NULL;
    char *sEscapedType = NULL;
    char *sEscapedErrorKind = NULL;
    char *sEscapedErrorMessage = NULL;
    char *sOutputText = NULL;
    xwork_status iStatus;

    if ( !pHost || !pResult ) {
        return XWORK_ERROR_INVALID_ARGUMENT;
    }

    iStatus = xwork__local_host_json_escape(sPath ? sPath : "", &sEscapedPath);
    if ( iStatus != XWORK_OK ) goto cleanup;
    iStatus = xwork__local_host_json_escape(
        sResolvedPath ? sResolvedPath : "",
        &sEscapedResolvedPath
    );
    if ( iStatus != XWORK_OK ) goto cleanup;
    iStatus = xwork__local_host_json_escape(sType ? sType : "other", &sEscapedType);
    if ( iStatus != XWORK_OK ) goto cleanup;

    if ( bOk ) {
        sOutputText = xwork__dup_printf(
            "{\"ok\":true,\"path\":\"%s\",\"resolved_path\":\"%s\","
            "\"exists\":true,\"type\":\"%s\",\"size_bytes\":%llu,"
            "\"mtime_unix\":%lld}",
            sEscapedPath,
            sEscapedResolvedPath,
            sEscapedType,
            (unsigned long long)iSizeBytes,
            iMtimeUnix
        );
    } else {
        iStatus = xwork__local_host_json_escape(
            sErrorKind ? sErrorKind : "external_failure",
            &sEscapedErrorKind
        );
        if ( iStatus != XWORK_OK ) goto cleanup;
        iStatus = xwork__local_host_json_escape(
            sErrorMessage ? sErrorMessage : "filesystem.stat failed",
            &sEscapedErrorMessage
        );
        if ( iStatus != XWORK_OK ) goto cleanup;
        sOutputText = xwork__dup_printf(
            "{\"ok\":false,\"path\":\"%s\",\"resolved_path\":\"%s\","
            "\"exists\":false,\"type\":\"%s\",\"size_bytes\":0,"
            "\"mtime_unix\":0,\"error_kind\":\"%s\",\"error\":\"%s\"}",
            sEscapedPath,
            sEscapedResolvedPath,
            sEscapedType,
            sEscapedErrorKind,
            sEscapedErrorMessage
        );
    }
    if ( !sOutputText ) {
        iStatus = XWORK_ERROR_NO_MEMORY;
        goto cleanup;
    }

    iStatus = xwork__local_host_set_result(
        pHost,
        sOutputText,
        sVisibleSummary ? sVisibleSummary : (bOk ? "filesystem.stat ok" : "filesystem.stat failed"),
        pResult
    );

cleanup:
    free(sEscapedPath);
    free(sEscapedResolvedPath);
    free(sEscapedType);
    free(sEscapedErrorKind);
    free(sEscapedErrorMessage);
    free(sOutputText);
    return iStatus;
}

static xwork_status xwork__local_host_set_filesystem_scan_result(
    xwork_local_host *pHost,
    const char *sOperationName,
    const char *sPath,
    const char *sResolvedPath,
    const char *sPattern,
    bool bRecursive,
    bool bIncludeHidden,
    size_t iLimit,
    const xwork__local_host_scan_context *pScan,
    bool bOk,
    const char *sVisibleSummary,
    const char *sErrorKind,
    const char *sErrorMessage,
    xwork_tool_result *pResult
)
{
    char *sEscapedPath = NULL;
    char *sEscapedResolvedPath = NULL;
    char *sEscapedPattern = NULL;
    char *sEscapedErrorKind = NULL;
    char *sEscapedErrorMessage = NULL;
    char *sOutputText = NULL;
    const char *sEntriesJson;
    xwork_status iStatus;

    if ( !pHost || !sOperationName || !pResult ) {
        return XWORK_ERROR_INVALID_ARGUMENT;
    }
    (void)sOperationName;

    iStatus = xwork__local_host_json_escape(sPath ? sPath : "", &sEscapedPath);
    if ( iStatus != XWORK_OK ) goto cleanup;
    iStatus = xwork__local_host_json_escape(
        sResolvedPath ? sResolvedPath : "",
        &sEscapedResolvedPath
    );
    if ( iStatus != XWORK_OK ) goto cleanup;
    iStatus = xwork__local_host_json_escape(sPattern ? sPattern : "", &sEscapedPattern);
    if ( iStatus != XWORK_OK ) goto cleanup;

    sEntriesJson = (pScan && pScan->sEntriesJson) ? pScan->sEntriesJson : "";
    if ( bOk ) {
        sOutputText = xwork__dup_printf(
            "{\"ok\":true,\"path\":\"%s\",\"resolved_path\":\"%s\","
            "\"pattern\":\"%s\",\"recursive\":%s,\"include_hidden\":%s,"
            "\"limit\":%llu,\"entry_count\":%llu,\"total_count\":%llu,"
            "\"has_more\":%s,\"entries\":[%s]}",
            sEscapedPath,
            sEscapedResolvedPath,
            sEscapedPattern,
            bRecursive ? "true" : "false",
            bIncludeHidden ? "true" : "false",
            (unsigned long long)iLimit,
            (unsigned long long)(pScan ? pScan->iItemCount : 0u),
            (unsigned long long)(pScan ? pScan->iTotalCount : 0u),
            (pScan && pScan->bHasMore) ? "true" : "false",
            sEntriesJson
        );
    } else {
        iStatus = xwork__local_host_json_escape(
            sErrorKind ? sErrorKind : "external_failure",
            &sEscapedErrorKind
        );
        if ( iStatus != XWORK_OK ) goto cleanup;
        iStatus = xwork__local_host_json_escape(
            sErrorMessage ? sErrorMessage : "filesystem scan failed",
            &sEscapedErrorMessage
        );
        if ( iStatus != XWORK_OK ) goto cleanup;
        sOutputText = xwork__dup_printf(
            "{\"ok\":false,\"path\":\"%s\",\"resolved_path\":\"%s\","
            "\"pattern\":\"%s\",\"recursive\":%s,\"include_hidden\":%s,"
            "\"limit\":%llu,\"entry_count\":0,\"total_count\":0,"
            "\"has_more\":false,\"entries\":[],\"error_kind\":\"%s\","
            "\"error\":\"%s\"}",
            sEscapedPath,
            sEscapedResolvedPath,
            sEscapedPattern,
            bRecursive ? "true" : "false",
            bIncludeHidden ? "true" : "false",
            (unsigned long long)iLimit,
            sEscapedErrorKind,
            sEscapedErrorMessage
        );
    }
    if ( !sOutputText ) {
        iStatus = XWORK_ERROR_NO_MEMORY;
        goto cleanup;
    }

    iStatus = xwork__local_host_set_result(
        pHost,
        sOutputText,
        sVisibleSummary
            ? sVisibleSummary
            : (bOk ? "filesystem scan ok" : "filesystem scan failed"),
        pResult
    );

cleanup:
    free(sEscapedPath);
    free(sEscapedResolvedPath);
    free(sEscapedPattern);
    free(sEscapedErrorKind);
    free(sEscapedErrorMessage);
    free(sOutputText);
    return iStatus;
}

static xwork_status xwork__local_host_set_filesystem_mutation_result(
    xwork_local_host *pHost,
    const char *sOperationName,
    const char *sPath,
    const char *sResolvedPath,
    const char *sTargetPath,
    const char *sResolvedTargetPath,
    const char *sType,
    bool bRecursive,
    bool bDryRun,
    bool bOverwrite,
    bool bCreateDirs,
    bool bChanged,
    bool bExisted,
    bool bOk,
    const char *sVisibleSummary,
    const char *sErrorKind,
    const char *sErrorMessage,
    xwork_tool_result *pResult
)
{
    char *sEscapedOperation = NULL;
    char *sEscapedPath = NULL;
    char *sEscapedResolvedPath = NULL;
    char *sEscapedTargetPath = NULL;
    char *sEscapedResolvedTargetPath = NULL;
    char *sEscapedType = NULL;
    char *sEscapedErrorKind = NULL;
    char *sEscapedErrorMessage = NULL;
    char *sOutputText = NULL;
    xwork_status iStatus;

    if ( !pHost || !sOperationName || !pResult ) {
        return XWORK_ERROR_INVALID_ARGUMENT;
    }

    iStatus = xwork__local_host_json_escape(sOperationName, &sEscapedOperation);
    if ( iStatus != XWORK_OK ) goto cleanup;
    iStatus = xwork__local_host_json_escape(sPath ? sPath : "", &sEscapedPath);
    if ( iStatus != XWORK_OK ) goto cleanup;
    iStatus = xwork__local_host_json_escape(
        sResolvedPath ? sResolvedPath : "",
        &sEscapedResolvedPath
    );
    if ( iStatus != XWORK_OK ) goto cleanup;
    iStatus = xwork__local_host_json_escape(
        sTargetPath ? sTargetPath : "",
        &sEscapedTargetPath
    );
    if ( iStatus != XWORK_OK ) goto cleanup;
    iStatus = xwork__local_host_json_escape(
        sResolvedTargetPath ? sResolvedTargetPath : "",
        &sEscapedResolvedTargetPath
    );
    if ( iStatus != XWORK_OK ) goto cleanup;
    iStatus = xwork__local_host_json_escape(sType ? sType : "other", &sEscapedType);
    if ( iStatus != XWORK_OK ) goto cleanup;

    if ( bOk ) {
        sOutputText = xwork__dup_printf(
            "{\"ok\":true,\"operation\":\"%s\",\"path\":\"%s\","
            "\"resolved_path\":\"%s\",\"target_path\":\"%s\","
            "\"resolved_target_path\":\"%s\",\"type\":\"%s\","
            "\"recursive\":%s,\"dry_run\":%s,\"overwrite\":%s,"
            "\"create_dirs\":%s,\"changed\":%s,\"existed\":%s}",
            sEscapedOperation,
            sEscapedPath,
            sEscapedResolvedPath,
            sEscapedTargetPath,
            sEscapedResolvedTargetPath,
            sEscapedType,
            bRecursive ? "true" : "false",
            bDryRun ? "true" : "false",
            bOverwrite ? "true" : "false",
            bCreateDirs ? "true" : "false",
            bChanged ? "true" : "false",
            bExisted ? "true" : "false"
        );
    } else {
        iStatus = xwork__local_host_json_escape(
            sErrorKind ? sErrorKind : "external_failure",
            &sEscapedErrorKind
        );
        if ( iStatus != XWORK_OK ) goto cleanup;
        iStatus = xwork__local_host_json_escape(
            sErrorMessage ? sErrorMessage : "filesystem operation failed",
            &sEscapedErrorMessage
        );
        if ( iStatus != XWORK_OK ) goto cleanup;
        sOutputText = xwork__dup_printf(
            "{\"ok\":false,\"operation\":\"%s\",\"path\":\"%s\","
            "\"resolved_path\":\"%s\",\"target_path\":\"%s\","
            "\"resolved_target_path\":\"%s\",\"type\":\"%s\","
            "\"recursive\":%s,\"dry_run\":%s,\"overwrite\":%s,"
            "\"create_dirs\":%s,\"changed\":%s,\"existed\":%s,"
            "\"error_kind\":\"%s\",\"error\":\"%s\"}",
            sEscapedOperation,
            sEscapedPath,
            sEscapedResolvedPath,
            sEscapedTargetPath,
            sEscapedResolvedTargetPath,
            sEscapedType,
            bRecursive ? "true" : "false",
            bDryRun ? "true" : "false",
            bOverwrite ? "true" : "false",
            bCreateDirs ? "true" : "false",
            bChanged ? "true" : "false",
            bExisted ? "true" : "false",
            sEscapedErrorKind,
            sEscapedErrorMessage
        );
    }
    if ( !sOutputText ) {
        iStatus = XWORK_ERROR_NO_MEMORY;
        goto cleanup;
    }

    iStatus = xwork__local_host_set_result(
        pHost,
        sOutputText,
        sVisibleSummary
            ? sVisibleSummary
            : (bOk ? "filesystem operation ok" : "filesystem operation failed"),
        pResult
    );

cleanup:
    free(sEscapedOperation);
    free(sEscapedPath);
    free(sEscapedResolvedPath);
    free(sEscapedTargetPath);
    free(sEscapedResolvedTargetPath);
    free(sEscapedType);
    free(sEscapedErrorKind);
    free(sEscapedErrorMessage);
    free(sOutputText);
    return iStatus;
}

static xwork_status xwork__local_host_set_filesystem_patch_result(
    xwork_local_host *pHost,
    const char *sPath,
    const char *sResolvedPath,
    const char *sPatchText,
    size_t iBytesBefore,
    size_t iBytesAfter,
    bool bDryRun,
    bool bChanged,
    bool bOk,
    const char *sVisibleSummary,
    const char *sErrorKind,
    const char *sErrorMessage,
    xwork_tool_result *pResult
)
{
    char *sEscapedPath = NULL;
    char *sEscapedResolvedPath = NULL;
    char *sEscapedPatchText = NULL;
    char *sEscapedErrorKind = NULL;
    char *sEscapedErrorMessage = NULL;
    char *sOutputText = NULL;
    xwork_status iStatus;

    if ( !pHost || !pResult ) {
        return XWORK_ERROR_INVALID_ARGUMENT;
    }

    iStatus = xwork__local_host_json_escape(sPath ? sPath : "", &sEscapedPath);
    if ( iStatus != XWORK_OK ) goto cleanup;
    iStatus = xwork__local_host_json_escape(
        sResolvedPath ? sResolvedPath : "",
        &sEscapedResolvedPath
    );
    if ( iStatus != XWORK_OK ) goto cleanup;
    iStatus = xwork__local_host_json_escape(
        sPatchText ? sPatchText : "",
        &sEscapedPatchText
    );
    if ( iStatus != XWORK_OK ) goto cleanup;

    if ( bOk ) {
        sOutputText = xwork__dup_printf(
            "{\"ok\":true,\"operation\":\"apply_patch\",\"path\":\"%s\","
            "\"resolved_path\":\"%s\",\"dry_run\":%s,\"changed\":%s,"
            "\"bytes_before\":%llu,\"bytes_after\":%llu,"
            "\"patch_text\":\"%s\"}",
            sEscapedPath,
            sEscapedResolvedPath,
            bDryRun ? "true" : "false",
            bChanged ? "true" : "false",
            (unsigned long long)iBytesBefore,
            (unsigned long long)iBytesAfter,
            sEscapedPatchText
        );
    } else {
        iStatus = xwork__local_host_json_escape(
            sErrorKind ? sErrorKind : "external_failure",
            &sEscapedErrorKind
        );
        if ( iStatus != XWORK_OK ) goto cleanup;
        iStatus = xwork__local_host_json_escape(
            sErrorMessage ? sErrorMessage : "filesystem.apply_patch failed",
            &sEscapedErrorMessage
        );
        if ( iStatus != XWORK_OK ) goto cleanup;
        sOutputText = xwork__dup_printf(
            "{\"ok\":false,\"operation\":\"apply_patch\",\"path\":\"%s\","
            "\"resolved_path\":\"%s\",\"dry_run\":%s,\"changed\":%s,"
            "\"bytes_before\":%llu,\"bytes_after\":%llu,"
            "\"patch_text\":\"%s\",\"error_kind\":\"%s\",\"error\":\"%s\"}",
            sEscapedPath,
            sEscapedResolvedPath,
            bDryRun ? "true" : "false",
            bChanged ? "true" : "false",
            (unsigned long long)iBytesBefore,
            (unsigned long long)iBytesAfter,
            sEscapedPatchText,
            sEscapedErrorKind,
            sEscapedErrorMessage
        );
    }
    if ( !sOutputText ) {
        iStatus = XWORK_ERROR_NO_MEMORY;
        goto cleanup;
    }

    iStatus = xwork__local_host_set_result(
        pHost,
        sOutputText,
        sVisibleSummary
            ? sVisibleSummary
            : (bOk ? "filesystem.apply_patch ok" : "filesystem.apply_patch failed"),
        pResult
    );

cleanup:
    free(sEscapedPath);
    free(sEscapedResolvedPath);
    free(sEscapedPatchText);
    free(sEscapedErrorKind);
    free(sEscapedErrorMessage);
    free(sOutputText);
    return iStatus;
}

static xwork_status xwork__local_host_invoke_filesystem(
    xwork_local_host *pHost,
    const char *sRequestJson,
    xwork_tool_result *pResult
)
{
    xvalue tRequest = NULL;
    const char *sPath;
    char *sResolvedPath = NULL;
    char *sText = NULL;
    bool bTruncated = false;
    bool bEof = false;
    size_t iFileSizeBytes = 0u;
    size_t iRequestOffsetBytes = 0u;
    size_t iRequestMaxBytes = 0u;
    size_t iBytesRead = 0u;
    size_t iReadLimit;
    const char *sFailureKind = NULL;
    const char *sFailureSummary = NULL;
    const char *sFailureMessage = NULL;
    xwork_status iStatus;

    if ( !pHost || !pResult ) {
        return XWORK_ERROR_INVALID_ARGUMENT;
    }
    if ( !pHost->bEnableFilesystemReadText ) {
        return XWORK_ERROR_UNSUPPORTED;
    }

    iStatus = xwork__local_host_parse_request_json(sRequestJson, &tRequest);
    if ( iStatus != XWORK_OK ) {
        return iStatus;
    }

    sPath = xwork__local_host_request_get_text(tRequest, "path");
    if ( !sPath || !sPath[0] ) {
        iStatus = XWORK_ERROR_INVALID_ARGUMENT;
        sFailureKind = "invalid_request";
        sFailureSummary = "filesystem.read_text invalid request";
        sFailureMessage = "path is required";
        goto cleanup;
    }

    (void)xwork__local_host_request_get_size(tRequest, "offset_bytes", &iRequestOffsetBytes);
    (void)xwork__local_host_request_get_size(tRequest, "max_bytes", &iRequestMaxBytes);
    iReadLimit = xwork__local_host_effective_limit(
        pHost->iMaxReadBytes ? pHost->iMaxReadBytes : XWORK__LOCAL_HOST_DEFAULT_READ_BYTES,
        iRequestMaxBytes
    );

    sResolvedPath = xwork__local_host_resolve_path(pHost, sPath);
    if ( !sResolvedPath ) {
        iStatus = XWORK_ERROR_NO_MEMORY;
        goto cleanup;
    }
    iStatus = xwork__local_host_check_filesystem_path(
        pHost,
        sResolvedPath,
        &sFailureMessage
    );
    if ( iStatus != XWORK_OK ) {
        sFailureKind = "path_denied";
        sFailureSummary = "filesystem.read_text denied by path policy";
        goto cleanup;
    }

    iStatus = xwork__local_host_read_text_file(
        sResolvedPath,
        iRequestOffsetBytes,
        iReadLimit,
        &sText,
        &iFileSizeBytes,
        &iBytesRead,
        &bTruncated,
        &bEof
    );
    if ( iStatus != XWORK_OK ) {
        if ( iStatus == XWORK_ERROR_NOT_FOUND ) {
            sFailureKind = "not_found";
            sFailureSummary = "filesystem.read_text not found";
            sFailureMessage = "path does not exist";
        } else if ( iStatus == XWORK_ERROR_EXTERNAL_FAILURE ) {
            sFailureKind = "read_failed";
            sFailureSummary = "filesystem.read_text failed";
            sFailureMessage = "failed to read file";
        }
        goto cleanup;
    }

    iStatus = xwork__local_host_set_filesystem_read_result(
        pHost,
        sPath,
        sResolvedPath,
        sText,
        iRequestOffsetBytes,
        iFileSizeBytes,
        iBytesRead,
        bTruncated,
        bEof,
        true,
        "filesystem.read_text ok",
        NULL,
        NULL,
        pResult
    );

cleanup:
    if ( iStatus != XWORK_OK && sFailureKind ) {
        (void)xwork__local_host_set_filesystem_read_result(
            pHost,
            sPath ? sPath : "",
            sResolvedPath ? sResolvedPath : sPath,
            "",
            iRequestOffsetBytes,
            iFileSizeBytes,
            iBytesRead,
            bTruncated,
            bEof,
            false,
            sFailureSummary,
            sFailureKind,
            sFailureMessage,
            pResult
        );
    }
    if ( tRequest ) {
        xvoUnref(tRequest);
    }
    free(sResolvedPath);
    free(sText);
    return iStatus;
}

static xwork_status xwork__local_host_scan_directory(
    const char *sResolvedPath,
    bool bRecursive,
    xwork__local_host_scan_context *pScan
)
{
    if ( !sResolvedPath || !sResolvedPath[0] || !pScan ) {
        return XWORK_ERROR_INVALID_ARGUMENT;
    }
#ifdef _WIN32
    {
        WIN32_FIND_DATAA tFindData;
        HANDLE hFind;
        char *sFindPath;
        xwork_status iStatus = XWORK_OK;

        sFindPath = xwork__local_host_join_path(sResolvedPath, "*");
        if ( !sFindPath ) {
            return XWORK_ERROR_NO_MEMORY;
        }

        hFind = FindFirstFileA(sFindPath, &tFindData);
        free(sFindPath);
        if ( hFind == INVALID_HANDLE_VALUE ) {
            return XWORK_ERROR_EXTERNAL_FAILURE;
        }

        do {
            char *sChildPath;
            bool bIsDirectory;

            if ( strcmp(tFindData.cFileName, ".") == 0 ||
                 strcmp(tFindData.cFileName, "..") == 0 ) {
                continue;
            }

            sChildPath = xwork__local_host_join_path(
                sResolvedPath,
                tFindData.cFileName
            );
            if ( !sChildPath ) {
                iStatus = XWORK_ERROR_NO_MEMORY;
                break;
            }

            bIsDirectory =
                (tFindData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
            iStatus = xwork__local_host_append_scan_entry(
                pScan,
                sChildPath,
                bIsDirectory ? "directory" : "file"
            );
            if ( iStatus == XWORK_OK && bRecursive && bIsDirectory ) {
                iStatus = xwork__local_host_scan_directory(
                    sChildPath,
                    bRecursive,
                    pScan
                );
            }
            free(sChildPath);
            if ( iStatus != XWORK_OK ) {
                break;
            }
        } while ( FindNextFileA(hFind, &tFindData) );

        FindClose(hFind);
        return iStatus;
    }
#else
    {
        DIR *pDir;
        struct dirent *pEntry;
        xwork_status iStatus = XWORK_OK;

        pDir = opendir(sResolvedPath);
        if ( !pDir ) {
            return errno == ENOENT ? XWORK_ERROR_NOT_FOUND : XWORK_ERROR_EXTERNAL_FAILURE;
        }

        while ( (pEntry = readdir(pDir)) != NULL ) {
            char *sChildPath;
            const char *sType = "other";
            size_t iSizeBytes = 0u;
            long long iMtimeUnix = 0;
            bool bIsDirectory;

            if ( strcmp(pEntry->d_name, ".") == 0 ||
                 strcmp(pEntry->d_name, "..") == 0 ) {
                continue;
            }

            sChildPath = xwork__local_host_join_path(sResolvedPath, pEntry->d_name);
            if ( !sChildPath ) {
                iStatus = XWORK_ERROR_NO_MEMORY;
                break;
            }

            iStatus = xwork__local_host_stat_path(
                sChildPath,
                &sType,
                &iSizeBytes,
                &iMtimeUnix
            );
            (void)iSizeBytes;
            (void)iMtimeUnix;
            if ( iStatus == XWORK_OK ) {
                bIsDirectory = strcmp(sType, "directory") == 0;
                iStatus = xwork__local_host_append_scan_entry(
                    pScan,
                    sChildPath,
                    sType
                );
                if ( iStatus == XWORK_OK && bRecursive && bIsDirectory ) {
                    iStatus = xwork__local_host_scan_directory(
                        sChildPath,
                        bRecursive,
                        pScan
                    );
                }
            }
            free(sChildPath);
            if ( iStatus != XWORK_OK ) {
                break;
            }
        }

        closedir(pDir);
        return iStatus;
    }
#endif
}

static xwork_status xwork__local_host_invoke_filesystem_stat(
    xwork_local_host *pHost,
    const char *sRequestJson,
    xwork_tool_result *pResult
)
{
    xvalue tRequest = NULL;
    const char *sPath;
    const char *sType = "other";
    char *sResolvedPath = NULL;
    size_t iSizeBytes = 0u;
    long long iMtimeUnix = 0;
    const char *sFailureKind = NULL;
    const char *sFailureSummary = NULL;
    const char *sFailureMessage = NULL;
    xwork_status iStatus;

    if ( !pHost || !pResult ) {
        return XWORK_ERROR_INVALID_ARGUMENT;
    }
    if ( !pHost->bEnableFilesystemReadText ) {
        return XWORK_ERROR_UNSUPPORTED;
    }

    iStatus = xwork__local_host_parse_request_json(sRequestJson, &tRequest);
    if ( iStatus != XWORK_OK ) {
        return iStatus;
    }

    sPath = xwork__local_host_request_get_text(tRequest, "path");
    if ( !sPath || !sPath[0] ) {
        iStatus = XWORK_ERROR_INVALID_ARGUMENT;
        sFailureKind = "invalid_request";
        sFailureSummary = "filesystem.stat invalid request";
        sFailureMessage = "path is required";
        goto cleanup;
    }

    sResolvedPath = xwork__local_host_resolve_path(pHost, sPath);
    if ( !sResolvedPath ) {
        iStatus = XWORK_ERROR_NO_MEMORY;
        goto cleanup;
    }
    iStatus = xwork__local_host_check_filesystem_path(
        pHost,
        sResolvedPath,
        &sFailureMessage
    );
    if ( iStatus != XWORK_OK ) {
        sFailureKind = "path_denied";
        sFailureSummary = "filesystem.stat denied by path policy";
        goto cleanup;
    }

    iStatus = xwork__local_host_stat_path(
        sResolvedPath,
        &sType,
        &iSizeBytes,
        &iMtimeUnix
    );
    if ( iStatus != XWORK_OK ) {
        if ( iStatus == XWORK_ERROR_NOT_FOUND ) {
            sFailureKind = "not_found";
            sFailureSummary = "filesystem.stat not found";
            sFailureMessage = "path does not exist";
        } else {
            sFailureKind = "stat_failed";
            sFailureSummary = "filesystem.stat failed";
            sFailureMessage = "failed to stat path";
        }
        goto cleanup;
    }

    iStatus = xwork__local_host_set_filesystem_stat_result(
        pHost,
        sPath,
        sResolvedPath,
        sType,
        iSizeBytes,
        iMtimeUnix,
        true,
        "filesystem.stat ok",
        NULL,
        NULL,
        pResult
    );

cleanup:
    if ( iStatus != XWORK_OK && sFailureKind ) {
        (void)xwork__local_host_set_filesystem_stat_result(
            pHost,
            sPath ? sPath : "",
            sResolvedPath ? sResolvedPath : sPath,
            "other",
            0u,
            0,
            false,
            sFailureSummary,
            sFailureKind,
            sFailureMessage,
            pResult
        );
    }
    if ( tRequest ) {
        xvoUnref(tRequest);
    }
    free(sResolvedPath);
    return iStatus;
}

static xwork_status xwork__local_host_invoke_filesystem_scan(
    xwork_local_host *pHost,
    const char *sRequestJson,
    const char *sOperationName,
    bool bDefaultRecursive,
    bool bRequirePattern,
    xwork_tool_result *pResult
)
{
    xvalue tRequest = NULL;
    xwork__local_host_scan_context tScan;
    const char *sPath = ".";
    const char *sPattern = "*";
    const char *sType = "other";
    char *sResolvedPath = NULL;
    size_t iSizeBytes = 0u;
    long long iMtimeUnix = 0;
    size_t iLimit = 0u;
    bool bRecursive = bDefaultRecursive;
    bool bIncludeHidden = false;
    bool bHasRecursive = false;
    bool bHasIncludeHidden = false;
    bool bHasLimit = false;
    const char *sFailureKind = NULL;
    const char *sFailureSummary = NULL;
    const char *sFailureMessage = NULL;
    xwork_status iStatus;

    if ( !pHost || !sOperationName || !pResult ) {
        return XWORK_ERROR_INVALID_ARGUMENT;
    }
    if ( !pHost->bEnableFilesystemReadText ) {
        return XWORK_ERROR_UNSUPPORTED;
    }

    memset(&tScan, 0, sizeof(tScan));
    tScan.iStatus = XWORK_OK;

    iStatus = xwork__local_host_parse_request_json(sRequestJson, &tRequest);
    if ( iStatus != XWORK_OK ) {
        return iStatus;
    }

    if ( xwork__local_host_request_get_text(tRequest, "path") ) {
        sPath = xwork__local_host_request_get_text(tRequest, "path");
    }
    if ( !sPath || !sPath[0] ) {
        iStatus = XWORK_ERROR_INVALID_ARGUMENT;
        sFailureKind = "invalid_request";
        sFailureSummary = "filesystem scan invalid request";
        sFailureMessage = "path must not be empty";
        goto cleanup;
    }

    if ( xwork__local_host_request_get_text(tRequest, "pattern") ) {
        sPattern = xwork__local_host_request_get_text(tRequest, "pattern");
    } else if ( xwork__local_host_request_get_text(tRequest, "glob") ) {
        sPattern = xwork__local_host_request_get_text(tRequest, "glob");
    }
    if ( bRequirePattern && (!sPattern || !sPattern[0]) ) {
        iStatus = XWORK_ERROR_INVALID_ARGUMENT;
        sFailureKind = "invalid_request";
        sFailureSummary = "filesystem.glob invalid request";
        sFailureMessage = "pattern is required";
        goto cleanup;
    }

    iStatus = xwork__local_host_request_get_bool(
        tRequest,
        "recursive",
        &bHasRecursive,
        &bRecursive
    );
    if ( iStatus != XWORK_OK ) {
        sFailureKind = "invalid_request";
        sFailureSummary = "filesystem scan invalid request";
        sFailureMessage = "recursive must be boolean";
        goto cleanup;
    }
    iStatus = xwork__local_host_request_get_bool(
        tRequest,
        "include_hidden",
        &bHasIncludeHidden,
        &bIncludeHidden
    );
    if ( iStatus != XWORK_OK ) {
        sFailureKind = "invalid_request";
        sFailureSummary = "filesystem scan invalid request";
        sFailureMessage = "include_hidden must be boolean";
        goto cleanup;
    }
    (void)bHasRecursive;
    (void)bHasIncludeHidden;

    iStatus = xwork__local_host_request_get_positive_size_strict(
        tRequest,
        "limit",
        &bHasLimit,
        &iLimit
    );
    if ( iStatus != XWORK_OK ) {
        sFailureKind = "invalid_request";
        sFailureSummary = "filesystem scan invalid request";
        sFailureMessage = "limit must be a positive integer";
        goto cleanup;
    }

    sResolvedPath = xwork__local_host_resolve_path(pHost, sPath);
    if ( !sResolvedPath ) {
        iStatus = XWORK_ERROR_NO_MEMORY;
        goto cleanup;
    }
    iStatus = xwork__local_host_check_filesystem_path(
        pHost,
        sResolvedPath,
        &sFailureMessage
    );
    if ( iStatus != XWORK_OK ) {
        sFailureKind = "path_denied";
        sFailureSummary = "filesystem scan denied by path policy";
        goto cleanup;
    }

    iStatus = xwork__local_host_stat_path(
        sResolvedPath,
        &sType,
        &iSizeBytes,
        &iMtimeUnix
    );
    (void)iSizeBytes;
    (void)iMtimeUnix;
    if ( iStatus != XWORK_OK ) {
        if ( iStatus == XWORK_ERROR_NOT_FOUND ) {
            sFailureKind = "not_found";
            sFailureSummary = "filesystem scan not found";
            sFailureMessage = "path does not exist";
        } else {
            sFailureKind = "stat_failed";
            sFailureSummary = "filesystem scan failed";
            sFailureMessage = "failed to stat path";
        }
        goto cleanup;
    }
    if ( strcmp(sType, "directory") != 0 ) {
        iStatus = XWORK_ERROR_INVALID_ARGUMENT;
        sFailureKind = "not_directory";
        sFailureSummary = "filesystem scan invalid request";
        sFailureMessage = "path must be a directory";
        goto cleanup;
    }

    tScan.sRootPath = sResolvedPath;
    tScan.sPattern = sPattern;
    tScan.bUsePattern = bRequirePattern;
    tScan.bIncludeHidden = bIncludeHidden;
    tScan.iLimit = bHasLimit ? iLimit : 0u;

    iStatus = xwork__local_host_scan_directory(sResolvedPath, bRecursive, &tScan);
    if ( iStatus != XWORK_OK ) {
        sFailureKind = "scan_failed";
        sFailureSummary = "filesystem scan failed";
        sFailureMessage = "failed to scan directory";
        goto cleanup;
    }

    iStatus = xwork__local_host_set_filesystem_scan_result(
        pHost,
        sOperationName,
        sPath,
        sResolvedPath,
        bRequirePattern ? sPattern : "",
        bRecursive,
        bIncludeHidden,
        tScan.iLimit,
        &tScan,
        true,
        strcmp(sOperationName, XWORK_HOST_FILESYSTEM_GLOB) == 0
            ? "filesystem.glob ok"
            : "filesystem.list ok",
        NULL,
        NULL,
        pResult
    );

cleanup:
    if ( iStatus != XWORK_OK && sFailureKind ) {
        (void)xwork__local_host_set_filesystem_scan_result(
            pHost,
            sOperationName,
            sPath ? sPath : "",
            sResolvedPath ? sResolvedPath : sPath,
            bRequirePattern ? sPattern : "",
            bRecursive,
            bIncludeHidden,
            bHasLimit ? iLimit : 0u,
            NULL,
            false,
            sFailureSummary,
            sFailureKind,
            sFailureMessage,
            pResult
        );
    }
    if ( tRequest ) {
        xvoUnref(tRequest);
    }
    free(tScan.sEntriesJson);
    free(sResolvedPath);
    return iStatus;
}

static xwork_status xwork__local_host_invoke_filesystem_write_text(
    xwork_local_host *pHost,
    const char *sRequestJson,
    xwork_tool_result *pResult
)
{
    xvalue tRequest = NULL;
    const char *sPath;
    const char *sText;
    const char *sMode;
    char *sResolvedPath = NULL;
    size_t iBytesWritten = 0u;
    bool bAppend = false;
    bool bCreate = false;
    bool bHasCreateDirs = false;
    bool bCreateDirs = false;
    const char *sModeText = "overwrite";
    const char *sFailureKind = NULL;
    const char *sFailureSummary = NULL;
    const char *sFailureMessage = NULL;
    xwork_status iStatus;

    if ( !pHost || !pResult ) {
        return XWORK_ERROR_INVALID_ARGUMENT;
    }
    if ( !pHost->bEnableFilesystemWriteText ) {
        return XWORK_ERROR_UNSUPPORTED;
    }

    iStatus = xwork__local_host_parse_request_json(sRequestJson, &tRequest);
    if ( iStatus != XWORK_OK ) {
        return iStatus;
    }

    sPath = xwork__local_host_request_get_text(tRequest, "path");
    sText = xwork__local_host_request_get_text(tRequest, "text");
    sMode = xwork__local_host_request_get_text(tRequest, "mode");
    if ( !sPath || !sPath[0] || !sText ) {
        iStatus = XWORK_ERROR_INVALID_ARGUMENT;
        sFailureKind = "invalid_request";
        sFailureSummary = "filesystem.write_text invalid request";
        sFailureMessage = "path and text are required";
        goto cleanup;
    }
    if ( sMode && sMode[0] ) {
        if ( strcmp(sMode, "append") == 0 ) {
            bAppend = true;
            sModeText = "append";
        } else if ( strcmp(sMode, "create") == 0 ) {
            bCreate = true;
            sModeText = "create";
        } else if ( strcmp(sMode, "overwrite") != 0 ) {
            iStatus = XWORK_ERROR_INVALID_ARGUMENT;
            sFailureKind = "invalid_request";
            sFailureSummary = "filesystem.write_text invalid request";
            sFailureMessage = "mode must be overwrite, append, or create";
            goto cleanup;
        }
    }
    iStatus = xwork__local_host_request_get_bool(
        tRequest,
        "create_dirs",
        &bHasCreateDirs,
        &bCreateDirs
    );
    if ( iStatus != XWORK_OK ) {
        sFailureKind = "invalid_request";
        sFailureSummary = "filesystem.write_text invalid request";
        sFailureMessage = "create_dirs must be boolean";
        goto cleanup;
    }

    sResolvedPath = xwork__local_host_resolve_path(pHost, sPath);
    if ( !sResolvedPath ) {
        iStatus = XWORK_ERROR_NO_MEMORY;
        goto cleanup;
    }
    iStatus = xwork__local_host_check_filesystem_path(
        pHost,
        sResolvedPath,
        &sFailureMessage
    );
    if ( iStatus != XWORK_OK ) {
        sFailureKind = "path_denied";
        sFailureSummary = "filesystem.write_text denied by path policy";
        goto cleanup;
    }
    if ( bHasCreateDirs && bCreateDirs ) {
        iStatus = xwork__local_host_ensure_parent_directories(sResolvedPath);
        if ( iStatus != XWORK_OK ) {
            sFailureKind = "create_dirs_failed";
            sFailureSummary = "filesystem.write_text failed (create_dirs)";
            sFailureMessage = "failed to create parent directories";
            goto cleanup;
        }
    } else {
        char *sParentDirectory = xwork__local_host_parent_directory(sResolvedPath);

        if ( sParentDirectory ) {
            if ( !xwork__local_host_directory_exists(sParentDirectory) ) {
                free(sParentDirectory);
                iStatus = XWORK_ERROR_EXTERNAL_FAILURE;
                sFailureKind = "parent_not_found";
                sFailureSummary = "filesystem.write_text failed (parent directory not found)";
                sFailureMessage = "parent directory does not exist";
                goto cleanup;
            }
            free(sParentDirectory);
        }
    }
    if ( bCreate && xwork__local_host_text_file_exists(sResolvedPath) ) {
        iStatus = XWORK_ERROR_EXTERNAL_FAILURE;
        sFailureKind = "already_exists";
        sFailureSummary = "filesystem.write_text failed (target exists)";
        sFailureMessage = "path already exists";
        goto cleanup;
    }

    iStatus = xwork__local_host_write_text_file(
        sResolvedPath,
        sText,
        bAppend,
        &iBytesWritten
    );
    if ( iStatus != XWORK_OK ) {
        if ( iStatus == XWORK_ERROR_EXTERNAL_FAILURE ) {
            sFailureKind = "write_failed";
            sFailureSummary = "filesystem.write_text failed";
            sFailureMessage = "failed to write file";
        }
        goto cleanup;
    }

    iStatus = xwork__local_host_set_filesystem_write_result(
        pHost,
        sPath,
        sResolvedPath,
        sModeText,
        bHasCreateDirs && bCreateDirs,
        iBytesWritten,
        true,
        "filesystem.write_text ok",
        NULL,
        NULL,
        pResult
    );

cleanup:
    if ( iStatus != XWORK_OK && sFailureKind ) {
        (void)xwork__local_host_set_filesystem_write_result(
            pHost,
            sPath ? sPath : "",
            sResolvedPath ? sResolvedPath : sPath,
            sModeText,
            bHasCreateDirs && bCreateDirs,
            iBytesWritten,
            false,
            sFailureSummary,
            sFailureKind,
            sFailureMessage,
            pResult
        );
    }
    if ( tRequest ) {
        xvoUnref(tRequest);
    }
    free(sResolvedPath);
    return iStatus;
}

static xwork_status xwork__local_host_invoke_filesystem_mkdir(
    xwork_local_host *pHost,
    const char *sRequestJson,
    xwork_tool_result *pResult
)
{
    xvalue tRequest = NULL;
    const char *sPath;
    const char *sType = "directory";
    char *sResolvedPath = NULL;
    size_t iSizeBytes = 0u;
    long long iMtimeUnix = 0;
    bool bRecursive = false;
    bool bExistOk = true;
    bool bDryRun = false;
    bool bHasRecursive = false;
    bool bHasExistOk = false;
    bool bHasDryRun = false;
    bool bExisted = false;
    bool bChanged = false;
    const char *sFailureKind = NULL;
    const char *sFailureSummary = NULL;
    const char *sFailureMessage = NULL;
    xwork_status iStatus;

    if ( !pHost || !pResult ) {
        return XWORK_ERROR_INVALID_ARGUMENT;
    }
    if ( !pHost->bEnableFilesystemWriteText ) {
        return XWORK_ERROR_UNSUPPORTED;
    }

    iStatus = xwork__local_host_parse_request_json(sRequestJson, &tRequest);
    if ( iStatus != XWORK_OK ) {
        return iStatus;
    }

    sPath = xwork__local_host_request_get_text(tRequest, "path");
    if ( !sPath || !sPath[0] ) {
        iStatus = XWORK_ERROR_INVALID_ARGUMENT;
        sFailureKind = "invalid_request";
        sFailureSummary = "filesystem.mkdir invalid request";
        sFailureMessage = "path is required";
        goto cleanup;
    }
    iStatus = xwork__local_host_request_get_bool(tRequest, "recursive", &bHasRecursive, &bRecursive);
    if ( iStatus != XWORK_OK ) {
        sFailureKind = "invalid_request";
        sFailureSummary = "filesystem.mkdir invalid request";
        sFailureMessage = "recursive must be boolean";
        goto cleanup;
    }
    iStatus = xwork__local_host_request_get_bool(tRequest, "exist_ok", &bHasExistOk, &bExistOk);
    if ( iStatus != XWORK_OK ) {
        sFailureKind = "invalid_request";
        sFailureSummary = "filesystem.mkdir invalid request";
        sFailureMessage = "exist_ok must be boolean";
        goto cleanup;
    }
    iStatus = xwork__local_host_request_get_bool(tRequest, "dry_run", &bHasDryRun, &bDryRun);
    if ( iStatus != XWORK_OK ) {
        sFailureKind = "invalid_request";
        sFailureSummary = "filesystem.mkdir invalid request";
        sFailureMessage = "dry_run must be boolean";
        goto cleanup;
    }
    (void)bHasRecursive;
    (void)bHasExistOk;
    (void)bHasDryRun;

    sResolvedPath = xwork__local_host_resolve_path(pHost, sPath);
    if ( !sResolvedPath ) {
        iStatus = XWORK_ERROR_NO_MEMORY;
        goto cleanup;
    }
    iStatus = xwork__local_host_check_filesystem_path(
        pHost,
        sResolvedPath,
        &sFailureMessage
    );
    if ( iStatus != XWORK_OK ) {
        sFailureKind = "path_denied";
        sFailureSummary = "filesystem.mkdir denied by path policy";
        goto cleanup;
    }

    iStatus = xwork__local_host_stat_path(sResolvedPath, &sType, &iSizeBytes, &iMtimeUnix);
    (void)iSizeBytes;
    (void)iMtimeUnix;
    if ( iStatus == XWORK_OK ) {
        bExisted = true;
        if ( strcmp(sType, "directory") == 0 && bExistOk ) {
            iStatus = XWORK_OK;
            goto success;
        }
        iStatus = XWORK_ERROR_ALREADY_EXISTS;
        sFailureKind = "already_exists";
        sFailureSummary = "filesystem.mkdir failed (target exists)";
        sFailureMessage = "path already exists";
        goto cleanup;
    }
    if ( iStatus != XWORK_ERROR_NOT_FOUND ) {
        sFailureKind = "stat_failed";
        sFailureSummary = "filesystem.mkdir failed";
        sFailureMessage = "failed to stat path";
        goto cleanup;
    }

    if ( !bDryRun ) {
        iStatus = bRecursive
            ? xwork__local_host_ensure_directory_tree(sResolvedPath)
            : xwork__local_host_create_directory(sResolvedPath);
        if ( iStatus != XWORK_OK ) {
            sFailureKind = "mkdir_failed";
            sFailureSummary = "filesystem.mkdir failed";
            sFailureMessage = bRecursive
                ? "failed to create directory tree"
                : "failed to create directory";
            goto cleanup;
        }
        bChanged = true;
    }
    iStatus = XWORK_OK;

success:
    iStatus = xwork__local_host_set_filesystem_mutation_result(
        pHost,
        XWORK_HOST_FILESYSTEM_MKDIR,
        sPath,
        sResolvedPath,
        "",
        "",
        "directory",
        bRecursive,
        bDryRun,
        false,
        bRecursive,
        bChanged,
        bExisted,
        true,
        "filesystem.mkdir ok",
        NULL,
        NULL,
        pResult
    );

cleanup:
    if ( iStatus != XWORK_OK && sFailureKind ) {
        (void)xwork__local_host_set_filesystem_mutation_result(
            pHost,
            XWORK_HOST_FILESYSTEM_MKDIR,
            sPath ? sPath : "",
            sResolvedPath ? sResolvedPath : sPath,
            "",
            "",
            sType,
            bRecursive,
            bDryRun,
            false,
            bRecursive,
            bChanged,
            bExisted,
            false,
            sFailureSummary,
            sFailureKind,
            sFailureMessage,
            pResult
        );
    }
    if ( tRequest ) {
        xvoUnref(tRequest);
    }
    free(sResolvedPath);
    return iStatus;
}

static xwork_status xwork__local_host_invoke_filesystem_move(
    xwork_local_host *pHost,
    const char *sRequestJson,
    xwork_tool_result *pResult
)
{
    xvalue tRequest = NULL;
    const char *sPath;
    const char *sTargetPath;
    const char *sType = "other";
    char *sResolvedPath = NULL;
    char *sResolvedTargetPath = NULL;
    size_t iSizeBytes = 0u;
    long long iMtimeUnix = 0;
    bool bOverwrite = false;
    bool bCreateDirs = false;
    bool bRecursive = false;
    bool bDryRun = false;
    bool bHasOverwrite = false;
    bool bHasCreateDirs = false;
    bool bHasRecursive = false;
    bool bHasDryRun = false;
    bool bTargetExisted = false;
    bool bChanged = false;
    const char *sFailureKind = NULL;
    const char *sFailureSummary = NULL;
    const char *sFailureMessage = NULL;
    xwork_status iStatus;

    if ( !pHost || !pResult ) {
        return XWORK_ERROR_INVALID_ARGUMENT;
    }
    if ( !pHost->bEnableFilesystemWriteText ) {
        return XWORK_ERROR_UNSUPPORTED;
    }

    iStatus = xwork__local_host_parse_request_json(sRequestJson, &tRequest);
    if ( iStatus != XWORK_OK ) {
        return iStatus;
    }

    sPath = xwork__local_host_request_get_text(tRequest, "source_path");
    if ( !sPath ) sPath = xwork__local_host_request_get_text(tRequest, "from");
    if ( !sPath ) sPath = xwork__local_host_request_get_text(tRequest, "path");
    sTargetPath = xwork__local_host_request_get_text(tRequest, "target_path");
    if ( !sTargetPath ) sTargetPath = xwork__local_host_request_get_text(tRequest, "to");
    if ( !sTargetPath ) sTargetPath = xwork__local_host_request_get_text(tRequest, "destination_path");
    if ( !sPath || !sPath[0] || !sTargetPath || !sTargetPath[0] ) {
        iStatus = XWORK_ERROR_INVALID_ARGUMENT;
        sFailureKind = "invalid_request";
        sFailureSummary = "filesystem.move invalid request";
        sFailureMessage = "source_path/path and target_path are required";
        goto cleanup;
    }

    iStatus = xwork__local_host_request_get_bool(tRequest, "overwrite", &bHasOverwrite, &bOverwrite);
    if ( iStatus != XWORK_OK ) {
        sFailureKind = "invalid_request";
        sFailureSummary = "filesystem.move invalid request";
        sFailureMessage = "overwrite must be boolean";
        goto cleanup;
    }
    iStatus = xwork__local_host_request_get_bool(tRequest, "create_dirs", &bHasCreateDirs, &bCreateDirs);
    if ( iStatus != XWORK_OK ) {
        sFailureKind = "invalid_request";
        sFailureSummary = "filesystem.move invalid request";
        sFailureMessage = "create_dirs must be boolean";
        goto cleanup;
    }
    iStatus = xwork__local_host_request_get_bool(tRequest, "recursive", &bHasRecursive, &bRecursive);
    if ( iStatus != XWORK_OK ) {
        sFailureKind = "invalid_request";
        sFailureSummary = "filesystem.move invalid request";
        sFailureMessage = "recursive must be boolean";
        goto cleanup;
    }
    iStatus = xwork__local_host_request_get_bool(tRequest, "dry_run", &bHasDryRun, &bDryRun);
    if ( iStatus != XWORK_OK ) {
        sFailureKind = "invalid_request";
        sFailureSummary = "filesystem.move invalid request";
        sFailureMessage = "dry_run must be boolean";
        goto cleanup;
    }
    (void)bHasOverwrite;
    (void)bHasCreateDirs;
    (void)bHasRecursive;
    (void)bHasDryRun;

    sResolvedPath = xwork__local_host_resolve_path(pHost, sPath);
    sResolvedTargetPath = xwork__local_host_resolve_path(pHost, sTargetPath);
    if ( !sResolvedPath || !sResolvedTargetPath ) {
        iStatus = XWORK_ERROR_NO_MEMORY;
        goto cleanup;
    }
    iStatus = xwork__local_host_check_filesystem_path(
        pHost,
        sResolvedPath,
        &sFailureMessage
    );
    if ( iStatus != XWORK_OK ) {
        sFailureKind = "path_denied";
        sFailureSummary = "filesystem.move denied by path policy";
        goto cleanup;
    }
    iStatus = xwork__local_host_check_filesystem_path(
        pHost,
        sResolvedTargetPath,
        &sFailureMessage
    );
    if ( iStatus != XWORK_OK ) {
        sFailureKind = "path_denied";
        sFailureSummary = "filesystem.move denied by path policy";
        goto cleanup;
    }

    iStatus = xwork__local_host_stat_path(sResolvedPath, &sType, &iSizeBytes, &iMtimeUnix);
    (void)iSizeBytes;
    (void)iMtimeUnix;
    if ( iStatus != XWORK_OK ) {
        sFailureKind = (iStatus == XWORK_ERROR_NOT_FOUND) ? "not_found" : "stat_failed";
        sFailureSummary = (iStatus == XWORK_ERROR_NOT_FOUND)
            ? "filesystem.move not found"
            : "filesystem.move failed";
        sFailureMessage = (iStatus == XWORK_ERROR_NOT_FOUND)
            ? "source path does not exist"
            : "failed to stat source path";
        goto cleanup;
    }

    {
        const char *sTargetType = "other";
        size_t iTargetSizeBytes = 0u;
        long long iTargetMtimeUnix = 0;
        xwork_status iTargetStatus = xwork__local_host_stat_path(
            sResolvedTargetPath,
            &sTargetType,
            &iTargetSizeBytes,
            &iTargetMtimeUnix
        );
        (void)iTargetSizeBytes;
        (void)iTargetMtimeUnix;
        if ( iTargetStatus == XWORK_OK ) {
            bTargetExisted = true;
            if ( !bOverwrite ) {
                iStatus = XWORK_ERROR_ALREADY_EXISTS;
                sFailureKind = "already_exists";
                sFailureSummary = "filesystem.move failed (target exists)";
                sFailureMessage = "target path already exists";
                goto cleanup;
            }
            if ( !bDryRun ) {
                iStatus = xwork__local_host_delete_path(
                    sResolvedTargetPath,
                    bRecursive,
                    NULL
                );
                if ( iStatus != XWORK_OK ) {
                    sFailureKind = (iStatus == XWORK_ERROR_INVALID_STATE)
                        ? "target_not_empty"
                        : "overwrite_failed";
                    sFailureSummary = "filesystem.move failed (overwrite)";
                    sFailureMessage = (iStatus == XWORK_ERROR_INVALID_STATE)
                        ? "target directory is not empty; recursive is required"
                        : "failed to remove existing target";
                    goto cleanup;
                }
            }
        } else if ( iTargetStatus != XWORK_ERROR_NOT_FOUND ) {
            iStatus = iTargetStatus;
            sFailureKind = "target_stat_failed";
            sFailureSummary = "filesystem.move failed";
            sFailureMessage = "failed to stat target path";
            goto cleanup;
        }
    }

    if ( bCreateDirs ) {
        iStatus = xwork__local_host_ensure_parent_directories(sResolvedTargetPath);
        if ( iStatus != XWORK_OK ) {
            sFailureKind = "create_dirs_failed";
            sFailureSummary = "filesystem.move failed (create_dirs)";
            sFailureMessage = "failed to create target parent directories";
            goto cleanup;
        }
    } else {
        char *sParentDirectory = xwork__local_host_parent_directory(sResolvedTargetPath);

        if ( sParentDirectory ) {
            if ( !xwork__local_host_directory_exists(sParentDirectory) ) {
                free(sParentDirectory);
                iStatus = XWORK_ERROR_NOT_FOUND;
                sFailureKind = "parent_not_found";
                sFailureSummary = "filesystem.move failed (parent directory not found)";
                sFailureMessage = "target parent directory does not exist";
                goto cleanup;
            }
            free(sParentDirectory);
        }
    }

    if ( !bDryRun ) {
        errno = 0;
        if ( rename(sResolvedPath, sResolvedTargetPath) != 0 ) {
            iStatus = XWORK_ERROR_EXTERNAL_FAILURE;
            sFailureKind = "move_failed";
            sFailureSummary = "filesystem.move failed";
            sFailureMessage = "failed to move path";
            goto cleanup;
        }
        bChanged = true;
    }
    iStatus = xwork__local_host_set_filesystem_mutation_result(
        pHost,
        XWORK_HOST_FILESYSTEM_MOVE,
        sPath,
        sResolvedPath,
        sTargetPath,
        sResolvedTargetPath,
        sType,
        bRecursive,
        bDryRun,
        bOverwrite,
        bCreateDirs,
        bChanged,
        bTargetExisted,
        true,
        "filesystem.move ok",
        NULL,
        NULL,
        pResult
    );

cleanup:
    if ( iStatus != XWORK_OK && sFailureKind ) {
        (void)xwork__local_host_set_filesystem_mutation_result(
            pHost,
            XWORK_HOST_FILESYSTEM_MOVE,
            sPath ? sPath : "",
            sResolvedPath ? sResolvedPath : sPath,
            sTargetPath ? sTargetPath : "",
            sResolvedTargetPath ? sResolvedTargetPath : sTargetPath,
            sType,
            bRecursive,
            bDryRun,
            bOverwrite,
            bCreateDirs,
            bChanged,
            bTargetExisted,
            false,
            sFailureSummary,
            sFailureKind,
            sFailureMessage,
            pResult
        );
    }
    if ( tRequest ) {
        xvoUnref(tRequest);
    }
    free(sResolvedPath);
    free(sResolvedTargetPath);
    return iStatus;
}

static xwork_status xwork__local_host_invoke_filesystem_delete(
    xwork_local_host *pHost,
    const char *sRequestJson,
    xwork_tool_result *pResult
)
{
    xvalue tRequest = NULL;
    const char *sPath;
    const char *sType = "other";
    char *sResolvedPath = NULL;
    size_t iSizeBytes = 0u;
    long long iMtimeUnix = 0;
    bool bRecursive = false;
    bool bDryRun = false;
    bool bHasRecursive = false;
    bool bHasDryRun = false;
    bool bChanged = false;
    bool bExisted = false;
    const char *sFailureKind = NULL;
    const char *sFailureSummary = NULL;
    const char *sFailureMessage = NULL;
    xwork_status iStatus;

    if ( !pHost || !pResult ) {
        return XWORK_ERROR_INVALID_ARGUMENT;
    }
    if ( !pHost->bEnableFilesystemWriteText ) {
        return XWORK_ERROR_UNSUPPORTED;
    }

    iStatus = xwork__local_host_parse_request_json(sRequestJson, &tRequest);
    if ( iStatus != XWORK_OK ) {
        return iStatus;
    }

    sPath = xwork__local_host_request_get_text(tRequest, "path");
    if ( !sPath || !sPath[0] ) {
        iStatus = XWORK_ERROR_INVALID_ARGUMENT;
        sFailureKind = "invalid_request";
        sFailureSummary = "filesystem.delete invalid request";
        sFailureMessage = "path is required";
        goto cleanup;
    }
    iStatus = xwork__local_host_request_get_bool(tRequest, "recursive", &bHasRecursive, &bRecursive);
    if ( iStatus != XWORK_OK ) {
        sFailureKind = "invalid_request";
        sFailureSummary = "filesystem.delete invalid request";
        sFailureMessage = "recursive must be boolean";
        goto cleanup;
    }
    iStatus = xwork__local_host_request_get_bool(tRequest, "dry_run", &bHasDryRun, &bDryRun);
    if ( iStatus != XWORK_OK ) {
        sFailureKind = "invalid_request";
        sFailureSummary = "filesystem.delete invalid request";
        sFailureMessage = "dry_run must be boolean";
        goto cleanup;
    }
    (void)bHasRecursive;
    (void)bHasDryRun;

    sResolvedPath = xwork__local_host_resolve_path(pHost, sPath);
    if ( !sResolvedPath ) {
        iStatus = XWORK_ERROR_NO_MEMORY;
        goto cleanup;
    }
    iStatus = xwork__local_host_check_filesystem_path(
        pHost,
        sResolvedPath,
        &sFailureMessage
    );
    if ( iStatus != XWORK_OK ) {
        sFailureKind = "path_denied";
        sFailureSummary = "filesystem.delete denied by path policy";
        goto cleanup;
    }

    iStatus = xwork__local_host_stat_path(sResolvedPath, &sType, &iSizeBytes, &iMtimeUnix);
    (void)iSizeBytes;
    (void)iMtimeUnix;
    if ( iStatus != XWORK_OK ) {
        sFailureKind = (iStatus == XWORK_ERROR_NOT_FOUND) ? "not_found" : "stat_failed";
        sFailureSummary = (iStatus == XWORK_ERROR_NOT_FOUND)
            ? "filesystem.delete not found"
            : "filesystem.delete failed";
        sFailureMessage = (iStatus == XWORK_ERROR_NOT_FOUND)
            ? "path does not exist"
            : "failed to stat path";
        goto cleanup;
    }
    bExisted = true;

    if ( !bDryRun ) {
        iStatus = xwork__local_host_delete_path(sResolvedPath, bRecursive, &sType);
        if ( iStatus != XWORK_OK ) {
            sFailureKind = (iStatus == XWORK_ERROR_INVALID_STATE)
                ? "directory_not_empty"
                : "delete_failed";
            sFailureSummary = "filesystem.delete failed";
            sFailureMessage = (iStatus == XWORK_ERROR_INVALID_STATE)
                ? "directory is not empty; recursive is required"
                : "failed to delete path";
            goto cleanup;
        }
        bChanged = true;
    }

    iStatus = xwork__local_host_set_filesystem_mutation_result(
        pHost,
        XWORK_HOST_FILESYSTEM_DELETE,
        sPath,
        sResolvedPath,
        "",
        "",
        sType,
        bRecursive,
        bDryRun,
        false,
        false,
        bChanged,
        bExisted,
        true,
        "filesystem.delete ok",
        NULL,
        NULL,
        pResult
    );

cleanup:
    if ( iStatus != XWORK_OK && sFailureKind ) {
        (void)xwork__local_host_set_filesystem_mutation_result(
            pHost,
            XWORK_HOST_FILESYSTEM_DELETE,
            sPath ? sPath : "",
            sResolvedPath ? sResolvedPath : sPath,
            "",
            "",
            sType,
            bRecursive,
            bDryRun,
            false,
            false,
            bChanged,
            bExisted,
            false,
            sFailureSummary,
            sFailureKind,
            sFailureMessage,
            pResult
        );
    }
    if ( tRequest ) {
        xvoUnref(tRequest);
    }
    free(sResolvedPath);
    return iStatus;
}

static xwork_status xwork__local_host_invoke_filesystem_apply_patch(
    xwork_local_host *pHost,
    const char *sRequestJson,
    xwork_tool_result *pResult
)
{
    xvalue tRequest = NULL;
    const char *sPath;
    const char *sOldText;
    const char *sNewText;
    char *sResolvedPath = NULL;
    char *sCurrentText = NULL;
    char *sNextText = NULL;
    char *sPatchText = NULL;
    const char *sMatch;
    size_t iFileSizeBytes = 0u;
    size_t iBytesRead = 0u;
    size_t iBytesWritten = 0u;
    size_t iPrefixLength;
    size_t iOldLength;
    size_t iNewLength;
    size_t iSuffixLength;
    bool bTruncated = false;
    bool bEof = false;
    bool bDryRun = false;
    bool bHasDryRun = false;
    bool bChanged = false;
    const char *sFailureKind = NULL;
    const char *sFailureSummary = NULL;
    const char *sFailureMessage = NULL;
    xwork_status iStatus;

    if ( !pHost || !pResult ) {
        return XWORK_ERROR_INVALID_ARGUMENT;
    }
    if ( !pHost->bEnableFilesystemWriteText || !pHost->bEnableFilesystemReadText ) {
        return XWORK_ERROR_UNSUPPORTED;
    }

    iStatus = xwork__local_host_parse_request_json(sRequestJson, &tRequest);
    if ( iStatus != XWORK_OK ) {
        return iStatus;
    }

    sPath = xwork__local_host_request_get_text(tRequest, "path");
    sOldText = xwork__local_host_request_get_text(tRequest, "old_text");
    sNewText = xwork__local_host_request_get_text(tRequest, "new_text");
    if ( !sPath || !sPath[0] || !sOldText || !sOldText[0] || !sNewText ) {
        iStatus = XWORK_ERROR_INVALID_ARGUMENT;
        sFailureKind = "invalid_request";
        sFailureSummary = "filesystem.apply_patch invalid request";
        sFailureMessage = "path, non-empty old_text, and new_text are required";
        goto cleanup;
    }
    iStatus = xwork__local_host_request_get_bool(tRequest, "dry_run", &bHasDryRun, &bDryRun);
    if ( iStatus != XWORK_OK ) {
        sFailureKind = "invalid_request";
        sFailureSummary = "filesystem.apply_patch invalid request";
        sFailureMessage = "dry_run must be boolean";
        goto cleanup;
    }
    (void)bHasDryRun;

    sResolvedPath = xwork__local_host_resolve_path(pHost, sPath);
    if ( !sResolvedPath ) {
        iStatus = XWORK_ERROR_NO_MEMORY;
        goto cleanup;
    }
    iStatus = xwork__local_host_check_filesystem_path(
        pHost,
        sResolvedPath,
        &sFailureMessage
    );
    if ( iStatus != XWORK_OK ) {
        sFailureKind = "path_denied";
        sFailureSummary = "filesystem.apply_patch denied by path policy";
        goto cleanup;
    }

    iStatus = xwork__local_host_read_text_file(
        sResolvedPath,
        0u,
        (size_t)-1,
        &sCurrentText,
        &iFileSizeBytes,
        &iBytesRead,
        &bTruncated,
        &bEof
    );
    if ( iStatus != XWORK_OK ) {
        if ( iStatus == XWORK_ERROR_NOT_FOUND ) {
            sFailureKind = "not_found";
            sFailureSummary = "filesystem.apply_patch not found";
            sFailureMessage = "path does not exist";
        } else {
            sFailureKind = "read_failed";
            sFailureSummary = "filesystem.apply_patch failed";
            sFailureMessage = "failed to read file";
        }
        goto cleanup;
    }
    (void)iBytesRead;
    (void)bTruncated;
    (void)bEof;

    sMatch = strstr(sCurrentText, sOldText);
    if ( !sMatch ) {
        iStatus = XWORK_ERROR_INVALID_STATE;
        sFailureKind = "conflict";
        sFailureSummary = "filesystem.apply_patch conflict";
        sFailureMessage = "old_text was not found in file";
        goto cleanup;
    }

    iPrefixLength = (size_t)(sMatch - sCurrentText);
    iOldLength = strlen(sOldText);
    iNewLength = strlen(sNewText);
    iSuffixLength = strlen(sMatch + iOldLength);
    sNextText = (char *)malloc(iPrefixLength + iNewLength + iSuffixLength + 1u);
    if ( !sNextText ) {
        iStatus = XWORK_ERROR_NO_MEMORY;
        goto cleanup;
    }
    memcpy(sNextText, sCurrentText, iPrefixLength);
    memcpy(sNextText + iPrefixLength, sNewText, iNewLength);
    memcpy(
        sNextText + iPrefixLength + iNewLength,
        sMatch + iOldLength,
        iSuffixLength + 1u
    );

    iStatus = xwork__local_host_build_replacement_patch_text(
        sPath,
        sOldText,
        sNewText,
        &sPatchText
    );
    if ( iStatus != XWORK_OK ) {
        goto cleanup;
    }

    if ( !bDryRun ) {
        iStatus = xwork__local_host_write_text_file(
            sResolvedPath,
            sNextText,
            false,
            &iBytesWritten
        );
        if ( iStatus != XWORK_OK ) {
            sFailureKind = "write_failed";
            sFailureSummary = "filesystem.apply_patch failed";
            sFailureMessage = "failed to write patched file";
            goto cleanup;
        }
        (void)iBytesWritten;
        bChanged = true;
    }

    iStatus = xwork__local_host_set_filesystem_patch_result(
        pHost,
        sPath,
        sResolvedPath,
        sPatchText,
        iFileSizeBytes,
        strlen(sNextText),
        bDryRun,
        bChanged,
        true,
        "filesystem.apply_patch ok",
        NULL,
        NULL,
        pResult
    );

cleanup:
    if ( iStatus != XWORK_OK && sFailureKind ) {
        if ( !sPatchText && sPath && sOldText && sNewText ) {
            (void)xwork__local_host_build_replacement_patch_text(
                sPath,
                sOldText,
                sNewText,
                &sPatchText
            );
        }
        (void)xwork__local_host_set_filesystem_patch_result(
            pHost,
            sPath ? sPath : "",
            sResolvedPath ? sResolvedPath : sPath,
            sPatchText ? sPatchText : "",
            iFileSizeBytes,
            sNextText ? strlen(sNextText) : 0u,
            bDryRun,
            bChanged,
            false,
            sFailureSummary,
            sFailureKind,
            sFailureMessage,
            pResult
        );
    }
    if ( tRequest ) {
        xvoUnref(tRequest);
    }
    free(sResolvedPath);
    free(sCurrentText);
    free(sNextText);
    free(sPatchText);
    return iStatus;
}

static xwork_status xwork__local_host_invoke_process(
    xwork_local_host *pHost,
    const char *sRequestJson,
    const xwork_host_invoke_context *pContext,
    xwork_tool_result *pResult
)
{
    xvalue tRequest = NULL;
    xvalue tEnvList = NULL;
    const char *sCommand;
    const char *sRequestedCwd = NULL;
    const char *sInputText = NULL;
    char *sResolvedCwd = NULL;
    char **psEnvEntries = NULL;
    char *sStdout = NULL;
    char *sStderr = NULL;
    char *sDynamicFailureSummary = NULL;
    char *sDynamicFailureMessage = NULL;
    xwork__local_host_process_capture tStdoutCapture = {0};
    xwork__local_host_process_capture tStderrCapture = {0};
    xwork__local_host_process_capture_set tCaptureSet = {0};
    xprocessconfig tProcessConfig = {0};
    xprocessevents tProcessEvents = {0};
    xprocessexitinfo tExitInfo = {0};
    xprocessevent *pProcessEventsSnapshot = NULL;
    xprocess *pProcess = NULL;
    bool bHasAllowNonZeroExit = false;
    bool bAllowNonZeroExit = false;
    bool bHasIncludeEvents = false;
    bool bIncludeEvents = false;
    bool bHasUseTerminal = false;
    bool bUseTerminal = false;
    bool bHasTimeoutMs = false;
    bool bTimedOut = false;
    bool bCancelled = false;
    bool bTruncated = false;
    bool bHasMergeStderr = false;
    bool bMergeStderr = true;
    bool bStdoutTruncated = false;
    bool bStderrTruncated = false;
    int iTimeoutStopReason = XPROC_STOP_INTERRUPT;
    int iObservedStopReason = XPROC_STOP_NONE;
    size_t iRequestMaxBytes = 0u;
    size_t iTimeoutMs = 0u;
    size_t iCaptureLimit;
    size_t iEnvCount = 0u;
    size_t iRequestedEnvCount = 0u;
    size_t iStdinBytes = 0u;
    size_t iTerminalCols = 0u;
    size_t iTerminalRows = 0u;
    bool bHasTerminalCols = false;
    bool bHasTerminalRows = false;
    uint32 iProcessEventCount = 0u;
    int64 iBytesWritten = 0;
    int iWaitResult = XRT_WAIT_OK;
    int iExitCode = -1;
    const char *sFailureKind = NULL;
    const char *sFailureSummary = NULL;
    const char *sFailureMessage = NULL;
    const char *sTimeoutStopText = NULL;
    char *sEventsJson = NULL;
    xwork_status iStatus;

    if ( !pHost || !pResult ) {
        return XWORK_ERROR_INVALID_ARGUMENT;
    }
    if ( !pHost->bEnableProcessExec ) {
        return XWORK_ERROR_UNSUPPORTED;
    }

    iStatus = xwork__local_host_parse_request_json(sRequestJson, &tRequest);
    if ( iStatus != XWORK_OK ) {
        return iStatus;
    }

    sCommand = xwork__local_host_request_get_text(tRequest, "command");
    if ( !sCommand || !sCommand[0] ) {
        iStatus = XWORK_ERROR_INVALID_ARGUMENT;
        sFailureKind = "invalid_request";
        sFailureSummary = "process.exec invalid request";
        sFailureMessage = "command is required";
        goto cleanup;
    }
    sRequestedCwd = xwork__local_host_request_get_text(tRequest, "cwd");
    if ( sRequestedCwd && !sRequestedCwd[0] ) {
        iStatus = XWORK_ERROR_INVALID_ARGUMENT;
        sFailureKind = "invalid_request";
        sFailureSummary = "process.exec invalid request";
        sFailureMessage = "cwd must not be empty";
        goto cleanup;
    }
    iStatus = xwork__local_host_check_command_policy(
        pHost,
        sCommand,
        &sFailureKind,
        &sFailureMessage
    );
    if ( iStatus != XWORK_OK ) {
        sFailureSummary = "process.exec denied by command policy";
        goto cleanup;
    }
    sInputText = xwork__local_host_request_get_text(tRequest, "stdin_text");
    if ( sInputText &&
         pHost->iMaxProcessInputBytes > 0u &&
         strlen(sInputText) > pHost->iMaxProcessInputBytes ) {
        iStatus = XWORK_ERROR_INVALID_ARGUMENT;
        sFailureKind = "invalid_request";
        sFailureSummary = "process.exec invalid request (stdin_text exceeds limit)";
        sFailureMessage = "stdin_text exceeds max_process_input_bytes";
        goto cleanup;
    }
    iStatus = xwork__local_host_request_get_bool(
        tRequest,
        "allow_nonzero_exit",
        &bHasAllowNonZeroExit,
        &bAllowNonZeroExit
    );
    if ( iStatus != XWORK_OK ) {
        sFailureKind = "invalid_request";
        sFailureSummary = "process.exec invalid request";
        sFailureMessage = "allow_nonzero_exit must be boolean";
        goto cleanup;
    }
    iStatus = xwork__local_host_request_get_bool(
        tRequest,
        "include_events",
        &bHasIncludeEvents,
        &bIncludeEvents
    );
    if ( iStatus != XWORK_OK ) {
        sFailureKind = "invalid_request";
        sFailureSummary = "process.exec invalid request";
        sFailureMessage = "include_events must be boolean";
        goto cleanup;
    }
    iStatus = xwork__local_host_request_get_bool(
        tRequest,
        "use_terminal",
        &bHasUseTerminal,
        &bUseTerminal
    );
    if ( iStatus != XWORK_OK ) {
        sFailureKind = "invalid_request";
        sFailureSummary = "process.exec invalid request";
        sFailureMessage = "use_terminal must be boolean";
        goto cleanup;
    }
    iStatus = xwork__local_host_request_get_bool(
        tRequest,
        "merge_stderr",
        &bHasMergeStderr,
        &bMergeStderr
    );
    if ( iStatus != XWORK_OK ) {
        sFailureKind = "invalid_request";
        sFailureSummary = "process.exec invalid request";
        sFailureMessage = "merge_stderr must be boolean";
        goto cleanup;
    }
    iStatus = xwork__local_host_request_get_positive_size_strict(
        tRequest,
        "terminal_cols",
        &bHasTerminalCols,
        &iTerminalCols
    );
    if ( iStatus != XWORK_OK ) {
        sFailureKind = "invalid_request";
        sFailureSummary = "process.exec invalid request";
        sFailureMessage = "terminal_cols must be a positive integer";
        goto cleanup;
    }
    iStatus = xwork__local_host_request_get_positive_size_strict(
        tRequest,
        "terminal_rows",
        &bHasTerminalRows,
        &iTerminalRows
    );
    if ( iStatus != XWORK_OK ) {
        sFailureKind = "invalid_request";
        sFailureSummary = "process.exec invalid request";
        sFailureMessage = "terminal_rows must be a positive integer";
        goto cleanup;
    }
    if ( !bUseTerminal && (bHasTerminalCols || bHasTerminalRows) ) {
        iStatus = XWORK_ERROR_INVALID_ARGUMENT;
        sFailureKind = "invalid_request";
        sFailureSummary = "process.exec invalid request";
        sFailureMessage = "terminal_cols/terminal_rows require use_terminal:true";
        goto cleanup;
    }
    if ( bUseTerminal && bHasMergeStderr && !bMergeStderr ) {
        iStatus = XWORK_ERROR_INVALID_ARGUMENT;
        sFailureKind = "invalid_request";
        sFailureSummary = "process.exec invalid request";
        sFailureMessage = "merge_stderr=false is unsupported with use_terminal:true";
        goto cleanup;
    }
    if ( bUseTerminal && !xrtProcessTerminalSupported() ) {
        iStatus = XWORK_ERROR_UNSUPPORTED;
        sFailureKind = "unsupported";
        sFailureSummary = "process.exec terminal mode unsupported";
        sFailureMessage = "terminal mode is not supported on this platform";
        goto cleanup;
    }
    if ( bUseTerminal ) {
        if ( !bHasTerminalCols ) {
            iTerminalCols = XWORK__LOCAL_HOST_DEFAULT_TERMINAL_COLS;
        }
        if ( !bHasTerminalRows ) {
            iTerminalRows = XWORK__LOCAL_HOST_DEFAULT_TERMINAL_ROWS;
        }
    } else {
        iTerminalCols = 0u;
        iTerminalRows = 0u;
    }
    if ( iTerminalCols > 0xffffffffu || iTerminalRows > 0xffffffffu ) {
        iStatus = XWORK_ERROR_INVALID_ARGUMENT;
        sFailureKind = "invalid_request";
        sFailureSummary = "process.exec invalid request";
        sFailureMessage = "terminal_cols/terminal_rows exceed max supported value";
        goto cleanup;
    }
    iStatus = xwork__local_host_request_get_positive_size_strict(
        tRequest,
        "timeout_ms",
        &bHasTimeoutMs,
        &iTimeoutMs
    );
    if ( iStatus != XWORK_OK ) {
        sFailureKind = "invalid_request";
        sFailureSummary = "process.exec invalid request";
        sFailureMessage = "timeout_ms must be a positive integer";
        goto cleanup;
    }
    if ( bHasTimeoutMs && iTimeoutMs > 0xffffffffu ) {
        iStatus = XWORK_ERROR_INVALID_ARGUMENT;
        sFailureKind = "invalid_request";
        sFailureSummary = "process.exec invalid request";
        sFailureMessage = "timeout_ms exceeds max supported value";
        goto cleanup;
    }
    sTimeoutStopText = xwork__local_host_request_get_text(tRequest, "timeout_stop");
    iStatus = xwork__local_host_parse_process_stop_reason(
        sTimeoutStopText,
        &iTimeoutStopReason
    );
    if ( iStatus != XWORK_OK ) {
        sFailureKind = "invalid_request";
        sFailureSummary = "process.exec invalid request";
        sFailureMessage =
            "timeout_stop must be one of interrupt, terminate, kill, kill_tree";
        goto cleanup;
    }

    (void)xwork__local_host_request_get_size(tRequest, "max_output_bytes", &iRequestMaxBytes);
    iCaptureLimit = xwork__local_host_effective_limit(
        pHost->iMaxProcessOutputBytes,
        iRequestMaxBytes
    );
    iStatus = xwork__local_host_request_get_list(tRequest, "env", &tEnvList);
    if ( iStatus != XWORK_OK ) {
        sFailureKind = "invalid_request";
        sFailureSummary = "process.exec invalid request";
        sFailureMessage = "env must be a list of KEY=VALUE strings";
        goto cleanup;
    }
    iRequestedEnvCount = xwork__local_host_list_count(tEnvList);
    iStatus = xwork__local_host_build_process_env_entries(
        tEnvList,
        pHost->iMaxProcessEnvEntries,
        &psEnvEntries,
        &iEnvCount
    );
    if ( iStatus != XWORK_OK ) {
        if ( iStatus == XWORK_ERROR_INVALID_ARGUMENT ) {
            sFailureKind = "invalid_request";
            if ( pHost->iMaxProcessEnvEntries > 0u &&
                 iRequestedEnvCount > pHost->iMaxProcessEnvEntries ) {
                sFailureSummary = "process.exec invalid request (env limit exceeded)";
                sFailureMessage = "env exceeds max_process_env_entries";
            } else {
                sFailureSummary = "process.exec invalid request";
                sFailureMessage = "env entries are invalid";
            }
        }
        goto cleanup;
    }

    if ( sRequestedCwd ) {
        sResolvedCwd = xwork__local_host_resolve_path(pHost, sRequestedCwd);
        if ( !sResolvedCwd ) {
            iStatus = XWORK_ERROR_NO_MEMORY;
            goto cleanup;
        }
    } else if ( pHost->sDefaultWorkingDirectory && pHost->sDefaultWorkingDirectory[0] ) {
        sResolvedCwd = xwork__dup_cstr(pHost->sDefaultWorkingDirectory);
        if ( !sResolvedCwd ) {
            iStatus = XWORK_ERROR_NO_MEMORY;
            goto cleanup;
        }
    }

    if ( xwork_host_invoke_context_should_cancel(pContext, "before_process_spawn") ) {
        iStatus = XWORK_ERROR_CANCELLED;
        bCancelled = true;
        sFailureKind = "cancelled";
        sFailureSummary = "process.exec cancelled";
        sFailureMessage = "process execution was cancelled before start";
        goto cleanup;
    }

    xwork__local_host_process_capture_init(&tStdoutCapture, iCaptureLimit);
    xwork__local_host_process_capture_init(&tStderrCapture, iCaptureLimit);
    tCaptureSet.pStdoutCapture = &tStdoutCapture;
    tCaptureSet.pStderrCapture = &tStderrCapture;
    memset(&tProcessConfig, 0, sizeof(tProcessConfig));
    memset(&tProcessEvents, 0, sizeof(tProcessEvents));
    memset(&tExitInfo, 0, sizeof(tExitInfo));

    xrtProcessConfigInit(&tProcessConfig);
    tProcessEvents.OnStdout = xwork__local_host_process_stdout_cb;
    tProcessEvents.OnStderr = bMergeStderr ? NULL : xwork__local_host_process_stderr_cb;
    tProcessConfig.iTargetKind = XPROC_TARGET_SHELL;
    tProcessConfig.sCommand = (str)sCommand;
    tProcessConfig.sWorkDir =
        (str)((sResolvedCwd && sResolvedCwd[0]) ? sResolvedCwd : NULL);
    tProcessConfig.arrEnv = (str *)psEnvEntries;
    tProcessConfig.iEnvCount = (uint32)iEnvCount;
    tProcessConfig.bMergeStderr = bMergeStderr;
    tProcessConfig.bUseTerminal = bUseTerminal;
    tProcessConfig.bHideWindow = true;
    tProcessConfig.iTerminalCols = (uint32)iTerminalCols;
    tProcessConfig.iTerminalRows = (uint32)iTerminalRows;
    tProcessConfig.iMaxCaptureBytes = iCaptureLimit;
    tProcessConfig.Stdin.iMode = sInputText ? XPROC_STDIO_PIPE : XPROC_STDIO_NULL;
    tProcessConfig.Stdout.iMode = XPROC_STDIO_PIPE;
    tProcessConfig.Stderr.iMode = bMergeStderr ? XPROC_STDIO_NULL : XPROC_STDIO_PIPE;
    tProcessConfig.pEvents = &tProcessEvents;
    tProcessConfig.pUserData = &tCaptureSet;

    pProcess = xrtProcessSpawn(&tProcessConfig);
    if ( !pProcess ) {
        iStatus = XWORK_ERROR_EXTERNAL_FAILURE;
        sFailureKind = "spawn_failed";
        sFailureSummary = "process.exec failed to start";
        sFailureMessage = "failed to start process";
        goto cleanup;
    }

    if ( sInputText ) {
        iStdinBytes = strlen(sInputText);
        iBytesWritten = xrtProcessWriteText(pProcess, (str)sInputText, iStdinBytes);
        if ( iBytesWritten < 0 || (size_t)iBytesWritten != iStdinBytes ) {
            iStatus = XWORK_ERROR_EXTERNAL_FAILURE;
            sFailureKind = "stdin_failed";
            sFailureSummary = "process.exec failed to write stdin";
            sFailureMessage = "failed to write stdin_text";
            iObservedStopReason = xwork__local_host_process_stop_best_effort(
                pProcess,
                XPROC_STOP_INTERRUPT
            );
            goto cleanup;
        }
        if ( !xrtProcessCloseStdin(pProcess) ) {
            iStatus = XWORK_ERROR_EXTERNAL_FAILURE;
            sFailureKind = "stdin_failed";
            sFailureSummary = "process.exec failed to write stdin";
            sFailureMessage = "failed to close stdin";
            iObservedStopReason = xwork__local_host_process_stop_best_effort(
                pProcess,
                XPROC_STOP_INTERRUPT
            );
            goto cleanup;
        }
    }

    iWaitResult = xwork__local_host_process_wait_cancellable(
        pProcess,
        bHasTimeoutMs,
        iTimeoutMs,
        pContext,
        &bCancelled
    );
    if ( bCancelled ) {
        iStatus = XWORK_ERROR_CANCELLED;
        sFailureKind = "cancelled";
        sFailureSummary = "process.exec cancelled";
        sFailureMessage = "process execution was cancelled";
        iObservedStopReason = xwork__local_host_process_stop_best_effort(
            pProcess,
            XPROC_STOP_INTERRUPT
        );
    } else if ( iWaitResult == XRT_WAIT_TIMEOUT ) {
        iStatus = XWORK_ERROR_EXTERNAL_FAILURE;
        bTimedOut = true;
        sFailureKind = "timeout";
        sFailureSummary = "process.exec timed out";
        sFailureMessage = "process exceeded timeout_ms";
        iObservedStopReason = xwork__local_host_process_stop_best_effort(
            pProcess,
            iTimeoutStopReason
        );
    } else if ( iWaitResult == XRT_WAIT_ERROR ) {
        iStatus = XWORK_ERROR_EXTERNAL_FAILURE;
        sFailureKind = "wait_failed";
        sFailureSummary = "process.exec failed while waiting";
        sFailureMessage = "failed while waiting for process";
        iObservedStopReason = xwork__local_host_process_stop_best_effort(
            pProcess,
            XPROC_STOP_INTERRUPT
        );
    }

    if ( pProcess ) {
        (void)xrtProcessGetExitInfo(pProcess, &tExitInfo);
        iExitCode = tExitInfo.iExitCode;
        if ( tExitInfo.iStopReason != XPROC_STOP_NONE ) {
            iObservedStopReason = tExitInfo.iStopReason;
        }
        if ( bHasIncludeEvents && bIncludeEvents ) {
            pProcessEventsSnapshot = xrtProcessReadEventsSince(
                pProcess,
                0u,
                0u,
                &iProcessEventCount,
                NULL
            );
        }
        xrtProcessDestroy(pProcess);
        pProcess = NULL;
    }
    if ( tStdoutCapture.iStatus != XWORK_OK ) {
        iStatus = tStdoutCapture.iStatus;
        goto cleanup;
    }
    if ( tStderrCapture.iStatus != XWORK_OK ) {
        iStatus = tStderrCapture.iStatus;
        goto cleanup;
    }
    sStdout = xwork__local_host_process_capture_take_text(&tStdoutCapture);
    if ( !sStdout ) {
        iStatus = XWORK_ERROR_NO_MEMORY;
        goto cleanup;
    }
    sStderr = xwork__local_host_process_capture_take_text(&tStderrCapture);
    if ( !sStderr ) {
        iStatus = XWORK_ERROR_NO_MEMORY;
        goto cleanup;
    }
    bStdoutTruncated = tStdoutCapture.bTruncated;
    bStderrTruncated = tStderrCapture.bTruncated;
    bTruncated = bStdoutTruncated || bStderrTruncated;
    if ( bHasIncludeEvents && bIncludeEvents ) {
        iStatus = xwork__local_host_build_process_events_json(
            pProcessEventsSnapshot,
            iProcessEventCount,
            sStdout,
            sStderr,
            &sEventsJson
        );
        if ( iStatus != XWORK_OK ) {
            goto cleanup;
        }
    }

    if ( iStatus != XWORK_OK && sFailureKind ) {
        goto cleanup;
    }
    if ( iExitCode != 0 && !(bHasAllowNonZeroExit && bAllowNonZeroExit) ) {
        iStatus = XWORK_ERROR_EXTERNAL_FAILURE;
        sFailureKind = "nonzero_exit";
        sDynamicFailureSummary = xwork__dup_printf(
            "process.exec failed (exit %d)",
            iExitCode
        );
        sDynamicFailureMessage = xwork__dup_printf(
            "process exited with code %d",
            iExitCode
        );
        if ( !sDynamicFailureSummary || !sDynamicFailureMessage ) {
            iStatus = XWORK_ERROR_NO_MEMORY;
            goto cleanup;
        }
        sFailureSummary = sDynamicFailureSummary;
        sFailureMessage = sDynamicFailureMessage;
        goto cleanup;
    }

    iStatus = xwork__local_host_set_process_result(
        pHost,
        sCommand,
        sResolvedCwd,
        sStdout,
        sStderr,
        iExitCode,
        bTruncated,
        bStdoutTruncated,
        bStderrTruncated,
        bMergeStderr,
        bUseTerminal,
        iTerminalCols,
        iTerminalRows,
        iEnvCount,
        iStdinBytes,
        bHasTimeoutMs ? iTimeoutMs : 0u,
        bTimedOut,
        bCancelled,
        iTimeoutStopReason,
        iObservedStopReason,
        bHasAllowNonZeroExit && bAllowNonZeroExit,
        bHasIncludeEvents && bIncludeEvents,
        iProcessEventCount,
        sEventsJson,
        true,
        "process.exec ok",
        NULL,
        NULL,
        pResult
    );

cleanup:
    if ( pProcess ) {
        if ( xrtProcessIsRunning(pProcess) ) {
            iObservedStopReason = xwork__local_host_process_stop_best_effort(
                pProcess,
                iObservedStopReason != XPROC_STOP_NONE
                    ? iObservedStopReason
                    : XPROC_STOP_INTERRUPT
            );
        }
        (void)xrtProcessGetExitInfo(pProcess, &tExitInfo);
        if ( iExitCode < 0 ) {
            iExitCode = tExitInfo.iExitCode;
        }
        if ( tExitInfo.iStopReason != XPROC_STOP_NONE ) {
            iObservedStopReason = tExitInfo.iStopReason;
        }
        if ( bHasIncludeEvents && bIncludeEvents && !pProcessEventsSnapshot ) {
            pProcessEventsSnapshot = xrtProcessReadEventsSince(
                pProcess,
                0u,
                0u,
                &iProcessEventCount,
                NULL
            );
        }
        xrtProcessDestroy(pProcess);
        pProcess = NULL;
    }
    if ( !sStdout && tStdoutCapture.iStatus == XWORK_OK ) {
        sStdout = xwork__local_host_process_capture_take_text(&tStdoutCapture);
        if ( !sStdout && iStatus == XWORK_OK ) {
            iStatus = XWORK_ERROR_NO_MEMORY;
        }
    }
    if ( !sStderr && tStderrCapture.iStatus == XWORK_OK ) {
        sStderr = xwork__local_host_process_capture_take_text(&tStderrCapture);
        if ( !sStderr && iStatus == XWORK_OK ) {
            iStatus = XWORK_ERROR_NO_MEMORY;
        }
    }
    if ( (bHasIncludeEvents && bIncludeEvents) &&
         !sEventsJson &&
         sStdout &&
         sStderr ) {
        xwork_status iEventsStatus = xwork__local_host_build_process_events_json(
            pProcessEventsSnapshot,
            iProcessEventCount,
            sStdout,
            sStderr,
            &sEventsJson
        );
        if ( iEventsStatus != XWORK_OK && iStatus == XWORK_OK ) {
            iStatus = iEventsStatus;
        }
    }
    if ( iStatus != XWORK_OK && sFailureKind ) {
        (void)xwork__local_host_set_process_result(
            pHost,
            sCommand ? sCommand : "",
            sResolvedCwd ? sResolvedCwd : sRequestedCwd,
            sStdout ? sStdout : "",
            sStderr ? sStderr : "",
            iExitCode,
            bTruncated,
            bStdoutTruncated,
            bStderrTruncated,
            bMergeStderr,
            bUseTerminal,
            iTerminalCols,
            iTerminalRows,
            iEnvCount ? iEnvCount : iRequestedEnvCount,
            iStdinBytes,
            bHasTimeoutMs ? iTimeoutMs : 0u,
            bTimedOut,
            bCancelled,
            iTimeoutStopReason,
            iObservedStopReason,
            bHasAllowNonZeroExit && bAllowNonZeroExit,
            bHasIncludeEvents && bIncludeEvents,
            iProcessEventCount,
            sEventsJson,
            false,
            sFailureSummary,
            sFailureKind,
            sFailureMessage,
            pResult
        );
    }
    if ( tRequest ) {
        xvoUnref(tRequest);
    }
    xwork__local_host_process_capture_reset(&tStdoutCapture);
    xwork__local_host_process_capture_reset(&tStderrCapture);
    free(sResolvedCwd);
    xwork__local_host_free_process_env_entries(&psEnvEntries, iEnvCount);
    free(sStdout);
    free(sStderr);
    free(sEventsJson);
    free(sDynamicFailureSummary);
    free(sDynamicFailureMessage);
    xrtFree(pProcessEventsSnapshot);
    return iStatus;
}

static xwork_status xwork__local_host_terminal_session_snapshot(
    xwork__local_host_terminal_session *pSession,
    size_t iAfterSeq,
    size_t iMaxEvents,
    bool *pbTerminalOutputCaptured,
    bool *pbRunning,
    bool *pbEventStreamDone,
    xprocessexitinfo *pExitInfo,
    uint32 *piEventCount,
    uint64 *piNextAfterSeq,
    uint64 *piEventEndSeq,
    char **ppsEventsJson,
    char **ppsOutputText,
    size_t *piOutputBytes
)
{
    xprocesseventreadinfo tReadInfo;
    xprocessevent *pEvents = NULL;
    void *pStdoutData = NULL;
    void *pStderrData = NULL;
    size_t iStdoutSize = 0u;
    size_t iStderrSize = 0u;
    char *sStdout = NULL;
    char *sStderr = NULL;
    char *sEventsJson = NULL;
    char *sOutputText = NULL;
    xwork_status iStatus = XWORK_OK;

    if ( !pSession || !pSession->pProcess ||
         !pbTerminalOutputCaptured || !pbRunning || !pbEventStreamDone ||
         !pExitInfo || !piEventCount || !piNextAfterSeq || !piEventEndSeq ||
         !ppsEventsJson || !ppsOutputText || !piOutputBytes ) {
        return XWORK_ERROR_INVALID_ARGUMENT;
    }

    *pbTerminalOutputCaptured = false;
    *pbRunning = xrtProcessIsRunning(pSession->pProcess);
    *pbEventStreamDone = false;
    memset(pExitInfo, 0, sizeof(*pExitInfo));
    *piEventCount = 0u;
    *piNextAfterSeq = (uint64)iAfterSeq;
    *piEventEndSeq = (uint64)iAfterSeq;
    *ppsEventsJson = NULL;
    *ppsOutputText = NULL;
    *piOutputBytes = 0u;
    memset(&tReadInfo, 0, sizeof(tReadInfo));

    (void)xrtProcessGetExitInfo(pSession->pProcess, pExitInfo);
    pEvents = xrtProcessReadEventsSince(
        pSession->pProcess,
        (uint64)iAfterSeq,
        (iMaxEvents > 0xffffffffu) ? 0xffffffffu : (uint32)iMaxEvents,
        piEventCount,
        &tReadInfo
    );
    *piNextAfterSeq = tReadInfo.iNextSeq;
    *piEventEndSeq = tReadInfo.iEventEndSeq;
    *pbEventStreamDone = tReadInfo.bDone;

    pStdoutData = xrtProcessGetStdout(pSession->pProcess, &iStdoutSize);
    pStderrData = xrtProcessGetStderr(pSession->pProcess, &iStderrSize);
    sStdout = xwork__local_host_copy_xrt_text(pStdoutData, iStdoutSize);
    sStderr = xwork__local_host_copy_xrt_text(pStderrData, iStderrSize);
    if ( !sStdout || !sStderr ) {
        iStatus = XWORK_ERROR_NO_MEMORY;
        goto cleanup;
    }

    iStatus = xwork__local_host_build_process_events_json(
        pEvents,
        *piEventCount,
        sStdout,
        sStderr,
        &sEventsJson
    );
    if ( iStatus != XWORK_OK ) {
        goto cleanup;
    }
    iStatus = xwork__local_host_build_process_events_output_text(
        pEvents,
        *piEventCount,
        sStdout,
        sStderr,
        &sOutputText,
        piOutputBytes
    );
    if ( iStatus != XWORK_OK ) {
        goto cleanup;
    }
    *pbTerminalOutputCaptured = (*piOutputBytes > 0u);

    *ppsEventsJson = sEventsJson;
    sEventsJson = NULL;
    *ppsOutputText = sOutputText;
    sOutputText = NULL;

cleanup:
    free(sStdout);
    free(sStderr);
    free(sEventsJson);
    free(sOutputText);
    xrtFree(pStdoutData);
    xrtFree(pStderrData);
    xrtFree(pEvents);
    return iStatus;
}

static xwork_status xwork__local_host_invoke_start_terminal(
    xwork_local_host *pHost,
    const char *sRequestJson,
    xwork_tool_result *pResult
)
{
    xvalue tRequest = NULL;
    xvalue tEnvList = NULL;
    const char *sCommand;
    const char *sSessionName = NULL;
    const char *sRequestedCwd = NULL;
    char *sResolvedCwd = NULL;
    char **psEnvEntries = NULL;
    xwork__local_host_terminal_session *pSession = NULL;
    xprocessconfig tProcessConfig;
    xprocessexitinfo tExitInfo;
    char *sEventsJson = NULL;
    char *sOutputText = NULL;
    bool bTerminalOutputCaptured = false;
    bool bRunning = false;
    bool bEventStreamDone = false;
    bool bHasTerminalCols = false;
    bool bHasTerminalRows = false;
    bool bHasMaxEvents = false;
    bool bInsertedSession = false;
    size_t iEnvCount = 0u;
    size_t iRequestedEnvCount = 0u;
    size_t iTerminalCols = 0u;
    size_t iTerminalRows = 0u;
    size_t iMaxEvents = 0u;
    size_t iOutputBytes = 0u;
    uint32 iEventCount = 0u;
    uint64 iNextAfterSeq = 0u;
    uint64 iEventEndSeq = 0u;
    const char *sFailureKind = NULL;
    const char *sFailureSummary = NULL;
    const char *sFailureMessage = NULL;
    xwork_status iStatus;

    if ( !pHost || !pResult ) {
        return XWORK_ERROR_INVALID_ARGUMENT;
    }
    if ( !pHost->bEnableProcessExec ) {
        return XWORK_ERROR_UNSUPPORTED;
    }
    if ( !xrtProcessTerminalSupported() ) {
        return XWORK_ERROR_UNSUPPORTED;
    }

    iStatus = xwork__local_host_parse_request_json(sRequestJson, &tRequest);
    if ( iStatus != XWORK_OK ) {
        return iStatus;
    }

    sCommand = xwork__local_host_request_get_text(tRequest, "command");
    if ( !sCommand || !sCommand[0] ) {
        iStatus = XWORK_ERROR_INVALID_ARGUMENT;
        sFailureKind = "invalid_request";
        sFailureSummary = "process.start_terminal invalid request";
        sFailureMessage = "command is required";
        goto cleanup;
    }
    sSessionName = xwork__local_host_request_get_text(tRequest, "session_name");
    if ( sSessionName && !sSessionName[0] ) {
        iStatus = XWORK_ERROR_INVALID_ARGUMENT;
        sFailureKind = "invalid_request";
        sFailureSummary = "process.start_terminal invalid request";
        sFailureMessage = "session_name must not be empty";
        goto cleanup;
    }
    sRequestedCwd = xwork__local_host_request_get_text(tRequest, "cwd");
    if ( sRequestedCwd && !sRequestedCwd[0] ) {
        iStatus = XWORK_ERROR_INVALID_ARGUMENT;
        sFailureKind = "invalid_request";
        sFailureSummary = "process.start_terminal invalid request";
        sFailureMessage = "cwd must not be empty";
        goto cleanup;
    }
    iStatus = xwork__local_host_check_command_policy(
        pHost,
        sCommand,
        &sFailureKind,
        &sFailureMessage
    );
    if ( iStatus != XWORK_OK ) {
        sFailureSummary = "process.start_terminal denied by command policy";
        goto cleanup;
    }
    iStatus = xwork__local_host_request_get_positive_size_strict(
        tRequest,
        "terminal_cols",
        &bHasTerminalCols,
        &iTerminalCols
    );
    if ( iStatus != XWORK_OK ) {
        sFailureKind = "invalid_request";
        sFailureSummary = "process.start_terminal invalid request";
        sFailureMessage = "terminal_cols must be a positive integer";
        goto cleanup;
    }
    iStatus = xwork__local_host_request_get_positive_size_strict(
        tRequest,
        "terminal_rows",
        &bHasTerminalRows,
        &iTerminalRows
    );
    if ( iStatus != XWORK_OK ) {
        sFailureKind = "invalid_request";
        sFailureSummary = "process.start_terminal invalid request";
        sFailureMessage = "terminal_rows must be a positive integer";
        goto cleanup;
    }
    iStatus = xwork__local_host_request_get_positive_size_strict(
        tRequest,
        "max_events",
        &bHasMaxEvents,
        &iMaxEvents
    );
    if ( iStatus != XWORK_OK ) {
        sFailureKind = "invalid_request";
        sFailureSummary = "process.start_terminal invalid request";
        sFailureMessage = "max_events must be a positive integer";
        goto cleanup;
    }
    if ( !bHasTerminalCols ) {
        iTerminalCols = XWORK__LOCAL_HOST_DEFAULT_TERMINAL_COLS;
    }
    if ( !bHasTerminalRows ) {
        iTerminalRows = XWORK__LOCAL_HOST_DEFAULT_TERMINAL_ROWS;
    }
    if ( iTerminalCols > 0xffffffffu || iTerminalRows > 0xffffffffu ) {
        iStatus = XWORK_ERROR_INVALID_ARGUMENT;
        sFailureKind = "invalid_request";
        sFailureSummary = "process.start_terminal invalid request";
        sFailureMessage = "terminal size exceeds max supported value";
        goto cleanup;
    }

    iStatus = xwork__local_host_request_get_list(tRequest, "env", &tEnvList);
    if ( iStatus != XWORK_OK ) {
        sFailureKind = "invalid_request";
        sFailureSummary = "process.start_terminal invalid request";
        sFailureMessage = "env must be a list of KEY=VALUE strings";
        goto cleanup;
    }
    iRequestedEnvCount = xwork__local_host_list_count(tEnvList);
    iStatus = xwork__local_host_build_process_env_entries(
        tEnvList,
        pHost->iMaxProcessEnvEntries,
        &psEnvEntries,
        &iEnvCount
    );
    if ( iStatus != XWORK_OK ) {
        sFailureKind = "invalid_request";
        if ( pHost->iMaxProcessEnvEntries > 0u &&
             iRequestedEnvCount > pHost->iMaxProcessEnvEntries ) {
            sFailureSummary = "process.start_terminal invalid request (env limit exceeded)";
            sFailureMessage = "env exceeds max_process_env_entries";
        } else {
            sFailureSummary = "process.start_terminal invalid request";
            sFailureMessage = "env entries are invalid";
        }
        goto cleanup;
    }

    if ( sRequestedCwd ) {
        sResolvedCwd = xwork__local_host_resolve_path(pHost, sRequestedCwd);
        if ( !sResolvedCwd ) {
            iStatus = XWORK_ERROR_NO_MEMORY;
            goto cleanup;
        }
    } else if ( pHost->sDefaultWorkingDirectory && pHost->sDefaultWorkingDirectory[0] ) {
        sResolvedCwd = xwork__dup_cstr(pHost->sDefaultWorkingDirectory);
        if ( !sResolvedCwd ) {
            iStatus = XWORK_ERROR_NO_MEMORY;
            goto cleanup;
        }
    }

    pSession = (xwork__local_host_terminal_session *)calloc(1u, sizeof(*pSession));
    if ( !pSession ) {
        iStatus = XWORK_ERROR_NO_MEMORY;
        goto cleanup;
    }
    pSession->sSessionId = xwork__dup_printf(
        "terminal-session-%zu",
        pHost->iNextTerminalSessionId + 1u
    );
    pSession->sSessionName = xwork__dup_cstr(sSessionName ? sSessionName : "");
    pSession->sCommand = xwork__dup_cstr(sCommand);
    pSession->sResolvedCwd = xwork__dup_cstr(sResolvedCwd ? sResolvedCwd : "");
    pSession->iSessionIndex = pHost->iNextTerminalSessionId + 1u;
    pSession->iTerminalCols = iTerminalCols;
    pSession->iTerminalRows = iTerminalRows;
    if ( !pSession->sSessionId || !pSession->sSessionName ||
         !pSession->sCommand || !pSession->sResolvedCwd ) {
        iStatus = XWORK_ERROR_NO_MEMORY;
        goto cleanup;
    }

    memset(&tProcessConfig, 0, sizeof(tProcessConfig));
    memset(&tExitInfo, 0, sizeof(tExitInfo));
    xrtProcessConfigInit(&tProcessConfig);
    tProcessConfig.iTargetKind = XPROC_TARGET_SHELL;
    tProcessConfig.sCommand = (str)sCommand;
    tProcessConfig.sWorkDir =
        (str)((sResolvedCwd && sResolvedCwd[0]) ? sResolvedCwd : NULL);
    tProcessConfig.arrEnv = (str *)psEnvEntries;
    tProcessConfig.iEnvCount = (uint32)iEnvCount;
    tProcessConfig.bUseTerminal = true;
    tProcessConfig.bCreateProcessGroup = true;
    tProcessConfig.bHideWindow = true;
    tProcessConfig.iTerminalCols = (uint32)iTerminalCols;
    tProcessConfig.iTerminalRows = (uint32)iTerminalRows;
    tProcessConfig.iMaxCaptureBytes = pHost->iMaxProcessOutputBytes;
    tProcessConfig.Stdin.iMode = XPROC_STDIO_PIPE;
    tProcessConfig.Stdout.iMode = XPROC_STDIO_PIPE;
    tProcessConfig.Stderr.iMode = XPROC_STDIO_PIPE;

    pSession->pProcess = xrtProcessSpawn(&tProcessConfig);
    if ( !pSession->pProcess ) {
        iStatus = XWORK_ERROR_EXTERNAL_FAILURE;
        sFailureKind = "spawn_failed";
        sFailureSummary = "process.start_terminal failed to start";
        sFailureMessage = "failed to start terminal session";
        goto cleanup;
    }

    pSession->pNext = (xwork__local_host_terminal_session *)pHost->pTerminalSessions;
    pHost->pTerminalSessions = pSession;
    ++pHost->iNextTerminalSessionId;
    bInsertedSession = true;

    iStatus = xwork__local_host_terminal_session_snapshot(
        pSession,
        0u,
        bHasMaxEvents ? iMaxEvents : 0u,
        &bTerminalOutputCaptured,
        &bRunning,
        &bEventStreamDone,
        &tExitInfo,
        &iEventCount,
        &iNextAfterSeq,
        &iEventEndSeq,
        &sEventsJson,
        &sOutputText,
        &iOutputBytes
    );
    if ( iStatus != XWORK_OK ) {
        sFailureKind = "snapshot_failed";
        sFailureSummary = "process.start_terminal failed";
        sFailureMessage = "failed to snapshot terminal session";
        goto cleanup;
    }

        iStatus = xwork__local_host_set_terminal_state_result(
            pHost,
            pSession->sSessionId,
            pSession->sSessionName,
            pSession->sCommand,
            pSession->sResolvedCwd,
            pSession->iSessionIndex,
            sOutputText,
            iOutputBytes,
            pSession->iTerminalCols,
            pSession->iTerminalRows,
            pSession->bStdinClosed,
            bTerminalOutputCaptured,
            bRunning,
            bEventStreamDone,
        false,
        iNextAfterSeq,
        iEventEndSeq,
        iEventCount,
        sEventsJson,
        NULL,
        &tExitInfo,
        true,
        "process.start_terminal ok",
        NULL,
        NULL,
        pResult
    );

cleanup:
    if ( iStatus != XWORK_OK && sFailureKind ) {
        (void)xwork__local_host_set_terminal_state_result(
            pHost,
            pSession ? pSession->sSessionId : "",
            pSession ? pSession->sSessionName : sSessionName,
            sCommand ? sCommand : "",
            sResolvedCwd ? sResolvedCwd : sRequestedCwd,
            pSession ? pSession->iSessionIndex : 0u,
            sOutputText,
            iOutputBytes,
            iTerminalCols,
            iTerminalRows,
            pSession ? pSession->bStdinClosed : false,
            bTerminalOutputCaptured,
            false,
            bEventStreamDone,
            false,
            iNextAfterSeq,
            iEventEndSeq,
            iEventCount,
            sEventsJson,
            NULL,
            &tExitInfo,
            false,
            sFailureSummary,
            sFailureKind,
            sFailureMessage,
            pResult
        );
    }
    if ( tRequest ) {
        xvoUnref(tRequest);
    }
    free(sResolvedCwd);
    xwork__local_host_free_process_env_entries(&psEnvEntries, iEnvCount);
    free(sEventsJson);
    free(sOutputText);
    if ( pSession && !bInsertedSession ) {
        xwork__local_host_free_terminal_session(pSession);
    }
    if ( iStatus != XWORK_OK && bInsertedSession && pSession ) {
        xwork__local_host_remove_terminal_session(pHost, pSession);
    }
    return iStatus;
}

static xwork_status xwork__local_host_invoke_list_terminals(
    xwork_local_host *pHost,
    const char *sRequestJson,
    xwork_tool_result *pResult
)
{
    xvalue tRequest = NULL;
    xwork__local_host_terminal_session *pSession;
    xwork__local_host_terminal_session **ppMatchedSessions = NULL;
    char *sEscapedSessionNameFilter = NULL;
    char *sAfterSessionIndexText = NULL;
    char *sLimitText = NULL;
    char *sExtraJsonFields = NULL;
    char *sSessionsJson = NULL;
    const char *sSessionNameFilter = NULL;
    size_t iTotalSessionCount = 0u;
    size_t iMatchedSessionCount = 0u;
    size_t iSessionCount = 0u;
    size_t iAfterSessionIndex = 0u;
    size_t iNextAfterSessionIndex = 0u;
    size_t iLimit = 0u;
    bool bHasRunningFilter = false;
    bool bRunningFilter = false;
    bool bHasDoneFilter = false;
    bool bDoneFilter = false;
    bool bHasAfterSessionIndex = false;
    bool bHasLimit = false;
    bool bHasMoreSessions = false;
    xwork_status iStatus = XWORK_OK;

    if ( !pHost || !pResult ) {
        return XWORK_ERROR_INVALID_ARGUMENT;
    }
    if ( sRequestJson && sRequestJson[0] ) {
        iStatus = xwork__local_host_parse_request_json(sRequestJson, &tRequest);
        if ( iStatus != XWORK_OK ) {
            return iStatus;
        }
    }
    if ( tRequest ) {
        sSessionNameFilter = xwork__local_host_request_get_text(
            tRequest,
            "session_name"
        );
        if ( sSessionNameFilter && !sSessionNameFilter[0] ) {
            iStatus = XWORK_ERROR_INVALID_ARGUMENT;
            goto cleanup;
        }
        iStatus = xwork__local_host_request_get_bool(
            tRequest,
            "running",
            &bHasRunningFilter,
            &bRunningFilter
        );
        if ( iStatus != XWORK_OK ) {
            goto cleanup;
        }
        iStatus = xwork__local_host_request_get_bool(
            tRequest,
            "done",
            &bHasDoneFilter,
            &bDoneFilter
        );
        if ( iStatus != XWORK_OK ) {
            goto cleanup;
        }
        iStatus = xwork__local_host_request_get_positive_size_strict(
            tRequest,
            "after_session_index",
            &bHasAfterSessionIndex,
            &iAfterSessionIndex
        );
        if ( iStatus != XWORK_OK ) {
            goto cleanup;
        }
        iStatus = xwork__local_host_request_get_positive_size_strict(
            tRequest,
            "limit",
            &bHasLimit,
            &iLimit
        );
        if ( iStatus != XWORK_OK ) {
            goto cleanup;
        }
        if ( bHasRunningFilter && bHasDoneFilter &&
             bRunningFilter == bDoneFilter ) {
            iStatus = XWORK_ERROR_INVALID_ARGUMENT;
            goto cleanup;
        }
    }

    iTotalSessionCount = xwork__local_host_terminal_session_count(pHost);
    if ( sSessionNameFilter ) {
        iStatus = xwork__local_host_json_escape(
            sSessionNameFilter,
            &sEscapedSessionNameFilter
        );
        if ( iStatus != XWORK_OK ) {
            goto cleanup;
        }
    }
    sLimitText = bHasLimit
        ? xwork__dup_printf("%zu", iLimit)
        : xwork__dup_cstr("null");
    if ( !sLimitText ) {
        iStatus = XWORK_ERROR_NO_MEMORY;
        goto cleanup;
    }
    sAfterSessionIndexText = bHasAfterSessionIndex
        ? xwork__dup_printf("%zu", iAfterSessionIndex)
        : xwork__dup_cstr("null");
    if ( !sAfterSessionIndexText ) {
        iStatus = XWORK_ERROR_NO_MEMORY;
        goto cleanup;
    }
    sExtraJsonFields = xwork__dup_printf(
        "\"filters\":{\"session_name\":%s%s%s,\"running\":%s,\"done\":%s,"
        "\"after_session_index\":%s,\"limit\":%s},"
        "\"sort\":\"session_index_asc\",\"has_more_sessions\":false,"
        "\"next_after_session_index\":null,",
        sSessionNameFilter ? "\"" : "null",
        sSessionNameFilter ? (sEscapedSessionNameFilter ? sEscapedSessionNameFilter : "") : "",
        sSessionNameFilter ? "\"" : "",
        bHasRunningFilter ? (bRunningFilter ? "true" : "false") : "null",
        bHasDoneFilter ? (bDoneFilter ? "true" : "false") : "null",
        sAfterSessionIndexText,
        sLimitText
    );
    if ( !sExtraJsonFields ) {
        iStatus = XWORK_ERROR_NO_MEMORY;
        goto cleanup;
    }

    sSessionsJson = xwork__dup_cstr("[");
    if ( !sSessionsJson ) {
        iStatus = XWORK_ERROR_NO_MEMORY;
        goto cleanup;
    }

    if ( iTotalSessionCount > 0u ) {
        ppMatchedSessions = (xwork__local_host_terminal_session **)calloc(
            iTotalSessionCount,
            sizeof(xwork__local_host_terminal_session *)
        );
        if ( !ppMatchedSessions ) {
            iStatus = XWORK_ERROR_NO_MEMORY;
            goto cleanup;
        }
    }

    for ( pSession = (xwork__local_host_terminal_session *)pHost->pTerminalSessions;
          pSession;
          pSession = pSession->pNext ) {
        xprocessexitinfo tExitInfo;
        bool bRunning = false;

        memset(&tExitInfo, 0, sizeof(tExitInfo));
        if ( pSession->pProcess ) {
            bRunning = xrtProcessIsRunning(pSession->pProcess);
            (void)xrtProcessGetExitInfo(pSession->pProcess, &tExitInfo);
        }
        if ( sSessionNameFilter &&
             strcmp(
                 pSession->sSessionName ? pSession->sSessionName : "",
                 sSessionNameFilter
             ) != 0 ) {
            continue;
        }
        if ( bHasRunningFilter && bRunning != bRunningFilter ) {
            continue;
        }
        if ( bHasDoneFilter && (!bRunning) != bDoneFilter ) {
            continue;
        }
        ppMatchedSessions[iMatchedSessionCount++] = pSession;
    }

    if ( iMatchedSessionCount > 1u ) {
        qsort(
            ppMatchedSessions,
            iMatchedSessionCount,
            sizeof(xwork__local_host_terminal_session *),
            xwork__local_host_compare_terminal_session_index
        );
    }

    for ( iMatchedSessionCount = 0u;
          ppMatchedSessions && iMatchedSessionCount < iTotalSessionCount &&
              ppMatchedSessions[iMatchedSessionCount];
          ++iMatchedSessionCount ) {
        char *sEscapedSessionId = NULL;
        char *sEscapedSessionName = NULL;
        char *sEscapedCommand = NULL;
        char *sEscapedCwd = NULL;
        char *sEntryJson = NULL;
        char *sNextJson = NULL;
        xprocessexitinfo tExitInfo;
        bool bRunning = false;

        pSession = ppMatchedSessions[iMatchedSessionCount];
        if ( !pSession ) {
            break;
        }
        if ( bHasAfterSessionIndex &&
             pSession->iSessionIndex <= iAfterSessionIndex ) {
            continue;
        }
        if ( bHasLimit && iSessionCount >= iLimit ) {
            bHasMoreSessions = true;
            break;
        }
        memset(&tExitInfo, 0, sizeof(tExitInfo));
        if ( pSession->pProcess ) {
            bRunning = xrtProcessIsRunning(pSession->pProcess);
            (void)xrtProcessGetExitInfo(pSession->pProcess, &tExitInfo);
        }

        iStatus = xwork__local_host_json_escape(
            pSession->sSessionId ? pSession->sSessionId : "",
            &sEscapedSessionId
        );
        if ( iStatus != XWORK_OK ) goto session_cleanup;
        iStatus = xwork__local_host_json_escape(
            pSession->sSessionName ? pSession->sSessionName : "",
            &sEscapedSessionName
        );
        if ( iStatus != XWORK_OK ) goto session_cleanup;
        iStatus = xwork__local_host_json_escape(
            pSession->sCommand ? pSession->sCommand : "",
            &sEscapedCommand
        );
        if ( iStatus != XWORK_OK ) goto session_cleanup;
        iStatus = xwork__local_host_json_escape(
            pSession->sResolvedCwd ? pSession->sResolvedCwd : "",
            &sEscapedCwd
        );
        if ( iStatus != XWORK_OK ) goto session_cleanup;

        sEntryJson = xwork__dup_printf(
            "%s{\"session_id\":\"%s\",\"session_name\":\"%s\",\"session_index\":%zu,"
            "\"command\":\"%s\",\"cwd\":\"%s\","
            "\"running\":%s,\"done\":%s,"
            "\"terminal_cols\":%zu,\"terminal_rows\":%zu,"
            "\"stdin_closed\":%s,"
            "\"exit_kind\":\"%s\",\"exit_code\":%d,\"signal\":%d,"
            "\"stage\":\"%s\",\"os_error\":%d,"
            "\"stop_reason\":\"%s\",\"timed_out\":%s,\"cancelled\":%s}",
            (iSessionCount > 0u) ? "," : "",
            sEscapedSessionId,
            sEscapedSessionName,
            pSession->iSessionIndex,
            sEscapedCommand,
            sEscapedCwd,
            bRunning ? "true" : "false",
            bRunning ? "false" : "true",
            pSession->iTerminalCols,
            pSession->iTerminalRows,
            pSession->bStdinClosed ? "true" : "false",
            xwork__local_host_process_exit_kind_name(tExitInfo.iKind),
            tExitInfo.iExitCode,
            tExitInfo.iSignal,
            xwork__local_host_process_exit_stage_name(tExitInfo.iStage),
            tExitInfo.iOsError,
            xwork__local_host_process_stop_reason_name(tExitInfo.iStopReason),
            tExitInfo.bTimedOut ? "true" : "false",
            tExitInfo.bCancelled ? "true" : "false"
        );
        if ( !sEntryJson ) {
            iStatus = XWORK_ERROR_NO_MEMORY;
            goto session_cleanup;
        }

        sNextJson = xwork__dup_printf("%s%s", sSessionsJson, sEntryJson);
        if ( !sNextJson ) {
            iStatus = XWORK_ERROR_NO_MEMORY;
            goto session_cleanup;
        }

        free(sSessionsJson);
        sSessionsJson = sNextJson;
        sNextJson = NULL;
        iNextAfterSessionIndex = pSession->iSessionIndex;
        ++iSessionCount;

session_cleanup:
        free(sEscapedSessionId);
        free(sEscapedSessionName);
        free(sEscapedCommand);
        free(sEscapedCwd);
        free(sEntryJson);
        free(sNextJson);
        if ( iStatus != XWORK_OK ) {
            goto cleanup;
        }
    }

    {
        char *sUpdatedExtraJsonFields = NULL;

        if ( iSessionCount > 0u ) {
            sUpdatedExtraJsonFields = xwork__dup_printf(
                "\"filters\":{\"session_name\":%s%s%s,\"running\":%s,\"done\":%s,"
                "\"after_session_index\":%s,\"limit\":%s},"
                "\"sort\":\"session_index_asc\",\"has_more_sessions\":%s,"
                "\"next_after_session_index\":%zu,",
                sSessionNameFilter ? "\"" : "null",
                sSessionNameFilter ? (sEscapedSessionNameFilter ? sEscapedSessionNameFilter : "") : "",
                sSessionNameFilter ? "\"" : "",
                bHasRunningFilter ? (bRunningFilter ? "true" : "false") : "null",
                bHasDoneFilter ? (bDoneFilter ? "true" : "false") : "null",
                sAfterSessionIndexText,
                sLimitText,
                bHasMoreSessions ? "true" : "false",
                iNextAfterSessionIndex
            );
        } else {
            sUpdatedExtraJsonFields = xwork__dup_printf(
                "\"filters\":{\"session_name\":%s%s%s,\"running\":%s,\"done\":%s,"
                "\"after_session_index\":%s,\"limit\":%s},"
                "\"sort\":\"session_index_asc\",\"has_more_sessions\":%s,"
                "\"next_after_session_index\":null,",
                sSessionNameFilter ? "\"" : "null",
                sSessionNameFilter ? (sEscapedSessionNameFilter ? sEscapedSessionNameFilter : "") : "",
                sSessionNameFilter ? "\"" : "",
                bHasRunningFilter ? (bRunningFilter ? "true" : "false") : "null",
                bHasDoneFilter ? (bDoneFilter ? "true" : "false") : "null",
                sAfterSessionIndexText,
                sLimitText,
                bHasMoreSessions ? "true" : "false"
            );
        }
        if ( !sUpdatedExtraJsonFields ) {
            iStatus = XWORK_ERROR_NO_MEMORY;
            goto cleanup;
        }
        free(sExtraJsonFields);
        sExtraJsonFields = sUpdatedExtraJsonFields;
    }

    {
        char *sClosedJson = xwork__dup_printf("%s]", sSessionsJson);
        if ( !sClosedJson ) {
            iStatus = XWORK_ERROR_NO_MEMORY;
            goto cleanup;
        }
        free(sSessionsJson);
        sSessionsJson = sClosedJson;
    }

    iStatus = xwork__local_host_set_terminal_list_result(
        pHost,
        iSessionCount,
        iTotalSessionCount,
        sSessionsJson,
        sExtraJsonFields,
        true,
        "process.list_terminals ok",
        NULL,
        NULL,
        pResult
    );

cleanup:
    if ( iStatus != XWORK_OK ) {
        (void)xwork__local_host_set_terminal_list_result(
            pHost,
            0u,
            iTotalSessionCount,
            "[]",
            sExtraJsonFields,
            false,
            (iStatus == XWORK_ERROR_INVALID_ARGUMENT)
                ? "process.list_terminals invalid request"
                : "process.list_terminals failed",
            (iStatus == XWORK_ERROR_INVALID_ARGUMENT)
                ? "invalid_request"
                : "external_failure",
            (iStatus == XWORK_ERROR_INVALID_ARGUMENT)
                ? "session_name must not be empty, running/done must be boolean, limit must be a positive integer, and running/done filters must not conflict"
                : "failed to build terminal session list",
            pResult
        );
    }
    if ( tRequest ) {
        xvoUnref(tRequest);
    }
    free(sEscapedSessionNameFilter);
    free(sAfterSessionIndexText);
    free(sLimitText);
    free(sExtraJsonFields);
    free(sSessionsJson);
    free(ppMatchedSessions);
    return iStatus;
}

static xwork_status xwork__local_host_invoke_terminal_read(
    xwork_local_host *pHost,
    const char *sRequestJson,
    xwork_tool_result *pResult
)
{
    xvalue tRequest = NULL;
    const char *sSessionId;
    xwork__local_host_terminal_session *pSession = NULL;
    xprocessexitinfo tExitInfo;
    char *sEventsJson = NULL;
    char *sOutputText = NULL;
    bool bTerminalOutputCaptured = false;
    bool bRunning = false;
    bool bEventStreamDone = false;
    bool bHasAfterSeq = false;
    bool bHasMaxEvents = false;
    size_t iAfterSeq = 0u;
    size_t iMaxEvents = 0u;
    size_t iOutputBytes = 0u;
    uint32 iEventCount = 0u;
    uint64 iNextAfterSeq = 0u;
    uint64 iEventEndSeq = 0u;
    const char *sFailureKind = NULL;
    const char *sFailureSummary = NULL;
    const char *sFailureMessage = NULL;
    xwork_status iStatus;

    if ( !pHost || !pResult ) {
        return XWORK_ERROR_INVALID_ARGUMENT;
    }

    iStatus = xwork__local_host_parse_request_json(sRequestJson, &tRequest);
    if ( iStatus != XWORK_OK ) {
        return iStatus;
    }

    sSessionId = xwork__local_host_request_get_text(tRequest, "session_id");
    if ( !sSessionId || !sSessionId[0] ) {
        iStatus = XWORK_ERROR_INVALID_ARGUMENT;
        sFailureKind = "invalid_request";
        sFailureSummary = "process.terminal_read invalid request";
        sFailureMessage = "session_id is required";
        goto cleanup;
    }
    iStatus = xwork__local_host_request_get_positive_size_strict(
        tRequest,
        "after_seq",
        &bHasAfterSeq,
        &iAfterSeq
    );
    if ( iStatus != XWORK_OK ) {
        sFailureKind = "invalid_request";
        sFailureSummary = "process.terminal_read invalid request";
        sFailureMessage = "after_seq must be a positive integer";
        goto cleanup;
    }
    iStatus = xwork__local_host_request_get_positive_size_strict(
        tRequest,
        "max_events",
        &bHasMaxEvents,
        &iMaxEvents
    );
    if ( iStatus != XWORK_OK ) {
        sFailureKind = "invalid_request";
        sFailureSummary = "process.terminal_read invalid request";
        sFailureMessage = "max_events must be a positive integer";
        goto cleanup;
    }

    pSession = xwork__local_host_find_terminal_session(pHost, sSessionId);
    if ( !pSession ) {
        iStatus = XWORK_ERROR_NOT_FOUND;
        sFailureKind = "not_found";
        sFailureSummary = "process.terminal_read failed";
        sFailureMessage = "session_id not found";
        goto cleanup;
    }

    memset(&tExitInfo, 0, sizeof(tExitInfo));
    iStatus = xwork__local_host_terminal_session_snapshot(
        pSession,
        bHasAfterSeq ? iAfterSeq : 0u,
        bHasMaxEvents ? iMaxEvents : 0u,
        &bTerminalOutputCaptured,
        &bRunning,
        &bEventStreamDone,
        &tExitInfo,
        &iEventCount,
        &iNextAfterSeq,
        &iEventEndSeq,
        &sEventsJson,
        &sOutputText,
        &iOutputBytes
    );
    if ( iStatus != XWORK_OK ) {
        sFailureKind = "snapshot_failed";
        sFailureSummary = "process.terminal_read failed";
        sFailureMessage = "failed to read terminal session";
        goto cleanup;
    }

    iStatus = xwork__local_host_set_terminal_state_result(
        pHost,
        pSession->sSessionId,
        pSession->sSessionName,
        pSession->sCommand,
        pSession->sResolvedCwd,
        pSession->iSessionIndex,
        sOutputText,
        iOutputBytes,
        pSession->iTerminalCols,
        pSession->iTerminalRows,
        pSession->bStdinClosed,
        bTerminalOutputCaptured,
        bRunning,
        bEventStreamDone,
        false,
        iNextAfterSeq,
        iEventEndSeq,
        iEventCount,
        sEventsJson,
        NULL,
        &tExitInfo,
        true,
        "process.terminal_read ok",
        NULL,
        NULL,
        pResult
    );

cleanup:
    if ( iStatus != XWORK_OK && sFailureKind ) {
        (void)xwork__local_host_set_terminal_state_result(
            pHost,
            sSessionId ? sSessionId : "",
            pSession ? pSession->sSessionName : "",
            pSession ? pSession->sCommand : "",
            pSession ? pSession->sResolvedCwd : NULL,
            pSession ? pSession->iSessionIndex : 0u,
            sOutputText,
            iOutputBytes,
            pSession ? pSession->iTerminalCols : 0u,
            pSession ? pSession->iTerminalRows : 0u,
            pSession ? pSession->bStdinClosed : false,
            bTerminalOutputCaptured,
            false,
            bEventStreamDone,
            false,
            iNextAfterSeq,
            iEventEndSeq,
            iEventCount,
            sEventsJson,
            NULL,
            &tExitInfo,
            false,
            sFailureSummary,
            sFailureKind,
            sFailureMessage,
            pResult
        );
    }
    if ( tRequest ) {
        xvoUnref(tRequest);
    }
    free(sEventsJson);
    free(sOutputText);
    return iStatus;
}

static xwork_status xwork__local_host_invoke_terminal_write(
    xwork_local_host *pHost,
    const char *sRequestJson,
    xwork_tool_result *pResult
)
{
    xvalue tRequest = NULL;
    const char *sSessionId;
    const char *sInputText;
    xwork__local_host_terminal_session *pSession = NULL;
    xprocessexitinfo tExitInfo;
    char *sEventsJson = NULL;
    char *sOutputText = NULL;
    bool bTerminalOutputCaptured = false;
    bool bRunning = false;
    bool bEventStreamDone = false;
    bool bHasIncludeState = false;
    bool bIncludeState = false;
    bool bHasWriteEof = false;
    bool bWriteEof = false;
    bool bStdinClosed = false;
    bool bHasStateSnapshot = false;
    bool bHasAfterSeq = false;
    bool bHasMaxEvents = false;
    size_t iInputBytes = 0u;
    size_t iAfterSeq = 0u;
    size_t iMaxEvents = 0u;
    size_t iOutputBytes = 0u;
    uint32 iEventCount = 0u;
    uint64 iNextAfterSeq = 0u;
    uint64 iEventEndSeq = 0u;
    int64 iBytesWritten = 0;
    const char *sFailureKind = NULL;
    const char *sFailureSummary = NULL;
    const char *sFailureMessage = NULL;
    xwork_status iStatus;

    if ( !pHost || !pResult ) {
        return XWORK_ERROR_INVALID_ARGUMENT;
    }

    iStatus = xwork__local_host_parse_request_json(sRequestJson, &tRequest);
    if ( iStatus != XWORK_OK ) {
        return iStatus;
    }

    sSessionId = xwork__local_host_request_get_text(tRequest, "session_id");
    sInputText = xwork__local_host_request_get_text(tRequest, "input_text");
    if ( !sSessionId || !sSessionId[0] ) {
        iStatus = XWORK_ERROR_INVALID_ARGUMENT;
        sFailureKind = "invalid_request";
        sFailureSummary = "process.terminal_write invalid request";
        sFailureMessage = "session_id is required";
        goto cleanup;
    }
    iStatus = xwork__local_host_request_get_bool(
        tRequest,
        "write_eof",
        &bHasWriteEof,
        &bWriteEof
    );
    if ( iStatus != XWORK_OK ) {
        sFailureKind = "invalid_request";
        sFailureSummary = "process.terminal_write invalid request";
        sFailureMessage = "write_eof must be a boolean";
        goto cleanup;
    }
    if ( (!sInputText || !sInputText[0]) && !bWriteEof ) {
        iStatus = XWORK_ERROR_INVALID_ARGUMENT;
        sFailureKind = "invalid_request";
        sFailureSummary = "process.terminal_write invalid request";
        sFailureMessage = "input_text or write_eof is required";
        goto cleanup;
    }
    iStatus = xwork__local_host_request_get_positive_size_strict(
        tRequest,
        "after_seq",
        &bHasAfterSeq,
        &iAfterSeq
    );
    if ( iStatus != XWORK_OK ) {
        sFailureKind = "invalid_request";
        sFailureSummary = "process.terminal_write invalid request";
        sFailureMessage = "after_seq must be a positive integer";
        goto cleanup;
    }
    iStatus = xwork__local_host_request_get_positive_size_strict(
        tRequest,
        "max_events",
        &bHasMaxEvents,
        &iMaxEvents
    );
    if ( iStatus != XWORK_OK ) {
        sFailureKind = "invalid_request";
        sFailureSummary = "process.terminal_write invalid request";
        sFailureMessage = "max_events must be a positive integer";
        goto cleanup;
    }
    iStatus = xwork__local_host_request_get_bool(
        tRequest,
        "include_state",
        &bHasIncludeState,
        &bIncludeState
    );
    if ( iStatus != XWORK_OK ) {
        sFailureKind = "invalid_request";
        sFailureSummary = "process.terminal_write invalid request";
        sFailureMessage = "include_state must be a boolean";
        goto cleanup;
    }
    bHasStateSnapshot = bIncludeState || bHasAfterSeq || bHasMaxEvents;
    iInputBytes = sInputText ? strlen(sInputText) : 0u;
    if ( sInputText &&
         pHost->iMaxProcessInputBytes > 0u &&
         iInputBytes > pHost->iMaxProcessInputBytes ) {
        iStatus = XWORK_ERROR_INVALID_ARGUMENT;
        sFailureKind = "invalid_request";
        sFailureSummary = "process.terminal_write invalid request";
        sFailureMessage = "input_text exceeds max_process_input_bytes";
        goto cleanup;
    }

    pSession = xwork__local_host_find_terminal_session(pHost, sSessionId);
    if ( !pSession ) {
        iStatus = XWORK_ERROR_NOT_FOUND;
        sFailureKind = "not_found";
        sFailureSummary = "process.terminal_write failed";
        sFailureMessage = "session_id not found";
        goto cleanup;
    }
    if ( !pSession->pProcess || !xrtProcessIsRunning(pSession->pProcess) ) {
        iStatus = XWORK_ERROR_INVALID_STATE;
        sFailureKind = "invalid_state";
        sFailureSummary = "process.terminal_write failed";
        sFailureMessage = "terminal session is not running";
        goto cleanup;
    }

    if ( sInputText && iInputBytes > 0u ) {
        iBytesWritten = xrtProcessWriteText(
            pSession->pProcess,
            (str)sInputText,
            iInputBytes
        );
        if ( iBytesWritten < 0 || (size_t)iBytesWritten != iInputBytes ) {
            iStatus = XWORK_ERROR_EXTERNAL_FAILURE;
            sFailureKind = "write_failed";
            sFailureSummary = "process.terminal_write failed";
            sFailureMessage = "failed to write terminal input";
            goto cleanup;
        }
    }
    if ( bWriteEof ) {
        if ( !xrtProcessCloseStdin(pSession->pProcess) ) {
            iStatus = XWORK_ERROR_EXTERNAL_FAILURE;
            sFailureKind = "stdin_failed";
            sFailureSummary = "process.terminal_write failed";
            sFailureMessage = "failed to close terminal stdin";
            goto cleanup;
        }
        bStdinClosed = true;
        pSession->bStdinClosed = true;
    }

    if ( bHasStateSnapshot ) {
        memset(&tExitInfo, 0, sizeof(tExitInfo));
        iStatus = xwork__local_host_terminal_session_snapshot(
            pSession,
            bHasAfterSeq ? iAfterSeq : 0u,
            bHasMaxEvents ? iMaxEvents : 0u,
            &bTerminalOutputCaptured,
            &bRunning,
            &bEventStreamDone,
            &tExitInfo,
            &iEventCount,
            &iNextAfterSeq,
            &iEventEndSeq,
            &sEventsJson,
            &sOutputText,
            &iOutputBytes
        );
        if ( iStatus != XWORK_OK ) {
            sFailureKind = "snapshot_failed";
            sFailureSummary = "process.terminal_write failed";
            sFailureMessage = "failed to snapshot terminal session";
            goto cleanup;
        }
    }

    iStatus = xwork__local_host_set_terminal_write_result(
        pHost,
        pSession->sSessionId,
        pSession->sSessionName,
        pSession->iSessionIndex,
        iInputBytes,
        bWriteEof,
        bStdinClosed,
        bHasStateSnapshot,
        pSession->sCommand,
        pSession->sResolvedCwd,
        sOutputText,
        iOutputBytes,
        pSession->iTerminalCols,
        pSession->iTerminalRows,
        bTerminalOutputCaptured,
        bRunning,
        bEventStreamDone,
        iNextAfterSeq,
        iEventEndSeq,
        iEventCount,
        sEventsJson,
        true,
        "process.terminal_write ok",
        NULL,
        NULL,
        pResult
    );

cleanup:
    if ( iStatus != XWORK_OK && sFailureKind ) {
        (void)xwork__local_host_set_terminal_write_result(
            pHost,
            sSessionId ? sSessionId : "",
            pSession ? pSession->sSessionName : "",
            pSession ? pSession->iSessionIndex : 0u,
            (size_t)((iBytesWritten > 0) ? iBytesWritten : 0),
            bWriteEof,
            pSession ? pSession->bStdinClosed || bStdinClosed : bStdinClosed,
            bHasStateSnapshot,
            pSession ? pSession->sCommand : "",
            pSession ? pSession->sResolvedCwd : NULL,
            sOutputText,
            iOutputBytes,
            pSession ? pSession->iTerminalCols : 0u,
            pSession ? pSession->iTerminalRows : 0u,
            bTerminalOutputCaptured,
            false,
            bEventStreamDone,
            iNextAfterSeq,
            iEventEndSeq,
            iEventCount,
            sEventsJson,
            false,
            sFailureSummary,
            sFailureKind,
            sFailureMessage,
            pResult
        );
    }
    if ( tRequest ) {
        xvoUnref(tRequest);
    }
    free(sEventsJson);
    free(sOutputText);
    return iStatus;
}

static xwork_status xwork__local_host_invoke_terminal_resize(
    xwork_local_host *pHost,
    const char *sRequestJson,
    xwork_tool_result *pResult
)
{
    xvalue tRequest = NULL;
    const char *sSessionId;
    xwork__local_host_terminal_session *pSession = NULL;
    xprocessexitinfo tExitInfo;
    char *sEventsJson = NULL;
    char *sOutputText = NULL;
    bool bTerminalOutputCaptured = false;
    bool bRunning = false;
    bool bEventStreamDone = false;
    bool bHasAfterSeq = false;
    bool bHasMaxEvents = false;
    bool bHasTerminalCols = false;
    bool bHasTerminalRows = false;
    size_t iAfterSeq = 0u;
    size_t iMaxEvents = 0u;
    size_t iTerminalCols = 0u;
    size_t iTerminalRows = 0u;
    size_t iOutputBytes = 0u;
    uint32 iEventCount = 0u;
    uint64 iNextAfterSeq = 0u;
    uint64 iEventEndSeq = 0u;
    bool bResizeApplied = false;
    char sResizeExtraJson[32] = "";
    const char *sFailureKind = NULL;
    const char *sFailureSummary = NULL;
    const char *sFailureMessage = NULL;
    xwork_status iStatus;

    if ( !pHost || !pResult ) {
        return XWORK_ERROR_INVALID_ARGUMENT;
    }

    iStatus = xwork__local_host_parse_request_json(sRequestJson, &tRequest);
    if ( iStatus != XWORK_OK ) {
        return iStatus;
    }

    sSessionId = xwork__local_host_request_get_text(tRequest, "session_id");
    if ( !sSessionId || !sSessionId[0] ) {
        iStatus = XWORK_ERROR_INVALID_ARGUMENT;
        sFailureKind = "invalid_request";
        sFailureSummary = "process.terminal_resize invalid request";
        sFailureMessage = "session_id is required";
        goto cleanup;
    }
    iStatus = xwork__local_host_request_get_positive_size_strict(
        tRequest,
        "terminal_cols",
        &bHasTerminalCols,
        &iTerminalCols
    );
    if ( iStatus != XWORK_OK || !bHasTerminalCols ) {
        iStatus = XWORK_ERROR_INVALID_ARGUMENT;
        sFailureKind = "invalid_request";
        sFailureSummary = "process.terminal_resize invalid request";
        sFailureMessage = "terminal_cols must be a positive integer";
        goto cleanup;
    }
    iStatus = xwork__local_host_request_get_positive_size_strict(
        tRequest,
        "terminal_rows",
        &bHasTerminalRows,
        &iTerminalRows
    );
    if ( iStatus != XWORK_OK || !bHasTerminalRows ) {
        iStatus = XWORK_ERROR_INVALID_ARGUMENT;
        sFailureKind = "invalid_request";
        sFailureSummary = "process.terminal_resize invalid request";
        sFailureMessage = "terminal_rows must be a positive integer";
        goto cleanup;
    }
    iStatus = xwork__local_host_request_get_positive_size_strict(
        tRequest,
        "after_seq",
        &bHasAfterSeq,
        &iAfterSeq
    );
    if ( iStatus != XWORK_OK ) {
        sFailureKind = "invalid_request";
        sFailureSummary = "process.terminal_resize invalid request";
        sFailureMessage = "after_seq must be a positive integer";
        goto cleanup;
    }
    iStatus = xwork__local_host_request_get_positive_size_strict(
        tRequest,
        "max_events",
        &bHasMaxEvents,
        &iMaxEvents
    );
    if ( iStatus != XWORK_OK ) {
        sFailureKind = "invalid_request";
        sFailureSummary = "process.terminal_resize invalid request";
        sFailureMessage = "max_events must be a positive integer";
        goto cleanup;
    }
    if ( iTerminalCols > 0xffffffffu || iTerminalRows > 0xffffffffu ) {
        iStatus = XWORK_ERROR_INVALID_ARGUMENT;
        sFailureKind = "invalid_request";
        sFailureSummary = "process.terminal_resize invalid request";
        sFailureMessage = "terminal size exceeds max supported value";
        goto cleanup;
    }

    pSession = xwork__local_host_find_terminal_session(pHost, sSessionId);
    if ( !pSession ) {
        iStatus = XWORK_ERROR_NOT_FOUND;
        sFailureKind = "not_found";
        sFailureSummary = "process.terminal_resize failed";
        sFailureMessage = "session_id not found";
        goto cleanup;
    }
    bResizeApplied = xrtProcessResizeTerminal(
        pSession->pProcess,
        (uint32)iTerminalCols,
        (uint32)iTerminalRows
    );
    pSession->iTerminalCols = iTerminalCols;
    pSession->iTerminalRows = iTerminalRows;
    snprintf(
        sResizeExtraJson,
        sizeof(sResizeExtraJson),
        "\"resize_applied\":%s,",
        bResizeApplied ? "true" : "false"
    );

    memset(&tExitInfo, 0, sizeof(tExitInfo));
    iStatus = xwork__local_host_terminal_session_snapshot(
        pSession,
        bHasAfterSeq ? iAfterSeq : 0u,
        bHasMaxEvents ? iMaxEvents : 0u,
        &bTerminalOutputCaptured,
        &bRunning,
        &bEventStreamDone,
        &tExitInfo,
        &iEventCount,
        &iNextAfterSeq,
        &iEventEndSeq,
        &sEventsJson,
        &sOutputText,
        &iOutputBytes
    );
    if ( iStatus != XWORK_OK ) {
        sFailureKind = "snapshot_failed";
        sFailureSummary = "process.terminal_resize failed";
        sFailureMessage = "failed to snapshot terminal session";
        goto cleanup;
    }

    iStatus = xwork__local_host_set_terminal_state_result(
        pHost,
        pSession->sSessionId,
        pSession->sSessionName,
        pSession->sCommand,
        pSession->sResolvedCwd,
        pSession->iSessionIndex,
        sOutputText,
        iOutputBytes,
        pSession->iTerminalCols,
        pSession->iTerminalRows,
        pSession->bStdinClosed,
        bTerminalOutputCaptured,
        bRunning,
        bEventStreamDone,
        false,
        iNextAfterSeq,
        iEventEndSeq,
        iEventCount,
        sEventsJson,
        sResizeExtraJson,
        &tExitInfo,
        true,
        "process.terminal_resize ok",
        NULL,
        NULL,
        pResult
    );

cleanup:
    if ( iStatus != XWORK_OK && sFailureKind ) {
        (void)xwork__local_host_set_terminal_state_result(
            pHost,
            sSessionId ? sSessionId : "",
            pSession ? pSession->sSessionName : "",
            pSession ? pSession->sCommand : "",
            pSession ? pSession->sResolvedCwd : NULL,
            pSession ? pSession->iSessionIndex : 0u,
            sOutputText,
            iOutputBytes,
            pSession ? pSession->iTerminalCols : iTerminalCols,
            pSession ? pSession->iTerminalRows : iTerminalRows,
            pSession ? pSession->bStdinClosed : false,
            bTerminalOutputCaptured,
            false,
            bEventStreamDone,
            false,
            iNextAfterSeq,
            iEventEndSeq,
            iEventCount,
            sEventsJson,
            sResizeExtraJson,
            &tExitInfo,
            false,
            sFailureSummary,
            sFailureKind,
            sFailureMessage,
            pResult
        );
    }
    if ( tRequest ) {
        xvoUnref(tRequest);
    }
    free(sEventsJson);
    free(sOutputText);
    return iStatus;
}

static xwork_status xwork__local_host_invoke_terminal_stop(
    xwork_local_host *pHost,
    const char *sRequestJson,
    xwork_tool_result *pResult
)
{
    xvalue tRequest = NULL;
    const char *sSessionId;
    const char *sStopText = NULL;
    xwork__local_host_terminal_session *pSession = NULL;
    xprocessexitinfo tExitInfo;
    char *sEventsJson = NULL;
    char *sOutputText = NULL;
    bool bTerminalOutputCaptured = false;
    bool bRunning = false;
    bool bEventStreamDone = false;
    bool bHasAfterSeq = false;
    bool bHasMaxEvents = false;
    size_t iAfterSeq = 0u;
    size_t iMaxEvents = 0u;
    size_t iOutputBytes = 0u;
    uint32 iEventCount = 0u;
    uint64 iNextAfterSeq = 0u;
    uint64 iEventEndSeq = 0u;
    int iRequestedStopReason = XPROC_STOP_INTERRUPT;
    const char *sFailureKind = NULL;
    const char *sFailureSummary = NULL;
    const char *sFailureMessage = NULL;
    xwork_status iStatus;

    if ( !pHost || !pResult ) {
        return XWORK_ERROR_INVALID_ARGUMENT;
    }

    iStatus = xwork__local_host_parse_request_json(sRequestJson, &tRequest);
    if ( iStatus != XWORK_OK ) {
        return iStatus;
    }

    sSessionId = xwork__local_host_request_get_text(tRequest, "session_id");
    if ( !sSessionId || !sSessionId[0] ) {
        iStatus = XWORK_ERROR_INVALID_ARGUMENT;
        sFailureKind = "invalid_request";
        sFailureSummary = "process.terminal_stop invalid request";
        sFailureMessage = "session_id is required";
        goto cleanup;
    }
    iStatus = xwork__local_host_request_get_positive_size_strict(
        tRequest,
        "after_seq",
        &bHasAfterSeq,
        &iAfterSeq
    );
    if ( iStatus != XWORK_OK ) {
        sFailureKind = "invalid_request";
        sFailureSummary = "process.terminal_stop invalid request";
        sFailureMessage = "after_seq must be a positive integer";
        goto cleanup;
    }
    iStatus = xwork__local_host_request_get_positive_size_strict(
        tRequest,
        "max_events",
        &bHasMaxEvents,
        &iMaxEvents
    );
    if ( iStatus != XWORK_OK ) {
        sFailureKind = "invalid_request";
        sFailureSummary = "process.terminal_stop invalid request";
        sFailureMessage = "max_events must be a positive integer";
        goto cleanup;
    }
    sStopText = xwork__local_host_request_get_text(tRequest, "stop");
    iStatus = xwork__local_host_parse_process_stop_reason(
        sStopText,
        &iRequestedStopReason
    );
    if ( iStatus != XWORK_OK ) {
        sFailureKind = "invalid_request";
        sFailureSummary = "process.terminal_stop invalid request";
        sFailureMessage = "stop must be one of interrupt, terminate, kill, kill_tree";
        goto cleanup;
    }

    pSession = xwork__local_host_find_terminal_session(pHost, sSessionId);
    if ( !pSession ) {
        iStatus = XWORK_ERROR_NOT_FOUND;
        sFailureKind = "not_found";
        sFailureSummary = "process.terminal_stop failed";
        sFailureMessage = "session_id not found";
        goto cleanup;
    }

    memset(&tExitInfo, 0, sizeof(tExitInfo));
    if ( pSession->pProcess && xrtProcessIsRunning(pSession->pProcess) ) {
        int iObservedStopReason = xwork__local_host_process_stop_best_effort(
            pSession->pProcess,
            iRequestedStopReason
        );
        (void)xrtProcessGetExitInfo(pSession->pProcess, &tExitInfo);
        if ( tExitInfo.iStopReason == XPROC_STOP_NONE ) {
            tExitInfo.iStopReason = iObservedStopReason;
        }
    }

    iStatus = xwork__local_host_terminal_session_snapshot(
        pSession,
        bHasAfterSeq ? iAfterSeq : 0u,
        bHasMaxEvents ? iMaxEvents : 0u,
        &bTerminalOutputCaptured,
        &bRunning,
        &bEventStreamDone,
        &tExitInfo,
        &iEventCount,
        &iNextAfterSeq,
        &iEventEndSeq,
        &sEventsJson,
        &sOutputText,
        &iOutputBytes
    );
    if ( iStatus != XWORK_OK ) {
        sFailureKind = "snapshot_failed";
        sFailureSummary = "process.terminal_stop failed";
        sFailureMessage = "failed to snapshot terminal session";
        goto cleanup;
    }

    iStatus = xwork__local_host_set_terminal_state_result(
        pHost,
        pSession->sSessionId,
        pSession->sSessionName,
        pSession->sCommand,
        pSession->sResolvedCwd,
        pSession->iSessionIndex,
        sOutputText,
        iOutputBytes,
        pSession->iTerminalCols,
        pSession->iTerminalRows,
        pSession->bStdinClosed,
        bTerminalOutputCaptured,
        bRunning,
        bEventStreamDone,
        true,
        iNextAfterSeq,
        iEventEndSeq,
        iEventCount,
        sEventsJson,
        NULL,
        &tExitInfo,
        true,
        "process.terminal_stop ok",
        NULL,
        NULL,
        pResult
    );
    if ( iStatus == XWORK_OK ) {
        xwork__local_host_remove_terminal_session(pHost, pSession);
        pSession = NULL;
    }

cleanup:
    if ( iStatus != XWORK_OK && sFailureKind ) {
        (void)xwork__local_host_set_terminal_state_result(
            pHost,
            sSessionId ? sSessionId : "",
            pSession ? pSession->sSessionName : "",
            pSession ? pSession->sCommand : "",
            pSession ? pSession->sResolvedCwd : NULL,
            pSession ? pSession->iSessionIndex : 0u,
            sOutputText,
            iOutputBytes,
            pSession ? pSession->iTerminalCols : 0u,
            pSession ? pSession->iTerminalRows : 0u,
            pSession ? pSession->bStdinClosed : false,
            bTerminalOutputCaptured,
            false,
            bEventStreamDone,
            false,
            iNextAfterSeq,
            iEventEndSeq,
            iEventCount,
            sEventsJson,
            NULL,
            &tExitInfo,
            false,
            sFailureSummary,
            sFailureKind,
            sFailureMessage,
            pResult
        );
    }
    if ( tRequest ) {
        xvoUnref(tRequest);
    }
    free(sEventsJson);
    free(sOutputText);
    return iStatus;
}

static char *xwork__local_host_vcs_extract_branch(const char *sStatus)
{
    const char *sStart;
    const char *sEnd;
    size_t iLength;
    char *sBranch;

    if ( !sStatus || strncmp(sStatus, "## ", 3u) != 0 ) {
        return xwork__dup_cstr("");
    }

    sStart = sStatus + 3u;
    sEnd = sStart;
    while ( *sEnd &&
            *sEnd != '\r' &&
            *sEnd != '\n' &&
            *sEnd != ' ' &&
            strncmp(sEnd, "...", 3u) != 0 ) {
        ++sEnd;
    }
    iLength = (size_t)(sEnd - sStart);
    sBranch = (char *)malloc(iLength + 1u);
    if ( !sBranch ) {
        return NULL;
    }
    memcpy(sBranch, sStart, iLength);
    sBranch[iLength] = '\0';
    return sBranch;
}

static bool xwork__local_host_vcs_status_is_dirty(const char *sStatus)
{
    const char *sLine;

    if ( !sStatus || !sStatus[0] ) {
        return false;
    }

    sLine = sStatus;
    while ( *sLine ) {
        if ( strncmp(sLine, "## ", 3u) != 0 &&
             *sLine != '\r' &&
             *sLine != '\n' ) {
            return true;
        }
        while ( *sLine && *sLine != '\n' ) {
            ++sLine;
        }
        if ( *sLine == '\n' ) {
            ++sLine;
        }
    }
    return false;
}

static xwork_status xwork__local_host_invoke_vcs(
    xwork_local_host *pHost,
    const char *sOperationId,
    const char *sRequestJson,
    xwork_tool_result *pResult
)
{
    xvalue tRequest = NULL;
    const char *sPath = ".";
    char *sResolvedPath = NULL;
    char *sQuotedPath = NULL;
    char *sCommand = NULL;
    char *sOutput = NULL;
    char *sEscapedPath = NULL;
    char *sEscapedResolvedPath = NULL;
    char *sEscapedOutput = NULL;
    char *sEscapedBranch = NULL;
    char *sEscapedFailureMessage = NULL;
    char *sFailureOutputText = NULL;
    char *sBranch = NULL;
    char *sOutputText = NULL;
    const char *sOutputKey = "status";
    const char *sSummary = "vcs.status ok";
    const char *sCommandText = NULL;
    const char *sFailureKind = NULL;
    const char *sFailureSummary = NULL;
    const char *sFailureMessage = NULL;
    bool bTruncated = false;
    bool bStaged = false;
    bool bHasStaged = false;
    bool bDirty = false;
    size_t iRequestMaxBytes = 0u;
    size_t iCaptureLimit;
    size_t iLimit = 20u;
    bool bHasLimit = false;
    int iExitCode = -1;
    xwork_status iStatus;

    if ( !pHost || !pResult ) {
        return XWORK_ERROR_INVALID_ARGUMENT;
    }
    if ( !sOperationId ) {
        return XWORK_ERROR_UNSUPPORTED;
    }
    if ( (strcmp(sOperationId, XWORK_HOST_VCS_STATUS) == 0 && !pHost->bEnableVcsStatus) ||
         (strcmp(sOperationId, XWORK_HOST_VCS_DIFF) == 0 && !pHost->bEnableVcsDiff) ||
         (strcmp(sOperationId, XWORK_HOST_VCS_LOG) == 0 && !pHost->bEnableVcsLog) ||
         (strcmp(sOperationId, XWORK_HOST_VCS_BRANCH) == 0 && !pHost->bEnableVcsBranch) ) {
        return XWORK_ERROR_UNSUPPORTED;
    }
    if ( strcmp(sOperationId, XWORK_HOST_VCS_STATUS) != 0 &&
         strcmp(sOperationId, XWORK_HOST_VCS_DIFF) != 0 &&
         strcmp(sOperationId, XWORK_HOST_VCS_LOG) != 0 &&
         strcmp(sOperationId, XWORK_HOST_VCS_BRANCH) != 0 ) {
        return XWORK_ERROR_UNSUPPORTED;
    }

    iStatus = xwork__local_host_parse_request_json(sRequestJson, &tRequest);
    if ( iStatus != XWORK_OK ) {
        return iStatus;
    }

    if ( xwork__local_host_request_get_text(tRequest, "path") ) {
        sPath = xwork__local_host_request_get_text(tRequest, "path");
    }
    iStatus = xwork__local_host_request_get_bool(
        tRequest,
        "staged",
        &bHasStaged,
        &bStaged
    );
    if ( iStatus != XWORK_OK ) {
        sFailureKind = "invalid_request";
        sFailureSummary = "vcs request invalid";
        goto cleanup;
    }
    if ( !bHasStaged ) {
        (void)xwork__local_host_request_get_bool(
            tRequest,
            "cached",
            &bHasStaged,
            &bStaged
        );
    }
    if ( xwork__local_host_request_get_size(tRequest, "limit", &iLimit) ) {
        bHasLimit = true;
        if ( iLimit == 0u ) {
            iLimit = 1u;
        } else if ( iLimit > 200u ) {
            iLimit = 200u;
        }
    }

    (void)xwork__local_host_request_get_size(tRequest, "max_output_bytes", &iRequestMaxBytes);
    iCaptureLimit = xwork__local_host_effective_limit(
        pHost->iMaxProcessOutputBytes,
        iRequestMaxBytes
    );

    sResolvedPath = xwork__local_host_resolve_path(pHost, sPath);
    if ( !sResolvedPath ) {
        iStatus = XWORK_ERROR_NO_MEMORY;
        goto cleanup;
    }
    iStatus = xwork__local_host_check_filesystem_path(
        pHost,
        sResolvedPath,
        &sFailureMessage
    );
    if ( iStatus != XWORK_OK ) {
        sFailureKind = "path_denied";
        sFailureSummary = "vcs operation denied by path policy";
        goto cleanup;
    }

    sQuotedPath = xwork__local_host_quote_shell_arg(sResolvedPath);
    if ( !sQuotedPath ) {
        iStatus = XWORK_ERROR_INVALID_ARGUMENT;
        goto cleanup;
    }
    if ( strcmp(sOperationId, XWORK_HOST_VCS_STATUS) == 0 ) {
        sOutputKey = "status";
        sSummary = "vcs.status ok";
        sCommandText = "git status --short --branch";
        sCommand = xwork__dup_printf(
            "git -C %s status --short --branch 2>&1",
            sQuotedPath
        );
    } else if ( strcmp(sOperationId, XWORK_HOST_VCS_DIFF) == 0 ) {
        sOutputKey = "diff";
        sSummary = "vcs.diff ok";
        sCommandText = bStaged ? "git diff --staged --no-ext-diff" : "git diff --no-ext-diff";
        sCommand = xwork__dup_printf(
            "git -C %s diff %s--no-ext-diff 2>&1",
            sQuotedPath,
            bStaged ? "--staged " : ""
        );
    } else if ( strcmp(sOperationId, XWORK_HOST_VCS_LOG) == 0 ) {
        sOutputKey = "log";
        sSummary = "vcs.log ok";
        sCommandText = "git log --oneline";
        sCommand = xwork__dup_printf(
            "git -C %s log --oneline -n %lu 2>&1",
            sQuotedPath,
            (unsigned long)(bHasLimit ? iLimit : 20u)
        );
    } else {
        sOutputKey = "branch_status";
        sSummary = "vcs.branch ok";
        sCommandText = "git status --short --branch";
        sCommand = xwork__dup_printf(
            "git -C %s status --short --branch 2>&1",
            sQuotedPath
        );
    }
    if ( !sCommand ) {
        iStatus = XWORK_ERROR_NO_MEMORY;
        goto cleanup;
    }
    iStatus = xwork__local_host_check_command_policy(
        pHost,
        sCommand,
        &sFailureKind,
        &sFailureMessage
    );
    if ( iStatus != XWORK_OK ) {
        sFailureSummary = "vcs operation denied by command policy";
        goto cleanup;
    }

    iStatus = xwork__local_host_capture_command_output(
        sCommand,
        iCaptureLimit,
        &sOutput,
        &iExitCode,
        &bTruncated
    );
    if ( iStatus != XWORK_OK ) {
        goto cleanup;
    }
    if ( iExitCode != 0 ) {
        sFailureKind = "git_failed";
        sFailureSummary = "vcs operation failed";
        iStatus = XWORK_ERROR_EXTERNAL_FAILURE;
        goto cleanup;
    }

    if ( strcmp(sOperationId, XWORK_HOST_VCS_BRANCH) == 0 ) {
        sBranch = xwork__local_host_vcs_extract_branch(sOutput);
        if ( !sBranch ) {
            iStatus = XWORK_ERROR_NO_MEMORY;
            goto cleanup;
        }
        bDirty = xwork__local_host_vcs_status_is_dirty(sOutput);
    }

    iStatus = xwork__local_host_json_escape(sPath, &sEscapedPath);
    if ( iStatus != XWORK_OK ) goto cleanup;
    iStatus = xwork__local_host_json_escape(sResolvedPath, &sEscapedResolvedPath);
    if ( iStatus != XWORK_OK ) goto cleanup;
    iStatus = xwork__local_host_json_escape(sOutput, &sEscapedOutput);
    if ( iStatus != XWORK_OK ) goto cleanup;
    if ( sBranch ) {
        iStatus = xwork__local_host_json_escape(sBranch, &sEscapedBranch);
        if ( iStatus != XWORK_OK ) goto cleanup;
    }

    if ( strcmp(sOperationId, XWORK_HOST_VCS_BRANCH) == 0 ) {
        sOutputText = xwork__dup_printf(
            "{\"ok\":true,\"operation\":\"%s\",\"path\":\"%s\",\"resolved_path\":\"%s\","
            "\"branch\":\"%s\",\"dirty\":%s,\"branch_status\":\"%s\",\"truncated\":%s}",
            sOperationId,
            sEscapedPath,
            sEscapedResolvedPath,
            sEscapedBranch ? sEscapedBranch : "",
            bDirty ? "true" : "false",
            sEscapedOutput,
            bTruncated ? "true" : "false"
        );
    } else {
        sOutputText = xwork__dup_printf(
            "{\"ok\":true,\"operation\":\"%s\",\"path\":\"%s\",\"resolved_path\":\"%s\","
            "\"command\":\"%s\",\"staged\":%s,\"limit\":%lu,\"%s\":\"%s\","
            "\"truncated\":%s}",
            sOperationId,
            sEscapedPath,
            sEscapedResolvedPath,
            sCommandText ? sCommandText : "",
            bStaged ? "true" : "false",
            (unsigned long)(bHasLimit ? iLimit : 0u),
            sOutputKey,
            sEscapedOutput,
            bTruncated ? "true" : "false"
        );
    }
    if ( !sOutputText ) {
        iStatus = XWORK_ERROR_NO_MEMORY;
        goto cleanup;
    }

    iStatus = xwork__local_host_set_result(
        pHost,
        sOutputText,
        sSummary,
        pResult
    );

cleanup:
    if ( iStatus != XWORK_OK && sFailureKind ) {
        if ( !sEscapedPath ) {
            (void)xwork__local_host_json_escape(sPath ? sPath : "", &sEscapedPath);
        }
        if ( !sEscapedResolvedPath ) {
            (void)xwork__local_host_json_escape(
                sResolvedPath ? sResolvedPath : "",
                &sEscapedResolvedPath
            );
        }
        (void)xwork__local_host_json_escape(
            sFailureMessage ? sFailureMessage : "",
            &sEscapedFailureMessage
        );
        sFailureOutputText = xwork__dup_printf(
            "{\"ok\":false,\"operation\":\"%s\",\"path\":\"%s\","
            "\"resolved_path\":\"%s\",\"error_kind\":\"%s\","
            "\"error_message\":\"%s\"}",
            sOperationId ? sOperationId : "",
            sEscapedPath ? sEscapedPath : "",
            sEscapedResolvedPath ? sEscapedResolvedPath : "",
            sFailureKind,
            sEscapedFailureMessage ? sEscapedFailureMessage : ""
        );
        if ( sFailureOutputText ) {
            (void)xwork__local_host_set_result(
                pHost,
                sFailureOutputText,
                sFailureSummary ? sFailureSummary : "vcs operation failed",
                pResult
            );
        }
    }
    if ( tRequest ) {
        xvoUnref(tRequest);
    }
    free(sResolvedPath);
    free(sQuotedPath);
    free(sCommand);
    free(sOutput);
    free(sEscapedPath);
    free(sEscapedResolvedPath);
    free(sEscapedOutput);
    free(sEscapedBranch);
    free(sEscapedFailureMessage);
    free(sFailureOutputText);
    free(sBranch);
    free(sOutputText);
    return iStatus;
}

static xwork_status xwork__local_host_invoke_filesystem_cb(
    const char *sOperationId,
    const char *sRequestJson,
    xwork_tool_result *pResult,
    void *pUserData
)
{
    if ( !sOperationId ) {
        return XWORK_ERROR_UNSUPPORTED;
    }
    if ( strcmp(sOperationId, XWORK_HOST_FILESYSTEM_READ_TEXT) == 0 ) {
        return xwork__local_host_invoke_filesystem(
            (xwork_local_host *)pUserData,
            sRequestJson,
            pResult
        );
    }
    if ( strcmp(sOperationId, XWORK_HOST_FILESYSTEM_WRITE_TEXT) == 0 ) {
        return xwork__local_host_invoke_filesystem_write_text(
            (xwork_local_host *)pUserData,
            sRequestJson,
            pResult
        );
    }
    if ( strcmp(sOperationId, XWORK_HOST_FILESYSTEM_LIST) == 0 ) {
        return xwork__local_host_invoke_filesystem_scan(
            (xwork_local_host *)pUserData,
            sRequestJson,
            XWORK_HOST_FILESYSTEM_LIST,
            false,
            false,
            pResult
        );
    }
    if ( strcmp(sOperationId, XWORK_HOST_FILESYSTEM_STAT) == 0 ) {
        return xwork__local_host_invoke_filesystem_stat(
            (xwork_local_host *)pUserData,
            sRequestJson,
            pResult
        );
    }
    if ( strcmp(sOperationId, XWORK_HOST_FILESYSTEM_GLOB) == 0 ) {
        return xwork__local_host_invoke_filesystem_scan(
            (xwork_local_host *)pUserData,
            sRequestJson,
            XWORK_HOST_FILESYSTEM_GLOB,
            true,
            true,
            pResult
        );
    }
    if ( strcmp(sOperationId, XWORK_HOST_FILESYSTEM_MKDIR) == 0 ) {
        return xwork__local_host_invoke_filesystem_mkdir(
            (xwork_local_host *)pUserData,
            sRequestJson,
            pResult
        );
    }
    if ( strcmp(sOperationId, XWORK_HOST_FILESYSTEM_MOVE) == 0 ) {
        return xwork__local_host_invoke_filesystem_move(
            (xwork_local_host *)pUserData,
            sRequestJson,
            pResult
        );
    }
    if ( strcmp(sOperationId, XWORK_HOST_FILESYSTEM_DELETE) == 0 ) {
        return xwork__local_host_invoke_filesystem_delete(
            (xwork_local_host *)pUserData,
            sRequestJson,
            pResult
        );
    }
    if ( strcmp(sOperationId, XWORK_HOST_FILESYSTEM_APPLY_PATCH) == 0 ) {
        return xwork__local_host_invoke_filesystem_apply_patch(
            (xwork_local_host *)pUserData,
            sRequestJson,
            pResult
        );
    }
    return XWORK_ERROR_UNSUPPORTED;
}

static xwork_status xwork__local_host_invoke_editor_open_buffer(
    xwork_local_host *pHost,
    const char *sRequestJson,
    xwork_tool_result *pResult
)
{
    xvalue tRequest = NULL;
    const char *sPath;
    const char *sFailureMessage = NULL;
    char *sResolvedPath = NULL;
    char *sText = NULL;
    xwork__local_editor_buffer *pBuffer;
    size_t iFileSizeBytes = 0u;
    size_t iBytesRead = 0u;
    size_t iSelectionStart = 0u;
    size_t iSelectionEnd = 0u;
    bool bTruncated = false;
    bool bEof = false;
    xwork_status iStatus;

    if ( !pHost || !pResult ) {
        return XWORK_ERROR_INVALID_ARGUMENT;
    }
    if ( !pHost->bEnableEditorBuffers ) {
        return XWORK_ERROR_UNSUPPORTED;
    }

    iStatus = xwork__local_host_parse_request_json(sRequestJson, &tRequest);
    if ( iStatus != XWORK_OK ) {
        return iStatus;
    }

    sPath = xwork__local_host_request_get_text(tRequest, "path");
    if ( !sPath || !sPath[0] ) {
        iStatus = xwork__local_host_set_editor_error_result(
            pHost,
            XWORK_HOST_EDITOR_OPEN_BUFFER,
            NULL,
            sPath,
            NULL,
            "invalid_request",
            "path is required",
            "editor.open_buffer invalid request",
            pResult
        );
        goto cleanup;
    }
    (void)xwork__local_host_request_get_size(tRequest, "selection_start", &iSelectionStart);
    (void)xwork__local_host_request_get_size(tRequest, "selection_end", &iSelectionEnd);

    sResolvedPath = xwork__local_host_resolve_path(pHost, sPath);
    if ( !sResolvedPath ) {
        iStatus = XWORK_ERROR_NO_MEMORY;
        goto cleanup;
    }
    iStatus = xwork__local_host_check_filesystem_path(
        pHost,
        sResolvedPath,
        &sFailureMessage
    );
    if ( iStatus != XWORK_OK ) {
        (void)xwork__local_host_set_editor_error_result(
            pHost,
            XWORK_HOST_EDITOR_OPEN_BUFFER,
            NULL,
            sPath,
            sResolvedPath,
            "path_denied",
            sFailureMessage,
            "editor.open_buffer denied by path policy",
            pResult
        );
        goto cleanup;
    }

    pBuffer = xwork__local_host_find_editor_buffer(pHost, NULL, sResolvedPath);
    if ( !pBuffer ) {
        iStatus = xwork__local_host_read_text_file(
            sResolvedPath,
            0u,
            pHost->iMaxReadBytes ? pHost->iMaxReadBytes : XWORK__LOCAL_HOST_DEFAULT_READ_BYTES,
            &sText,
            &iFileSizeBytes,
            &iBytesRead,
            &bTruncated,
            &bEof
        );
        if ( iStatus != XWORK_OK ) {
            (void)xwork__local_host_set_editor_error_result(
                pHost,
                XWORK_HOST_EDITOR_OPEN_BUFFER,
                NULL,
                sPath,
                sResolvedPath,
                "read_failed",
                "failed to read buffer text",
                "editor.open_buffer failed",
                pResult
            );
            goto cleanup;
        }

        pBuffer = (xwork__local_editor_buffer *)calloc(1u, sizeof(*pBuffer));
        if ( !pBuffer ) {
            iStatus = XWORK_ERROR_NO_MEMORY;
            goto cleanup;
        }
        pBuffer->sBufferId = xwork__dup_printf(
            "buffer-%llu",
            (unsigned long long)(++pHost->iNextEditorBufferId)
        );
        pBuffer->sPath = xwork__dup_cstr(sPath);
        pBuffer->sResolvedPath = xwork__dup_cstr(sResolvedPath);
        pBuffer->sText = sText;
        sText = NULL;
        if ( !pBuffer->sBufferId ||
             !pBuffer->sPath ||
             !pBuffer->sResolvedPath ||
             !pBuffer->sText ) {
            free(pBuffer->sBufferId);
            free(pBuffer->sPath);
            free(pBuffer->sResolvedPath);
            free(pBuffer->sText);
            free(pBuffer);
            iStatus = XWORK_ERROR_NO_MEMORY;
            goto cleanup;
        }
        pBuffer->pNext = (xwork__local_editor_buffer *)pHost->pEditorBuffers;
        pHost->pEditorBuffers = pBuffer;
    }

    if ( iSelectionEnd < iSelectionStart ) {
        iSelectionEnd = iSelectionStart;
    }
    if ( pBuffer->sText && iSelectionStart > strlen(pBuffer->sText) ) {
        iSelectionStart = strlen(pBuffer->sText);
    }
    if ( pBuffer->sText && iSelectionEnd > strlen(pBuffer->sText) ) {
        iSelectionEnd = strlen(pBuffer->sText);
    }
    pBuffer->iSelectionStart = iSelectionStart;
    pBuffer->iSelectionEnd = iSelectionEnd;

    iStatus = xwork__local_host_set_editor_buffer_result(
        pHost,
        XWORK_HOST_EDITOR_OPEN_BUFFER,
        pBuffer,
        false,
        "editor.open_buffer ok",
        pResult
    );

cleanup:
    if ( tRequest ) {
        xvoUnref(tRequest);
    }
    free(sResolvedPath);
    free(sText);
    return iStatus;
}

static xwork_status xwork__local_host_invoke_editor_apply_edit(
    xwork_local_host *pHost,
    const char *sRequestJson,
    xwork_tool_result *pResult
)
{
    xvalue tRequest = NULL;
    const char *sBufferId;
    const char *sPath;
    const char *sNewText;
    const char *sFailureMessage = NULL;
    char *sResolvedPath = NULL;
    char *sUpdatedText = NULL;
    xwork__local_editor_buffer *pBuffer;
    size_t iRangeStart = 0u;
    size_t iRangeEnd = 0u;
    size_t iTextLength;
    size_t iNewTextLength;
    xwork_status iStatus;

    if ( !pHost || !pResult ) {
        return XWORK_ERROR_INVALID_ARGUMENT;
    }
    if ( !pHost->bEnableEditorBuffers ) {
        return XWORK_ERROR_UNSUPPORTED;
    }

    iStatus = xwork__local_host_parse_request_json(sRequestJson, &tRequest);
    if ( iStatus != XWORK_OK ) {
        return iStatus;
    }

    sBufferId = xwork__local_host_request_get_text(tRequest, "buffer_id");
    sPath = xwork__local_host_request_get_text(tRequest, "path");
    sNewText = xwork__local_host_request_get_text(tRequest, "new_text");
    (void)xwork__local_host_request_get_size(tRequest, "range_start", &iRangeStart);
    (void)xwork__local_host_request_get_size(tRequest, "range_end", &iRangeEnd);

    if ( (!sBufferId || !sBufferId[0]) && (!sPath || !sPath[0]) ) {
        iStatus = xwork__local_host_set_editor_error_result(
            pHost,
            XWORK_HOST_EDITOR_APPLY_EDIT,
            sBufferId,
            sPath,
            NULL,
            "invalid_request",
            "buffer_id or path is required",
            "editor.apply_edit invalid request",
            pResult
        );
        goto cleanup;
    }
    if ( !sNewText ) {
        iStatus = xwork__local_host_set_editor_error_result(
            pHost,
            XWORK_HOST_EDITOR_APPLY_EDIT,
            sBufferId,
            sPath,
            NULL,
            "invalid_request",
            "new_text is required",
            "editor.apply_edit invalid request",
            pResult
        );
        goto cleanup;
    }

    if ( sPath && sPath[0] ) {
        sResolvedPath = xwork__local_host_resolve_path(pHost, sPath);
        if ( !sResolvedPath ) {
            iStatus = XWORK_ERROR_NO_MEMORY;
            goto cleanup;
        }
        iStatus = xwork__local_host_check_filesystem_path(
            pHost,
            sResolvedPath,
            &sFailureMessage
        );
        if ( iStatus != XWORK_OK ) {
            (void)xwork__local_host_set_editor_error_result(
                pHost,
                XWORK_HOST_EDITOR_APPLY_EDIT,
                sBufferId,
                sPath,
                sResolvedPath,
                "path_denied",
                sFailureMessage,
                "editor.apply_edit denied by path policy",
                pResult
            );
            goto cleanup;
        }
    }

    pBuffer = xwork__local_host_find_editor_buffer(pHost, sBufferId, sResolvedPath);
    if ( !pBuffer ) {
        iStatus = xwork__local_host_set_editor_error_result(
            pHost,
            XWORK_HOST_EDITOR_APPLY_EDIT,
            sBufferId,
            sPath,
            sResolvedPath,
            "not_found",
            "buffer not found",
            "editor.apply_edit failed",
            pResult
        );
        goto cleanup;
    }

    iTextLength = pBuffer->sText ? strlen(pBuffer->sText) : 0u;
    iNewTextLength = strlen(sNewText);
    if ( iRangeEnd < iRangeStart ) {
        iStatus = xwork__local_host_set_editor_error_result(
            pHost,
            XWORK_HOST_EDITOR_APPLY_EDIT,
            sBufferId,
            sPath,
            sResolvedPath,
            "invalid_request",
            "range_end must be >= range_start",
            "editor.apply_edit invalid request",
            pResult
        );
        goto cleanup;
    }
    if ( iRangeStart > iTextLength || iRangeEnd > iTextLength ) {
        iStatus = xwork__local_host_set_editor_error_result(
            pHost,
            XWORK_HOST_EDITOR_APPLY_EDIT,
            sBufferId,
            sPath,
            sResolvedPath,
            "invalid_request",
            "edit range is outside buffer",
            "editor.apply_edit invalid request",
            pResult
        );
        goto cleanup;
    }

    sUpdatedText = (char *)calloc(
        iTextLength - (iRangeEnd - iRangeStart) + iNewTextLength + 1u,
        sizeof(char)
    );
    if ( !sUpdatedText ) {
        iStatus = XWORK_ERROR_NO_MEMORY;
        goto cleanup;
    }
    if ( iRangeStart > 0u ) {
        memcpy(sUpdatedText, pBuffer->sText, iRangeStart);
    }
    if ( iNewTextLength > 0u ) {
        memcpy(sUpdatedText + iRangeStart, sNewText, iNewTextLength);
    }
    if ( iRangeEnd < iTextLength ) {
        memcpy(
            sUpdatedText + iRangeStart + iNewTextLength,
            pBuffer->sText + iRangeEnd,
            iTextLength - iRangeEnd
        );
    }

    free(pBuffer->sText);
    pBuffer->sText = sUpdatedText;
    sUpdatedText = NULL;
    pBuffer->bDirty = true;
    pBuffer->iSelectionStart = iRangeStart;
    pBuffer->iSelectionEnd = iRangeStart + iNewTextLength;

    iStatus = xwork__local_host_set_editor_buffer_result(
        pHost,
        XWORK_HOST_EDITOR_APPLY_EDIT,
        pBuffer,
        true,
        "editor.apply_edit ok",
        pResult
    );

cleanup:
    if ( tRequest ) {
        xvoUnref(tRequest);
    }
    free(sResolvedPath);
    free(sUpdatedText);
    return iStatus;
}

static xwork_status xwork__local_host_invoke_editor_cb(
    const char *sOperationId,
    const char *sRequestJson,
    xwork_tool_result *pResult,
    void *pUserData
)
{
    if ( !sOperationId ) {
        return XWORK_ERROR_UNSUPPORTED;
    }
    if ( strcmp(sOperationId, XWORK_HOST_EDITOR_OPEN_BUFFER) == 0 ) {
        return xwork__local_host_invoke_editor_open_buffer(
            (xwork_local_host *)pUserData,
            sRequestJson,
            pResult
        );
    }
    if ( strcmp(sOperationId, XWORK_HOST_EDITOR_APPLY_EDIT) == 0 ) {
        return xwork__local_host_invoke_editor_apply_edit(
            (xwork_local_host *)pUserData,
            sRequestJson,
            pResult
        );
    }
    return XWORK_ERROR_UNSUPPORTED;
}

static xwork_status xwork__local_host_invoke_process_cb_ex(
    const char *sOperationId,
    const char *sRequestJson,
    const xwork_host_invoke_context *pContext,
    xwork_tool_result *pResult,
    void *pUserData
);

static xwork_status xwork__local_host_invoke_process_cb(
    const char *sOperationId,
    const char *sRequestJson,
    xwork_tool_result *pResult,
    void *pUserData
)
{
    return xwork__local_host_invoke_process_cb_ex(
        sOperationId,
        sRequestJson,
        NULL,
        pResult,
        pUserData
    );
}

static xwork_status xwork__local_host_invoke_process_cb_ex(
    const char *sOperationId,
    const char *sRequestJson,
    const xwork_host_invoke_context *pContext,
    xwork_tool_result *pResult,
    void *pUserData
)
{
    xwork_local_host *pHost = (xwork_local_host *)pUserData;

    if ( !sOperationId ) {
        return XWORK_ERROR_UNSUPPORTED;
    }
    if ( strcmp(sOperationId, XWORK_HOST_PROCESS_EXEC) == 0 ) {
        return xwork__local_host_invoke_process(
            pHost,
            sRequestJson,
            pContext,
            pResult
        );
    }
    if ( strcmp(sOperationId, XWORK_HOST_PROCESS_START_TERMINAL) == 0 ) {
        return xwork__local_host_invoke_start_terminal(
            pHost,
            sRequestJson,
            pResult
        );
    }
    if ( strcmp(sOperationId, XWORK_HOST_PROCESS_LIST_TERMINALS) == 0 ) {
        return xwork__local_host_invoke_list_terminals(
            pHost,
            sRequestJson,
            pResult
        );
    }
    if ( strcmp(sOperationId, XWORK_HOST_PROCESS_TERMINAL_READ) == 0 ) {
        return xwork__local_host_invoke_terminal_read(
            pHost,
            sRequestJson,
            pResult
        );
    }
    if ( strcmp(sOperationId, XWORK_HOST_PROCESS_TERMINAL_WRITE) == 0 ) {
        return xwork__local_host_invoke_terminal_write(
            pHost,
            sRequestJson,
            pResult
        );
    }
    if ( strcmp(sOperationId, XWORK_HOST_PROCESS_TERMINAL_RESIZE) == 0 ) {
        return xwork__local_host_invoke_terminal_resize(
            pHost,
            sRequestJson,
            pResult
        );
    }
    if ( strcmp(sOperationId, XWORK_HOST_PROCESS_TERMINAL_STOP) == 0 ) {
        return xwork__local_host_invoke_terminal_stop(
            pHost,
            sRequestJson,
            pResult
        );
    }
    return XWORK_ERROR_UNSUPPORTED;
}

static xwork_status xwork__local_host_invoke_vcs_cb(
    const char *sOperationId,
    const char *sRequestJson,
    xwork_tool_result *pResult,
    void *pUserData
)
{
    if ( !sOperationId ||
         (strcmp(sOperationId, XWORK_HOST_VCS_STATUS) != 0 &&
          strcmp(sOperationId, XWORK_HOST_VCS_DIFF) != 0 &&
          strcmp(sOperationId, XWORK_HOST_VCS_LOG) != 0 &&
          strcmp(sOperationId, XWORK_HOST_VCS_BRANCH) != 0) ) {
        return XWORK_ERROR_UNSUPPORTED;
    }
    return xwork__local_host_invoke_vcs(
        (xwork_local_host *)pUserData,
        sOperationId,
        sRequestJson,
        pResult
    );
}

void xwork_host_service_init(xwork_host_service *pService)
{
    if ( pService ) {
        memset(pService, 0, sizeof(*pService));
    }
}

void xwork_host_services_init(xwork_host_services *pServices)
{
    if ( pServices ) {
        memset(pServices, 0, sizeof(*pServices));
    }
}

void xwork_local_host_options_init(xwork_local_host_options *pOptions)
{
    if ( pOptions ) {
        memset(pOptions, 0, sizeof(*pOptions));
        pOptions->iMaxReadBytes = XWORK__LOCAL_HOST_DEFAULT_READ_BYTES;
        pOptions->iMaxProcessInputBytes = XWORK__LOCAL_HOST_DEFAULT_PROCESS_INPUT_BYTES;
        pOptions->iMaxProcessEnvEntries = XWORK__LOCAL_HOST_DEFAULT_PROCESS_ENV_ENTRIES;
        pOptions->iMaxProcessOutputBytes = XWORK__LOCAL_HOST_DEFAULT_PROCESS_OUTPUT_BYTES;
        pOptions->bEnableFilesystemReadText = true;
        pOptions->bEnableFilesystemWriteText = true;
        pOptions->bEnableProcessExec = true;
        pOptions->bEnableVcsStatus = true;
        pOptions->bEnableVcsDiff = true;
        pOptions->bEnableVcsLog = true;
        pOptions->bEnableVcsBranch = true;
        pOptions->bEnableEditorBuffers = true;
    }
}

void xwork_local_host_init(xwork_local_host *pHost)
{
    if ( pHost ) {
        memset(pHost, 0, sizeof(*pHost));
    }
}

void xwork_local_host_reset(xwork_local_host *pHost)
{
    if ( !pHost ) {
        return;
    }

    xwork__local_host_reset_terminal_sessions(pHost);
    xwork__local_host_free_editor_buffers(pHost);
    xwork__free_cstr(&pHost->sDefaultWorkingDirectory);
    xwork__free_str_array(
        &pHost->psFilesystemAllowPathPrefixes,
        &pHost->iFilesystemAllowPathPrefixCount
    );
    xwork__free_str_array(
        &pHost->psFilesystemDenyPathPrefixes,
        &pHost->iFilesystemDenyPathPrefixCount
    );
    xwork__free_str_array(
        &pHost->psCommandAllowPatterns,
        &pHost->iCommandAllowPatternCount
    );
    xwork__free_str_array(
        &pHost->psCommandDenyPatterns,
        &pHost->iCommandDenyPatternCount
    );
    xwork__free_cstr(&pHost->sLastOutputText);
    xwork__free_cstr(&pHost->sLastVisibleSummary);
    xwork_local_host_init(pHost);
}

xwork_status xwork_local_host_configure_services(
    xwork_local_host *pHost,
    const xwork_local_host_options *pOptions,
    xwork_host_services *pServices
)
{
    xwork_status iStatus;

    if ( !pHost || !pOptions || !pServices ) {
        return XWORK_ERROR_INVALID_ARGUMENT;
    }

    xwork_local_host_reset(pHost);
    if ( pOptions->sDefaultWorkingDirectory && pOptions->sDefaultWorkingDirectory[0] ) {
        pHost->sDefaultWorkingDirectory = xwork__dup_cstr(
            pOptions->sDefaultWorkingDirectory
        );
        if ( !pHost->sDefaultWorkingDirectory ) {
            return XWORK_ERROR_NO_MEMORY;
        }
    }

    pHost->bEnforceFilesystemRoot = pOptions->bEnforceFilesystemRoot;
    iStatus = xwork__local_host_copy_path_prefixes(
        &pHost->psFilesystemAllowPathPrefixes,
        &pHost->iFilesystemAllowPathPrefixCount,
        pOptions->psFilesystemAllowPathPrefixes,
        pOptions->iFilesystemAllowPathPrefixCount
    );
    if ( iStatus != XWORK_OK ) {
        xwork_local_host_reset(pHost);
        return iStatus;
    }
    iStatus = xwork__local_host_copy_path_prefixes(
        &pHost->psFilesystemDenyPathPrefixes,
        &pHost->iFilesystemDenyPathPrefixCount,
        pOptions->psFilesystemDenyPathPrefixes,
        pOptions->iFilesystemDenyPathPrefixCount
    );
    if ( iStatus != XWORK_OK ) {
        xwork_local_host_reset(pHost);
        return iStatus;
    }
    iStatus = xwork__local_host_copy_path_prefixes(
        &pHost->psCommandAllowPatterns,
        &pHost->iCommandAllowPatternCount,
        pOptions->psCommandAllowPatterns,
        pOptions->iCommandAllowPatternCount
    );
    if ( iStatus != XWORK_OK ) {
        xwork_local_host_reset(pHost);
        return iStatus;
    }
    iStatus = xwork__local_host_copy_path_prefixes(
        &pHost->psCommandDenyPatterns,
        &pHost->iCommandDenyPatternCount,
        pOptions->psCommandDenyPatterns,
        pOptions->iCommandDenyPatternCount
    );
    if ( iStatus != XWORK_OK ) {
        xwork_local_host_reset(pHost);
        return iStatus;
    }
    pHost->bDenyDestructiveCommands = pOptions->bDenyDestructiveCommands;

    pHost->iMaxReadBytes = pOptions->iMaxReadBytes;
    pHost->iMaxProcessInputBytes = pOptions->iMaxProcessInputBytes;
    pHost->iMaxProcessEnvEntries = pOptions->iMaxProcessEnvEntries;
    pHost->iMaxProcessOutputBytes = pOptions->iMaxProcessOutputBytes;
    pHost->bEnableFilesystemReadText = pOptions->bEnableFilesystemReadText;
    pHost->bEnableFilesystemWriteText = pOptions->bEnableFilesystemWriteText;
    pHost->bEnableProcessExec = pOptions->bEnableProcessExec;
    pHost->bEnableVcsStatus = pOptions->bEnableVcsStatus;
    pHost->bEnableVcsDiff = pOptions->bEnableVcsDiff;
    pHost->bEnableVcsLog = pOptions->bEnableVcsLog;
    pHost->bEnableVcsBranch = pOptions->bEnableVcsBranch;
    pHost->bEnableEditorBuffers = pOptions->bEnableEditorBuffers;

    xwork_host_service_init(&pServices->tFilesystem);
    xwork_host_service_init(&pServices->tProcess);
    xwork_host_service_init(&pServices->tVcs);
    xwork_host_service_init(&pServices->tEditor);

    if ( pHost->bEnableFilesystemReadText || pHost->bEnableFilesystemWriteText ) {
        pServices->tFilesystem.pfnInvoke = xwork__local_host_invoke_filesystem_cb;
        pServices->tFilesystem.pUserData = pHost;
    }
    if ( pHost->bEnableProcessExec ) {
        pServices->tProcess.pfnInvoke = xwork__local_host_invoke_process_cb;
        pServices->tProcess.pfnInvokeEx = xwork__local_host_invoke_process_cb_ex;
        pServices->tProcess.pUserData = pHost;
    }
    if ( pHost->bEnableVcsStatus ||
         pHost->bEnableVcsDiff ||
         pHost->bEnableVcsLog ||
         pHost->bEnableVcsBranch ) {
        pServices->tVcs.pfnInvoke = xwork__local_host_invoke_vcs_cb;
        pServices->tVcs.pUserData = pHost;
    }
    if ( pHost->bEnableEditorBuffers ) {
        pServices->tEditor.pfnInvoke = xwork__local_host_invoke_editor_cb;
        pServices->tEditor.pUserData = pHost;
    }

    return XWORK_OK;
}

xwork_status xwork_runtime_invoke_host_service(
    const xwork_runtime *pRuntime,
    xwork_host_service_kind eKind,
    const char *sOperationId,
    const char *sRequestJson,
    xwork_tool_result *pResult
)
{
    return xwork_runtime_invoke_host_service_ex(
        pRuntime,
        eKind,
        sOperationId,
        sRequestJson,
        NULL,
        pResult
    );
}

xwork_status xwork_runtime_invoke_host_service_ex(
    const xwork_runtime *pRuntime,
    xwork_host_service_kind eKind,
    const char *sOperationId,
    const char *sRequestJson,
    const xwork_host_invoke_context *pContext,
    xwork_tool_result *pResult
)
{
    const xwork_host_service *pService;
    xwork_replay_engine *pReplayEngine;
    xwork_replay_mode eReplayMode;
    xwork_replay_entry_kind eReplayEntryKind;
    xwork_replay_entry_options tReplayEntry;
    xwork_replay_entry_summary tReplayActual;
    xwork_status iStatus;

    if ( !pRuntime || !sOperationId || !sOperationId[0] || !pResult ) {
        return XWORK_ERROR_INVALID_ARGUMENT;
    }

    pReplayEngine = pRuntime->pReplayEngine;
    eReplayEntryKind = xwork__host_replay_entry_kind(eKind);
    if ( pReplayEngine ) {
        eReplayMode = xwork_replay_engine_get_mode(pReplayEngine);
        if ( eReplayMode != XWORK_REPLAY_MODE_RECORD ) {
            xwork_replay_entry_options_init(&tReplayEntry);
            xwork_replay_entry_summary_init(&tReplayActual);
            tReplayEntry.eKind = eReplayEntryKind;
            tReplayEntry.sKey = sOperationId;
            tReplayEntry.sOperationId = sOperationId;
            tReplayEntry.sArgumentsJson = sRequestJson;
            iStatus = xwork_replay_engine_replay_entry(
                pReplayEngine,
                &tReplayEntry,
                &tReplayActual
            );
            if ( iStatus == XWORK_OK ) {
                iStatus = xwork__runtime_replay_host_result(
                    pRuntime,
                    &tReplayActual,
                    pResult
                );
            }
            xwork_replay_entry_summary_reset(&tReplayActual);
            return iStatus;
        }
        if ( xwork_replay_engine_blocks_side_effects(pReplayEngine) ) {
            return XWORK_ERROR_PAUSED;
        }
    }

    pService = xwork__runtime_get_host_service_slot(pRuntime, eKind);
    if ( !pService || (!pService->pfnInvokeEx && !pService->pfnInvoke) ) {
        return XWORK_ERROR_UNSUPPORTED;
    }

    xwork_tool_result_init(pResult);
    if ( pService->pfnInvokeEx ) {
        iStatus = pService->pfnInvokeEx(
            sOperationId,
            sRequestJson,
            pContext,
            pResult,
            pService->pUserData
        );
    } else {
        iStatus = pService->pfnInvoke(
            sOperationId,
            sRequestJson,
            pResult,
            pService->pUserData
        );
    }

    if ( pReplayEngine ) {
        xwork_replay_entry_options_init(&tReplayEntry);
        tReplayEntry.eKind = eReplayEntryKind;
        tReplayEntry.sKey = sOperationId;
        tReplayEntry.sOperationId = sOperationId;
        tReplayEntry.sArgumentsJson = sRequestJson;
        tReplayEntry.sResultJson = pResult->sOutputText;
        tReplayEntry.iStatus = iStatus;
        {
            xwork_status iRecordStatus = xwork_replay_engine_record_entry(
                pReplayEngine,
                &tReplayEntry
            );
            if ( iRecordStatus != XWORK_OK ) {
                return iRecordStatus;
            }
        }
    }
    return iStatus;
}
