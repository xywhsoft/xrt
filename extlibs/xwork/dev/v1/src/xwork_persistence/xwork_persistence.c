#include "../xwork_core/xwork_internal.h"

#include <errno.h>
#include <stdint.h>
#include <stdio.h>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <direct.h>
#include <windows.h>
#else
#include <dirent.h>
#include <sys/stat.h>
#include <sys/types.h>
#endif

static const unsigned char xwork__snapshot_magic_v0[] = {
    'X', 'W', 'O', 'R', 'K', 'S', 'N', 'A'
};

static const unsigned char xwork__artifact_magic_v0[] = {
    'X', 'W', 'O', 'R', 'K', 'A', 'R', '7'
};

static const unsigned char xwork__snapshot_magic[] = {
    'X', 'W', 'O', 'R', 'K', 'S', 'N', '1'
};

static const unsigned char xwork__artifact_magic[] = {
    'X', 'W', 'O', 'R', 'K', 'A', 'R', '8'
};

static const unsigned char xwork__task_graph_magic[] = {
    'X', 'W', 'O', 'R', 'K', 'T', 'G', '1'
};

static const unsigned char xwork__agent_pool_magic[] = {
    'X', 'W', 'O', 'R', 'K', 'A', 'P', '1'
};

static const unsigned char xwork__control_plane_magic[] = {
    'X', 'W', 'O', 'R', 'K', 'C', 'P', '1'
};

static const unsigned char xwork__replay_magic[] = {
    'X', 'W', 'O', 'R', 'K', 'R', 'P', '1'
};

#define XWORK_EVENT_LOG_FORMAT_VERSION 1u

static const char xwork__event_log_header_prefix[] = "#xwork-events\t";

static xwork_status xwork__file_persistence_list_runs_cb(
    xwork_string_list *pList,
    void *pUserData
);
static xwork_status xwork__file_persistence_list_checkpoints_cb(
    const char *sRunId,
    xwork_string_list *pList,
    void *pUserData
);
static xwork_status xwork__file_persistence_list_events_cb(
    const char *sRunId,
    xwork_string_list *pList,
    void *pUserData
);
static xwork_status xwork__file_persistence_list_artifacts_cb(
    const char *sRunId,
    xwork_string_list *pList,
    void *pUserData
);
static xwork_status xwork__file_persistence_query_run_index_cb(
    const xwork_run_index_query *pQuery,
    xwork_run_index_list *pList,
    void *pUserData
);
static xwork_status xwork__file_persistence_load_run_summary_cb(
    const char *sRunId,
    xwork_run_summary *pSummary,
    void *pUserData
);
static xwork_status xwork__file_persistence_load_last_event_cb(
    const char *sRunId,
    xwork_event *pEvent,
    void *pUserData
);
static xwork_status xwork__file_persistence_load_last_approval_request_cb(
    const char *sRunId,
    xwork_approval_request *pRequest,
    void *pUserData
);
static xwork_status xwork__file_persistence_load_last_checkpoint_cb(
    const char *sRunId,
    xwork_checkpoint *pCheckpoint,
    void *pUserData
);
static xwork_status xwork__file_persistence_load_last_artifact_cb(
    const char *sRunId,
    xwork_artifact *pArtifact,
    void *pUserData
);
static xwork_status xwork__file_persistence_load_event_cb(
    const char *sRunId,
    const char *sEventId,
    xwork_event *pEvent,
    void *pUserData
);
static xwork_status xwork__file_persistence_load_checkpoint_cb(
    const char *sRunId,
    const char *sCheckpointId,
    xwork_checkpoint *pCheckpoint,
    void *pUserData
);
static xwork_status xwork__file_persistence_load_artifact_cb(
    const char *sRunId,
    const char *sArtifactId,
    xwork_artifact *pArtifact,
    void *pUserData
);

static bool xwork__path_is_separator(char c)
{
    return c == '/' || c == '\\';
}

static xwork_status xwork__ensure_directory_single(const char *sPath)
{
    int iResult;

    if ( !sPath || !sPath[0] ) {
        return XWORK_ERROR_INVALID_ARGUMENT;
    }

#ifdef _WIN32
    iResult = _mkdir(sPath);
#else
    iResult = mkdir(sPath, 0777);
#endif

    if ( iResult == 0 || errno == EEXIST ) {
        return XWORK_OK;
    }
    return XWORK_ERROR_EXTERNAL_FAILURE;
}

static xwork_status xwork__ensure_directory_tree(const char *sPath)
{
    char *sMutable;
    size_t i;
    xwork_status iStatus;

    if ( !sPath || !sPath[0] ) {
        return XWORK_ERROR_INVALID_ARGUMENT;
    }

    sMutable = xwork__dup_cstr(sPath);
    if ( !sMutable ) {
        return XWORK_ERROR_NO_MEMORY;
    }

    for ( i = 0u; sMutable[i]; ++i ) {
        if ( !xwork__path_is_separator(sMutable[i]) ) {
            continue;
        }
        if ( i == 0u || (i == 2u && sMutable[1] == ':') ) {
            continue;
        }
        if ( xwork__path_is_separator(sMutable[i - 1u]) ) {
            continue;
        }

        sMutable[i] = '\0';
        iStatus = xwork__ensure_directory_single(sMutable);
        sMutable[i] = '/';
        if ( iStatus != XWORK_OK ) {
            free(sMutable);
            return iStatus;
        }
    }

    i = strlen(sMutable);
    while ( i > 0u && xwork__path_is_separator(sMutable[i - 1u]) ) {
        if ( i == 1u || (i == 3u && sMutable[1] == ':') ) {
            break;
        }
        sMutable[i - 1u] = '\0';
        --i;
    }

    iStatus = xwork__ensure_directory_single(sMutable);
    free(sMutable);
    return iStatus;
}

static char *xwork__encode_path_component(const char *sText)
{
    static const char sHex[] = "0123456789ABCDEF";
    const unsigned char *pBytes;
    char *sEncoded;
    size_t i;
    size_t iLen;

    if ( !sText || !sText[0] ) {
        return NULL;
    }

    iLen = strlen(sText);
    pBytes = (const unsigned char *)sText;
    sEncoded = (char *)calloc((iLen * 2u) + 1u, sizeof(char));
    if ( !sEncoded ) {
        return NULL;
    }

    for ( i = 0u; i < iLen; ++i ) {
        sEncoded[i * 2u] = sHex[(pBytes[i] >> 4) & 0x0Fu];
        sEncoded[(i * 2u) + 1u] = sHex[pBytes[i] & 0x0Fu];
    }

    return sEncoded;
}

static int xwork__hex_value(char c)
{
    if ( c >= '0' && c <= '9' ) {
        return c - '0';
    }
    if ( c >= 'A' && c <= 'F' ) {
        return 10 + (c - 'A');
    }
    if ( c >= 'a' && c <= 'f' ) {
        return 10 + (c - 'a');
    }
    return -1;
}

static char *xwork__decode_path_component(const char *sText)
{
    char *sDecoded;
    size_t i;
    size_t iLen;

    if ( !sText || !sText[0] ) {
        return NULL;
    }

    iLen = strlen(sText);
    if ( (iLen % 2u) != 0u ) {
        return NULL;
    }

    sDecoded = (char *)calloc((iLen / 2u) + 1u, sizeof(char));
    if ( !sDecoded ) {
        return NULL;
    }

    for ( i = 0u; i < iLen; i += 2u ) {
        int iHigh = xwork__hex_value(sText[i]);
        int iLow = xwork__hex_value(sText[i + 1u]);
        if ( iHigh < 0 || iLow < 0 ) {
            free(sDecoded);
            return NULL;
        }
        sDecoded[i / 2u] = (char)((iHigh << 4) | iLow);
    }

    return sDecoded;
}

static xwork_status xwork__string_list_append_owned(
    xwork_string_list *pList,
    char *sText
)
{
    char **psItems;

    if ( !pList || !sText ) {
        free(sText);
        return XWORK_ERROR_INVALID_ARGUMENT;
    }

    psItems = (char **)realloc(
        (char **)pList->psItems,
        (pList->iCount + 1u) * sizeof(*psItems)
    );
    if ( !psItems ) {
        free(sText);
        return XWORK_ERROR_NO_MEMORY;
    }

    pList->psItems = (const char **)psItems;
    psItems[pList->iCount] = sText;
    ++pList->iCount;
    return XWORK_OK;
}

static int xwork__compare_string_items(const void *pLeft, const void *pRight)
{
    const char *const *psLeft = (const char *const *)pLeft;
    const char *const *psRight = (const char *const *)pRight;

    if ( !*psLeft && !*psRight ) {
        return 0;
    }
    if ( !*psLeft ) {
        return -1;
    }
    if ( !*psRight ) {
        return 1;
    }
    return strcmp(*psLeft, *psRight);
}

static int xwork__compare_nullable_cstr(const char *sLeft, const char *sRight)
{
    if ( !sLeft && !sRight ) {
        return 0;
    }
    if ( !sLeft ) {
        return -1;
    }
    if ( !sRight ) {
        return 1;
    }
    return strcmp(sLeft, sRight);
}

static void xwork__string_list_sort(xwork_string_list *pList)
{
    if ( pList && pList->psItems && pList->iCount > 1u ) {
        qsort(
            (char **)pList->psItems,
            pList->iCount,
            sizeof(char *),
            xwork__compare_string_items
        );
    }
}

static xwork_status xwork__append_decoded_entry_name(
    const char *sEncodedName,
    size_t iEncodedLen,
    xwork_string_list *pList
)
{
    char *sEncodedCopy;
    char *sDecoded;
    xwork_status iStatus;

    if ( !sEncodedName || iEncodedLen == 0u || !pList ) {
        return XWORK_ERROR_INVALID_ARGUMENT;
    }

    sEncodedCopy = (char *)calloc(iEncodedLen + 1u, sizeof(char));
    if ( !sEncodedCopy ) {
        return XWORK_ERROR_NO_MEMORY;
    }
    memcpy(sEncodedCopy, sEncodedName, iEncodedLen);
    sEncodedCopy[iEncodedLen] = '\0';

    sDecoded = xwork__decode_path_component(sEncodedCopy);
    free(sEncodedCopy);
    if ( !sDecoded ) {
        return XWORK_OK;
    }

    iStatus = xwork__string_list_append_owned(pList, sDecoded);
    if ( iStatus != XWORK_OK ) {
        return iStatus;
    }
    return XWORK_OK;
}

static bool xwork__run_index_matches_query(
    const xwork_run_index_entry *pEntry,
    const xwork_run_index_query *pQuery
)
{
    if ( !pEntry || !pQuery ) {
        return true;
    }

    if ( pQuery->bFilterState && pEntry->tSummary.eState != pQuery->eState ) {
        return false;
    }
    if ( pQuery->bFilterAutonomy &&
         pEntry->tSummary.eAutonomy != pQuery->eAutonomy ) {
        return false;
    }
    if ( pQuery->bRequireParentRunId && !pEntry->tSummary.sParentRunId ) {
        return false;
    }
    if ( pQuery->sParentRunId &&
         xwork__compare_nullable_cstr(
             pEntry->tSummary.sParentRunId,
             pQuery->sParentRunId
         ) != 0 ) {
        return false;
    }
    if ( pQuery->sParentRunIdPrefix ) {
        size_t iPrefixLen = strlen(pQuery->sParentRunIdPrefix);

        if ( !pEntry->tSummary.sParentRunId ||
             strncmp(
                 pEntry->tSummary.sParentRunId,
                 pQuery->sParentRunIdPrefix,
                 iPrefixLen
             ) != 0 ) {
            return false;
        }
    }
    if ( pQuery->sAgentId &&
         xwork__compare_nullable_cstr(
             pEntry->tSummary.sAgentId,
             pQuery->sAgentId
         ) != 0 ) {
        return false;
    }
    if ( pQuery->sTaskId &&
         xwork__compare_nullable_cstr(
             pEntry->tSummary.sTaskId,
             pQuery->sTaskId
         ) != 0 ) {
        return false;
    }
    if ( pQuery->bFilterLastApprovalState ) {
        if ( !pEntry->bHasLastApprovalRequest ||
             pEntry->tLastApprovalRequest.eState != pQuery->eLastApprovalState ) {
            return false;
        }
    }
    if ( pQuery->bRequireLastEvent && !pEntry->bHasLastEvent ) {
        return false;
    }
    if ( pQuery->bFilterLastEventKind ) {
        if ( !pEntry->bHasLastEvent ||
             pEntry->tLastEvent.eKind != pQuery->eLastEventKind ) {
            return false;
        }
    }
    if ( pQuery->bRequireLastApprovalRequest && !pEntry->bHasLastApprovalRequest ) {
        return false;
    }
    if ( pQuery->bRequireLastCheckpoint && !pEntry->bHasLastCheckpoint ) {
        return false;
    }
    if ( pQuery->bFilterLastCheckpointKind ) {
        if ( !pEntry->bHasLastCheckpoint ||
             pEntry->tLastCheckpoint.eKind != pQuery->eLastCheckpointKind ) {
            return false;
        }
    }
    if ( pQuery->bRequireArtifacts && pEntry->iArtifactCount == 0u ) {
        return false;
    }
    if ( pQuery->bFilterMinEventCount &&
         pEntry->iEventCount < pQuery->iMinEventCount ) {
        return false;
    }
    if ( pQuery->bFilterMaxEventCount &&
         pEntry->iEventCount > pQuery->iMaxEventCount ) {
        return false;
    }
    if ( pQuery->bFilterMinCheckpointCount &&
         pEntry->iCheckpointCount < pQuery->iMinCheckpointCount ) {
        return false;
    }
    if ( pQuery->bFilterMaxCheckpointCount &&
         pEntry->iCheckpointCount > pQuery->iMaxCheckpointCount ) {
        return false;
    }
    if ( pQuery->bFilterMinArtifactCount &&
         pEntry->iArtifactCount < pQuery->iMinArtifactCount ) {
        return false;
    }
    if ( pQuery->bFilterMaxArtifactCount &&
         pEntry->iArtifactCount > pQuery->iMaxArtifactCount ) {
        return false;
    }
    if ( pQuery->bFilterMinLastEventSequence ) {
        if ( !pEntry->bHasLastEvent ||
             pEntry->tLastEvent.iSequence < pQuery->iMinLastEventSequence ) {
            return false;
        }
    }
    if ( pQuery->bFilterMaxLastEventSequence ) {
        if ( !pEntry->bHasLastEvent ||
             pEntry->tLastEvent.iSequence > pQuery->iMaxLastEventSequence ) {
            return false;
        }
    }
    if ( pQuery->bFilterMinLastCheckpointSequence ) {
        if ( !pEntry->bHasLastCheckpoint ||
             pEntry->tLastCheckpoint.iSequence <
                 pQuery->iMinLastCheckpointSequence ) {
            return false;
        }
    }
    if ( pQuery->bFilterMaxLastCheckpointSequence ) {
        if ( !pEntry->bHasLastCheckpoint ||
             pEntry->tLastCheckpoint.iSequence >
                 pQuery->iMaxLastCheckpointSequence ) {
            return false;
        }
    }
    return true;
}

static int xwork__run_index_compare(
    const xwork_run_index_entry *pLeft,
    const xwork_run_index_entry *pRight,
    xwork_run_index_sort eSort
)
{
    size_t iLeftSequence = 0u;
    size_t iRightSequence = 0u;
    int iCmp;

    if ( !pLeft || !pRight ) {
        return 0;
    }

    switch ( eSort ) {
    case XWORK_RUN_INDEX_SORT_RUN_ID_DESC:
        return -xwork__compare_nullable_cstr(
            pLeft->tSummary.sRunId,
            pRight->tSummary.sRunId
        );
    case XWORK_RUN_INDEX_SORT_LAST_EVENT_SEQUENCE_DESC:
        iLeftSequence = pLeft->bHasLastEvent ? pLeft->tLastEvent.iSequence : 0u;
        iRightSequence = pRight->bHasLastEvent ? pRight->tLastEvent.iSequence : 0u;
        if ( iLeftSequence != iRightSequence ) {
            return iLeftSequence > iRightSequence ? -1 : 1;
        }
        break;
    case XWORK_RUN_INDEX_SORT_LAST_CHECKPOINT_SEQUENCE_DESC:
        iLeftSequence = pLeft->bHasLastCheckpoint ? pLeft->tLastCheckpoint.iSequence : 0u;
        iRightSequence = pRight->bHasLastCheckpoint ? pRight->tLastCheckpoint.iSequence : 0u;
        if ( iLeftSequence != iRightSequence ) {
            return iLeftSequence > iRightSequence ? -1 : 1;
        }
        break;
    case XWORK_RUN_INDEX_SORT_EVENT_COUNT_DESC:
        if ( pLeft->iEventCount != pRight->iEventCount ) {
            return pLeft->iEventCount > pRight->iEventCount ? -1 : 1;
        }
        break;
    case XWORK_RUN_INDEX_SORT_CHECKPOINT_COUNT_DESC:
        if ( pLeft->iCheckpointCount != pRight->iCheckpointCount ) {
            return pLeft->iCheckpointCount > pRight->iCheckpointCount ? -1 : 1;
        }
        break;
    case XWORK_RUN_INDEX_SORT_ARTIFACT_COUNT_DESC:
        if ( pLeft->iArtifactCount != pRight->iArtifactCount ) {
            return pLeft->iArtifactCount > pRight->iArtifactCount ? -1 : 1;
        }
        break;
    case XWORK_RUN_INDEX_SORT_RUN_ID_ASC:
    default:
        break;
    }

    iCmp = xwork__compare_nullable_cstr(
        pLeft->tSummary.sRunId,
        pRight->tSummary.sRunId
    );
    if ( eSort == XWORK_RUN_INDEX_SORT_RUN_ID_DESC ) {
        return -iCmp;
    }
    return iCmp;
}

static void xwork__run_index_sort_items(
    xwork_run_index_entry *pItems,
    size_t iCount,
    xwork_run_index_sort eSort
)
{
    size_t i;

    if ( !pItems || iCount < 2u ) {
        return;
    }

    for ( i = 1u; i < iCount; ++i ) {
        size_t j = i;
        xwork_run_index_entry tValue = pItems[i];

        while ( j > 0u &&
                xwork__run_index_compare(&tValue, &pItems[j - 1u], eSort) < 0 ) {
            pItems[j] = pItems[j - 1u];
            --j;
        }
        pItems[j] = tValue;
    }
}

static xwork_status xwork__list_encoded_entries(
    const char *sDirPath,
    bool bDirectories,
    const char *sSuffix,
    xwork_string_list *pList
)
{
    xwork_status iStatus = XWORK_OK;
    size_t iSuffixLen = sSuffix ? strlen(sSuffix) : 0u;

    if ( !sDirPath || !sDirPath[0] || !pList ) {
        return XWORK_ERROR_INVALID_ARGUMENT;
    }

    xwork_string_list_reset(pList);

#ifdef _WIN32
    WIN32_FIND_DATAA tFindData;
    HANDLE hFind;
    char *sPattern;
    DWORD iError;
    DWORD iFindError = ERROR_SUCCESS;

    sPattern = xwork__dup_printf("%s/*", sDirPath);
    if ( !sPattern ) {
        return XWORK_ERROR_NO_MEMORY;
    }

    hFind = FindFirstFileA(sPattern, &tFindData);
    free(sPattern);
    if ( hFind == INVALID_HANDLE_VALUE ) {
        iError = GetLastError();
        if ( iError == ERROR_FILE_NOT_FOUND || iError == ERROR_PATH_NOT_FOUND ) {
            return XWORK_OK;
        }
        return XWORK_ERROR_EXTERNAL_FAILURE;
    }

    do {
        bool bIsDirectory = (tFindData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
        const char *sName = tFindData.cFileName;
        size_t iNameLen;

        if ( strcmp(sName, ".") == 0 || strcmp(sName, "..") == 0 ) {
            continue;
        }
        if ( bIsDirectory != bDirectories ) {
            continue;
        }

        iNameLen = strlen(sName);
        if ( iSuffixLen > 0u ) {
            if ( iNameLen <= iSuffixLen ) {
                continue;
            }
            if ( strcmp(sName + (iNameLen - iSuffixLen), sSuffix) != 0 ) {
                continue;
            }
            iNameLen -= iSuffixLen;
        }

        iStatus = xwork__append_decoded_entry_name(sName, iNameLen, pList);
        if ( iStatus != XWORK_OK ) {
            break;
        }
    } while ( FindNextFileA(hFind, &tFindData) != 0 );

    if ( iStatus == XWORK_OK ) {
        iFindError = GetLastError();
    }

    if ( FindClose(hFind) == 0 && iStatus == XWORK_OK ) {
        iStatus = XWORK_ERROR_EXTERNAL_FAILURE;
    }
    if ( iStatus == XWORK_OK ) {
        if ( iFindError != ERROR_NO_MORE_FILES ) {
            iStatus = XWORK_ERROR_EXTERNAL_FAILURE;
        }
    }
#else
    DIR *pDir;
    struct dirent *pEntry;

    pDir = opendir(sDirPath);
    if ( !pDir ) {
        if ( errno == ENOENT ) {
            return XWORK_OK;
        }
        return XWORK_ERROR_EXTERNAL_FAILURE;
    }

    errno = 0;
    while ( (pEntry = readdir(pDir)) != NULL ) {
        const char *sName = pEntry->d_name;
        size_t iNameLen;
        bool bIsDirectory;

        if ( strcmp(sName, ".") == 0 || strcmp(sName, "..") == 0 ) {
            continue;
        }

#if defined(DT_DIR) && defined(DT_REG)
        if ( pEntry->d_type == DT_UNKNOWN ) {
            struct stat tStat;
            char *sEntryPath = xwork__dup_printf("%s/%s", sDirPath, sName);
            if ( !sEntryPath ) {
                iStatus = XWORK_ERROR_NO_MEMORY;
                break;
            }
            if ( stat(sEntryPath, &tStat) != 0 ) {
                free(sEntryPath);
                continue;
            }
            free(sEntryPath);
            bIsDirectory = S_ISDIR(tStat.st_mode);
        } else {
            bIsDirectory = pEntry->d_type == DT_DIR;
        }
#else
        {
            struct stat tStat;
            char *sEntryPath = xwork__dup_printf("%s/%s", sDirPath, sName);
            if ( !sEntryPath ) {
                iStatus = XWORK_ERROR_NO_MEMORY;
                break;
            }
            if ( stat(sEntryPath, &tStat) != 0 ) {
                free(sEntryPath);
                continue;
            }
            free(sEntryPath);
            bIsDirectory = S_ISDIR(tStat.st_mode);
        }
#endif

        if ( bIsDirectory != bDirectories ) {
            continue;
        }

        iNameLen = strlen(sName);
        if ( iSuffixLen > 0u ) {
            if ( iNameLen <= iSuffixLen ) {
                continue;
            }
            if ( strcmp(sName + (iNameLen - iSuffixLen), sSuffix) != 0 ) {
                continue;
            }
            iNameLen -= iSuffixLen;
        }

        iStatus = xwork__append_decoded_entry_name(sName, iNameLen, pList);
        if ( iStatus != XWORK_OK ) {
            break;
        }
    }

    if ( closedir(pDir) != 0 && iStatus == XWORK_OK ) {
        iStatus = XWORK_ERROR_EXTERNAL_FAILURE;
    }
    if ( errno != 0 && iStatus == XWORK_OK ) {
        iStatus = XWORK_ERROR_EXTERNAL_FAILURE;
    }
#endif

    if ( iStatus != XWORK_OK ) {
        xwork_string_list_reset(pList);
        return iStatus;
    }

    xwork__string_list_sort(pList);
    return XWORK_OK;
}

static char *xwork__build_run_dir_path(
    const xwork_file_persistence *pStore,
    const char *sRunId
)
{
    char *sEncodedRunId;
    char *sPath;

    if ( !pStore || !pStore->sRootPath || !sRunId || !sRunId[0] ) {
        return NULL;
    }

    sEncodedRunId = xwork__encode_path_component(sRunId);
    if ( !sEncodedRunId ) {
        return NULL;
    }

    sPath = xwork__dup_printf("%s/runs/%s", pStore->sRootPath, sEncodedRunId);
    free(sEncodedRunId);
    return sPath;
}

static char *xwork__build_checkpoint_file_path(
    const char *sCheckpointsDir,
    const char *sCheckpointId
)
{
    char *sEncodedCheckpointId;
    char *sPath;

    if ( !sCheckpointsDir || !sCheckpointId || !sCheckpointId[0] ) {
        return NULL;
    }

    sEncodedCheckpointId = xwork__encode_path_component(sCheckpointId);
    if ( !sEncodedCheckpointId ) {
        return NULL;
    }

    sPath = xwork__dup_printf("%s/%s.snapshot", sCheckpointsDir, sEncodedCheckpointId);
    free(sEncodedCheckpointId);
    return sPath;
}

static char *xwork__build_artifact_file_path(
    const char *sArtifactsDir,
    const char *sArtifactId
)
{
    char *sEncodedArtifactId;
    char *sPath;

    if ( !sArtifactsDir || !sArtifactId || !sArtifactId[0] ) {
        return NULL;
    }

    sEncodedArtifactId = xwork__encode_path_component(sArtifactId);
    if ( !sEncodedArtifactId ) {
        return NULL;
    }

    sPath = xwork__dup_printf("%s/%s.meta", sArtifactsDir, sEncodedArtifactId);
    free(sEncodedArtifactId);
    return sPath;
}

static char *xwork__build_task_graph_file_path(
    const char *sTaskGraphsDir,
    const char *sGraphId
)
{
    char *sEncodedGraphId;
    char *sPath;

    if ( !sTaskGraphsDir || !sGraphId || !sGraphId[0] ) {
        return NULL;
    }

    sEncodedGraphId = xwork__encode_path_component(sGraphId);
    if ( !sEncodedGraphId ) {
        return NULL;
    }

    sPath = xwork__dup_printf("%s/%s.graph", sTaskGraphsDir, sEncodedGraphId);
    free(sEncodedGraphId);
    return sPath;
}

static char *xwork__build_agent_pool_file_path(
    const char *sAgentPoolsDir,
    const char *sPoolId
)
{
    char *sEncodedPoolId;
    char *sPath;

    if ( !sAgentPoolsDir || !sPoolId || !sPoolId[0] ) {
        return NULL;
    }

    sEncodedPoolId = xwork__encode_path_component(sPoolId);
    if ( !sEncodedPoolId ) {
        return NULL;
    }

    sPath = xwork__dup_printf("%s/%s.pool", sAgentPoolsDir, sEncodedPoolId);
    free(sEncodedPoolId);
    return sPath;
}

static char *xwork__build_control_plane_file_path(
    const char *sControlPlanesDir,
    const char *sPlaneId
)
{
    char *sEncodedPlaneId;
    char *sPath;

    if ( !sControlPlanesDir || !sPlaneId || !sPlaneId[0] ) {
        return NULL;
    }

    sEncodedPlaneId = xwork__encode_path_component(sPlaneId);
    if ( !sEncodedPlaneId ) {
        return NULL;
    }

    sPath = xwork__dup_printf("%s/%s.plane", sControlPlanesDir, sEncodedPlaneId);
    free(sEncodedPlaneId);
    return sPath;
}

static char *xwork__build_replay_file_path(
    const char *sReplaysDir,
    const char *sReplayId
)
{
    char *sEncodedReplayId;
    char *sPath;

    if ( !sReplaysDir || !sReplayId || !sReplayId[0] ) {
        return NULL;
    }

    sEncodedReplayId = xwork__encode_path_component(sReplayId);
    if ( !sEncodedReplayId ) {
        return NULL;
    }

    sPath = xwork__dup_printf("%s/%s.replay", sReplaysDir, sEncodedReplayId);
    free(sEncodedReplayId);
    return sPath;
}

static char *xwork__build_events_log_path(const char *sRunDir)
{
    if ( !sRunDir || !sRunDir[0] ) {
        return NULL;
    }
    return xwork__dup_printf("%s/events.log", sRunDir);
}

static xwork_status xwork__ensure_run_storage(
    const xwork_file_persistence *pStore,
    const char *sRunId,
    char **psRunDir,
    char **psCheckpointsDir
)
{
    char *sRunDir;
    char *sRunsRoot;
    char *sCheckpointsDir = NULL;
    xwork_status iStatus;

    if ( !pStore || !pStore->sRootPath || !sRunId || !sRunId[0] ) {
        return XWORK_ERROR_INVALID_ARGUMENT;
    }

    sRunDir = xwork__build_run_dir_path(pStore, sRunId);
    if ( !sRunDir ) {
        return XWORK_ERROR_NO_MEMORY;
    }

    iStatus = xwork__ensure_directory_tree(pStore->sRootPath);
    if ( iStatus != XWORK_OK ) {
        free(sRunDir);
        return iStatus;
    }
    sRunsRoot = xwork__dup_printf("%s/runs", pStore->sRootPath);
    if ( !sRunsRoot ) {
        free(sRunDir);
        return XWORK_ERROR_NO_MEMORY;
    }
    iStatus = xwork__ensure_directory_tree(sRunsRoot);
    free(sRunsRoot);
    if ( iStatus != XWORK_OK ) {
        free(sRunDir);
        return iStatus;
    }
    iStatus = xwork__ensure_directory_tree(sRunDir);
    if ( iStatus != XWORK_OK ) {
        free(sRunDir);
        return iStatus;
    }

    if ( psCheckpointsDir ) {
        sCheckpointsDir = xwork__dup_printf("%s/checkpoints", sRunDir);
        if ( !sCheckpointsDir ) {
            free(sRunDir);
            return XWORK_ERROR_NO_MEMORY;
        }
        iStatus = xwork__ensure_directory_tree(sCheckpointsDir);
        if ( iStatus != XWORK_OK ) {
            free(sCheckpointsDir);
            free(sRunDir);
            return iStatus;
        }
        *psCheckpointsDir = sCheckpointsDir;
    }

    *psRunDir = sRunDir;
    return XWORK_OK;
}

static xwork_status xwork__ensure_artifact_storage(
    const xwork_file_persistence *pStore,
    const char *sRunId,
    char **psRunDir,
    char **psArtifactsDir
)
{
    char *sRunDir;
    char *sArtifactsDir;
    xwork_status iStatus;

    if ( !pStore || !psRunDir || !psArtifactsDir ) {
        return XWORK_ERROR_INVALID_ARGUMENT;
    }

    *psRunDir = NULL;
    *psArtifactsDir = NULL;

    iStatus = xwork__ensure_run_storage(pStore, sRunId, &sRunDir, NULL);
    if ( iStatus != XWORK_OK ) {
        return iStatus;
    }

    sArtifactsDir = xwork__dup_printf("%s/artifacts", sRunDir);
    if ( !sArtifactsDir ) {
        free(sRunDir);
        return XWORK_ERROR_NO_MEMORY;
    }

    iStatus = xwork__ensure_directory_tree(sArtifactsDir);
    if ( iStatus != XWORK_OK ) {
        free(sArtifactsDir);
        free(sRunDir);
        return iStatus;
    }

    *psRunDir = sRunDir;
    *psArtifactsDir = sArtifactsDir;
    return XWORK_OK;
}

static xwork_status xwork__file_write_bytes(
    FILE *pFile,
    const void *pData,
    size_t iSize
)
{
    if ( !pFile || (!pData && iSize > 0u) ) {
        return XWORK_ERROR_INVALID_ARGUMENT;
    }
    if ( iSize == 0u ) {
        return XWORK_OK;
    }
    if ( fwrite(pData, 1u, iSize, pFile) != iSize ) {
        return XWORK_ERROR_EXTERNAL_FAILURE;
    }
    return XWORK_OK;
}

static xwork_status xwork__file_read_bytes(
    FILE *pFile,
    void *pData,
    size_t iSize
)
{
    if ( !pFile || (!pData && iSize > 0u) ) {
        return XWORK_ERROR_INVALID_ARGUMENT;
    }
    if ( iSize == 0u ) {
        return XWORK_OK;
    }
    if ( fread(pData, 1u, iSize, pFile) != iSize ) {
        return XWORK_ERROR_EXTERNAL_FAILURE;
    }
    return XWORK_OK;
}

static xwork_status xwork__file_write_u64(FILE *pFile, uint64_t iValue)
{
    return xwork__file_write_bytes(pFile, &iValue, sizeof(iValue));
}

static xwork_status xwork__file_read_u64(FILE *pFile, uint64_t *piValue)
{
    return xwork__file_read_bytes(pFile, piValue, sizeof(*piValue));
}

static xwork_status xwork__write_persistence_header(
    FILE *pFile,
    const unsigned char *pMagic,
    size_t iMagicSize
)
{
    xwork_status iStatus;

    if ( !pFile || !pMagic || iMagicSize == 0u ) {
        return XWORK_ERROR_INVALID_ARGUMENT;
    }

    iStatus = xwork__file_write_bytes(pFile, pMagic, iMagicSize);
    if ( iStatus != XWORK_OK ) {
        return iStatus;
    }
    return xwork__file_write_u64(
        pFile,
        (uint64_t)XWORK_PERSISTENCE_FORMAT_VERSION
    );
}

static xwork_status xwork__read_persistence_header(
    FILE *pFile,
    const unsigned char *pMagic,
    const unsigned char *pLegacyMagic,
    size_t iMagicSize,
    uint64_t *piFormatVersion
)
{
    unsigned char aMagic[8];
    uint64_t iFormatVersion;
    xwork_status iStatus;

    if ( !pFile || !pMagic || !pLegacyMagic || iMagicSize == 0u ||
         iMagicSize > sizeof(aMagic) ) {
        return XWORK_ERROR_INVALID_ARGUMENT;
    }
    if ( piFormatVersion ) {
        *piFormatVersion = 0u;
    }

    iStatus = xwork__file_read_bytes(pFile, aMagic, iMagicSize);
    if ( iStatus != XWORK_OK ) {
        return iStatus;
    }
    if ( memcmp(aMagic, pLegacyMagic, iMagicSize) == 0 ) {
        return XWORK_OK;
    }
    if ( memcmp(aMagic, pMagic, iMagicSize) != 0 ) {
        return XWORK_ERROR_EXTERNAL_FAILURE;
    }

    iStatus = xwork__file_read_u64(pFile, &iFormatVersion);
    if ( iStatus != XWORK_OK ) {
        return iStatus;
    }
    if ( iFormatVersion > (uint64_t)XWORK_PERSISTENCE_FORMAT_VERSION ) {
        return XWORK_ERROR_UNSUPPORTED;
    }
    if ( iFormatVersion == 0u ) {
        return XWORK_ERROR_EXTERNAL_FAILURE;
    }
    if ( piFormatVersion ) {
        *piFormatVersion = iFormatVersion;
    }
    return XWORK_OK;
}

static xwork_status xwork__file_write_bool(FILE *pFile, bool bValue)
{
    unsigned char iValue = bValue ? 1u : 0u;
    return xwork__file_write_bytes(pFile, &iValue, sizeof(iValue));
}

static xwork_status xwork__file_read_bool(FILE *pFile, bool *pbValue)
{
    unsigned char iValue;
    xwork_status iStatus;

    if ( !pbValue ) {
        return XWORK_ERROR_INVALID_ARGUMENT;
    }

    iStatus = xwork__file_read_bytes(pFile, &iValue, sizeof(iValue));
    if ( iStatus != XWORK_OK ) {
        return iStatus;
    }
    *pbValue = iValue != 0u;
    return XWORK_OK;
}

static xwork_status xwork__file_write_size(FILE *pFile, size_t iValue)
{
    return xwork__file_write_u64(pFile, (uint64_t)iValue);
}

static xwork_status xwork__file_write_i64(FILE *pFile, int64_t iValue)
{
    return xwork__file_write_u64(pFile, (uint64_t)iValue);
}

static xwork_status xwork__file_read_size(FILE *pFile, size_t *piValue)
{
    uint64_t iValue;
    xwork_status iStatus;

    if ( !piValue ) {
        return XWORK_ERROR_INVALID_ARGUMENT;
    }

    iStatus = xwork__file_read_u64(pFile, &iValue);
    if ( iStatus != XWORK_OK ) {
        return iStatus;
    }
    if ( iValue > (uint64_t)SIZE_MAX ) {
        return XWORK_ERROR_EXTERNAL_FAILURE;
    }
    *piValue = (size_t)iValue;
    return XWORK_OK;
}

static xwork_status xwork__file_read_i64(FILE *pFile, int64_t *piValue)
{
    uint64_t iValue;
    xwork_status iStatus;

    if ( !piValue ) {
        return XWORK_ERROR_INVALID_ARGUMENT;
    }

    iStatus = xwork__file_read_u64(pFile, &iValue);
    if ( iStatus != XWORK_OK ) {
        return iStatus;
    }

    *piValue = (int64_t)iValue;
    return XWORK_OK;
}

static xwork_status xwork__file_write_double(FILE *pFile, double fValue)
{
    return xwork__file_write_bytes(pFile, &fValue, sizeof(fValue));
}

static xwork_status xwork__file_read_double(FILE *pFile, double *pfValue)
{
    if ( !pfValue ) {
        return XWORK_ERROR_INVALID_ARGUMENT;
    }
    return xwork__file_read_bytes(pFile, pfValue, sizeof(*pfValue));
}

static xwork_status xwork__file_write_string(FILE *pFile, const char *sText)
{
    size_t iLen = 0u;
    xwork_status iStatus;

    iStatus = xwork__file_write_bool(pFile, sText != NULL);
    if ( iStatus != XWORK_OK || !sText ) {
        return iStatus;
    }

    iLen = strlen(sText);
    iStatus = xwork__file_write_size(pFile, iLen);
    if ( iStatus != XWORK_OK ) {
        return iStatus;
    }
    return xwork__file_write_bytes(pFile, sText, iLen);
}

static xwork_status xwork__file_read_string(FILE *pFile, const char **psText)
{
    bool bHasValue;
    size_t iLen;
    char *sText;
    xwork_status iStatus;

    if ( !psText ) {
        return XWORK_ERROR_INVALID_ARGUMENT;
    }

    *psText = NULL;
    iStatus = xwork__file_read_bool(pFile, &bHasValue);
    if ( iStatus != XWORK_OK || !bHasValue ) {
        return iStatus;
    }

    iStatus = xwork__file_read_size(pFile, &iLen);
    if ( iStatus != XWORK_OK ) {
        return iStatus;
    }

    sText = (char *)calloc(iLen + 1u, sizeof(char));
    if ( !sText ) {
        return XWORK_ERROR_NO_MEMORY;
    }

    iStatus = xwork__file_read_bytes(pFile, sText, iLen);
    if ( iStatus != XWORK_OK ) {
        free(sText);
        return iStatus;
    }

    sText[iLen] = '\0';
    *psText = sText;
    return XWORK_OK;
}

static xwork_status xwork__file_write_string_array(
    FILE *pFile,
    const char *const *psItems,
    size_t iCount
)
{
    size_t i;
    xwork_status iStatus;

    iStatus = xwork__file_write_size(pFile, iCount);
    if ( iStatus != XWORK_OK ) {
        return iStatus;
    }

    for ( i = 0u; i < iCount; ++i ) {
        iStatus = xwork__file_write_string(pFile, psItems ? psItems[i] : NULL);
        if ( iStatus != XWORK_OK ) {
            return iStatus;
        }
    }

    return XWORK_OK;
}

static xwork_status xwork__file_read_string_array(
    FILE *pFile,
    const char ***ppsItems,
    size_t *piCount
)
{
    char **psItems = NULL;
    size_t i;
    size_t iCount;
    xwork_status iStatus;

    if ( !ppsItems || !piCount ) {
        return XWORK_ERROR_INVALID_ARGUMENT;
    }

    *ppsItems = NULL;
    *piCount = 0u;

    iStatus = xwork__file_read_size(pFile, &iCount);
    if ( iStatus != XWORK_OK ) {
        return iStatus;
    }
    if ( iCount == 0u ) {
        return XWORK_OK;
    }

    psItems = (char **)calloc(iCount, sizeof(*psItems));
    if ( !psItems ) {
        return XWORK_ERROR_NO_MEMORY;
    }

    for ( i = 0u; i < iCount; ++i ) {
        const char *sItem = NULL;

        iStatus = xwork__file_read_string(pFile, &sItem);
        if ( iStatus != XWORK_OK ) {
            xwork__free_str_array(&psItems, &i);
            return iStatus;
        }
        psItems[i] = (char *)sItem;
    }

    *ppsItems = (const char **)psItems;
    *piCount = iCount;
    return XWORK_OK;
}

static xwork_status xwork__file_write_artifact_fields(
    FILE *pFile,
    const xwork_artifact *pArtifact
)
{
    xwork_status iStatus;

    if ( !pFile || !pArtifact ) {
        return XWORK_ERROR_INVALID_ARGUMENT;
    }

    iStatus = xwork__file_write_string(pFile, pArtifact->sArtifactId);
    if ( iStatus != XWORK_OK ) return iStatus;
    iStatus = xwork__file_write_string(pFile, pArtifact->sRunId);
    if ( iStatus != XWORK_OK ) return iStatus;
    iStatus = xwork__file_write_size(pFile, (size_t)pArtifact->eKind);
    if ( iStatus != XWORK_OK ) return iStatus;
    iStatus = xwork__file_write_string(pFile, pArtifact->sName);
    if ( iStatus != XWORK_OK ) return iStatus;
    iStatus = xwork__file_write_string(pFile, pArtifact->sMimeType);
    if ( iStatus != XWORK_OK ) return iStatus;
    iStatus = xwork__file_write_string(pFile, pArtifact->sStorageRef);
    if ( iStatus != XWORK_OK ) return iStatus;
    iStatus = xwork__file_write_string(pFile, pArtifact->sSummary);
    if ( iStatus != XWORK_OK ) return iStatus;
    iStatus = xwork__file_write_size(pFile, (size_t)pArtifact->eOutputClass);
    if ( iStatus != XWORK_OK ) return iStatus;
    iStatus = xwork__file_write_string(pFile, pArtifact->sOutputRole);
    if ( iStatus != XWORK_OK ) return iStatus;
    iStatus = xwork__file_write_size(pFile, (size_t)pArtifact->eReportClass);
    if ( iStatus != XWORK_OK ) return iStatus;
    iStatus = xwork__file_write_string(pFile, pArtifact->sReportSubjectRef);
    if ( iStatus != XWORK_OK ) return iStatus;
    iStatus = xwork__file_write_string(pFile, pArtifact->sContentText);
    if ( iStatus != XWORK_OK ) return iStatus;
    iStatus = xwork__file_write_string(pFile, pArtifact->sPatchApplyResultJson);
    if ( iStatus != XWORK_OK ) return iStatus;
    iStatus = xwork__file_write_string(pFile, pArtifact->sPatchFileSummaryJson);
    if ( iStatus != XWORK_OK ) return iStatus;
    iStatus = xwork__file_write_string(pFile, pArtifact->sCommandText);
    if ( iStatus != XWORK_OK ) return iStatus;
    iStatus = xwork__file_write_size(pFile, pArtifact->bHasContentStats ? 1u : 0u);
    if ( iStatus != XWORK_OK ) return iStatus;
    iStatus = xwork__file_write_size(pFile, pArtifact->iContentByteCount);
    if ( iStatus != XWORK_OK ) return iStatus;
    iStatus = xwork__file_write_size(pFile, pArtifact->iContentLineCount);
    if ( iStatus != XWORK_OK ) return iStatus;
    iStatus = xwork__file_write_size(pFile, pArtifact->bHasPatchStats ? 1u : 0u);
    if ( iStatus != XWORK_OK ) return iStatus;
    iStatus = xwork__file_write_size(pFile, pArtifact->iPatchFileCount);
    if ( iStatus != XWORK_OK ) return iStatus;
    iStatus = xwork__file_write_size(pFile, pArtifact->iPatchHunkCount);
    if ( iStatus != XWORK_OK ) return iStatus;
    iStatus = xwork__file_write_size(pFile, pArtifact->iPatchAddedLineCount);
    if ( iStatus != XWORK_OK ) return iStatus;
    iStatus = xwork__file_write_size(pFile, pArtifact->iPatchDeletedLineCount);
    if ( iStatus != XWORK_OK ) return iStatus;
    iStatus = xwork__file_write_size(pFile, pArtifact->bHasCommandIoStats ? 1u : 0u);
    if ( iStatus != XWORK_OK ) return iStatus;
    iStatus = xwork__file_write_size(pFile, pArtifact->iStdoutByteCount);
    if ( iStatus != XWORK_OK ) return iStatus;
    iStatus = xwork__file_write_size(pFile, pArtifact->iStderrByteCount);
    if ( iStatus != XWORK_OK ) return iStatus;
    iStatus = xwork__file_write_size(pFile, pArtifact->bStdoutTruncated ? 1u : 0u);
    if ( iStatus != XWORK_OK ) return iStatus;
    iStatus = xwork__file_write_size(pFile, pArtifact->bStderrTruncated ? 1u : 0u);
    if ( iStatus != XWORK_OK ) return iStatus;
    iStatus = xwork__file_write_size(pFile, pArtifact->bHasExitCode ? 1u : 0u);
    if ( iStatus != XWORK_OK ) return iStatus;
    iStatus = xwork__file_write_i64(
        pFile,
        pArtifact->bHasExitCode ? (int64_t)pArtifact->iExitCode : 0
    );
    if ( iStatus != XWORK_OK ) return iStatus;
    return xwork__file_write_size(pFile, pArtifact->iSequence);
}

static xwork_status xwork__file_read_artifact_fields(
    FILE *pFile,
    uint64_t iFormatVersion,
    xwork_artifact *pArtifact
)
{
    size_t iValue;
    xwork_status iStatus;

    if ( !pFile || !pArtifact ) {
        return XWORK_ERROR_INVALID_ARGUMENT;
    }

    xwork_artifact_init(pArtifact);

    iStatus = xwork__file_read_string(pFile, &pArtifact->sArtifactId);
    if ( iStatus != XWORK_OK ) return iStatus;
    iStatus = xwork__file_read_string(pFile, &pArtifact->sRunId);
    if ( iStatus != XWORK_OK ) return iStatus;
    iStatus = xwork__file_read_size(pFile, &iValue);
    if ( iStatus != XWORK_OK ) return iStatus;
    pArtifact->eKind = (xwork_artifact_kind)iValue;
    iStatus = xwork__file_read_string(pFile, &pArtifact->sName);
    if ( iStatus != XWORK_OK ) return iStatus;
    iStatus = xwork__file_read_string(pFile, &pArtifact->sMimeType);
    if ( iStatus != XWORK_OK ) return iStatus;
    iStatus = xwork__file_read_string(pFile, &pArtifact->sStorageRef);
    if ( iStatus != XWORK_OK ) return iStatus;
    iStatus = xwork__file_read_string(pFile, &pArtifact->sSummary);
    if ( iStatus != XWORK_OK ) return iStatus;
    iStatus = xwork__file_read_size(pFile, &iValue);
    if ( iStatus != XWORK_OK ) return iStatus;
    pArtifact->eOutputClass = (xwork_artifact_output_class)iValue;
    iStatus = xwork__file_read_string(pFile, &pArtifact->sOutputRole);
    if ( iStatus != XWORK_OK ) return iStatus;
    iStatus = xwork__file_read_size(pFile, &iValue);
    if ( iStatus != XWORK_OK ) return iStatus;
    pArtifact->eReportClass = (xwork_artifact_report_class)iValue;
    iStatus = xwork__file_read_string(pFile, &pArtifact->sReportSubjectRef);
    if ( iStatus != XWORK_OK ) return iStatus;
    iStatus = xwork__file_read_string(pFile, &pArtifact->sContentText);
    if ( iStatus != XWORK_OK ) return iStatus;
    if ( iFormatVersion >= 2u ) {
        iStatus = xwork__file_read_string(pFile, &pArtifact->sPatchApplyResultJson);
        if ( iStatus != XWORK_OK ) return iStatus;
        iStatus = xwork__file_read_string(pFile, &pArtifact->sPatchFileSummaryJson);
        if ( iStatus != XWORK_OK ) return iStatus;
    }
    iStatus = xwork__file_read_string(pFile, &pArtifact->sCommandText);
    if ( iStatus != XWORK_OK ) return iStatus;
    iStatus = xwork__file_read_size(pFile, &iValue);
    if ( iStatus != XWORK_OK ) return iStatus;
    pArtifact->bHasContentStats = (iValue != 0u);
    iStatus = xwork__file_read_size(pFile, &pArtifact->iContentByteCount);
    if ( iStatus != XWORK_OK ) return iStatus;
    iStatus = xwork__file_read_size(pFile, &pArtifact->iContentLineCount);
    if ( iStatus != XWORK_OK ) return iStatus;
    if ( !pArtifact->bHasContentStats ) {
        pArtifact->iContentByteCount = 0u;
        pArtifact->iContentLineCount = 0u;
    }
    iStatus = xwork__file_read_size(pFile, &iValue);
    if ( iStatus != XWORK_OK ) return iStatus;
    pArtifact->bHasPatchStats = (iValue != 0u);
    iStatus = xwork__file_read_size(pFile, &pArtifact->iPatchFileCount);
    if ( iStatus != XWORK_OK ) return iStatus;
    iStatus = xwork__file_read_size(pFile, &pArtifact->iPatchHunkCount);
    if ( iStatus != XWORK_OK ) return iStatus;
    iStatus = xwork__file_read_size(pFile, &pArtifact->iPatchAddedLineCount);
    if ( iStatus != XWORK_OK ) return iStatus;
    iStatus = xwork__file_read_size(pFile, &pArtifact->iPatchDeletedLineCount);
    if ( iStatus != XWORK_OK ) return iStatus;
    if ( !pArtifact->bHasPatchStats ) {
        pArtifact->iPatchFileCount = 0u;
        pArtifact->iPatchHunkCount = 0u;
        pArtifact->iPatchAddedLineCount = 0u;
        pArtifact->iPatchDeletedLineCount = 0u;
    }
    iStatus = xwork__file_read_size(pFile, &iValue);
    if ( iStatus != XWORK_OK ) return iStatus;
    pArtifact->bHasCommandIoStats = (iValue != 0u);
    iStatus = xwork__file_read_size(pFile, &pArtifact->iStdoutByteCount);
    if ( iStatus != XWORK_OK ) return iStatus;
    iStatus = xwork__file_read_size(pFile, &pArtifact->iStderrByteCount);
    if ( iStatus != XWORK_OK ) return iStatus;
    iStatus = xwork__file_read_size(pFile, &iValue);
    if ( iStatus != XWORK_OK ) return iStatus;
    pArtifact->bStdoutTruncated = (iValue != 0u);
    iStatus = xwork__file_read_size(pFile, &iValue);
    if ( iStatus != XWORK_OK ) return iStatus;
    pArtifact->bStderrTruncated = (iValue != 0u);
    if ( !pArtifact->bHasCommandIoStats ) {
        pArtifact->iStdoutByteCount = 0u;
        pArtifact->iStderrByteCount = 0u;
        pArtifact->bStdoutTruncated = false;
        pArtifact->bStderrTruncated = false;
    }
    iStatus = xwork__file_read_size(pFile, &iValue);
    if ( iStatus != XWORK_OK ) return iStatus;
    pArtifact->bHasExitCode = (iValue != 0u);
    {
        int64_t iExitCodeValue;

        iStatus = xwork__file_read_i64(pFile, &iExitCodeValue);
        if ( iStatus != XWORK_OK ) return iStatus;
        pArtifact->iExitCode = pArtifact->bHasExitCode ? (int)iExitCodeValue : 0;
    }
    return xwork__file_read_size(pFile, &pArtifact->iSequence);
}

static xwork_status xwork__file_write_artifact_array(
    FILE *pFile,
    const xwork_artifact *pArtifacts,
    size_t iArtifactCount
)
{
    size_t i;
    xwork_status iStatus;

    iStatus = xwork__file_write_size(pFile, iArtifactCount);
    if ( iStatus != XWORK_OK ) {
        return iStatus;
    }

    for ( i = 0u; i < iArtifactCount; ++i ) {
        iStatus = xwork__file_write_artifact_fields(pFile, &pArtifacts[i]);
        if ( iStatus != XWORK_OK ) {
            return iStatus;
        }
    }

    return XWORK_OK;
}

static xwork_status xwork__file_read_artifact_array(
    FILE *pFile,
    uint64_t iFormatVersion,
    xwork_run_snapshot *pSnapshot
)
{
    xwork_artifact *pArtifacts = NULL;
    size_t i;
    size_t iArtifactCount;
    xwork_status iStatus;

    if ( !pFile || !pSnapshot ) {
        return XWORK_ERROR_INVALID_ARGUMENT;
    }

    iStatus = xwork__file_read_size(pFile, &iArtifactCount);
    if ( iStatus != XWORK_OK ) {
        return iStatus;
    }
    if ( iArtifactCount == 0u ) {
        pSnapshot->pArtifacts = NULL;
        pSnapshot->iArtifactCount = 0u;
        return XWORK_OK;
    }

    pArtifacts = (xwork_artifact *)calloc(iArtifactCount, sizeof(*pArtifacts));
    if ( !pArtifacts ) {
        return XWORK_ERROR_NO_MEMORY;
    }

    pSnapshot->pArtifacts = pArtifacts;
    pSnapshot->iArtifactCount = iArtifactCount;

    for ( i = 0u; i < iArtifactCount; ++i ) {
        iStatus = xwork__file_read_artifact_fields(
            pFile,
            iFormatVersion,
            &pArtifacts[i]
        );
        if ( iStatus != XWORK_OK ) {
            pSnapshot->iArtifactCount = i + 1u;
            xwork__run_snapshot_reset_artifacts(pSnapshot);
            return iStatus;
        }
        if ( pArtifacts[i].sRunId && pSnapshot->sRunId &&
             strcmp(pArtifacts[i].sRunId, pSnapshot->sRunId) != 0 ) {
            pSnapshot->iArtifactCount = i + 1u;
            xwork__run_snapshot_reset_artifacts(pSnapshot);
            return XWORK_ERROR_EXTERNAL_FAILURE;
        }
        if ( pArtifacts[i].sRunId ) {
            free((void *)pArtifacts[i].sRunId);
        }
        pArtifacts[i].sRunId = pSnapshot->sRunId;
    }

    return XWORK_OK;
}

static xwork_status xwork__file_write_snapshot_fields(
    FILE *pFile,
    const xwork_run_snapshot *pSnapshot
)
{
    xwork_status iStatus;

    if ( !pFile || !pSnapshot ) {
        return XWORK_ERROR_INVALID_ARGUMENT;
    }

    iStatus = xwork__file_write_string(pFile, pSnapshot->sRunId);
    if ( iStatus != XWORK_OK ) return iStatus;
    iStatus = xwork__file_write_string(pFile, pSnapshot->sParentRunId);
    if ( iStatus != XWORK_OK ) return iStatus;
    iStatus = xwork__file_write_string(pFile, pSnapshot->sAgentId);
    if ( iStatus != XWORK_OK ) return iStatus;
    iStatus = xwork__file_write_string(pFile, pSnapshot->sTaskId);
    if ( iStatus != XWORK_OK ) return iStatus;
    iStatus = xwork__file_write_string(pFile, pSnapshot->sInstruction);
    if ( iStatus != XWORK_OK ) return iStatus;
    iStatus = xwork__file_write_string(pFile, pSnapshot->sLlmProfileId);
    if ( iStatus != XWORK_OK ) return iStatus;
    iStatus = xwork__file_write_string(pFile, pSnapshot->sSessionProfileId);
    if ( iStatus != XWORK_OK ) return iStatus;
    iStatus = xwork__file_write_bool(
        pFile,
        pSnapshot->tSessionPolicy.bEnableAutoCompact
    );
    if ( iStatus != XWORK_OK ) return iStatus;
    iStatus = xwork__file_write_double(
        pFile,
        pSnapshot->tSessionPolicy.fCompactTriggerRatio
    );
    if ( iStatus != XWORK_OK ) return iStatus;
    iStatus = xwork__file_write_size(
        pFile,
        pSnapshot->tSessionPolicy.iCompactTriggerTurns
    );
    if ( iStatus != XWORK_OK ) return iStatus;
    iStatus = xwork__file_write_size(
        pFile,
        pSnapshot->tSessionPolicy.iReserveOutputTokens
    );
    if ( iStatus != XWORK_OK ) return iStatus;
    iStatus = xwork__file_write_size(
        pFile,
        pSnapshot->tSessionPolicy.iKeepRecentTurns
    );
    if ( iStatus != XWORK_OK ) return iStatus;
    iStatus = xwork__file_write_bool(
        pFile,
        pSnapshot->tSessionPolicy.bKeepActiveToolChain
    );
    if ( iStatus != XWORK_OK ) return iStatus;
    iStatus = xwork__file_write_size(
        pFile,
        (size_t)pSnapshot->tSessionPolicy.eCompactStrategy
    );
    if ( iStatus != XWORK_OK ) return iStatus;
    iStatus = xwork__file_write_string(pFile, pSnapshot->sSessionStateData);
    if ( iStatus != XWORK_OK ) return iStatus;
    iStatus = xwork__file_write_string_array(
        pFile,
        pSnapshot->psWorkspaceIds,
        pSnapshot->iWorkspaceCount
    );
    if ( iStatus != XWORK_OK ) return iStatus;
    iStatus = xwork__file_write_size(pFile, (size_t)pSnapshot->eAutonomy);
    if ( iStatus != XWORK_OK ) return iStatus;
    iStatus = xwork__file_write_size(pFile, (size_t)pSnapshot->eState);
    if ( iStatus != XWORK_OK ) return iStatus;
    iStatus = xwork__file_write_string(pFile, pSnapshot->sLastOutputText);
    if ( iStatus != XWORK_OK ) return iStatus;
    iStatus = xwork__file_write_bool(pFile, pSnapshot->bHasMemoryContext);
    if ( iStatus != XWORK_OK ) return iStatus;
    iStatus = xwork__file_write_string(pFile, pSnapshot->sLastMemoryContextText);
    if ( iStatus != XWORK_OK ) return iStatus;
    iStatus = xwork__file_write_size(pFile, pSnapshot->iLastMemoryWorkspaceCount);
    if ( iStatus != XWORK_OK ) return iStatus;
    iStatus = xwork__file_write_bool(pFile, pSnapshot->bHasToolCall);
    if ( iStatus != XWORK_OK ) return iStatus;
    iStatus = xwork__file_write_string(pFile, pSnapshot->sLastToolCallId);
    if ( iStatus != XWORK_OK ) return iStatus;
    iStatus = xwork__file_write_string(pFile, pSnapshot->sLastToolId);
    if ( iStatus != XWORK_OK ) return iStatus;
    iStatus = xwork__file_write_string(pFile, pSnapshot->sLastToolArgumentsJson);
    if ( iStatus != XWORK_OK ) return iStatus;
    iStatus = xwork__file_write_bool(pFile, pSnapshot->bHasToolResult);
    if ( iStatus != XWORK_OK ) return iStatus;
    iStatus = xwork__file_write_string(pFile, pSnapshot->sLastToolResultText);
    if ( iStatus != XWORK_OK ) return iStatus;
    iStatus = xwork__file_write_string(pFile, pSnapshot->sLastToolVisibleSummary);
    if ( iStatus != XWORK_OK ) return iStatus;
    iStatus = xwork__file_write_bool(pFile, pSnapshot->bHasApprovalRequest);
    if ( iStatus != XWORK_OK ) return iStatus;
    iStatus = xwork__file_write_string(pFile, pSnapshot->sLastApprovalRequestId);
    if ( iStatus != XWORK_OK ) return iStatus;
    iStatus = xwork__file_write_string(pFile, pSnapshot->sLastApprovalToolId);
    if ( iStatus != XWORK_OK ) return iStatus;
    iStatus = xwork__file_write_string(pFile, pSnapshot->sLastApprovalReason);
    if ( iStatus != XWORK_OK ) return iStatus;
    iStatus = xwork__file_write_string(pFile, pSnapshot->sLastApprovalScope);
    if ( iStatus != XWORK_OK ) return iStatus;
    iStatus = xwork__file_write_string(pFile, pSnapshot->sLastApprovalActionSummary);
    if ( iStatus != XWORK_OK ) return iStatus;
    iStatus = xwork__file_write_size(pFile, (size_t)pSnapshot->eLastApprovalRiskLevel);
    if ( iStatus != XWORK_OK ) return iStatus;
    iStatus = xwork__file_write_size(pFile, (size_t)pSnapshot->eLastApprovalState);
    if ( iStatus != XWORK_OK ) return iStatus;
    iStatus = xwork__file_write_size(pFile, pSnapshot->iLastApprovalSequence);
    if ( iStatus != XWORK_OK ) return iStatus;
    iStatus = xwork__file_write_bool(pFile, pSnapshot->bHasCheckpoint);
    if ( iStatus != XWORK_OK ) return iStatus;
    iStatus = xwork__file_write_string(pFile, pSnapshot->sLastCheckpointId);
    if ( iStatus != XWORK_OK ) return iStatus;
    iStatus = xwork__file_write_string(pFile, pSnapshot->sLastCheckpointPendingStep);
    if ( iStatus != XWORK_OK ) return iStatus;
    iStatus = xwork__file_write_string(pFile, pSnapshot->sLastCheckpointSessionStateRef);
    if ( iStatus != XWORK_OK ) return iStatus;
    iStatus = xwork__file_write_string(pFile, pSnapshot->sLastCheckpointToolOutputsRef);
    if ( iStatus != XWORK_OK ) return iStatus;
    iStatus = xwork__file_write_string(
        pFile,
        pSnapshot->sLastCheckpointWorkspaceSnapshotRef
    );
    if ( iStatus != XWORK_OK ) return iStatus;
    iStatus = xwork__file_write_string(pFile, pSnapshot->sLastCheckpointArtifactRefs);
    if ( iStatus != XWORK_OK ) return iStatus;
    iStatus = xwork__file_write_size(pFile, (size_t)pSnapshot->eLastCheckpointKind);
    if ( iStatus != XWORK_OK ) return iStatus;
    iStatus = xwork__file_write_size(pFile, (size_t)pSnapshot->eLastCheckpointRunState);
    if ( iStatus != XWORK_OK ) return iStatus;
    iStatus = xwork__file_write_size(pFile, pSnapshot->iLastCheckpointSequence);
    if ( iStatus != XWORK_OK ) return iStatus;
    iStatus = xwork__file_write_artifact_array(
        pFile,
        pSnapshot->pArtifacts,
        pSnapshot->iArtifactCount
    );
    if ( iStatus != XWORK_OK ) return iStatus;
    iStatus = xwork__file_write_size(pFile, pSnapshot->iNextEventSequence);
    if ( iStatus != XWORK_OK ) return iStatus;
    iStatus = xwork__file_write_size(pFile, pSnapshot->iNextArtifactSequence);
    if ( iStatus != XWORK_OK ) return iStatus;
    iStatus = xwork__file_write_size(pFile, pSnapshot->iNextApprovalSequence);
    if ( iStatus != XWORK_OK ) return iStatus;
    return xwork__file_write_size(pFile, pSnapshot->iNextCheckpointSequence);
}

static xwork_status xwork__file_read_snapshot_fields(
    FILE *pFile,
    uint64_t iFormatVersion,
    xwork_run_snapshot *pSnapshot
)
{
    size_t iValue;
    xwork_status iStatus;

    if ( !pFile || !pSnapshot ) {
        return XWORK_ERROR_INVALID_ARGUMENT;
    }

    iStatus = xwork__file_read_string(pFile, &pSnapshot->sRunId);
    if ( iStatus != XWORK_OK ) return iStatus;
    iStatus = xwork__file_read_string(pFile, &pSnapshot->sParentRunId);
    if ( iStatus != XWORK_OK ) return iStatus;
    if ( iFormatVersion >= 4u ) {
        iStatus = xwork__file_read_string(pFile, &pSnapshot->sAgentId);
        if ( iStatus != XWORK_OK ) return iStatus;
        iStatus = xwork__file_read_string(pFile, &pSnapshot->sTaskId);
        if ( iStatus != XWORK_OK ) return iStatus;
    }
    iStatus = xwork__file_read_string(pFile, &pSnapshot->sInstruction);
    if ( iStatus != XWORK_OK ) return iStatus;
    iStatus = xwork__file_read_string(pFile, &pSnapshot->sLlmProfileId);
    if ( iStatus != XWORK_OK ) return iStatus;
    iStatus = xwork__file_read_string(pFile, &pSnapshot->sSessionProfileId);
    if ( iStatus != XWORK_OK ) return iStatus;
    iStatus = xwork__file_read_bool(
        pFile,
        &pSnapshot->tSessionPolicy.bEnableAutoCompact
    );
    if ( iStatus != XWORK_OK ) return iStatus;
    iStatus = xwork__file_read_double(
        pFile,
        &pSnapshot->tSessionPolicy.fCompactTriggerRatio
    );
    if ( iStatus != XWORK_OK ) return iStatus;
    iStatus = xwork__file_read_size(
        pFile,
        &pSnapshot->tSessionPolicy.iCompactTriggerTurns
    );
    if ( iStatus != XWORK_OK ) return iStatus;
    if ( iFormatVersion >= 3u ) {
        iStatus = xwork__file_read_size(
            pFile,
            &pSnapshot->tSessionPolicy.iReserveOutputTokens
        );
        if ( iStatus != XWORK_OK ) return iStatus;
        iStatus = xwork__file_read_size(
            pFile,
            &pSnapshot->tSessionPolicy.iKeepRecentTurns
        );
        if ( iStatus != XWORK_OK ) return iStatus;
        iStatus = xwork__file_read_bool(
            pFile,
            &pSnapshot->tSessionPolicy.bKeepActiveToolChain
        );
        if ( iStatus != XWORK_OK ) return iStatus;
        iStatus = xwork__file_read_size(pFile, &iValue);
        if ( iStatus != XWORK_OK ) return iStatus;
        pSnapshot->tSessionPolicy.eCompactStrategy =
            (xwork_session_compact_strategy)iValue;
    }
    iStatus = xwork__file_read_string(pFile, &pSnapshot->sSessionStateData);
    if ( iStatus != XWORK_OK ) return iStatus;
    iStatus = xwork__file_read_string_array(
        pFile,
        &pSnapshot->psWorkspaceIds,
        &pSnapshot->iWorkspaceCount
    );
    if ( iStatus != XWORK_OK ) return iStatus;
    iStatus = xwork__file_read_size(pFile, &iValue);
    if ( iStatus != XWORK_OK ) return iStatus;
    pSnapshot->eAutonomy = (xwork_autonomy_mode)iValue;
    iStatus = xwork__file_read_size(pFile, &iValue);
    if ( iStatus != XWORK_OK ) return iStatus;
    pSnapshot->eState = (xwork_run_state)iValue;
    iStatus = xwork__file_read_string(pFile, &pSnapshot->sLastOutputText);
    if ( iStatus != XWORK_OK ) return iStatus;
    iStatus = xwork__file_read_bool(pFile, &pSnapshot->bHasMemoryContext);
    if ( iStatus != XWORK_OK ) return iStatus;
    iStatus = xwork__file_read_string(pFile, &pSnapshot->sLastMemoryContextText);
    if ( iStatus != XWORK_OK ) return iStatus;
    iStatus = xwork__file_read_size(pFile, &pSnapshot->iLastMemoryWorkspaceCount);
    if ( iStatus != XWORK_OK ) return iStatus;
    iStatus = xwork__file_read_bool(pFile, &pSnapshot->bHasToolCall);
    if ( iStatus != XWORK_OK ) return iStatus;
    iStatus = xwork__file_read_string(pFile, &pSnapshot->sLastToolCallId);
    if ( iStatus != XWORK_OK ) return iStatus;
    iStatus = xwork__file_read_string(pFile, &pSnapshot->sLastToolId);
    if ( iStatus != XWORK_OK ) return iStatus;
    iStatus = xwork__file_read_string(pFile, &pSnapshot->sLastToolArgumentsJson);
    if ( iStatus != XWORK_OK ) return iStatus;
    iStatus = xwork__file_read_bool(pFile, &pSnapshot->bHasToolResult);
    if ( iStatus != XWORK_OK ) return iStatus;
    iStatus = xwork__file_read_string(pFile, &pSnapshot->sLastToolResultText);
    if ( iStatus != XWORK_OK ) return iStatus;
    iStatus = xwork__file_read_string(pFile, &pSnapshot->sLastToolVisibleSummary);
    if ( iStatus != XWORK_OK ) return iStatus;
    iStatus = xwork__file_read_bool(pFile, &pSnapshot->bHasApprovalRequest);
    if ( iStatus != XWORK_OK ) return iStatus;
    iStatus = xwork__file_read_string(pFile, &pSnapshot->sLastApprovalRequestId);
    if ( iStatus != XWORK_OK ) return iStatus;
    iStatus = xwork__file_read_string(pFile, &pSnapshot->sLastApprovalToolId);
    if ( iStatus != XWORK_OK ) return iStatus;
    iStatus = xwork__file_read_string(pFile, &pSnapshot->sLastApprovalReason);
    if ( iStatus != XWORK_OK ) return iStatus;
    iStatus = xwork__file_read_string(pFile, &pSnapshot->sLastApprovalScope);
    if ( iStatus != XWORK_OK ) return iStatus;
    iStatus = xwork__file_read_string(pFile, &pSnapshot->sLastApprovalActionSummary);
    if ( iStatus != XWORK_OK ) return iStatus;
    iStatus = xwork__file_read_size(pFile, &iValue);
    if ( iStatus != XWORK_OK ) return iStatus;
    pSnapshot->eLastApprovalRiskLevel = (xwork_risk_level)iValue;
    iStatus = xwork__file_read_size(pFile, &iValue);
    if ( iStatus != XWORK_OK ) return iStatus;
    pSnapshot->eLastApprovalState = (xwork_approval_state)iValue;
    iStatus = xwork__file_read_size(pFile, &pSnapshot->iLastApprovalSequence);
    if ( iStatus != XWORK_OK ) return iStatus;
    iStatus = xwork__file_read_bool(pFile, &pSnapshot->bHasCheckpoint);
    if ( iStatus != XWORK_OK ) return iStatus;
    iStatus = xwork__file_read_string(pFile, &pSnapshot->sLastCheckpointId);
    if ( iStatus != XWORK_OK ) return iStatus;
    iStatus = xwork__file_read_string(pFile, &pSnapshot->sLastCheckpointPendingStep);
    if ( iStatus != XWORK_OK ) return iStatus;
    iStatus = xwork__file_read_string(pFile, &pSnapshot->sLastCheckpointSessionStateRef);
    if ( iStatus != XWORK_OK ) return iStatus;
    iStatus = xwork__file_read_string(pFile, &pSnapshot->sLastCheckpointToolOutputsRef);
    if ( iStatus != XWORK_OK ) return iStatus;
    iStatus = xwork__file_read_string(
        pFile,
        &pSnapshot->sLastCheckpointWorkspaceSnapshotRef
    );
    if ( iStatus != XWORK_OK ) return iStatus;
    iStatus = xwork__file_read_string(pFile, &pSnapshot->sLastCheckpointArtifactRefs);
    if ( iStatus != XWORK_OK ) return iStatus;
    iStatus = xwork__file_read_size(pFile, &iValue);
    if ( iStatus != XWORK_OK ) return iStatus;
    pSnapshot->eLastCheckpointKind = (xwork_checkpoint_kind)iValue;
    iStatus = xwork__file_read_size(pFile, &iValue);
    if ( iStatus != XWORK_OK ) return iStatus;
    pSnapshot->eLastCheckpointRunState = (xwork_run_state)iValue;
    iStatus = xwork__file_read_size(pFile, &pSnapshot->iLastCheckpointSequence);
    if ( iStatus != XWORK_OK ) return iStatus;
    iStatus = xwork__file_read_artifact_array(pFile, iFormatVersion, pSnapshot);
    if ( iStatus != XWORK_OK ) return iStatus;
    iStatus = xwork__file_read_size(pFile, &pSnapshot->iNextEventSequence);
    if ( iStatus != XWORK_OK ) return iStatus;
    iStatus = xwork__file_read_size(pFile, &pSnapshot->iNextArtifactSequence);
    if ( iStatus != XWORK_OK ) return iStatus;
    iStatus = xwork__file_read_size(pFile, &pSnapshot->iNextApprovalSequence);
    if ( iStatus != XWORK_OK ) return iStatus;
    return xwork__file_read_size(pFile, &pSnapshot->iNextCheckpointSequence);
}

static xwork_status xwork__file_write_session_policy(
    FILE *pFile,
    const xwork_session_policy *pPolicy
)
{
    xwork_status iStatus;

    if ( !pFile || !pPolicy ) {
        return XWORK_ERROR_INVALID_ARGUMENT;
    }
    iStatus = xwork__file_write_bool(pFile, pPolicy->bEnableAutoCompact);
    if ( iStatus != XWORK_OK ) return iStatus;
    iStatus = xwork__file_write_double(pFile, pPolicy->fCompactTriggerRatio);
    if ( iStatus != XWORK_OK ) return iStatus;
    iStatus = xwork__file_write_size(pFile, pPolicy->iCompactTriggerTurns);
    if ( iStatus != XWORK_OK ) return iStatus;
    iStatus = xwork__file_write_size(pFile, pPolicy->iReserveOutputTokens);
    if ( iStatus != XWORK_OK ) return iStatus;
    iStatus = xwork__file_write_size(pFile, pPolicy->iKeepRecentTurns);
    if ( iStatus != XWORK_OK ) return iStatus;
    iStatus = xwork__file_write_bool(pFile, pPolicy->bKeepActiveToolChain);
    if ( iStatus != XWORK_OK ) return iStatus;
    return xwork__file_write_size(pFile, (size_t)pPolicy->eCompactStrategy);
}

static xwork_status xwork__file_read_session_policy(
    FILE *pFile,
    xwork_session_policy *pPolicy
)
{
    size_t iValue;
    xwork_status iStatus;

    if ( !pFile || !pPolicy ) {
        return XWORK_ERROR_INVALID_ARGUMENT;
    }
    xwork_session_policy_init(pPolicy);
    iStatus = xwork__file_read_bool(pFile, &pPolicy->bEnableAutoCompact);
    if ( iStatus != XWORK_OK ) return iStatus;
    iStatus = xwork__file_read_double(pFile, &pPolicy->fCompactTriggerRatio);
    if ( iStatus != XWORK_OK ) return iStatus;
    iStatus = xwork__file_read_size(pFile, &pPolicy->iCompactTriggerTurns);
    if ( iStatus != XWORK_OK ) return iStatus;
    iStatus = xwork__file_read_size(pFile, &pPolicy->iReserveOutputTokens);
    if ( iStatus != XWORK_OK ) return iStatus;
    iStatus = xwork__file_read_size(pFile, &pPolicy->iKeepRecentTurns);
    if ( iStatus != XWORK_OK ) return iStatus;
    iStatus = xwork__file_read_bool(pFile, &pPolicy->bKeepActiveToolChain);
    if ( iStatus != XWORK_OK ) return iStatus;
    iStatus = xwork__file_read_size(pFile, &iValue);
    if ( iStatus != XWORK_OK ) return iStatus;
    pPolicy->eCompactStrategy = (xwork_session_compact_strategy)iValue;
    return XWORK_OK;
}

static xwork_status xwork__file_write_task_graph_result(
    FILE *pFile,
    const xwork_task_graph_result *pResult
)
{
    xwork_status iStatus;

    if ( !pFile || !pResult ) {
        return XWORK_ERROR_INVALID_ARGUMENT;
    }
    iStatus = xwork__file_write_i64(pFile, (int64_t)pResult->iStatus);
    if ( iStatus != XWORK_OK ) return iStatus;
    iStatus = xwork__file_write_size(pFile, pResult->iTotalCount);
    if ( iStatus != XWORK_OK ) return iStatus;
    iStatus = xwork__file_write_size(pFile, pResult->iCompletedCount);
    if ( iStatus != XWORK_OK ) return iStatus;
    iStatus = xwork__file_write_size(pFile, pResult->iFailedCount);
    if ( iStatus != XWORK_OK ) return iStatus;
    iStatus = xwork__file_write_size(pFile, pResult->iCancelledCount);
    if ( iStatus != XWORK_OK ) return iStatus;
    return xwork__file_write_size(pFile, pResult->iSkippedCount);
}

static xwork_status xwork__file_read_task_graph_result(
    FILE *pFile,
    xwork_task_graph_result *pResult
)
{
    int64_t iStatusValue;
    xwork_status iStatus;

    if ( !pFile || !pResult ) {
        return XWORK_ERROR_INVALID_ARGUMENT;
    }
    xwork_task_graph_result_init(pResult);
    iStatus = xwork__file_read_i64(pFile, &iStatusValue);
    if ( iStatus != XWORK_OK ) return iStatus;
    pResult->iStatus = (xwork_status)iStatusValue;
    iStatus = xwork__file_read_size(pFile, &pResult->iTotalCount);
    if ( iStatus != XWORK_OK ) return iStatus;
    iStatus = xwork__file_read_size(pFile, &pResult->iCompletedCount);
    if ( iStatus != XWORK_OK ) return iStatus;
    iStatus = xwork__file_read_size(pFile, &pResult->iFailedCount);
    if ( iStatus != XWORK_OK ) return iStatus;
    iStatus = xwork__file_read_size(pFile, &pResult->iCancelledCount);
    if ( iStatus != XWORK_OK ) return iStatus;
    return xwork__file_read_size(pFile, &pResult->iSkippedCount);
}

static xwork_status xwork__file_write_task_node_snapshot(
    FILE *pFile,
    const xwork_task_node_snapshot *pNode
)
{
    xwork_status iStatus;

    if ( !pFile || !pNode ) {
        return XWORK_ERROR_INVALID_ARGUMENT;
    }
    iStatus = xwork__file_write_string(pFile, pNode->sTaskId);
    if ( iStatus != XWORK_OK ) return iStatus;
    iStatus = xwork__file_write_string(pFile, pNode->sAgentId);
    if ( iStatus != XWORK_OK ) return iStatus;
    iStatus = xwork__file_write_string(pFile, pNode->sRunId);
    if ( iStatus != XWORK_OK ) return iStatus;
    iStatus = xwork__file_write_string(pFile, pNode->sParentRunId);
    if ( iStatus != XWORK_OK ) return iStatus;
    iStatus = xwork__file_write_string(pFile, pNode->sInstruction);
    if ( iStatus != XWORK_OK ) return iStatus;
    iStatus = xwork__file_write_string(pFile, pNode->sLlmProfileId);
    if ( iStatus != XWORK_OK ) return iStatus;
    iStatus = xwork__file_write_string(pFile, pNode->sSessionProfileId);
    if ( iStatus != XWORK_OK ) return iStatus;
    iStatus = xwork__file_write_string_array(
        pFile,
        pNode->psWorkspaceIds,
        pNode->iWorkspaceCount
    );
    if ( iStatus != XWORK_OK ) return iStatus;
    iStatus = xwork__file_write_string_array(
        pFile,
        pNode->psDependencyTaskIds,
        pNode->iDependencyCount
    );
    if ( iStatus != XWORK_OK ) return iStatus;
    iStatus = xwork__file_write_size(pFile, (size_t)pNode->eAutonomy);
    if ( iStatus != XWORK_OK ) return iStatus;
    iStatus = xwork__file_write_session_policy(pFile, &pNode->tSessionPolicy);
    if ( iStatus != XWORK_OK ) return iStatus;
    iStatus = xwork__file_write_size(pFile, (size_t)pNode->eState);
    if ( iStatus != XWORK_OK ) return iStatus;
    iStatus = xwork__file_write_i64(pFile, (int64_t)pNode->iStatus);
    if ( iStatus != XWORK_OK ) return iStatus;
    iStatus = xwork__file_write_size(pFile, pNode->iAttemptCount);
    if ( iStatus != XWORK_OK ) return iStatus;
    iStatus = xwork__file_write_size(pFile, pNode->iMaxTurns);
    if ( iStatus != XWORK_OK ) return iStatus;
    iStatus = xwork__file_write_size(pFile, pNode->iTimeoutMs);
    if ( iStatus != XWORK_OK ) return iStatus;
    return xwork__file_write_size(pFile, pNode->iMaxRetries);
}

static xwork_status xwork__file_read_task_node_snapshot(
    FILE *pFile,
    uint64_t iFormatVersion,
    xwork_task_node_snapshot *pNode
)
{
    size_t iValue;
    int64_t iStatusValue;
    xwork_status iStatus;

    if ( !pFile || !pNode ) {
        return XWORK_ERROR_INVALID_ARGUMENT;
    }
    xwork_task_node_snapshot_init(pNode);
    iStatus = xwork__file_read_string(pFile, &pNode->sTaskId);
    if ( iStatus != XWORK_OK ) return iStatus;
    iStatus = xwork__file_read_string(pFile, &pNode->sAgentId);
    if ( iStatus != XWORK_OK ) return iStatus;
    iStatus = xwork__file_read_string(pFile, &pNode->sRunId);
    if ( iStatus != XWORK_OK ) return iStatus;
    iStatus = xwork__file_read_string(pFile, &pNode->sParentRunId);
    if ( iStatus != XWORK_OK ) return iStatus;
    iStatus = xwork__file_read_string(pFile, &pNode->sInstruction);
    if ( iStatus != XWORK_OK ) return iStatus;
    iStatus = xwork__file_read_string(pFile, &pNode->sLlmProfileId);
    if ( iStatus != XWORK_OK ) return iStatus;
    iStatus = xwork__file_read_string(pFile, &pNode->sSessionProfileId);
    if ( iStatus != XWORK_OK ) return iStatus;
    iStatus = xwork__file_read_string_array(
        pFile,
        &pNode->psWorkspaceIds,
        &pNode->iWorkspaceCount
    );
    if ( iStatus != XWORK_OK ) return iStatus;
    iStatus = xwork__file_read_string_array(
        pFile,
        &pNode->psDependencyTaskIds,
        &pNode->iDependencyCount
    );
    if ( iStatus != XWORK_OK ) return iStatus;
    iStatus = xwork__file_read_size(pFile, &iValue);
    if ( iStatus != XWORK_OK ) return iStatus;
    pNode->eAutonomy = (xwork_autonomy_mode)iValue;
    iStatus = xwork__file_read_session_policy(pFile, &pNode->tSessionPolicy);
    if ( iStatus != XWORK_OK ) return iStatus;
    iStatus = xwork__file_read_size(pFile, &iValue);
    if ( iStatus != XWORK_OK ) return iStatus;
    pNode->eState = (xwork_task_state)iValue;
    iStatus = xwork__file_read_i64(pFile, &iStatusValue);
    if ( iStatus != XWORK_OK ) return iStatus;
    pNode->iStatus = (xwork_status)iStatusValue;
    iStatus = xwork__file_read_size(pFile, &pNode->iAttemptCount);
    if ( iStatus != XWORK_OK ) return iStatus;
    if ( iFormatVersion >= 14u ) {
        iStatus = xwork__file_read_size(pFile, &pNode->iMaxTurns);
        if ( iStatus != XWORK_OK ) return iStatus;
        iStatus = xwork__file_read_size(pFile, &pNode->iTimeoutMs);
        if ( iStatus != XWORK_OK ) return iStatus;
    }
    return xwork__file_read_size(pFile, &pNode->iMaxRetries);
}

static xwork_status xwork__file_write_handoff_summary(
    FILE *pFile,
    const xwork_handoff_summary *pHandoff
)
{
    xwork_status iStatus;

    if ( !pFile || !pHandoff ) {
        return XWORK_ERROR_INVALID_ARGUMENT;
    }
    iStatus = xwork__file_write_string(pFile, pHandoff->sHandoffId);
    if ( iStatus != XWORK_OK ) return iStatus;
    iStatus = xwork__file_write_string(pFile, pHandoff->sFromTaskId);
    if ( iStatus != XWORK_OK ) return iStatus;
    iStatus = xwork__file_write_string(pFile, pHandoff->sToTaskId);
    if ( iStatus != XWORK_OK ) return iStatus;
    iStatus = xwork__file_write_string(pFile, pHandoff->sReason);
    if ( iStatus != XWORK_OK ) return iStatus;
    iStatus = xwork__file_write_size(pFile, (size_t)pHandoff->eState);
    if ( iStatus != XWORK_OK ) return iStatus;
    iStatus = xwork__file_write_i64(pFile, (int64_t)pHandoff->iStatus);
    if ( iStatus != XWORK_OK ) return iStatus;
    iStatus = xwork__file_write_string(pFile, pHandoff->sMessage);
    if ( iStatus != XWORK_OK ) return iStatus;
    iStatus = xwork__file_write_string_array(
        pFile,
        pHandoff->psArtifactRefs,
        pHandoff->iArtifactRefCount
    );
    if ( iStatus != XWORK_OK ) return iStatus;
    iStatus = xwork__file_write_string_array(
        pFile,
        pHandoff->psMemoryContextRefs,
        pHandoff->iMemoryContextRefCount
    );
    if ( iStatus != XWORK_OK ) return iStatus;
    iStatus = xwork__file_write_string_array(
        pFile,
        pHandoff->psSharedWorkspaceIds,
        pHandoff->iSharedWorkspaceCount
    );
    if ( iStatus != XWORK_OK ) return iStatus;
    iStatus = xwork__file_write_bool(pFile, pHandoff->bReadOnlySharedContext);
    if ( iStatus != XWORK_OK ) return iStatus;
    return xwork__file_write_bool(pFile, pHandoff->bWritableWorkspace);
}

static xwork_status xwork__file_read_handoff_summary(
    FILE *pFile,
    xwork_handoff_summary *pHandoff
)
{
    size_t iValue;
    int64_t iStatusValue;
    xwork_status iStatus;

    if ( !pFile || !pHandoff ) {
        return XWORK_ERROR_INVALID_ARGUMENT;
    }
    xwork_handoff_summary_init(pHandoff);
    iStatus = xwork__file_read_string(pFile, &pHandoff->sHandoffId);
    if ( iStatus != XWORK_OK ) return iStatus;
    iStatus = xwork__file_read_string(pFile, &pHandoff->sFromTaskId);
    if ( iStatus != XWORK_OK ) return iStatus;
    iStatus = xwork__file_read_string(pFile, &pHandoff->sToTaskId);
    if ( iStatus != XWORK_OK ) return iStatus;
    iStatus = xwork__file_read_string(pFile, &pHandoff->sReason);
    if ( iStatus != XWORK_OK ) return iStatus;
    iStatus = xwork__file_read_size(pFile, &iValue);
    if ( iStatus != XWORK_OK ) return iStatus;
    pHandoff->eState = (xwork_handoff_state)iValue;
    iStatus = xwork__file_read_i64(pFile, &iStatusValue);
    if ( iStatus != XWORK_OK ) return iStatus;
    pHandoff->iStatus = (xwork_status)iStatusValue;
    iStatus = xwork__file_read_string(pFile, &pHandoff->sMessage);
    if ( iStatus != XWORK_OK ) return iStatus;
    iStatus = xwork__file_read_string_array(
        pFile,
        &pHandoff->psArtifactRefs,
        &pHandoff->iArtifactRefCount
    );
    if ( iStatus != XWORK_OK ) return iStatus;
    iStatus = xwork__file_read_string_array(
        pFile,
        &pHandoff->psMemoryContextRefs,
        &pHandoff->iMemoryContextRefCount
    );
    if ( iStatus != XWORK_OK ) return iStatus;
    iStatus = xwork__file_read_string_array(
        pFile,
        &pHandoff->psSharedWorkspaceIds,
        &pHandoff->iSharedWorkspaceCount
    );
    if ( iStatus != XWORK_OK ) return iStatus;
    iStatus = xwork__file_read_bool(pFile, &pHandoff->bReadOnlySharedContext);
    if ( iStatus != XWORK_OK ) return iStatus;
    return xwork__file_read_bool(pFile, &pHandoff->bWritableWorkspace);
}

static xwork_status xwork__file_write_task_graph_snapshot_fields(
    FILE *pFile,
    const xwork_task_graph_snapshot *pSnapshot
)
{
    xwork_status iStatus;
    size_t i;

    if ( !pFile || !pSnapshot ) {
        return XWORK_ERROR_INVALID_ARGUMENT;
    }
    iStatus = xwork__file_write_string(pFile, pSnapshot->sGraphId);
    if ( iStatus != XWORK_OK ) return iStatus;
    iStatus = xwork__file_write_size(pFile, pSnapshot->iMaxConcurrency);
    if ( iStatus != XWORK_OK ) return iStatus;
    iStatus = xwork__file_write_size(pFile, (size_t)pSnapshot->eFailurePolicy);
    if ( iStatus != XWORK_OK ) return iStatus;
    iStatus = xwork__file_write_bool(pFile, pSnapshot->bCancelRequested);
    if ( iStatus != XWORK_OK ) return iStatus;
    iStatus = xwork__file_write_string(pFile, pSnapshot->sCancelReason);
    if ( iStatus != XWORK_OK ) return iStatus;
    iStatus = xwork__file_write_bool(pFile, pSnapshot->bPauseRequested);
    if ( iStatus != XWORK_OK ) return iStatus;
    iStatus = xwork__file_write_string(pFile, pSnapshot->sPauseReason);
    if ( iStatus != XWORK_OK ) return iStatus;
    iStatus = xwork__file_write_task_graph_result(pFile, &pSnapshot->tResult);
    if ( iStatus != XWORK_OK ) return iStatus;
    iStatus = xwork__file_write_size(pFile, pSnapshot->tNodes.iCount);
    if ( iStatus != XWORK_OK ) return iStatus;
    for ( i = 0u; i < pSnapshot->tNodes.iCount; ++i ) {
        iStatus = xwork__file_write_task_node_snapshot(
            pFile,
            &pSnapshot->tNodes.pItems[i]
        );
        if ( iStatus != XWORK_OK ) {
            return iStatus;
        }
    }
    iStatus = xwork__file_write_size(pFile, pSnapshot->tHandoffs.iCount);
    if ( iStatus != XWORK_OK ) return iStatus;
    for ( i = 0u; i < pSnapshot->tHandoffs.iCount; ++i ) {
        iStatus = xwork__file_write_handoff_summary(
            pFile,
            &pSnapshot->tHandoffs.pItems[i]
        );
        if ( iStatus != XWORK_OK ) {
            return iStatus;
        }
    }
    return XWORK_OK;
}

static xwork_status xwork__file_read_task_graph_snapshot_fields(
    FILE *pFile,
    uint64_t iFormatVersion,
    xwork_task_graph_snapshot *pSnapshot
)
{
    xwork_status iStatus;
    size_t i;
    size_t iNodeCount;
    size_t iHandoffCount;
    size_t iValue;

    if ( !pFile || !pSnapshot ) {
        return XWORK_ERROR_INVALID_ARGUMENT;
    }
    iStatus = xwork__file_read_string(pFile, &pSnapshot->sGraphId);
    if ( iStatus != XWORK_OK ) return iStatus;
    iStatus = xwork__file_read_size(pFile, &pSnapshot->iMaxConcurrency);
    if ( iStatus != XWORK_OK ) return iStatus;
    iStatus = xwork__file_read_size(pFile, &iValue);
    if ( iStatus != XWORK_OK ) return iStatus;
    pSnapshot->eFailurePolicy = (xwork_task_failure_policy)iValue;
    iStatus = xwork__file_read_bool(pFile, &pSnapshot->bCancelRequested);
    if ( iStatus != XWORK_OK ) return iStatus;
    iStatus = xwork__file_read_string(pFile, &pSnapshot->sCancelReason);
    if ( iStatus != XWORK_OK ) return iStatus;
    if ( iFormatVersion >= 5u ) {
        iStatus = xwork__file_read_bool(pFile, &pSnapshot->bPauseRequested);
        if ( iStatus != XWORK_OK ) return iStatus;
        iStatus = xwork__file_read_string(pFile, &pSnapshot->sPauseReason);
        if ( iStatus != XWORK_OK ) return iStatus;
    }
    iStatus = xwork__file_read_task_graph_result(pFile, &pSnapshot->tResult);
    if ( iStatus != XWORK_OK ) return iStatus;
    iStatus = xwork__file_read_size(pFile, &iNodeCount);
    if ( iStatus != XWORK_OK ) return iStatus;
    if ( iNodeCount > 0u ) {
        pSnapshot->tNodes.pItems = (xwork_task_node_snapshot *)calloc(
            iNodeCount,
            sizeof(*pSnapshot->tNodes.pItems)
        );
        if ( !pSnapshot->tNodes.pItems ) {
            return XWORK_ERROR_NO_MEMORY;
        }
        pSnapshot->tNodes.iCount = iNodeCount;
        for ( i = 0u; i < iNodeCount; ++i ) {
        iStatus = xwork__file_read_task_node_snapshot(
            pFile,
            iFormatVersion,
            &pSnapshot->tNodes.pItems[i]
        );
            if ( iStatus != XWORK_OK ) {
                return iStatus;
            }
        }
    }
    if ( iFormatVersion < 10u ) {
        return XWORK_OK;
    }
    iStatus = xwork__file_read_size(pFile, &iHandoffCount);
    if ( iStatus != XWORK_OK ) return iStatus;
    if ( iHandoffCount == 0u ) {
        return XWORK_OK;
    }
    pSnapshot->tHandoffs.pItems = (xwork_handoff_summary *)calloc(
        iHandoffCount,
        sizeof(*pSnapshot->tHandoffs.pItems)
    );
    if ( !pSnapshot->tHandoffs.pItems ) {
        return XWORK_ERROR_NO_MEMORY;
    }
    pSnapshot->tHandoffs.iCount = iHandoffCount;
    for ( i = 0u; i < iHandoffCount; ++i ) {
        iStatus = xwork__file_read_handoff_summary(
            pFile,
            &pSnapshot->tHandoffs.pItems[i]
        );
        if ( iStatus != XWORK_OK ) {
            return iStatus;
        }
    }
    return XWORK_OK;
}

static xwork_status xwork__file_write_agent_snapshot(
    FILE *pFile,
    const xwork_agent_snapshot *pAgent
)
{
    xwork_status iStatus;

    if ( !pFile || !pAgent ) {
        return XWORK_ERROR_INVALID_ARGUMENT;
    }
    iStatus = xwork__file_write_string(pFile, pAgent->sAgentId);
    if ( iStatus != XWORK_OK ) return iStatus;
    iStatus = xwork__file_write_string(pFile, pAgent->sDisplayName);
    if ( iStatus != XWORK_OK ) return iStatus;
    iStatus = xwork__file_write_string(pFile, pAgent->sDescription);
    if ( iStatus != XWORK_OK ) return iStatus;
    iStatus = xwork__file_write_size(pFile, (size_t)pAgent->eRole);
    if ( iStatus != XWORK_OK ) return iStatus;
    iStatus = xwork__file_write_string(pFile, pAgent->sLlmProfileId);
    if ( iStatus != XWORK_OK ) return iStatus;
    iStatus = xwork__file_write_string(pFile, pAgent->sSessionProfileId);
    if ( iStatus != XWORK_OK ) return iStatus;
    iStatus = xwork__file_write_size(pFile, (size_t)pAgent->eAutonomy);
    if ( iStatus != XWORK_OK ) return iStatus;
    iStatus = xwork__file_write_size(pFile, pAgent->iMaxTurns);
    if ( iStatus != XWORK_OK ) return iStatus;
    iStatus = xwork__file_write_size(pFile, pAgent->iTimeoutMs);
    if ( iStatus != XWORK_OK ) return iStatus;
    return xwork__file_write_size(pFile, pAgent->iMaxRetries);
}

static xwork_status xwork__file_read_agent_snapshot(
    FILE *pFile,
    xwork_agent_snapshot *pAgent
)
{
    size_t iValue;
    xwork_status iStatus;

    if ( !pFile || !pAgent ) {
        return XWORK_ERROR_INVALID_ARGUMENT;
    }
    xwork_agent_snapshot_init(pAgent);
    iStatus = xwork__file_read_string(pFile, &pAgent->sAgentId);
    if ( iStatus != XWORK_OK ) return iStatus;
    iStatus = xwork__file_read_string(pFile, &pAgent->sDisplayName);
    if ( iStatus != XWORK_OK ) return iStatus;
    iStatus = xwork__file_read_string(pFile, &pAgent->sDescription);
    if ( iStatus != XWORK_OK ) return iStatus;
    iStatus = xwork__file_read_size(pFile, &iValue);
    if ( iStatus != XWORK_OK ) return iStatus;
    pAgent->eRole = (xwork_agent_role)iValue;
    iStatus = xwork__file_read_string(pFile, &pAgent->sLlmProfileId);
    if ( iStatus != XWORK_OK ) return iStatus;
    iStatus = xwork__file_read_string(pFile, &pAgent->sSessionProfileId);
    if ( iStatus != XWORK_OK ) return iStatus;
    iStatus = xwork__file_read_size(pFile, &iValue);
    if ( iStatus != XWORK_OK ) return iStatus;
    pAgent->eAutonomy = (xwork_autonomy_mode)iValue;
    iStatus = xwork__file_read_size(pFile, &pAgent->iMaxTurns);
    if ( iStatus != XWORK_OK ) return iStatus;
    iStatus = xwork__file_read_size(pFile, &pAgent->iTimeoutMs);
    if ( iStatus != XWORK_OK ) return iStatus;
    return xwork__file_read_size(pFile, &pAgent->iMaxRetries);
}

static xwork_status xwork__file_write_agent_pool_snapshot_fields(
    FILE *pFile,
    const xwork_agent_pool_snapshot *pSnapshot
)
{
    size_t i;
    xwork_status iStatus;

    if ( !pFile || !pSnapshot ) {
        return XWORK_ERROR_INVALID_ARGUMENT;
    }
    iStatus = xwork__file_write_string(pFile, pSnapshot->sPoolId);
    if ( iStatus != XWORK_OK ) return iStatus;
    iStatus = xwork__file_write_size(pFile, pSnapshot->tAgents.iCount);
    if ( iStatus != XWORK_OK ) return iStatus;
    for ( i = 0u; i < pSnapshot->tAgents.iCount; ++i ) {
        iStatus = xwork__file_write_agent_snapshot(
            pFile,
            &pSnapshot->tAgents.pItems[i]
        );
        if ( iStatus != XWORK_OK ) {
            return iStatus;
        }
    }
    return XWORK_OK;
}

static xwork_status xwork__file_read_agent_pool_snapshot_fields(
    FILE *pFile,
    xwork_agent_pool_snapshot *pSnapshot
)
{
    size_t i;
    size_t iAgentCount;
    xwork_status iStatus;

    if ( !pFile || !pSnapshot ) {
        return XWORK_ERROR_INVALID_ARGUMENT;
    }
    iStatus = xwork__file_read_string(pFile, &pSnapshot->sPoolId);
    if ( iStatus != XWORK_OK ) return iStatus;
    iStatus = xwork__file_read_size(pFile, &iAgentCount);
    if ( iStatus != XWORK_OK ) return iStatus;
    if ( iAgentCount == 0u ) {
        return XWORK_OK;
    }
    pSnapshot->tAgents.pItems = (xwork_agent_snapshot *)calloc(
        iAgentCount,
        sizeof(*pSnapshot->tAgents.pItems)
    );
    if ( !pSnapshot->tAgents.pItems ) {
        return XWORK_ERROR_NO_MEMORY;
    }
    pSnapshot->tAgents.iCount = iAgentCount;
    for ( i = 0u; i < iAgentCount; ++i ) {
        iStatus = xwork__file_read_agent_snapshot(
            pFile,
            &pSnapshot->tAgents.pItems[i]
        );
        if ( iStatus != XWORK_OK ) {
            return iStatus;
        }
    }
    return XWORK_OK;
}

static xwork_status xwork__file_write_artifact_summary(
    FILE *pFile,
    const xwork_artifact_summary *pSummary
)
{
    xwork_status iStatus;

    if ( !pFile || !pSummary ) {
        return XWORK_ERROR_INVALID_ARGUMENT;
    }
    iStatus = xwork__file_write_string(pFile, pSummary->sArtifactId);
    if ( iStatus != XWORK_OK ) return iStatus;
    iStatus = xwork__file_write_size(pFile, (size_t)pSummary->eKind);
    if ( iStatus != XWORK_OK ) return iStatus;
    iStatus = xwork__file_write_size(pFile, (size_t)pSummary->eOutputClass);
    if ( iStatus != XWORK_OK ) return iStatus;
    iStatus = xwork__file_write_string(pFile, pSummary->sOutputRole);
    if ( iStatus != XWORK_OK ) return iStatus;
    iStatus = xwork__file_write_size(pFile, (size_t)pSummary->eReportClass);
    if ( iStatus != XWORK_OK ) return iStatus;
    iStatus = xwork__file_write_string(pFile, pSummary->sReportSubjectRef);
    if ( iStatus != XWORK_OK ) return iStatus;
    iStatus = xwork__file_write_string(pFile, pSummary->sName);
    if ( iStatus != XWORK_OK ) return iStatus;
    iStatus = xwork__file_write_string(pFile, pSummary->sMimeType);
    if ( iStatus != XWORK_OK ) return iStatus;
    iStatus = xwork__file_write_string(pFile, pSummary->sStorageRef);
    if ( iStatus != XWORK_OK ) return iStatus;
    iStatus = xwork__file_write_string(pFile, pSummary->sSummary);
    if ( iStatus != XWORK_OK ) return iStatus;
    iStatus = xwork__file_write_bool(pFile, pSummary->bHasContentStats);
    if ( iStatus != XWORK_OK ) return iStatus;
    iStatus = xwork__file_write_size(pFile, pSummary->iContentByteCount);
    if ( iStatus != XWORK_OK ) return iStatus;
    iStatus = xwork__file_write_size(pFile, pSummary->iContentLineCount);
    if ( iStatus != XWORK_OK ) return iStatus;
    iStatus = xwork__file_write_bool(pFile, pSummary->bHasPatchStats);
    if ( iStatus != XWORK_OK ) return iStatus;
    iStatus = xwork__file_write_size(pFile, pSummary->iPatchFileCount);
    if ( iStatus != XWORK_OK ) return iStatus;
    iStatus = xwork__file_write_size(pFile, pSummary->iPatchHunkCount);
    if ( iStatus != XWORK_OK ) return iStatus;
    iStatus = xwork__file_write_size(pFile, pSummary->iPatchAddedLineCount);
    if ( iStatus != XWORK_OK ) return iStatus;
    iStatus = xwork__file_write_size(pFile, pSummary->iPatchDeletedLineCount);
    if ( iStatus != XWORK_OK ) return iStatus;
    iStatus = xwork__file_write_string(pFile, pSummary->sPatchApplyResultJson);
    if ( iStatus != XWORK_OK ) return iStatus;
    iStatus = xwork__file_write_string(pFile, pSummary->sPatchFileSummaryJson);
    if ( iStatus != XWORK_OK ) return iStatus;
    iStatus = xwork__file_write_bool(pFile, pSummary->bHasCommandIoStats);
    if ( iStatus != XWORK_OK ) return iStatus;
    iStatus = xwork__file_write_size(pFile, pSummary->iStdoutByteCount);
    if ( iStatus != XWORK_OK ) return iStatus;
    iStatus = xwork__file_write_size(pFile, pSummary->iStderrByteCount);
    if ( iStatus != XWORK_OK ) return iStatus;
    iStatus = xwork__file_write_bool(pFile, pSummary->bStdoutTruncated);
    if ( iStatus != XWORK_OK ) return iStatus;
    iStatus = xwork__file_write_bool(pFile, pSummary->bStderrTruncated);
    if ( iStatus != XWORK_OK ) return iStatus;
    iStatus = xwork__file_write_bool(pFile, pSummary->bHasExitCode);
    if ( iStatus != XWORK_OK ) return iStatus;
    iStatus = xwork__file_write_i64(pFile, (int64_t)pSummary->iExitCode);
    if ( iStatus != XWORK_OK ) return iStatus;
    return xwork__file_write_size(pFile, pSummary->iSequence);
}

static xwork_status xwork__file_read_artifact_summary(
    FILE *pFile,
    xwork_artifact_summary *pSummary
)
{
    size_t iValue;
    int64_t iExitCode;
    xwork_status iStatus;

    if ( !pFile || !pSummary ) {
        return XWORK_ERROR_INVALID_ARGUMENT;
    }
    xwork_artifact_summary_init(pSummary);
    iStatus = xwork__file_read_string(pFile, &pSummary->sArtifactId);
    if ( iStatus != XWORK_OK ) return iStatus;
    iStatus = xwork__file_read_size(pFile, &iValue);
    if ( iStatus != XWORK_OK ) return iStatus;
    pSummary->eKind = (xwork_artifact_kind)iValue;
    iStatus = xwork__file_read_size(pFile, &iValue);
    if ( iStatus != XWORK_OK ) return iStatus;
    pSummary->eOutputClass = (xwork_artifact_output_class)iValue;
    iStatus = xwork__file_read_string(pFile, &pSummary->sOutputRole);
    if ( iStatus != XWORK_OK ) return iStatus;
    iStatus = xwork__file_read_size(pFile, &iValue);
    if ( iStatus != XWORK_OK ) return iStatus;
    pSummary->eReportClass = (xwork_artifact_report_class)iValue;
    iStatus = xwork__file_read_string(pFile, &pSummary->sReportSubjectRef);
    if ( iStatus != XWORK_OK ) return iStatus;
    iStatus = xwork__file_read_string(pFile, &pSummary->sName);
    if ( iStatus != XWORK_OK ) return iStatus;
    iStatus = xwork__file_read_string(pFile, &pSummary->sMimeType);
    if ( iStatus != XWORK_OK ) return iStatus;
    iStatus = xwork__file_read_string(pFile, &pSummary->sStorageRef);
    if ( iStatus != XWORK_OK ) return iStatus;
    iStatus = xwork__file_read_string(pFile, &pSummary->sSummary);
    if ( iStatus != XWORK_OK ) return iStatus;
    iStatus = xwork__file_read_bool(pFile, &pSummary->bHasContentStats);
    if ( iStatus != XWORK_OK ) return iStatus;
    iStatus = xwork__file_read_size(pFile, &pSummary->iContentByteCount);
    if ( iStatus != XWORK_OK ) return iStatus;
    iStatus = xwork__file_read_size(pFile, &pSummary->iContentLineCount);
    if ( iStatus != XWORK_OK ) return iStatus;
    iStatus = xwork__file_read_bool(pFile, &pSummary->bHasPatchStats);
    if ( iStatus != XWORK_OK ) return iStatus;
    iStatus = xwork__file_read_size(pFile, &pSummary->iPatchFileCount);
    if ( iStatus != XWORK_OK ) return iStatus;
    iStatus = xwork__file_read_size(pFile, &pSummary->iPatchHunkCount);
    if ( iStatus != XWORK_OK ) return iStatus;
    iStatus = xwork__file_read_size(pFile, &pSummary->iPatchAddedLineCount);
    if ( iStatus != XWORK_OK ) return iStatus;
    iStatus = xwork__file_read_size(pFile, &pSummary->iPatchDeletedLineCount);
    if ( iStatus != XWORK_OK ) return iStatus;
    iStatus = xwork__file_read_string(pFile, &pSummary->sPatchApplyResultJson);
    if ( iStatus != XWORK_OK ) return iStatus;
    iStatus = xwork__file_read_string(pFile, &pSummary->sPatchFileSummaryJson);
    if ( iStatus != XWORK_OK ) return iStatus;
    iStatus = xwork__file_read_bool(pFile, &pSummary->bHasCommandIoStats);
    if ( iStatus != XWORK_OK ) return iStatus;
    iStatus = xwork__file_read_size(pFile, &pSummary->iStdoutByteCount);
    if ( iStatus != XWORK_OK ) return iStatus;
    iStatus = xwork__file_read_size(pFile, &pSummary->iStderrByteCount);
    if ( iStatus != XWORK_OK ) return iStatus;
    iStatus = xwork__file_read_bool(pFile, &pSummary->bStdoutTruncated);
    if ( iStatus != XWORK_OK ) return iStatus;
    iStatus = xwork__file_read_bool(pFile, &pSummary->bStderrTruncated);
    if ( iStatus != XWORK_OK ) return iStatus;
    iStatus = xwork__file_read_bool(pFile, &pSummary->bHasExitCode);
    if ( iStatus != XWORK_OK ) return iStatus;
    iStatus = xwork__file_read_i64(pFile, &iExitCode);
    if ( iStatus != XWORK_OK ) return iStatus;
    pSummary->iExitCode = (int)iExitCode;
    return xwork__file_read_size(pFile, &pSummary->iSequence);
}

static xwork_status xwork__file_write_worker_snapshot(
    FILE *pFile,
    const xwork_worker_snapshot *pWorker
)
{
    xwork_status iStatus;

    if ( !pFile || !pWorker ) {
        return XWORK_ERROR_INVALID_ARGUMENT;
    }
    iStatus = xwork__file_write_string(pFile, pWorker->sWorkerId);
    if ( iStatus != XWORK_OK ) return iStatus;
    iStatus = xwork__file_write_string(pFile, pWorker->sDisplayName);
    if ( iStatus != XWORK_OK ) return iStatus;
    iStatus = xwork__file_write_string(pFile, pWorker->sEndpoint);
    if ( iStatus != XWORK_OK ) return iStatus;
    iStatus = xwork__file_write_size(pFile, pWorker->iProtocolVersion);
    if ( iStatus != XWORK_OK ) return iStatus;
    iStatus = xwork__file_write_string_array(
        pFile,
        pWorker->psCapabilities,
        pWorker->iCapabilityCount
    );
    if ( iStatus != XWORK_OK ) return iStatus;
    iStatus = xwork__file_write_string_array(
        pFile,
        pWorker->psLabels,
        pWorker->iLabelCount
    );
    if ( iStatus != XWORK_OK ) return iStatus;
    iStatus = xwork__file_write_size(pFile, pWorker->iLeaseTimeoutMs);
    if ( iStatus != XWORK_OK ) return iStatus;
    iStatus = xwork__file_write_size(pFile, pWorker->iLastHeartbeatMs);
    if ( iStatus != XWORK_OK ) return iStatus;
    iStatus = xwork__file_write_size(pFile, pWorker->iLeaseExpiresMs);
    if ( iStatus != XWORK_OK ) return iStatus;
    iStatus = xwork__file_write_size(pFile, pWorker->iClaimedCount);
    if ( iStatus != XWORK_OK ) return iStatus;
    iStatus = xwork__file_write_size(pFile, pWorker->iCompletedCount);
    if ( iStatus != XWORK_OK ) return iStatus;
    iStatus = xwork__file_write_size(pFile, pWorker->iFailedCount);
    if ( iStatus != XWORK_OK ) return iStatus;
    return xwork__file_write_size(pFile, (size_t)pWorker->eState);
}

static xwork_status xwork__file_read_worker_snapshot(
    FILE *pFile,
    uint64_t iFormatVersion,
    xwork_worker_snapshot *pWorker
)
{
    size_t iValue;
    xwork_status iStatus;

    if ( !pFile || !pWorker ) {
        return XWORK_ERROR_INVALID_ARGUMENT;
    }
    xwork_worker_snapshot_init(pWorker);
    iStatus = xwork__file_read_string(pFile, &pWorker->sWorkerId);
    if ( iStatus != XWORK_OK ) return iStatus;
    iStatus = xwork__file_read_string(pFile, &pWorker->sDisplayName);
    if ( iStatus != XWORK_OK ) return iStatus;
    iStatus = xwork__file_read_string(pFile, &pWorker->sEndpoint);
    if ( iStatus != XWORK_OK ) return iStatus;
    if ( iFormatVersion >= 8u ) {
        iStatus = xwork__file_read_size(pFile, &pWorker->iProtocolVersion);
        if ( iStatus != XWORK_OK ) return iStatus;
    } else {
        pWorker->iProtocolVersion = XWORK_REMOTE_PROTOCOL_VERSION_CURRENT;
    }
    iStatus = xwork__file_read_string_array(
        pFile,
        &pWorker->psCapabilities,
        &pWorker->iCapabilityCount
    );
    if ( iStatus != XWORK_OK ) return iStatus;
    iStatus = xwork__file_read_string_array(
        pFile,
        &pWorker->psLabels,
        &pWorker->iLabelCount
    );
    if ( iStatus != XWORK_OK ) return iStatus;
    iStatus = xwork__file_read_size(pFile, &pWorker->iLeaseTimeoutMs);
    if ( iStatus != XWORK_OK ) return iStatus;
    iStatus = xwork__file_read_size(pFile, &pWorker->iLastHeartbeatMs);
    if ( iStatus != XWORK_OK ) return iStatus;
    iStatus = xwork__file_read_size(pFile, &pWorker->iLeaseExpiresMs);
    if ( iStatus != XWORK_OK ) return iStatus;
    iStatus = xwork__file_read_size(pFile, &pWorker->iClaimedCount);
    if ( iStatus != XWORK_OK ) return iStatus;
    iStatus = xwork__file_read_size(pFile, &pWorker->iCompletedCount);
    if ( iStatus != XWORK_OK ) return iStatus;
    iStatus = xwork__file_read_size(pFile, &pWorker->iFailedCount);
    if ( iStatus != XWORK_OK ) return iStatus;
    iStatus = xwork__file_read_size(pFile, &iValue);
    if ( iStatus != XWORK_OK ) return iStatus;
    pWorker->eState = (xwork_worker_state)iValue;
    return XWORK_OK;
}

static xwork_status xwork__file_write_remote_task_snapshot(
    FILE *pFile,
    const xwork_remote_task_snapshot *pTask
)
{
    xwork_status iStatus;

    if ( !pFile || !pTask ) {
        return XWORK_ERROR_INVALID_ARGUMENT;
    }
    iStatus = xwork__file_write_string(pFile, pTask->sTaskId);
    if ( iStatus != XWORK_OK ) return iStatus;
    iStatus = xwork__file_write_string(pFile, pTask->sAssignmentId);
    if ( iStatus != XWORK_OK ) return iStatus;
    iStatus = xwork__file_write_string(pFile, pTask->sWorkerId);
    if ( iStatus != XWORK_OK ) return iStatus;
    iStatus = xwork__file_write_size(pFile, (size_t)pTask->eKind);
    if ( iStatus != XWORK_OK ) return iStatus;
    iStatus = xwork__file_write_size(pFile, (size_t)pTask->eState);
    if ( iStatus != XWORK_OK ) return iStatus;
    iStatus = xwork__file_write_size(pFile, (size_t)pTask->eHostService);
    if ( iStatus != XWORK_OK ) return iStatus;
    iStatus = xwork__file_write_string(pFile, pTask->sOperationId);
    if ( iStatus != XWORK_OK ) return iStatus;
    iStatus = xwork__file_write_string(pFile, pTask->sRequestJson);
    if ( iStatus != XWORK_OK ) return iStatus;
    iStatus = xwork__file_write_string(pFile, pTask->sRequiredCapability);
    if ( iStatus != XWORK_OK ) return iStatus;
    iStatus = xwork__file_write_size(pFile, pTask->iAttemptCount);
    if ( iStatus != XWORK_OK ) return iStatus;
    iStatus = xwork__file_write_size(pFile, pTask->iAssignedAtMs);
    if ( iStatus != XWORK_OK ) return iStatus;
    iStatus = xwork__file_write_size(pFile, pTask->iCompletedAtMs);
    if ( iStatus != XWORK_OK ) return iStatus;
    iStatus = xwork__file_write_size(pFile, pTask->iTimeoutMs);
    if ( iStatus != XWORK_OK ) return iStatus;
    iStatus = xwork__file_write_i64(pFile, (int64_t)pTask->iStatus);
    if ( iStatus != XWORK_OK ) return iStatus;
    iStatus = xwork__file_write_bool(pFile, pTask->bRetryable);
    if ( iStatus != XWORK_OK ) return iStatus;
    iStatus = xwork__file_write_string(pFile, pTask->sOutputText);
    if ( iStatus != XWORK_OK ) return iStatus;
    iStatus = xwork__file_write_string(pFile, pTask->sVisibleSummary);
    if ( iStatus != XWORK_OK ) return iStatus;
    iStatus = xwork__file_write_string(pFile, pTask->sErrorKind);
    if ( iStatus != XWORK_OK ) return iStatus;
    iStatus = xwork__file_write_string(pFile, pTask->sErrorMessage);
    if ( iStatus != XWORK_OK ) return iStatus;
    iStatus = xwork__file_write_size(pFile, pTask->iProtocolVersion);
    if ( iStatus != XWORK_OK ) return iStatus;
    iStatus = xwork__file_write_size(pFile, pTask->iArtifactCount);
    if ( iStatus != XWORK_OK ) return iStatus;
    {
        size_t i;
        for ( i = 0u; i < pTask->iArtifactCount; ++i ) {
            iStatus = xwork__file_write_artifact_summary(
                pFile,
                &pTask->pArtifacts[i]
            );
            if ( iStatus != XWORK_OK ) return iStatus;
        }
    }
    iStatus = xwork__file_write_size(pFile, pTask->iOutputChunkCount);
    if ( iStatus != XWORK_OK ) return iStatus;
    {
        size_t i;
        for ( i = 0u; i < pTask->iOutputChunkCount; ++i ) {
            const xwork_remote_output_chunk_summary *pChunk =
                &pTask->pOutputChunks[i];
            iStatus = xwork__file_write_size(pFile, (size_t)pChunk->eStream);
            if ( iStatus != XWORK_OK ) return iStatus;
            iStatus = xwork__file_write_size(pFile, pChunk->iChunkIndex);
            if ( iStatus != XWORK_OK ) return iStatus;
            iStatus = xwork__file_write_size(pFile, pChunk->iOffsetBytes);
            if ( iStatus != XWORK_OK ) return iStatus;
            iStatus = xwork__file_write_size(pFile, pChunk->iByteCount);
            if ( iStatus != XWORK_OK ) return iStatus;
            iStatus = xwork__file_write_bool(pFile, pChunk->bFinalChunk);
            if ( iStatus != XWORK_OK ) return iStatus;
            iStatus = xwork__file_write_string(pFile, pChunk->sContentHash);
            if ( iStatus != XWORK_OK ) return iStatus;
            iStatus = xwork__file_write_string(pFile, pChunk->sText);
            if ( iStatus != XWORK_OK ) return iStatus;
        }
    }
    return XWORK_OK;
}

static xwork_status xwork__file_read_remote_task_snapshot(
    FILE *pFile,
    uint64_t iFormatVersion,
    xwork_remote_task_snapshot *pTask
)
{
    size_t iValue;
    size_t iArtifactCount;
    size_t i;
    int64_t iStatusValue;
    xwork_status iStatus;

    if ( !pFile || !pTask ) {
        return XWORK_ERROR_INVALID_ARGUMENT;
    }
    xwork_remote_task_snapshot_init(pTask);
    iStatus = xwork__file_read_string(pFile, &pTask->sTaskId);
    if ( iStatus != XWORK_OK ) return iStatus;
    iStatus = xwork__file_read_string(pFile, &pTask->sAssignmentId);
    if ( iStatus != XWORK_OK ) return iStatus;
    iStatus = xwork__file_read_string(pFile, &pTask->sWorkerId);
    if ( iStatus != XWORK_OK ) return iStatus;
    iStatus = xwork__file_read_size(pFile, &iValue);
    if ( iStatus != XWORK_OK ) return iStatus;
    pTask->eKind = (xwork_remote_task_kind)iValue;
    iStatus = xwork__file_read_size(pFile, &iValue);
    if ( iStatus != XWORK_OK ) return iStatus;
    pTask->eState = (xwork_remote_task_state)iValue;
    iStatus = xwork__file_read_size(pFile, &iValue);
    if ( iStatus != XWORK_OK ) return iStatus;
    pTask->eHostService = (xwork_host_service_kind)iValue;
    iStatus = xwork__file_read_string(pFile, &pTask->sOperationId);
    if ( iStatus != XWORK_OK ) return iStatus;
    iStatus = xwork__file_read_string(pFile, &pTask->sRequestJson);
    if ( iStatus != XWORK_OK ) return iStatus;
    iStatus = xwork__file_read_string(pFile, &pTask->sRequiredCapability);
    if ( iStatus != XWORK_OK ) return iStatus;
    iStatus = xwork__file_read_size(pFile, &pTask->iAttemptCount);
    if ( iStatus != XWORK_OK ) return iStatus;
    iStatus = xwork__file_read_size(pFile, &pTask->iAssignedAtMs);
    if ( iStatus != XWORK_OK ) return iStatus;
    iStatus = xwork__file_read_size(pFile, &pTask->iCompletedAtMs);
    if ( iStatus != XWORK_OK ) return iStatus;
    iStatus = xwork__file_read_size(pFile, &pTask->iTimeoutMs);
    if ( iStatus != XWORK_OK ) return iStatus;
    iStatus = xwork__file_read_i64(pFile, &iStatusValue);
    if ( iStatus != XWORK_OK ) return iStatus;
    pTask->iStatus = (xwork_status)iStatusValue;
    iStatus = xwork__file_read_bool(pFile, &pTask->bRetryable);
    if ( iStatus != XWORK_OK ) return iStatus;
    iStatus = xwork__file_read_string(pFile, &pTask->sOutputText);
    if ( iStatus != XWORK_OK ) return iStatus;
    iStatus = xwork__file_read_string(pFile, &pTask->sVisibleSummary);
    if ( iStatus != XWORK_OK ) return iStatus;
    if ( iFormatVersion >= 8u ) {
        iStatus = xwork__file_read_string(pFile, &pTask->sErrorKind);
        if ( iStatus != XWORK_OK ) return iStatus;
        iStatus = xwork__file_read_string(pFile, &pTask->sErrorMessage);
        if ( iStatus != XWORK_OK ) return iStatus;
        iStatus = xwork__file_read_size(pFile, &pTask->iProtocolVersion);
        if ( iStatus != XWORK_OK ) return iStatus;
    } else {
        pTask->iProtocolVersion = XWORK_REMOTE_PROTOCOL_VERSION_CURRENT;
    }
    if ( iFormatVersion < 7u ) {
        return XWORK_OK;
    }
    iStatus = xwork__file_read_size(pFile, &iArtifactCount);
    if ( iStatus != XWORK_OK ) return iStatus;
    if ( iArtifactCount > 0u ) {
        pTask->pArtifacts = (xwork_artifact_summary *)calloc(
            iArtifactCount,
            sizeof(*pTask->pArtifacts)
        );
        if ( !pTask->pArtifacts ) {
            return XWORK_ERROR_NO_MEMORY;
        }
        pTask->iArtifactCount = iArtifactCount;
        for ( i = 0u; i < iArtifactCount; ++i ) {
            iStatus = xwork__file_read_artifact_summary(
                pFile,
                &pTask->pArtifacts[i]
            );
            if ( iStatus != XWORK_OK ) return iStatus;
        }
    }
    if ( iFormatVersion < 11u ) {
        return XWORK_OK;
    }
    {
        size_t iChunkCount;
        iStatus = xwork__file_read_size(pFile, &iChunkCount);
        if ( iStatus != XWORK_OK ) return iStatus;
        if ( iChunkCount == 0u ) {
            return XWORK_OK;
        }
        pTask->pOutputChunks = (xwork_remote_output_chunk_summary *)calloc(
            iChunkCount,
            sizeof(*pTask->pOutputChunks)
        );
        if ( !pTask->pOutputChunks ) {
            return XWORK_ERROR_NO_MEMORY;
        }
        pTask->iOutputChunkCount = iChunkCount;
        for ( i = 0u; i < iChunkCount; ++i ) {
            xwork_remote_output_chunk_summary *pChunk = &pTask->pOutputChunks[i];
            xwork_remote_output_chunk_summary_init(pChunk);
            iStatus = xwork__file_read_size(pFile, &iValue);
            if ( iStatus != XWORK_OK ) return iStatus;
            pChunk->eStream = (xwork_remote_output_stream)iValue;
            iStatus = xwork__file_read_size(pFile, &pChunk->iChunkIndex);
            if ( iStatus != XWORK_OK ) return iStatus;
            iStatus = xwork__file_read_size(pFile, &pChunk->iOffsetBytes);
            if ( iStatus != XWORK_OK ) return iStatus;
            iStatus = xwork__file_read_size(pFile, &pChunk->iByteCount);
            if ( iStatus != XWORK_OK ) return iStatus;
            iStatus = xwork__file_read_bool(pFile, &pChunk->bFinalChunk);
            if ( iStatus != XWORK_OK ) return iStatus;
            iStatus = xwork__file_read_string(pFile, &pChunk->sContentHash);
            if ( iStatus != XWORK_OK ) return iStatus;
            iStatus = xwork__file_read_string(pFile, &pChunk->sText);
            if ( iStatus != XWORK_OK ) return iStatus;
        }
    }
    return XWORK_OK;
}

static xwork_status xwork__file_write_remote_blob_chunk_summary(
    FILE *pFile,
    const xwork_remote_blob_chunk_summary *pChunk
)
{
    xwork_status iStatus;

    if ( !pFile || !pChunk ) {
        return XWORK_ERROR_INVALID_ARGUMENT;
    }
    if ( pChunk->iChunkSize > 0u && !pChunk->pChunkData ) {
        return XWORK_ERROR_INVALID_ARGUMENT;
    }
    iStatus = xwork__file_write_string(pFile, pChunk->sTaskId);
    if ( iStatus != XWORK_OK ) return iStatus;
    iStatus = xwork__file_write_string(pFile, pChunk->sAssignmentId);
    if ( iStatus != XWORK_OK ) return iStatus;
    iStatus = xwork__file_write_string(pFile, pChunk->sWorkerId);
    if ( iStatus != XWORK_OK ) return iStatus;
    iStatus = xwork__file_write_string(pFile, pChunk->sArtifactId);
    if ( iStatus != XWORK_OK ) return iStatus;
    iStatus = xwork__file_write_string(pFile, pChunk->sBlobRef);
    if ( iStatus != XWORK_OK ) return iStatus;
    iStatus = xwork__file_write_string(pFile, pChunk->sContentHash);
    if ( iStatus != XWORK_OK ) return iStatus;
    iStatus = xwork__file_write_size(pFile, pChunk->iChunkIndex);
    if ( iStatus != XWORK_OK ) return iStatus;
    iStatus = xwork__file_write_size(pFile, pChunk->iChunkCount);
    if ( iStatus != XWORK_OK ) return iStatus;
    iStatus = xwork__file_write_size(pFile, pChunk->iOffsetBytes);
    if ( iStatus != XWORK_OK ) return iStatus;
    iStatus = xwork__file_write_size(pFile, pChunk->iChunkSize);
    if ( iStatus != XWORK_OK ) return iStatus;
    if ( pChunk->iChunkSize > 0u ) {
        iStatus = xwork__file_write_bytes(
            pFile,
            pChunk->pChunkData,
            pChunk->iChunkSize
        );
        if ( iStatus != XWORK_OK ) return iStatus;
    }
    return xwork__file_write_bool(pFile, pChunk->bFinalChunk);
}

static xwork_status xwork__file_read_remote_blob_chunk_summary(
    FILE *pFile,
    xwork_remote_blob_chunk_summary *pChunk
)
{
    unsigned char *pData = NULL;
    xwork_status iStatus;

    if ( !pFile || !pChunk ) {
        return XWORK_ERROR_INVALID_ARGUMENT;
    }
    xwork_remote_blob_chunk_summary_init(pChunk);
    iStatus = xwork__file_read_string(pFile, &pChunk->sTaskId);
    if ( iStatus != XWORK_OK ) return iStatus;
    iStatus = xwork__file_read_string(pFile, &pChunk->sAssignmentId);
    if ( iStatus != XWORK_OK ) return iStatus;
    iStatus = xwork__file_read_string(pFile, &pChunk->sWorkerId);
    if ( iStatus != XWORK_OK ) return iStatus;
    iStatus = xwork__file_read_string(pFile, &pChunk->sArtifactId);
    if ( iStatus != XWORK_OK ) return iStatus;
    iStatus = xwork__file_read_string(pFile, &pChunk->sBlobRef);
    if ( iStatus != XWORK_OK ) return iStatus;
    iStatus = xwork__file_read_string(pFile, &pChunk->sContentHash);
    if ( iStatus != XWORK_OK ) return iStatus;
    iStatus = xwork__file_read_size(pFile, &pChunk->iChunkIndex);
    if ( iStatus != XWORK_OK ) return iStatus;
    iStatus = xwork__file_read_size(pFile, &pChunk->iChunkCount);
    if ( iStatus != XWORK_OK ) return iStatus;
    iStatus = xwork__file_read_size(pFile, &pChunk->iOffsetBytes);
    if ( iStatus != XWORK_OK ) return iStatus;
    iStatus = xwork__file_read_size(pFile, &pChunk->iChunkSize);
    if ( iStatus != XWORK_OK ) return iStatus;
    if ( pChunk->iChunkSize > 0u ) {
        pData = (unsigned char *)malloc(pChunk->iChunkSize);
        if ( !pData ) {
            return XWORK_ERROR_NO_MEMORY;
        }
        iStatus = xwork__file_read_bytes(pFile, pData, pChunk->iChunkSize);
        if ( iStatus != XWORK_OK ) {
            free(pData);
            return iStatus;
        }
        pChunk->pChunkData = pData;
    }
    return xwork__file_read_bool(pFile, &pChunk->bFinalChunk);
}

static xwork_status xwork__file_write_control_plane_snapshot_fields(
    FILE *pFile,
    const xwork_control_plane_snapshot *pSnapshot
)
{
    size_t i;
    xwork_status iStatus;

    if ( !pFile || !pSnapshot ) {
        return XWORK_ERROR_INVALID_ARGUMENT;
    }
    iStatus = xwork__file_write_string(pFile, pSnapshot->sPlaneId);
    if ( iStatus != XWORK_OK ) return iStatus;
    iStatus = xwork__file_write_size(pFile, (size_t)pSnapshot->eTransport);
    if ( iStatus != XWORK_OK ) return iStatus;
    iStatus = xwork__file_write_size(pFile, pSnapshot->iProtocolVersion);
    if ( iStatus != XWORK_OK ) return iStatus;
    iStatus = xwork__file_write_size(pFile, pSnapshot->iDefaultLeaseTimeoutMs);
    if ( iStatus != XWORK_OK ) return iStatus;
    iStatus = xwork__file_write_size(pFile, pSnapshot->iNowMs);
    if ( iStatus != XWORK_OK ) return iStatus;
    iStatus = xwork__file_write_bool(pFile, pSnapshot->bStarted);
    if ( iStatus != XWORK_OK ) return iStatus;
    iStatus = xwork__file_write_size(pFile, pSnapshot->iNextAssignmentSequence);
    if ( iStatus != XWORK_OK ) return iStatus;
    iStatus = xwork__file_write_size(pFile, pSnapshot->tWorkers.iCount);
    if ( iStatus != XWORK_OK ) return iStatus;
    for ( i = 0u; i < pSnapshot->tWorkers.iCount; ++i ) {
        iStatus = xwork__file_write_worker_snapshot(
            pFile,
            &pSnapshot->tWorkers.pItems[i]
        );
        if ( iStatus != XWORK_OK ) return iStatus;
    }
    iStatus = xwork__file_write_size(pFile, pSnapshot->tTasks.iCount);
    if ( iStatus != XWORK_OK ) return iStatus;
    for ( i = 0u; i < pSnapshot->tTasks.iCount; ++i ) {
        iStatus = xwork__file_write_remote_task_snapshot(
            pFile,
            &pSnapshot->tTasks.pItems[i]
        );
        if ( iStatus != XWORK_OK ) return iStatus;
    }
    iStatus = xwork__file_write_size(pFile, pSnapshot->tBlobChunks.iCount);
    if ( iStatus != XWORK_OK ) return iStatus;
    for ( i = 0u; i < pSnapshot->tBlobChunks.iCount; ++i ) {
        iStatus = xwork__file_write_remote_blob_chunk_summary(
            pFile,
            &pSnapshot->tBlobChunks.pItems[i]
        );
        if ( iStatus != XWORK_OK ) return iStatus;
    }
    return XWORK_OK;
}

static xwork_status xwork__file_read_control_plane_snapshot_fields(
    FILE *pFile,
    uint64_t iFormatVersion,
    xwork_control_plane_snapshot *pSnapshot
)
{
    size_t i;
    size_t iWorkerCount;
    size_t iTaskCount;
    size_t iBlobChunkCount;
    size_t iValue;
    xwork_status iStatus;

    if ( !pFile || !pSnapshot ) {
        return XWORK_ERROR_INVALID_ARGUMENT;
    }
    iStatus = xwork__file_read_string(pFile, &pSnapshot->sPlaneId);
    if ( iStatus != XWORK_OK ) return iStatus;
    iStatus = xwork__file_read_size(pFile, &iValue);
    if ( iStatus != XWORK_OK ) return iStatus;
    pSnapshot->eTransport = (xwork_remote_transport_kind)iValue;
    if ( iFormatVersion >= 8u ) {
        iStatus = xwork__file_read_size(pFile, &pSnapshot->iProtocolVersion);
        if ( iStatus != XWORK_OK ) return iStatus;
    } else {
        pSnapshot->iProtocolVersion = XWORK_REMOTE_PROTOCOL_VERSION_CURRENT;
    }
    iStatus = xwork__file_read_size(pFile, &pSnapshot->iDefaultLeaseTimeoutMs);
    if ( iStatus != XWORK_OK ) return iStatus;
    iStatus = xwork__file_read_size(pFile, &pSnapshot->iNowMs);
    if ( iStatus != XWORK_OK ) return iStatus;
    iStatus = xwork__file_read_bool(pFile, &pSnapshot->bStarted);
    if ( iStatus != XWORK_OK ) return iStatus;
    iStatus = xwork__file_read_size(pFile, &pSnapshot->iNextAssignmentSequence);
    if ( iStatus != XWORK_OK ) return iStatus;

    iStatus = xwork__file_read_size(pFile, &iWorkerCount);
    if ( iStatus != XWORK_OK ) return iStatus;
    if ( iWorkerCount > 0u ) {
        pSnapshot->tWorkers.pItems = (xwork_worker_snapshot *)calloc(
            iWorkerCount,
            sizeof(*pSnapshot->tWorkers.pItems)
        );
        if ( !pSnapshot->tWorkers.pItems ) {
            return XWORK_ERROR_NO_MEMORY;
        }
        pSnapshot->tWorkers.iCount = iWorkerCount;
        for ( i = 0u; i < iWorkerCount; ++i ) {
            iStatus = xwork__file_read_worker_snapshot(
                pFile,
                iFormatVersion,
                &pSnapshot->tWorkers.pItems[i]
            );
            if ( iStatus != XWORK_OK ) return iStatus;
        }
    }

    iStatus = xwork__file_read_size(pFile, &iTaskCount);
    if ( iStatus != XWORK_OK ) return iStatus;
    if ( iTaskCount > 0u ) {
        pSnapshot->tTasks.pItems = (xwork_remote_task_snapshot *)calloc(
            iTaskCount,
            sizeof(*pSnapshot->tTasks.pItems)
        );
        if ( !pSnapshot->tTasks.pItems ) {
            return XWORK_ERROR_NO_MEMORY;
        }
        pSnapshot->tTasks.iCount = iTaskCount;
        for ( i = 0u; i < iTaskCount; ++i ) {
            iStatus = xwork__file_read_remote_task_snapshot(
                pFile,
                iFormatVersion,
                &pSnapshot->tTasks.pItems[i]
            );
            if ( iStatus != XWORK_OK ) return iStatus;
        }
    }

    if ( iFormatVersion < 12u ) {
        return XWORK_OK;
    }
    iStatus = xwork__file_read_size(pFile, &iBlobChunkCount);
    if ( iStatus != XWORK_OK ) return iStatus;
    if ( iBlobChunkCount > 0u ) {
        pSnapshot->tBlobChunks.pItems =
            (xwork_remote_blob_chunk_summary *)calloc(
                iBlobChunkCount,
                sizeof(*pSnapshot->tBlobChunks.pItems)
            );
        if ( !pSnapshot->tBlobChunks.pItems ) {
            return XWORK_ERROR_NO_MEMORY;
        }
        pSnapshot->tBlobChunks.iCount = iBlobChunkCount;
        for ( i = 0u; i < iBlobChunkCount; ++i ) {
            iStatus = xwork__file_read_remote_blob_chunk_summary(
                pFile,
                &pSnapshot->tBlobChunks.pItems[i]
            );
            if ( iStatus != XWORK_OK ) return iStatus;
        }
    }

    return XWORK_OK;
}

static char *xwork__build_atomic_temp_path(const char *sPath)
{
    if ( !sPath || !sPath[0] ) {
        return NULL;
    }
    return xwork__dup_printf("%s.tmp", sPath);
}

static xwork_status xwork__replace_file_atomic(
    const char *sTempPath,
    const char *sFinalPath
)
{
    if ( !sTempPath || !sFinalPath ) {
        return XWORK_ERROR_INVALID_ARGUMENT;
    }

#ifdef _WIN32
    if ( !MoveFileExA(
            sTempPath,
            sFinalPath,
            MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH
         ) ) {
        return XWORK_ERROR_EXTERNAL_FAILURE;
    }
#else
    if ( rename(sTempPath, sFinalPath) != 0 ) {
        return XWORK_ERROR_EXTERNAL_FAILURE;
    }
#endif

    return XWORK_OK;
}

static xwork_status xwork__write_snapshot_file(
    const char *sPath,
    const xwork_run_snapshot *pSnapshot
)
{
    FILE *pFile;
    char *sTempPath;
    xwork_status iStatus;

    if ( !sPath || !pSnapshot ) {
        return XWORK_ERROR_INVALID_ARGUMENT;
    }

    sTempPath = xwork__build_atomic_temp_path(sPath);
    if ( !sTempPath ) {
        return XWORK_ERROR_NO_MEMORY;
    }

    pFile = fopen(sTempPath, "wb");
    if ( !pFile ) {
        free(sTempPath);
        return XWORK_ERROR_EXTERNAL_FAILURE;
    }

    iStatus = xwork__write_persistence_header(
        pFile,
        xwork__snapshot_magic,
        sizeof(xwork__snapshot_magic)
    );
    if ( iStatus == XWORK_OK ) {
        iStatus = xwork__file_write_snapshot_fields(pFile, pSnapshot);
    }

    if ( fclose(pFile) != 0 && iStatus == XWORK_OK ) {
        iStatus = XWORK_ERROR_EXTERNAL_FAILURE;
    }
    if ( iStatus == XWORK_OK ) {
        iStatus = xwork__replace_file_atomic(sTempPath, sPath);
    }
    if ( iStatus != XWORK_OK ) {
        (void)remove(sTempPath);
    }
    free(sTempPath);
    return iStatus;
}

static xwork_status xwork__read_snapshot_file(
    const char *sPath,
    xwork_run_snapshot *pSnapshot
)
{
    FILE *pFile;
    uint64_t iFormatVersion = 0u;
    xwork_status iStatus;

    if ( !sPath || !pSnapshot ) {
        return XWORK_ERROR_INVALID_ARGUMENT;
    }

    pFile = fopen(sPath, "rb");
    if ( !pFile ) {
        return errno == ENOENT ? XWORK_ERROR_NOT_FOUND : XWORK_ERROR_EXTERNAL_FAILURE;
    }

    xwork_run_snapshot_reset(pSnapshot);
    iStatus = xwork__read_persistence_header(
        pFile,
        xwork__snapshot_magic,
        xwork__snapshot_magic_v0,
        sizeof(xwork__snapshot_magic),
        &iFormatVersion
    );
    if ( iStatus == XWORK_OK ) {
        iStatus = xwork__file_read_snapshot_fields(
            pFile,
            iFormatVersion,
            pSnapshot
        );
    }

    if ( fclose(pFile) != 0 && iStatus == XWORK_OK ) {
        iStatus = XWORK_ERROR_EXTERNAL_FAILURE;
    }
    if ( iStatus != XWORK_OK ) {
        xwork_run_snapshot_reset(pSnapshot);
    }
    return iStatus;
}

static xwork_status xwork__write_task_graph_snapshot_file(
    const char *sPath,
    const xwork_task_graph_snapshot *pSnapshot
)
{
    FILE *pFile;
    char *sTempPath;
    xwork_status iStatus;

    if ( !sPath || !pSnapshot ) {
        return XWORK_ERROR_INVALID_ARGUMENT;
    }

    sTempPath = xwork__build_atomic_temp_path(sPath);
    if ( !sTempPath ) {
        return XWORK_ERROR_NO_MEMORY;
    }

    pFile = fopen(sTempPath, "wb");
    if ( !pFile ) {
        free(sTempPath);
        return XWORK_ERROR_EXTERNAL_FAILURE;
    }

    iStatus = xwork__write_persistence_header(
        pFile,
        xwork__task_graph_magic,
        sizeof(xwork__task_graph_magic)
    );
    if ( iStatus == XWORK_OK ) {
        iStatus = xwork__file_write_task_graph_snapshot_fields(pFile, pSnapshot);
    }

    if ( fclose(pFile) != 0 && iStatus == XWORK_OK ) {
        iStatus = XWORK_ERROR_EXTERNAL_FAILURE;
    }
    if ( iStatus == XWORK_OK ) {
        iStatus = xwork__replace_file_atomic(sTempPath, sPath);
    }
    if ( iStatus != XWORK_OK ) {
        (void)remove(sTempPath);
    }
    free(sTempPath);
    return iStatus;
}

static xwork_status xwork__read_task_graph_snapshot_file(
    const char *sPath,
    xwork_task_graph_snapshot *pSnapshot
)
{
    FILE *pFile;
    uint64_t iFormatVersion = 0u;
    xwork_status iStatus;

    if ( !sPath || !pSnapshot ) {
        return XWORK_ERROR_INVALID_ARGUMENT;
    }

    pFile = fopen(sPath, "rb");
    if ( !pFile ) {
        return errno == ENOENT ? XWORK_ERROR_NOT_FOUND : XWORK_ERROR_EXTERNAL_FAILURE;
    }

    xwork_task_graph_snapshot_reset(pSnapshot);
    iStatus = xwork__read_persistence_header(
        pFile,
        xwork__task_graph_magic,
        xwork__snapshot_magic_v0,
        sizeof(xwork__task_graph_magic),
        &iFormatVersion
    );
    if ( iStatus == XWORK_OK ) {
        iStatus = xwork__file_read_task_graph_snapshot_fields(
            pFile,
            iFormatVersion,
            pSnapshot
        );
    }

    if ( fclose(pFile) != 0 && iStatus == XWORK_OK ) {
        iStatus = XWORK_ERROR_EXTERNAL_FAILURE;
    }
    if ( iStatus != XWORK_OK ) {
        xwork_task_graph_snapshot_reset(pSnapshot);
    }
    return iStatus;
}

static xwork_status xwork__write_agent_pool_snapshot_file(
    const char *sPath,
    const xwork_agent_pool_snapshot *pSnapshot
)
{
    FILE *pFile;
    char *sTempPath;
    xwork_status iStatus;

    if ( !sPath || !pSnapshot ) {
        return XWORK_ERROR_INVALID_ARGUMENT;
    }

    sTempPath = xwork__build_atomic_temp_path(sPath);
    if ( !sTempPath ) {
        return XWORK_ERROR_NO_MEMORY;
    }

    pFile = fopen(sTempPath, "wb");
    if ( !pFile ) {
        free(sTempPath);
        return XWORK_ERROR_EXTERNAL_FAILURE;
    }

    iStatus = xwork__write_persistence_header(
        pFile,
        xwork__agent_pool_magic,
        sizeof(xwork__agent_pool_magic)
    );
    if ( iStatus == XWORK_OK ) {
        iStatus = xwork__file_write_agent_pool_snapshot_fields(pFile, pSnapshot);
    }

    if ( fclose(pFile) != 0 && iStatus == XWORK_OK ) {
        iStatus = XWORK_ERROR_EXTERNAL_FAILURE;
    }
    if ( iStatus == XWORK_OK ) {
        iStatus = xwork__replace_file_atomic(sTempPath, sPath);
    }
    if ( iStatus != XWORK_OK ) {
        (void)remove(sTempPath);
    }
    free(sTempPath);
    return iStatus;
}

static xwork_status xwork__read_agent_pool_snapshot_file(
    const char *sPath,
    xwork_agent_pool_snapshot *pSnapshot
)
{
    FILE *pFile;
    uint64_t iFormatVersion = 0u;
    xwork_status iStatus;

    if ( !sPath || !pSnapshot ) {
        return XWORK_ERROR_INVALID_ARGUMENT;
    }

    pFile = fopen(sPath, "rb");
    if ( !pFile ) {
        return errno == ENOENT ? XWORK_ERROR_NOT_FOUND : XWORK_ERROR_EXTERNAL_FAILURE;
    }

    xwork_agent_pool_snapshot_reset(pSnapshot);
    iStatus = xwork__read_persistence_header(
        pFile,
        xwork__agent_pool_magic,
        xwork__snapshot_magic_v0,
        sizeof(xwork__agent_pool_magic),
        &iFormatVersion
    );
    if ( iStatus == XWORK_OK ) {
        iStatus = xwork__file_read_agent_pool_snapshot_fields(pFile, pSnapshot);
    }

    if ( fclose(pFile) != 0 && iStatus == XWORK_OK ) {
        iStatus = XWORK_ERROR_EXTERNAL_FAILURE;
    }
    if ( iStatus != XWORK_OK ) {
        xwork_agent_pool_snapshot_reset(pSnapshot);
    }
    return iStatus;
}

static xwork_status xwork__write_control_plane_snapshot_file(
    const char *sPath,
    const xwork_control_plane_snapshot *pSnapshot
)
{
    FILE *pFile;
    char *sTempPath;
    xwork_status iStatus;

    if ( !sPath || !pSnapshot ) {
        return XWORK_ERROR_INVALID_ARGUMENT;
    }

    sTempPath = xwork__build_atomic_temp_path(sPath);
    if ( !sTempPath ) {
        return XWORK_ERROR_NO_MEMORY;
    }

    pFile = fopen(sTempPath, "wb");
    if ( !pFile ) {
        free(sTempPath);
        return XWORK_ERROR_EXTERNAL_FAILURE;
    }

    iStatus = xwork__write_persistence_header(
        pFile,
        xwork__control_plane_magic,
        sizeof(xwork__control_plane_magic)
    );
    if ( iStatus == XWORK_OK ) {
        iStatus = xwork__file_write_control_plane_snapshot_fields(
            pFile,
            pSnapshot
        );
    }

    if ( fclose(pFile) != 0 && iStatus == XWORK_OK ) {
        iStatus = XWORK_ERROR_EXTERNAL_FAILURE;
    }
    if ( iStatus == XWORK_OK ) {
        iStatus = xwork__replace_file_atomic(sTempPath, sPath);
    }
    if ( iStatus != XWORK_OK ) {
        (void)remove(sTempPath);
    }
    free(sTempPath);
    return iStatus;
}

static xwork_status xwork__read_control_plane_snapshot_file(
    const char *sPath,
    xwork_control_plane_snapshot *pSnapshot
)
{
    FILE *pFile;
    uint64_t iFormatVersion = 0u;
    xwork_status iStatus;

    if ( !sPath || !pSnapshot ) {
        return XWORK_ERROR_INVALID_ARGUMENT;
    }

    pFile = fopen(sPath, "rb");
    if ( !pFile ) {
        return errno == ENOENT ? XWORK_ERROR_NOT_FOUND : XWORK_ERROR_EXTERNAL_FAILURE;
    }

    xwork_control_plane_snapshot_reset(pSnapshot);
    iStatus = xwork__read_persistence_header(
        pFile,
        xwork__control_plane_magic,
        xwork__snapshot_magic_v0,
        sizeof(xwork__control_plane_magic),
        &iFormatVersion
    );
    if ( iStatus == XWORK_OK ) {
        iStatus = xwork__file_read_control_plane_snapshot_fields(
            pFile,
            iFormatVersion,
            pSnapshot
        );
    }

    if ( fclose(pFile) != 0 && iStatus == XWORK_OK ) {
        iStatus = XWORK_ERROR_EXTERNAL_FAILURE;
    }
    if ( iStatus != XWORK_OK ) {
        xwork_control_plane_snapshot_reset(pSnapshot);
    }
    return iStatus;
}

static xwork_status xwork__file_write_replay_manifest(
    FILE *pFile,
    const xwork_replay_manifest *pManifest
)
{
    xwork_status iStatus;

    iStatus = xwork__file_write_string(pFile, pManifest->sManifestId);
    if ( iStatus != XWORK_OK ) return iStatus;
    iStatus = xwork__file_write_string(pFile, pManifest->sReplayId);
    if ( iStatus != XWORK_OK ) return iStatus;
    iStatus = xwork__file_write_string(pFile, pManifest->sSourceRunId);
    if ( iStatus != XWORK_OK ) return iStatus;
    iStatus = xwork__file_write_string(pFile, pManifest->sCreatedAtText);
    if ( iStatus != XWORK_OK ) return iStatus;
    iStatus = xwork__file_write_string(pFile, pManifest->sContentHashAlgorithm);
    if ( iStatus != XWORK_OK ) return iStatus;
    return xwork__file_write_size(pFile, pManifest->iEntryCount);
}

static xwork_status xwork__file_read_replay_manifest(
    FILE *pFile,
    xwork_replay_manifest *pManifest
)
{
    xwork_status iStatus;

    xwork_replay_manifest_reset(pManifest);
    iStatus = xwork__file_read_string(pFile, &pManifest->sManifestId);
    if ( iStatus != XWORK_OK ) return iStatus;
    iStatus = xwork__file_read_string(pFile, &pManifest->sReplayId);
    if ( iStatus != XWORK_OK ) return iStatus;
    iStatus = xwork__file_read_string(pFile, &pManifest->sSourceRunId);
    if ( iStatus != XWORK_OK ) return iStatus;
    iStatus = xwork__file_read_string(pFile, &pManifest->sCreatedAtText);
    if ( iStatus != XWORK_OK ) return iStatus;
    iStatus = xwork__file_read_string(pFile, &pManifest->sContentHashAlgorithm);
    if ( iStatus != XWORK_OK ) return iStatus;
    return xwork__file_read_size(pFile, &pManifest->iEntryCount);
}

static xwork_status xwork__file_write_replay_entry_summary(
    FILE *pFile,
    const xwork_replay_entry_summary *pSummary
)
{
    xwork_status iStatus;

    iStatus = xwork__file_write_size(pFile, pSummary->iSequence);
    if ( iStatus != XWORK_OK ) return iStatus;
    iStatus = xwork__file_write_size(pFile, (size_t)pSummary->eKind);
    if ( iStatus != XWORK_OK ) return iStatus;
    iStatus = xwork__file_write_string(pFile, pSummary->sKey);
    if ( iStatus != XWORK_OK ) return iStatus;
    iStatus = xwork__file_write_string(pFile, pSummary->sOperationId);
    if ( iStatus != XWORK_OK ) return iStatus;
    iStatus = xwork__file_write_string(pFile, pSummary->sRequestJson);
    if ( iStatus != XWORK_OK ) return iStatus;
    iStatus = xwork__file_write_string(pFile, pSummary->sResponseJson);
    if ( iStatus != XWORK_OK ) return iStatus;
    iStatus = xwork__file_write_string(pFile, pSummary->sArgumentsJson);
    if ( iStatus != XWORK_OK ) return iStatus;
    iStatus = xwork__file_write_string(pFile, pSummary->sResultJson);
    if ( iStatus != XWORK_OK ) return iStatus;
    iStatus = xwork__file_write_string(pFile, pSummary->sRequestHash);
    if ( iStatus != XWORK_OK ) return iStatus;
    iStatus = xwork__file_write_string(pFile, pSummary->sResponseHash);
    if ( iStatus != XWORK_OK ) return iStatus;
    iStatus = xwork__file_write_string(pFile, pSummary->sArgumentsHash);
    if ( iStatus != XWORK_OK ) return iStatus;
    iStatus = xwork__file_write_string(pFile, pSummary->sResultHash);
    if ( iStatus != XWORK_OK ) return iStatus;
    iStatus = xwork__file_write_string(pFile, pSummary->sContentHash);
    if ( iStatus != XWORK_OK ) return iStatus;
    return xwork__file_write_i64(pFile, (int64_t)pSummary->iStatus);
}

static xwork_status xwork__file_read_replay_entry_summary(
    FILE *pFile,
    uint64_t iFormatVersion,
    xwork_replay_entry_summary *pSummary
)
{
    size_t iValue;
    int64_t iStatusValue;
    xwork_status iStatus;

    xwork_replay_entry_summary_reset(pSummary);
    iStatus = xwork__file_read_size(pFile, &pSummary->iSequence);
    if ( iStatus != XWORK_OK ) return iStatus;
    iStatus = xwork__file_read_size(pFile, &iValue);
    if ( iStatus != XWORK_OK ) return iStatus;
    pSummary->eKind = (xwork_replay_entry_kind)iValue;
    iStatus = xwork__file_read_string(pFile, &pSummary->sKey);
    if ( iStatus != XWORK_OK ) return iStatus;
    iStatus = xwork__file_read_string(pFile, &pSummary->sOperationId);
    if ( iStatus != XWORK_OK ) return iStatus;
    if ( iFormatVersion >= 13u ) {
        iStatus = xwork__file_read_string(pFile, &pSummary->sRequestJson);
        if ( iStatus != XWORK_OK ) return iStatus;
        iStatus = xwork__file_read_string(pFile, &pSummary->sResponseJson);
        if ( iStatus != XWORK_OK ) return iStatus;
        iStatus = xwork__file_read_string(pFile, &pSummary->sArgumentsJson);
        if ( iStatus != XWORK_OK ) return iStatus;
        iStatus = xwork__file_read_string(pFile, &pSummary->sResultJson);
        if ( iStatus != XWORK_OK ) return iStatus;
    }
    iStatus = xwork__file_read_string(pFile, &pSummary->sRequestHash);
    if ( iStatus != XWORK_OK ) return iStatus;
    iStatus = xwork__file_read_string(pFile, &pSummary->sResponseHash);
    if ( iStatus != XWORK_OK ) return iStatus;
    iStatus = xwork__file_read_string(pFile, &pSummary->sArgumentsHash);
    if ( iStatus != XWORK_OK ) return iStatus;
    iStatus = xwork__file_read_string(pFile, &pSummary->sResultHash);
    if ( iStatus != XWORK_OK ) return iStatus;
    iStatus = xwork__file_read_string(pFile, &pSummary->sContentHash);
    if ( iStatus != XWORK_OK ) return iStatus;
    iStatus = xwork__file_read_i64(pFile, &iStatusValue);
    if ( iStatus != XWORK_OK ) return iStatus;
    pSummary->iStatus = (xwork_status)iStatusValue;
    return XWORK_OK;
}

static xwork_status xwork__file_write_replay_divergence(
    FILE *pFile,
    const xwork_replay_divergence *pDivergence
)
{
    xwork_status iStatus;

    iStatus = xwork__file_write_size(pFile, (size_t)pDivergence->eKind);
    if ( iStatus != XWORK_OK ) return iStatus;
    iStatus = xwork__file_write_size(pFile, pDivergence->iSequence);
    if ( iStatus != XWORK_OK ) return iStatus;
    iStatus = xwork__file_write_string(pFile, pDivergence->sExpectedKey);
    if ( iStatus != XWORK_OK ) return iStatus;
    iStatus = xwork__file_write_string(pFile, pDivergence->sActualKey);
    if ( iStatus != XWORK_OK ) return iStatus;
    iStatus = xwork__file_write_size(pFile, (size_t)pDivergence->eExpectedEntryKind);
    if ( iStatus != XWORK_OK ) return iStatus;
    iStatus = xwork__file_write_size(pFile, (size_t)pDivergence->eActualEntryKind);
    if ( iStatus != XWORK_OK ) return iStatus;
    iStatus = xwork__file_write_string(pFile, pDivergence->sExpectedHash);
    if ( iStatus != XWORK_OK ) return iStatus;
    iStatus = xwork__file_write_string(pFile, pDivergence->sActualHash);
    if ( iStatus != XWORK_OK ) return iStatus;
    return xwork__file_write_string(pFile, pDivergence->sMessage);
}

static xwork_status xwork__file_read_replay_divergence(
    FILE *pFile,
    xwork_replay_divergence *pDivergence
)
{
    size_t iValue;
    xwork_status iStatus;

    xwork_replay_divergence_reset(pDivergence);
    iStatus = xwork__file_read_size(pFile, &iValue);
    if ( iStatus != XWORK_OK ) return iStatus;
    pDivergence->eKind = (xwork_replay_divergence_kind)iValue;
    iStatus = xwork__file_read_size(pFile, &pDivergence->iSequence);
    if ( iStatus != XWORK_OK ) return iStatus;
    iStatus = xwork__file_read_string(pFile, &pDivergence->sExpectedKey);
    if ( iStatus != XWORK_OK ) return iStatus;
    iStatus = xwork__file_read_string(pFile, &pDivergence->sActualKey);
    if ( iStatus != XWORK_OK ) return iStatus;
    iStatus = xwork__file_read_size(pFile, &iValue);
    if ( iStatus != XWORK_OK ) return iStatus;
    pDivergence->eExpectedEntryKind = (xwork_replay_entry_kind)iValue;
    iStatus = xwork__file_read_size(pFile, &iValue);
    if ( iStatus != XWORK_OK ) return iStatus;
    pDivergence->eActualEntryKind = (xwork_replay_entry_kind)iValue;
    iStatus = xwork__file_read_string(pFile, &pDivergence->sExpectedHash);
    if ( iStatus != XWORK_OK ) return iStatus;
    iStatus = xwork__file_read_string(pFile, &pDivergence->sActualHash);
    if ( iStatus != XWORK_OK ) return iStatus;
    return xwork__file_read_string(pFile, &pDivergence->sMessage);
}

static xwork_status xwork__file_write_replay_result(
    FILE *pFile,
    const xwork_replay_result *pResult
)
{
    xwork_status iStatus;

    iStatus = xwork__file_write_i64(pFile, (int64_t)pResult->iStatus);
    if ( iStatus != XWORK_OK ) return iStatus;
    iStatus = xwork__file_write_size(pFile, pResult->iRecordedCount);
    if ( iStatus != XWORK_OK ) return iStatus;
    iStatus = xwork__file_write_size(pFile, pResult->iReplayedCount);
    if ( iStatus != XWORK_OK ) return iStatus;
    iStatus = xwork__file_write_size(pFile, pResult->iDivergenceCount);
    if ( iStatus != XWORK_OK ) return iStatus;
    iStatus = xwork__file_write_bool(pFile, pResult->bDiverged);
    if ( iStatus != XWORK_OK ) return iStatus;
    return xwork__file_write_replay_divergence(pFile, &pResult->tFirstDivergence);
}

static xwork_status xwork__file_read_replay_result(
    FILE *pFile,
    xwork_replay_result *pResult
)
{
    int64_t iStatusValue;
    xwork_status iStatus;

    xwork_replay_result_reset(pResult);
    iStatus = xwork__file_read_i64(pFile, &iStatusValue);
    if ( iStatus != XWORK_OK ) return iStatus;
    pResult->iStatus = (xwork_status)iStatusValue;
    iStatus = xwork__file_read_size(pFile, &pResult->iRecordedCount);
    if ( iStatus != XWORK_OK ) return iStatus;
    iStatus = xwork__file_read_size(pFile, &pResult->iReplayedCount);
    if ( iStatus != XWORK_OK ) return iStatus;
    iStatus = xwork__file_read_size(pFile, &pResult->iDivergenceCount);
    if ( iStatus != XWORK_OK ) return iStatus;
    iStatus = xwork__file_read_bool(pFile, &pResult->bDiverged);
    if ( iStatus != XWORK_OK ) return iStatus;
    return xwork__file_read_replay_divergence(pFile, &pResult->tFirstDivergence);
}

static xwork_status xwork__write_replay_file(
    const char *sPath,
    const xwork_replay_manifest *pManifest,
    const xwork_replay_entry_summary_list *pEntries,
    const xwork_replay_result *pResult
)
{
    FILE *pFile;
    char *sTempPath;
    xwork_status iStatus;
    size_t i;

    if ( !sPath || !pManifest || !pEntries || !pResult ) {
        return XWORK_ERROR_INVALID_ARGUMENT;
    }

    sTempPath = xwork__build_atomic_temp_path(sPath);
    if ( !sTempPath ) {
        return XWORK_ERROR_NO_MEMORY;
    }

    pFile = fopen(sTempPath, "wb");
    if ( !pFile ) {
        free(sTempPath);
        return XWORK_ERROR_EXTERNAL_FAILURE;
    }

    iStatus = xwork__write_persistence_header(
        pFile,
        xwork__replay_magic,
        sizeof(xwork__replay_magic)
    );
    if ( iStatus == XWORK_OK ) {
        iStatus = xwork__file_write_replay_manifest(pFile, pManifest);
    }
    if ( iStatus == XWORK_OK ) {
        iStatus = xwork__file_write_size(pFile, pEntries->iCount);
    }
    for ( i = 0u; iStatus == XWORK_OK && i < pEntries->iCount; ++i ) {
        iStatus = xwork__file_write_replay_entry_summary(
            pFile,
            &pEntries->pItems[i]
        );
    }
    if ( iStatus == XWORK_OK ) {
        iStatus = xwork__file_write_replay_result(pFile, pResult);
    }

    if ( fclose(pFile) != 0 && iStatus == XWORK_OK ) {
        iStatus = XWORK_ERROR_EXTERNAL_FAILURE;
    }
    if ( iStatus == XWORK_OK ) {
        iStatus = xwork__replace_file_atomic(sTempPath, sPath);
    }
    if ( iStatus != XWORK_OK ) {
        (void)remove(sTempPath);
    }
    free(sTempPath);
    return iStatus;
}

static xwork_status xwork__read_replay_file(
    const char *sPath,
    xwork_replay_manifest *pManifest,
    xwork_replay_entry_summary_list *pEntries,
    xwork_replay_result *pResult
)
{
    FILE *pFile;
    uint64_t iFormatVersion = 0u;
    xwork_replay_manifest tManifest;
    xwork_replay_entry_summary_list tEntries;
    xwork_replay_result tResult;
    size_t iCount;
    size_t i;
    xwork_status iStatus;

    if ( !sPath ) {
        return XWORK_ERROR_INVALID_ARGUMENT;
    }

    xwork_replay_manifest_init(&tManifest);
    xwork_replay_entry_summary_list_init(&tEntries);
    xwork_replay_result_init(&tResult);

    pFile = fopen(sPath, "rb");
    if ( !pFile ) {
        return errno == ENOENT ? XWORK_ERROR_NOT_FOUND : XWORK_ERROR_EXTERNAL_FAILURE;
    }

    iStatus = xwork__read_persistence_header(
        pFile,
        xwork__replay_magic,
        xwork__snapshot_magic_v0,
        sizeof(xwork__replay_magic),
        &iFormatVersion
    );
    (void)iFormatVersion;
    if ( iStatus == XWORK_OK ) {
        iStatus = xwork__file_read_replay_manifest(pFile, &tManifest);
    }
    if ( iStatus == XWORK_OK ) {
        iStatus = xwork__file_read_size(pFile, &iCount);
    }
    if ( iStatus == XWORK_OK && iCount > 0u ) {
        tEntries.pItems = (xwork_replay_entry_summary *)calloc(
            iCount,
            sizeof(*tEntries.pItems)
        );
        if ( !tEntries.pItems ) {
            iStatus = XWORK_ERROR_NO_MEMORY;
        } else {
            tEntries.iCount = iCount;
            for ( i = 0u; i < iCount; ++i ) {
                xwork_replay_entry_summary_init(&tEntries.pItems[i]);
            }
        }
    }
    for ( i = 0u; iStatus == XWORK_OK && i < tEntries.iCount; ++i ) {
        iStatus = xwork__file_read_replay_entry_summary(
            pFile,
            iFormatVersion,
            &tEntries.pItems[i]
        );
    }
    if ( iStatus == XWORK_OK ) {
        iStatus = xwork__file_read_replay_result(pFile, &tResult);
    }

    if ( fclose(pFile) != 0 && iStatus == XWORK_OK ) {
        iStatus = XWORK_ERROR_EXTERNAL_FAILURE;
    }

    if ( iStatus == XWORK_OK ) {
        if ( pManifest ) {
            xwork_replay_manifest_reset(pManifest);
            *pManifest = tManifest;
            xwork_replay_manifest_init(&tManifest);
        }
        if ( pEntries ) {
            xwork_replay_entry_summary_list_reset(pEntries);
            *pEntries = tEntries;
            xwork_replay_entry_summary_list_init(&tEntries);
        }
        if ( pResult ) {
            xwork_replay_result_reset(pResult);
            *pResult = tResult;
            xwork_replay_result_init(&tResult);
        }
    }

    xwork_replay_result_reset(&tResult);
    xwork_replay_entry_summary_list_reset(&tEntries);
    xwork_replay_manifest_reset(&tManifest);
    return iStatus;
}

static xwork_status xwork__write_artifact_file(
    const char *sPath,
    const xwork_artifact *pArtifact
)
{
    FILE *pFile;
    char *sTempPath;
    xwork_status iStatus;

    if ( !sPath || !pArtifact ) {
        return XWORK_ERROR_INVALID_ARGUMENT;
    }

    sTempPath = xwork__build_atomic_temp_path(sPath);
    if ( !sTempPath ) {
        return XWORK_ERROR_NO_MEMORY;
    }

    pFile = fopen(sTempPath, "wb");
    if ( !pFile ) {
        free(sTempPath);
        return XWORK_ERROR_EXTERNAL_FAILURE;
    }

    iStatus = xwork__write_persistence_header(
        pFile,
        xwork__artifact_magic,
        sizeof(xwork__artifact_magic)
    );
    if ( iStatus == XWORK_OK ) {
        iStatus = xwork__file_write_artifact_fields(pFile, pArtifact);
    }

    if ( fclose(pFile) != 0 && iStatus == XWORK_OK ) {
        iStatus = XWORK_ERROR_EXTERNAL_FAILURE;
    }
    if ( iStatus == XWORK_OK ) {
        iStatus = xwork__replace_file_atomic(sTempPath, sPath);
    }
    if ( iStatus != XWORK_OK ) {
        (void)remove(sTempPath);
    }
    free(sTempPath);
    return iStatus;
}

static xwork_status xwork__read_artifact_file(
    const char *sPath,
    xwork_artifact *pArtifact
)
{
    FILE *pFile;
    uint64_t iFormatVersion = 0u;
    xwork_status iStatus;

    if ( !sPath || !pArtifact ) {
        return XWORK_ERROR_INVALID_ARGUMENT;
    }

    pFile = fopen(sPath, "rb");
    if ( !pFile ) {
        return errno == ENOENT ? XWORK_ERROR_NOT_FOUND : XWORK_ERROR_EXTERNAL_FAILURE;
    }

    xwork_artifact_init(pArtifact);
    iStatus = xwork__read_persistence_header(
        pFile,
        xwork__artifact_magic,
        xwork__artifact_magic_v0,
        sizeof(xwork__artifact_magic),
        &iFormatVersion
    );
    if ( iStatus == XWORK_OK ) {
        iStatus = xwork__file_read_artifact_fields(
            pFile,
            iFormatVersion,
            pArtifact
        );
    }

    if ( fclose(pFile) != 0 && iStatus == XWORK_OK ) {
        iStatus = XWORK_ERROR_EXTERNAL_FAILURE;
    }
    if ( iStatus != XWORK_OK ) {
        xwork_artifact_reset(pArtifact);
    }
    return iStatus;
}

static xwork_status xwork__append_escaped_text(FILE *pFile, const char *sText)
{
    const unsigned char *pBytes;
    size_t i;

    if ( !pFile ) {
        return XWORK_ERROR_INVALID_ARGUMENT;
    }
    if ( !sText ) {
        return fputs("-", pFile) < 0 ? XWORK_ERROR_EXTERNAL_FAILURE : XWORK_OK;
    }

    pBytes = (const unsigned char *)sText;
    for ( i = 0u; pBytes[i] != '\0'; ++i ) {
        switch ( pBytes[i] ) {
            case '\\':
                if ( fputs("\\\\", pFile) < 0 ) return XWORK_ERROR_EXTERNAL_FAILURE;
                break;
            case '\n':
                if ( fputs("\\n", pFile) < 0 ) return XWORK_ERROR_EXTERNAL_FAILURE;
                break;
            case '\r':
                if ( fputs("\\r", pFile) < 0 ) return XWORK_ERROR_EXTERNAL_FAILURE;
                break;
            case '\t':
                if ( fputs("\\t", pFile) < 0 ) return XWORK_ERROR_EXTERNAL_FAILURE;
                break;
            default:
                if ( fputc((int)pBytes[i], pFile) == EOF ) {
                    return XWORK_ERROR_EXTERNAL_FAILURE;
                }
                break;
        }
    }

    return XWORK_OK;
}

static xwork_status xwork__read_line_owned(FILE *pFile, char **psLine)
{
    char *sBuffer = NULL;
    size_t iCapacity = 0u;
    size_t iLength = 0u;
    int iCh;

    if ( !pFile || !psLine ) {
        return XWORK_ERROR_INVALID_ARGUMENT;
    }

    *psLine = NULL;

    for ( ;; ) {
        iCh = fgetc(pFile);
        if ( iCh == EOF ) {
            break;
        }
        if ( iCh == '\r' ) {
            continue;
        }
        if ( iCh == '\n' ) {
            break;
        }
        if ( (iLength + 1u) >= iCapacity ) {
            size_t iNewCapacity = iCapacity ? iCapacity * 2u : 128u;
            char *sNewBuffer = (char *)realloc(sBuffer, iNewCapacity * sizeof(char));
            if ( !sNewBuffer ) {
                free(sBuffer);
                return XWORK_ERROR_NO_MEMORY;
            }
            sBuffer = sNewBuffer;
            iCapacity = iNewCapacity;
        }
        sBuffer[iLength++] = (char)iCh;
    }

    if ( iCh == EOF && iLength == 0u ) {
        free(sBuffer);
        return XWORK_ERROR_NOT_FOUND;
    }

    if ( !sBuffer ) {
        sBuffer = (char *)calloc(1u, sizeof(char));
        if ( !sBuffer ) {
            return XWORK_ERROR_NO_MEMORY;
        }
    } else {
        sBuffer[iLength] = '\0';
    }

    *psLine = sBuffer;
    return XWORK_OK;
}

static xwork_status xwork__event_log_has_content(
    const char *sPath,
    bool *pbHasContent
)
{
    FILE *pFile;
    int iCh;

    if ( !sPath || !pbHasContent ) {
        return XWORK_ERROR_INVALID_ARGUMENT;
    }

    *pbHasContent = false;
    pFile = fopen(sPath, "rb");
    if ( !pFile ) {
        return errno == ENOENT ? XWORK_OK : XWORK_ERROR_EXTERNAL_FAILURE;
    }

    iCh = fgetc(pFile);
    if ( fclose(pFile) != 0 ) {
        return XWORK_ERROR_EXTERNAL_FAILURE;
    }

    *pbHasContent = iCh != EOF;
    return XWORK_OK;
}

static xwork_status xwork__write_event_log_header(FILE *pFile)
{
    if ( !pFile ) {
        return XWORK_ERROR_INVALID_ARGUMENT;
    }

    if ( fprintf(
            pFile,
            "%s%u\n",
            xwork__event_log_header_prefix,
            (unsigned int)XWORK_EVENT_LOG_FORMAT_VERSION
        ) < 0 ) {
        return XWORK_ERROR_EXTERNAL_FAILURE;
    }
    return XWORK_OK;
}

static xwork_status xwork__check_event_log_line_header(
    const char *sLine,
    bool *pbIsHeader
)
{
    size_t iPrefixLen = sizeof(xwork__event_log_header_prefix) - 1u;
    char *sEnd;
    unsigned long iVersion;

    if ( !sLine || !pbIsHeader ) {
        return XWORK_ERROR_INVALID_ARGUMENT;
    }

    *pbIsHeader = false;
    if ( strncmp(sLine, xwork__event_log_header_prefix, iPrefixLen) != 0 ) {
        return XWORK_OK;
    }

    *pbIsHeader = true;
    errno = 0;
    iVersion = strtoul(sLine + iPrefixLen, &sEnd, 10);
    if ( errno != 0 || sEnd == (sLine + iPrefixLen) || *sEnd != '\0' ) {
        return XWORK_ERROR_EXTERNAL_FAILURE;
    }
    if ( iVersion > (unsigned long)XWORK_EVENT_LOG_FORMAT_VERSION ) {
        return XWORK_ERROR_UNSUPPORTED;
    }
    if ( iVersion == 0ul ) {
        return XWORK_ERROR_EXTERNAL_FAILURE;
    }
    return XWORK_OK;
}

static xwork_status xwork__decode_event_field(
    const char *sField,
    size_t iFieldLength,
    char **psText
)
{
    char *sDecoded;
    size_t iIn;
    size_t iOut = 0u;

    if ( !psText ) {
        return XWORK_ERROR_INVALID_ARGUMENT;
    }

    *psText = NULL;
    if ( iFieldLength == 1u && sField && sField[0] == '-' ) {
        return XWORK_OK;
    }

    sDecoded = (char *)calloc(iFieldLength + 1u, sizeof(char));
    if ( !sDecoded ) {
        return XWORK_ERROR_NO_MEMORY;
    }

    for ( iIn = 0u; iIn < iFieldLength; ++iIn ) {
        char c = sField ? sField[iIn] : '\0';

        if ( c == '\\' ) {
            if ( (iIn + 1u) >= iFieldLength ) {
                free(sDecoded);
                return XWORK_ERROR_EXTERNAL_FAILURE;
            }
            ++iIn;
            switch ( sField[iIn] ) {
                case '\\':
                    sDecoded[iOut++] = '\\';
                    break;
                case 'n':
                    sDecoded[iOut++] = '\n';
                    break;
                case 'r':
                    sDecoded[iOut++] = '\r';
                    break;
                case 't':
                    sDecoded[iOut++] = '\t';
                    break;
                default:
                    free(sDecoded);
                    return XWORK_ERROR_EXTERNAL_FAILURE;
            }
            continue;
        }

        sDecoded[iOut++] = c;
    }

    sDecoded[iOut] = '\0';
    *psText = sDecoded;
    return XWORK_OK;
}

static xwork_status xwork__consume_event_field(
    const char **psCursor,
    char **psText,
    bool *pbHasSeparator
)
{
    const char *sStart;
    const char *sSeparator;
    xwork_status iStatus;

    if ( !psCursor || !*psCursor || !psText ) {
        return XWORK_ERROR_INVALID_ARGUMENT;
    }

    sStart = *psCursor;
    sSeparator = strchr(sStart, '\t');
    iStatus = xwork__decode_event_field(
        sStart,
        sSeparator ? (size_t)(sSeparator - sStart) : strlen(sStart),
        psText
    );
    if ( iStatus != XWORK_OK ) {
        return iStatus;
    }

    if ( pbHasSeparator ) {
        *pbHasSeparator = sSeparator != NULL;
    }
    *psCursor = sSeparator ? (sSeparator + 1) : (sStart + strlen(sStart));
    return XWORK_OK;
}

static xwork_status xwork__parse_event_line(
    const char *sRunId,
    const char *sLine,
    xwork_event *pEvent
)
{
    const char *sCursor;
    char *sRunIdCopy = NULL;
    char *sEventId = NULL;
    char *sToolId = NULL;
    char *sApprovalRequestId = NULL;
    char *sCheckpointId = NULL;
    char *sSummary = NULL;
    char *sEnd;
    unsigned long long iSequence;
    unsigned long iKind;
    unsigned long iRunState;
    bool bHasSeparator;
    xwork_status iStatus;

    if ( !sRunId || !sRunId[0] || !sLine || !pEvent ) {
        return XWORK_ERROR_INVALID_ARGUMENT;
    }

    xwork_event_reset(pEvent);
    sCursor = sLine;

    errno = 0;
    iSequence = strtoull(sCursor, &sEnd, 10);
    if ( errno != 0 || sEnd == sCursor || *sEnd != '\t' ) {
        return XWORK_ERROR_EXTERNAL_FAILURE;
    }
    sCursor = sEnd + 1;

    errno = 0;
    iKind = strtoul(sCursor, &sEnd, 10);
    if ( errno != 0 || sEnd == sCursor || *sEnd != '\t' ) {
        return XWORK_ERROR_EXTERNAL_FAILURE;
    }
    sCursor = sEnd + 1;

    errno = 0;
    iRunState = strtoul(sCursor, &sEnd, 10);
    if ( errno != 0 || sEnd == sCursor || *sEnd != '\t' ) {
        return XWORK_ERROR_EXTERNAL_FAILURE;
    }
    sCursor = sEnd + 1;

    iStatus = xwork__consume_event_field(&sCursor, &sEventId, &bHasSeparator);
    if ( iStatus != XWORK_OK || !bHasSeparator || !sEventId || !sEventId[0] ) {
        free(sEventId);
        return iStatus != XWORK_OK ? iStatus : XWORK_ERROR_EXTERNAL_FAILURE;
    }
    iStatus = xwork__consume_event_field(&sCursor, &sToolId, &bHasSeparator);
    if ( iStatus != XWORK_OK || !bHasSeparator ) goto fail;
    iStatus = xwork__consume_event_field(&sCursor, &sApprovalRequestId, &bHasSeparator);
    if ( iStatus != XWORK_OK || !bHasSeparator ) goto fail;
    iStatus = xwork__consume_event_field(&sCursor, &sCheckpointId, &bHasSeparator);
    if ( iStatus != XWORK_OK || !bHasSeparator ) goto fail;
    iStatus = xwork__consume_event_field(&sCursor, &sSummary, &bHasSeparator);
    if ( iStatus != XWORK_OK || bHasSeparator || *sCursor != '\0' ) goto fail;

    sRunIdCopy = xwork__dup_cstr(sRunId);
    if ( !sRunIdCopy ) {
        iStatus = XWORK_ERROR_NO_MEMORY;
        goto fail;
    }

    pEvent->sEventId = sEventId;
    pEvent->sRunId = sRunIdCopy;
    pEvent->sToolId = sToolId;
    pEvent->sApprovalRequestId = sApprovalRequestId;
    pEvent->sCheckpointId = sCheckpointId;
    pEvent->sSummary = sSummary;
    pEvent->eKind = (xwork_event_kind)iKind;
    pEvent->eRunState = (xwork_run_state)iRunState;
    pEvent->iSequence = (size_t)iSequence;
    return XWORK_OK;

fail:
    free(sRunIdCopy);
    free(sEventId);
    free(sToolId);
    free(sApprovalRequestId);
    free(sCheckpointId);
    free(sSummary);
    xwork_event_reset(pEvent);
    return iStatus != XWORK_OK ? iStatus : XWORK_ERROR_EXTERNAL_FAILURE;
}

static xwork_status xwork__file_persistence_store_event_cb(
    const xwork_event *pEvent,
    void *pUserData
)
{
    xwork_file_persistence *pStore = (xwork_file_persistence *)pUserData;
    char *sRunDir = NULL;
    char *sEventsPath = NULL;
    FILE *pFile = NULL;
    bool bHasEventLogContent = false;
    xwork_status iStatus;

    if ( !pStore || !pStore->sRootPath || !pEvent || !pEvent->sRunId ) {
        return XWORK_ERROR_INVALID_ARGUMENT;
    }

    iStatus = xwork__ensure_run_storage(pStore, pEvent->sRunId, &sRunDir, NULL);
    if ( iStatus != XWORK_OK ) {
        return iStatus;
    }

    sEventsPath = xwork__build_events_log_path(sRunDir);
    if ( !sEventsPath ) {
        free(sRunDir);
        return XWORK_ERROR_NO_MEMORY;
    }

    iStatus = xwork__event_log_has_content(sEventsPath, &bHasEventLogContent);
    if ( iStatus != XWORK_OK ) {
        free(sEventsPath);
        free(sRunDir);
        return iStatus;
    }

    pFile = fopen(sEventsPath, "ab");
    if ( !pFile ) {
        free(sEventsPath);
        free(sRunDir);
        return XWORK_ERROR_EXTERNAL_FAILURE;
    }

    if ( !bHasEventLogContent ) {
        iStatus = xwork__write_event_log_header(pFile);
    }

    if ( iStatus != XWORK_OK ) {
        goto done;
    }

    if ( fprintf(
            pFile,
            "%llu\t%u\t%u\t",
            (unsigned long long)pEvent->iSequence,
            (unsigned int)pEvent->eKind,
            (unsigned int)pEvent->eRunState
        ) < 0 ) {
        iStatus = XWORK_ERROR_EXTERNAL_FAILURE;
    } else {
        iStatus = xwork__append_escaped_text(pFile, pEvent->sEventId);
    }
    if ( iStatus == XWORK_OK ) {
        iStatus = fputc('\t', pFile) == EOF ? XWORK_ERROR_EXTERNAL_FAILURE : XWORK_OK;
    }
    if ( iStatus == XWORK_OK ) iStatus = xwork__append_escaped_text(pFile, pEvent->sToolId);
    if ( iStatus == XWORK_OK ) iStatus = fputc('\t', pFile) == EOF ? XWORK_ERROR_EXTERNAL_FAILURE : XWORK_OK;
    if ( iStatus == XWORK_OK ) iStatus = xwork__append_escaped_text(pFile, pEvent->sApprovalRequestId);
    if ( iStatus == XWORK_OK ) iStatus = fputc('\t', pFile) == EOF ? XWORK_ERROR_EXTERNAL_FAILURE : XWORK_OK;
    if ( iStatus == XWORK_OK ) iStatus = xwork__append_escaped_text(pFile, pEvent->sCheckpointId);
    if ( iStatus == XWORK_OK ) iStatus = fputc('\t', pFile) == EOF ? XWORK_ERROR_EXTERNAL_FAILURE : XWORK_OK;
    if ( iStatus == XWORK_OK ) iStatus = xwork__append_escaped_text(pFile, pEvent->sSummary);
    if ( iStatus == XWORK_OK ) {
        iStatus = fputc('\n', pFile) == EOF ? XWORK_ERROR_EXTERNAL_FAILURE : XWORK_OK;
    }

done:
    if ( fclose(pFile) != 0 && iStatus == XWORK_OK ) {
        iStatus = XWORK_ERROR_EXTERNAL_FAILURE;
    }
    free(sEventsPath);
    free(sRunDir);
    return iStatus;
}

static xwork_status xwork__file_persistence_store_checkpoint_cb(
    const xwork_checkpoint *pCheckpoint,
    const xwork_run_snapshot *pSnapshot,
    void *pUserData
)
{
    xwork_file_persistence *pStore = (xwork_file_persistence *)pUserData;
    char *sRunDir = NULL;
    char *sCheckpointsDir = NULL;
    char *sLatestPath = NULL;
    char *sCheckpointPath = NULL;
    xwork_status iStatus;

    if ( !pStore || !pStore->sRootPath || !pCheckpoint || !pSnapshot ||
         !pCheckpoint->sRunId || !pSnapshot->sRunId ) {
        return XWORK_ERROR_INVALID_ARGUMENT;
    }
    if ( strcmp(pCheckpoint->sRunId, pSnapshot->sRunId) != 0 ) {
        return XWORK_ERROR_INVALID_ARGUMENT;
    }

    iStatus = xwork__ensure_run_storage(
        pStore,
        pCheckpoint->sRunId,
        &sRunDir,
        &sCheckpointsDir
    );
    if ( iStatus != XWORK_OK ) {
        return iStatus;
    }

    sLatestPath = xwork__dup_printf("%s/latest.snapshot", sRunDir);
    if ( !sLatestPath ) {
        iStatus = XWORK_ERROR_NO_MEMORY;
        goto cleanup;
    }
    iStatus = xwork__write_snapshot_file(sLatestPath, pSnapshot);
    if ( iStatus != XWORK_OK ) {
        goto cleanup;
    }

    if ( pCheckpoint->sCheckpointId && pCheckpoint->sCheckpointId[0] ) {
        sCheckpointPath = xwork__build_checkpoint_file_path(
            sCheckpointsDir,
            pCheckpoint->sCheckpointId
        );
        if ( !sCheckpointPath ) {
            iStatus = XWORK_ERROR_NO_MEMORY;
            goto cleanup;
        }
        iStatus = xwork__write_snapshot_file(sCheckpointPath, pSnapshot);
    }

cleanup:
    free(sCheckpointPath);
    free(sLatestPath);
    free(sCheckpointsDir);
    free(sRunDir);
    return iStatus;
}

static xwork_status xwork__file_persistence_store_artifact_cb(
    const xwork_artifact *pArtifact,
    void *pUserData
)
{
    xwork_file_persistence *pStore = (xwork_file_persistence *)pUserData;
    char *sRunDir = NULL;
    char *sArtifactsDir = NULL;
    char *sArtifactPath = NULL;
    xwork_status iStatus;

    if ( !pStore || !pStore->sRootPath || !pArtifact ||
         !pArtifact->sRunId || !pArtifact->sRunId[0] ||
         !pArtifact->sArtifactId || !pArtifact->sArtifactId[0] ) {
        return XWORK_ERROR_INVALID_ARGUMENT;
    }

    iStatus = xwork__ensure_artifact_storage(
        pStore,
        pArtifact->sRunId,
        &sRunDir,
        &sArtifactsDir
    );
    if ( iStatus != XWORK_OK ) {
        return iStatus;
    }

    sArtifactPath = xwork__build_artifact_file_path(
        sArtifactsDir,
        pArtifact->sArtifactId
    );
    if ( !sArtifactPath ) {
        free(sArtifactsDir);
        free(sRunDir);
        return XWORK_ERROR_NO_MEMORY;
    }

    iStatus = xwork__write_artifact_file(sArtifactPath, pArtifact);
    free(sArtifactPath);
    free(sArtifactsDir);
    free(sRunDir);
    return iStatus;
}

static xwork_status xwork__file_persistence_store_run_snapshot_cb(
    const xwork_run_snapshot *pSnapshot,
    void *pUserData
)
{
    xwork_file_persistence *pStore = (xwork_file_persistence *)pUserData;
    char *sRunDir = NULL;
    char *sCheckpointsDir = NULL;
    char *sLatestPath = NULL;
    xwork_status iStatus;

    if ( !pStore || !pStore->sRootPath || !pSnapshot || !pSnapshot->sRunId ) {
        return XWORK_ERROR_INVALID_ARGUMENT;
    }

    iStatus = xwork__ensure_run_storage(
        pStore,
        pSnapshot->sRunId,
        &sRunDir,
        &sCheckpointsDir
    );
    if ( iStatus != XWORK_OK ) {
        return iStatus;
    }

    sLatestPath = xwork__dup_printf("%s/latest.snapshot", sRunDir);
    if ( !sLatestPath ) {
        iStatus = XWORK_ERROR_NO_MEMORY;
        goto cleanup;
    }

    iStatus = xwork__write_snapshot_file(sLatestPath, pSnapshot);

cleanup:
    free(sLatestPath);
    free(sCheckpointsDir);
    free(sRunDir);
    return iStatus;
}

static xwork_status xwork__file_persistence_load_run_snapshot_cb(
    const char *sRunId,
    xwork_run_snapshot *pSnapshot,
    void *pUserData
)
{
    xwork_file_persistence *pStore = (xwork_file_persistence *)pUserData;
    char *sRunDir = NULL;
    char *sLatestPath = NULL;
    xwork_status iStatus;

    if ( !pStore || !pStore->sRootPath || !sRunId || !sRunId[0] || !pSnapshot ) {
        return XWORK_ERROR_INVALID_ARGUMENT;
    }

    sRunDir = xwork__build_run_dir_path(pStore, sRunId);
    if ( !sRunDir ) {
        return XWORK_ERROR_NO_MEMORY;
    }

    sLatestPath = xwork__dup_printf("%s/latest.snapshot", sRunDir);
    if ( !sLatestPath ) {
        free(sRunDir);
        return XWORK_ERROR_NO_MEMORY;
    }

    iStatus = xwork__read_snapshot_file(sLatestPath, pSnapshot);
    free(sLatestPath);
    free(sRunDir);
    return iStatus;
}

static xwork_status xwork__file_persistence_load_checkpoint_snapshot_cb(
    const char *sRunId,
    const char *sCheckpointId,
    xwork_run_snapshot *pSnapshot,
    void *pUserData
)
{
    xwork_file_persistence *pStore = (xwork_file_persistence *)pUserData;
    char *sRunDir = NULL;
    char *sCheckpointsDir = NULL;
    char *sCheckpointPath = NULL;
    xwork_status iStatus;

    if ( !pStore || !pStore->sRootPath || !sRunId || !sRunId[0] ||
         !sCheckpointId || !sCheckpointId[0] || !pSnapshot ) {
        return XWORK_ERROR_INVALID_ARGUMENT;
    }

    sRunDir = xwork__build_run_dir_path(pStore, sRunId);
    if ( !sRunDir ) {
        return XWORK_ERROR_NO_MEMORY;
    }

    sCheckpointsDir = xwork__dup_printf("%s/checkpoints", sRunDir);
    if ( !sCheckpointsDir ) {
        free(sRunDir);
        return XWORK_ERROR_NO_MEMORY;
    }

    sCheckpointPath = xwork__build_checkpoint_file_path(sCheckpointsDir, sCheckpointId);
    if ( !sCheckpointPath ) {
        free(sCheckpointsDir);
        free(sRunDir);
        return XWORK_ERROR_NO_MEMORY;
    }

    iStatus = xwork__read_snapshot_file(sCheckpointPath, pSnapshot);
    free(sCheckpointPath);
    free(sCheckpointsDir);
    free(sRunDir);
    return iStatus;
}

static void xwork__event_record_reset(xwork_event_record *pRecord)
{
    if ( !pRecord ) {
        return;
    }

    xwork__free_cstr(&pRecord->sEventId);
    xwork__free_cstr(&pRecord->sToolId);
    xwork__free_cstr(&pRecord->sApprovalRequestId);
    xwork__free_cstr(&pRecord->sCheckpointId);
    xwork__free_cstr(&pRecord->sSummary);
    memset(&pRecord->tEvent, 0, sizeof(pRecord->tEvent));
}

static void xwork__checkpoint_record_reset(xwork_checkpoint_record *pRecord)
{
    if ( !pRecord ) {
        return;
    }

    xwork__free_cstr(&pRecord->sCheckpointId);
    xwork__free_cstr(&pRecord->sPendingStep);
    xwork__free_cstr(&pRecord->sSessionStateRef);
    xwork__free_cstr(&pRecord->sSessionStateData);
    xwork__free_cstr(&pRecord->sToolOutputsRef);
    xwork__free_cstr(&pRecord->sWorkspaceSnapshotRef);
    xwork__free_cstr(&pRecord->sArtifactRefs);
    xwork__free_cstr(&pRecord->sLastOutputText);
    xwork__free_cstr(&pRecord->sMemoryContextText);
    xwork__free_cstr(&pRecord->sToolCallId);
    xwork__free_cstr(&pRecord->sToolId);
    xwork__free_cstr(&pRecord->sToolArgumentsJson);
    xwork__free_cstr(&pRecord->sToolResultText);
    xwork__free_cstr(&pRecord->sToolVisibleSummary);
    xwork__free_cstr(&pRecord->sApprovalRequestId);
    xwork__free_cstr(&pRecord->sApprovalToolId);
    xwork__free_cstr(&pRecord->sApprovalReason);
    xwork__free_cstr(&pRecord->sApprovalScope);
    xwork__free_cstr(&pRecord->sApprovalActionSummary);
    memset(&pRecord->tCheckpoint, 0, sizeof(pRecord->tCheckpoint));
    pRecord->bHasMemoryContext = false;
    pRecord->iMemoryWorkspaceCount = 0u;
    pRecord->bHasToolCall = false;
    pRecord->bHasToolResult = false;
    pRecord->bHasApprovalRequest = false;
    pRecord->eApprovalRiskLevel = XWORK_RISK_LOW;
    pRecord->eApprovalState = XWORK_APPROVAL_PENDING;
    pRecord->iApprovalSequence = 0u;
}

static xwork_status xwork__ensure_event_capacity(xwork_run *pRun)
{
    xwork_event_record *pNewItems;
    size_t iNewCapacity;

    if ( !pRun ) {
        return XWORK_ERROR_INVALID_ARGUMENT;
    }
    if ( pRun->iEventCount < pRun->iEventCapacity ) {
        return XWORK_OK;
    }

    iNewCapacity = pRun->iEventCapacity ? pRun->iEventCapacity * 2u : 4u;
    pNewItems = (xwork_event_record *)realloc(
        pRun->pEventLog,
        iNewCapacity * sizeof(*pNewItems)
    );
    if ( !pNewItems ) {
        return XWORK_ERROR_NO_MEMORY;
    }

    memset(pNewItems + pRun->iEventCapacity, 0, (iNewCapacity - pRun->iEventCapacity) * sizeof(*pNewItems));
    pRun->pEventLog = pNewItems;
    pRun->iEventCapacity = iNewCapacity;
    return XWORK_OK;
}

static xwork_status xwork__ensure_checkpoint_capacity(xwork_run *pRun)
{
    xwork_checkpoint_record *pNewItems;
    size_t iNewCapacity;

    if ( !pRun ) {
        return XWORK_ERROR_INVALID_ARGUMENT;
    }
    if ( pRun->iCheckpointCount < pRun->iCheckpointCapacity ) {
        return XWORK_OK;
    }

    iNewCapacity = pRun->iCheckpointCapacity ? pRun->iCheckpointCapacity * 2u : 4u;
    pNewItems = (xwork_checkpoint_record *)realloc(
        pRun->pCheckpointLog,
        iNewCapacity * sizeof(*pNewItems)
    );
    if ( !pNewItems ) {
        return XWORK_ERROR_NO_MEMORY;
    }

    memset(
        pNewItems + pRun->iCheckpointCapacity,
        0,
        (iNewCapacity - pRun->iCheckpointCapacity) * sizeof(*pNewItems)
    );
    pRun->pCheckpointLog = pNewItems;
    pRun->iCheckpointCapacity = iNewCapacity;
    return XWORK_OK;
}

void xwork__run_reset_persistence(xwork_run *pRun)
{
    size_t i;

    if ( !pRun ) {
        return;
    }

    for ( i = 0u; i < pRun->iEventCount; ++i ) {
        xwork__event_record_reset(&pRun->pEventLog[i]);
    }
    free(pRun->pEventLog);
    pRun->pEventLog = NULL;
    pRun->iEventCount = 0u;
    pRun->iEventCapacity = 0u;

    for ( i = 0u; i < pRun->iCheckpointCount; ++i ) {
        xwork__checkpoint_record_reset(&pRun->pCheckpointLog[i]);
    }
    free(pRun->pCheckpointLog);
    pRun->pCheckpointLog = NULL;
    pRun->iCheckpointCount = 0u;
    pRun->iCheckpointCapacity = 0u;
}

xwork_status xwork__run_append_event_snapshot(xwork_run *pRun)
{
    xwork_event_record *pRecord;
    xwork_status iStatus;

    if ( !pRun || !pRun->sLastEventId ) {
        return XWORK_ERROR_INVALID_ARGUMENT;
    }

    iStatus = xwork__ensure_event_capacity(pRun);
    if ( iStatus != XWORK_OK ) {
        return iStatus;
    }

    pRecord = &pRun->pEventLog[pRun->iEventCount];
    memset(pRecord, 0, sizeof(*pRecord));

    pRecord->sEventId = xwork__dup_cstr(pRun->sLastEventId);
    pRecord->sToolId = xwork__dup_cstr(pRun->sLastEventToolId);
    pRecord->sApprovalRequestId = xwork__dup_cstr(pRun->sLastEventApprovalRequestId);
    pRecord->sCheckpointId = xwork__dup_cstr(pRun->sLastEventCheckpointId);
    pRecord->sSummary = xwork__dup_cstr(pRun->sLastEventSummary);

    if ( !pRecord->sEventId ) {
        xwork__event_record_reset(pRecord);
        return XWORK_ERROR_NO_MEMORY;
    }

    pRecord->tEvent.sEventId = pRecord->sEventId;
    pRecord->tEvent.sRunId = pRun->sRunId;
    pRecord->tEvent.sToolId = pRecord->sToolId;
    pRecord->tEvent.sApprovalRequestId = pRecord->sApprovalRequestId;
    pRecord->tEvent.sCheckpointId = pRecord->sCheckpointId;
    pRecord->tEvent.sSummary = pRecord->sSummary;
    pRecord->tEvent.eKind = pRun->eLastEventKind;
    pRecord->tEvent.eRunState = pRun->eLastEventRunState;
    pRecord->tEvent.iSequence = pRun->iLastEventSequence;

    ++pRun->iEventCount;
    return XWORK_OK;
}

xwork_status xwork__run_append_checkpoint_snapshot(xwork_run *pRun)
{
    xwork_checkpoint_record *pRecord;
    xwork_status iStatus;

    if ( !pRun || !pRun->sLastCheckpointId ) {
        return XWORK_ERROR_INVALID_ARGUMENT;
    }

    iStatus = xwork__ensure_checkpoint_capacity(pRun);
    if ( iStatus != XWORK_OK ) {
        return iStatus;
    }

    pRecord = &pRun->pCheckpointLog[pRun->iCheckpointCount];
    memset(pRecord, 0, sizeof(*pRecord));

    pRecord->sCheckpointId = xwork__dup_cstr(pRun->sLastCheckpointId);
    pRecord->sPendingStep = xwork__dup_cstr(pRun->sLastCheckpointPendingStep);
    pRecord->sSessionStateRef = xwork__dup_cstr(pRun->sLastCheckpointSessionStateRef);
    pRecord->sSessionStateData = xwork__dup_cstr(pRun->sSessionStateData);
    pRecord->sToolOutputsRef = xwork__dup_cstr(pRun->sLastCheckpointToolOutputsRef);
    pRecord->sWorkspaceSnapshotRef = xwork__dup_cstr(pRun->sLastCheckpointWorkspaceSnapshotRef);
    pRecord->sArtifactRefs = xwork__dup_cstr(pRun->sLastCheckpointArtifactRefs);
    pRecord->sLastOutputText = xwork__dup_cstr(pRun->sLastOutputText);
    pRecord->sMemoryContextText = xwork__dup_cstr(pRun->sLastMemoryContextText);
    pRecord->sToolCallId = xwork__dup_cstr(pRun->sLastToolCallId);
    pRecord->sToolId = xwork__dup_cstr(pRun->sLastToolId);
    pRecord->sToolArgumentsJson = xwork__dup_cstr(pRun->sLastToolArgumentsJson);
    pRecord->sToolResultText = xwork__dup_cstr(pRun->sLastToolResultText);
    pRecord->sToolVisibleSummary = xwork__dup_cstr(pRun->sLastToolVisibleSummary);
    pRecord->sApprovalRequestId = xwork__dup_cstr(pRun->sLastApprovalRequestId);
    pRecord->sApprovalToolId = xwork__dup_cstr(pRun->sLastApprovalToolId);
    pRecord->sApprovalReason = xwork__dup_cstr(pRun->sLastApprovalReason);
    pRecord->sApprovalScope = xwork__dup_cstr(pRun->sLastApprovalScope);
    pRecord->sApprovalActionSummary = xwork__dup_cstr(pRun->sLastApprovalActionSummary);

    if ( !pRecord->sCheckpointId ) {
        xwork__checkpoint_record_reset(pRecord);
        return XWORK_ERROR_NO_MEMORY;
    }

    pRecord->tCheckpoint.sCheckpointId = pRecord->sCheckpointId;
    pRecord->tCheckpoint.sRunId = pRun->sRunId;
    pRecord->tCheckpoint.sPendingStep = pRecord->sPendingStep;
    pRecord->tCheckpoint.sSessionStateRef = pRecord->sSessionStateRef;
    pRecord->tCheckpoint.sToolOutputsRef = pRecord->sToolOutputsRef;
    pRecord->tCheckpoint.sWorkspaceSnapshotRef = pRecord->sWorkspaceSnapshotRef;
    pRecord->tCheckpoint.sArtifactRefs = pRecord->sArtifactRefs;
    pRecord->tCheckpoint.eKind = pRun->eLastCheckpointKind;
    pRecord->tCheckpoint.eRunState = pRun->eLastCheckpointRunState;
    pRecord->tCheckpoint.iSequence = pRun->iLastCheckpointSequence;
    pRecord->bHasMemoryContext =
        pRun->bHasLastMemoryContext && pRecord->sMemoryContextText != NULL;
    pRecord->iMemoryWorkspaceCount =
        pRecord->bHasMemoryContext ? pRun->iLastMemoryWorkspaceCount : 0u;
    pRecord->bHasToolCall = pRun->bHasLastToolCall;
    pRecord->bHasToolResult = pRun->bHasLastToolResult;
    pRecord->bHasApprovalRequest = pRun->bHasLastApprovalRequest;
    pRecord->eApprovalRiskLevel = pRun->eLastApprovalRiskLevel;
    pRecord->eApprovalState = pRun->eLastApprovalState;
    pRecord->iApprovalSequence = pRun->iLastApprovalSequence;

    ++pRun->iCheckpointCount;
    return XWORK_OK;
}

xwork_status xwork__run_restore_checkpoint_snapshot(
    xwork_run *pRun,
    const xwork_checkpoint_record *pRecord
)
{
    size_t iNextApprovalSequence;
    size_t iNextCheckpointSequence;
    xwork_status iStatus;

    if ( !pRun || !pRecord || !pRecord->sCheckpointId ) {
        return XWORK_ERROR_INVALID_ARGUMENT;
    }

    iNextApprovalSequence = pRun->iNextApprovalSequence;
    iNextCheckpointSequence = pRun->iNextCheckpointSequence;

    iStatus = xwork__replace_cstr(&pRun->sLastOutputText, pRecord->sLastOutputText);
    if ( iStatus != XWORK_OK ) {
        return iStatus;
    }
    iStatus = xwork__replace_cstr(&pRun->sLastMemoryContextText, pRecord->sMemoryContextText);
    if ( iStatus != XWORK_OK ) {
        return iStatus;
    }
    pRun->bHasLastMemoryContext = pRecord->bHasMemoryContext;
    pRun->iLastMemoryWorkspaceCount = pRecord->iMemoryWorkspaceCount;

    iStatus = xwork__replace_cstr(&pRun->sLastToolCallId, pRecord->sToolCallId);
    if ( iStatus != XWORK_OK ) {
        return iStatus;
    }
    iStatus = xwork__replace_cstr(&pRun->sLastToolId, pRecord->sToolId);
    if ( iStatus != XWORK_OK ) {
        return iStatus;
    }
    iStatus = xwork__replace_cstr(&pRun->sLastToolArgumentsJson, pRecord->sToolArgumentsJson);
    if ( iStatus != XWORK_OK ) {
        return iStatus;
    }
    iStatus = xwork__replace_cstr(&pRun->sLastToolResultText, pRecord->sToolResultText);
    if ( iStatus != XWORK_OK ) {
        return iStatus;
    }
    iStatus = xwork__replace_cstr(&pRun->sLastToolVisibleSummary, pRecord->sToolVisibleSummary);
    if ( iStatus != XWORK_OK ) {
        return iStatus;
    }
    pRun->bHasLastToolCall = pRecord->bHasToolCall;
    pRun->bHasLastToolResult = pRecord->bHasToolResult;

    iStatus = xwork__replace_cstr(&pRun->sLastApprovalRequestId, pRecord->sApprovalRequestId);
    if ( iStatus != XWORK_OK ) {
        return iStatus;
    }
    iStatus = xwork__replace_cstr(&pRun->sLastApprovalToolId, pRecord->sApprovalToolId);
    if ( iStatus != XWORK_OK ) {
        return iStatus;
    }
    iStatus = xwork__replace_cstr(&pRun->sLastApprovalReason, pRecord->sApprovalReason);
    if ( iStatus != XWORK_OK ) {
        return iStatus;
    }
    iStatus = xwork__replace_cstr(&pRun->sLastApprovalScope, pRecord->sApprovalScope);
    if ( iStatus != XWORK_OK ) {
        return iStatus;
    }
    iStatus = xwork__replace_cstr(&pRun->sLastApprovalActionSummary, pRecord->sApprovalActionSummary);
    if ( iStatus != XWORK_OK ) {
        return iStatus;
    }
    pRun->bHasLastApprovalRequest = pRecord->bHasApprovalRequest;
    pRun->eLastApprovalRiskLevel = pRecord->eApprovalRiskLevel;
    pRun->eLastApprovalState = pRecord->eApprovalState;
    pRun->iLastApprovalSequence = pRecord->iApprovalSequence;
    if ( pRecord->iApprovalSequence > iNextApprovalSequence ) {
        iNextApprovalSequence = pRecord->iApprovalSequence;
    }
    pRun->iNextApprovalSequence = iNextApprovalSequence;

    iStatus = xwork__replace_cstr(&pRun->sLastCheckpointId, pRecord->sCheckpointId);
    if ( iStatus != XWORK_OK ) {
        return iStatus;
    }
    iStatus = xwork__replace_cstr(&pRun->sLastCheckpointPendingStep, pRecord->sPendingStep);
    if ( iStatus != XWORK_OK ) {
        return iStatus;
    }
    iStatus = xwork__replace_cstr(
        &pRun->sLastCheckpointSessionStateRef,
        pRecord->sSessionStateRef
    );
    if ( iStatus != XWORK_OK ) {
        return iStatus;
    }
    xwork__run_discard_session(pRun);
    iStatus = xwork__replace_cstr(&pRun->sSessionStateData, pRecord->sSessionStateData);
    if ( iStatus != XWORK_OK ) {
        return iStatus;
    }
    iStatus = xwork__replace_cstr(
        &pRun->sLastCheckpointToolOutputsRef,
        pRecord->sToolOutputsRef
    );
    if ( iStatus != XWORK_OK ) {
        return iStatus;
    }
    iStatus = xwork__replace_cstr(
        &pRun->sLastCheckpointWorkspaceSnapshotRef,
        pRecord->sWorkspaceSnapshotRef
    );
    if ( iStatus != XWORK_OK ) {
        return iStatus;
    }
    iStatus = xwork__replace_cstr(&pRun->sLastCheckpointArtifactRefs, pRecord->sArtifactRefs);
    if ( iStatus != XWORK_OK ) {
        return iStatus;
    }

    pRun->bHasLastCheckpoint = true;
    pRun->eLastCheckpointKind = pRecord->tCheckpoint.eKind;
    pRun->eLastCheckpointRunState = pRecord->tCheckpoint.eRunState;
    pRun->iLastCheckpointSequence = pRecord->tCheckpoint.iSequence;
    if ( pRecord->tCheckpoint.iSequence > iNextCheckpointSequence ) {
        iNextCheckpointSequence = pRecord->tCheckpoint.iSequence;
    }
    pRun->iNextCheckpointSequence = iNextCheckpointSequence;
    pRun->eState = pRecord->tCheckpoint.eRunState;

    return XWORK_OK;
}

void xwork_file_persistence_options_init(xwork_file_persistence_options *pOptions)
{
    if ( pOptions ) {
        memset(pOptions, 0, sizeof(*pOptions));
    }
}

void xwork_file_persistence_init(xwork_file_persistence *pStore)
{
    if ( pStore ) {
        memset(pStore, 0, sizeof(*pStore));
    }
}

void xwork_file_persistence_reset(xwork_file_persistence *pStore)
{
    if ( !pStore ) {
        return;
    }

    xwork__free_cstr(&pStore->sRootPath);
}

void xwork_string_list_init(xwork_string_list *pList)
{
    if ( pList ) {
        memset(pList, 0, sizeof(*pList));
    }
}

void xwork_string_list_reset(xwork_string_list *pList)
{
    size_t i;
    char **psItems;

    if ( !pList ) {
        return;
    }

    psItems = (char **)pList->psItems;
    if ( psItems ) {
        for ( i = 0u; i < pList->iCount; ++i ) {
            free(psItems[i]);
        }
        free(psItems);
    }

    xwork_string_list_init(pList);
}

xwork_status xwork_file_persistence_configure_backend(
    xwork_file_persistence *pStore,
    const xwork_file_persistence_options *pOptions,
    xwork_persistence_backend *pBackend
)
{
    char *sRunsRoot = NULL;
    char *sTaskGraphsRoot = NULL;
    char *sAgentPoolsRoot = NULL;
    char *sControlPlanesRoot = NULL;
    xwork_status iStatus;

    if ( !pStore || !pOptions || !pOptions->sRootPath || !pOptions->sRootPath[0] || !pBackend ) {
        return XWORK_ERROR_INVALID_ARGUMENT;
    }

    xwork_file_persistence_reset(pStore);
    xwork_persistence_backend_init(pBackend);

    pStore->sRootPath = xwork__dup_cstr(pOptions->sRootPath);
    if ( !pStore->sRootPath ) {
        return XWORK_ERROR_NO_MEMORY;
    }

    iStatus = xwork__ensure_directory_tree(pStore->sRootPath);
    if ( iStatus != XWORK_OK ) {
        xwork_file_persistence_reset(pStore);
        return iStatus;
    }

    sRunsRoot = xwork__dup_printf("%s/runs", pStore->sRootPath);
    if ( !sRunsRoot ) {
        xwork_file_persistence_reset(pStore);
        return XWORK_ERROR_NO_MEMORY;
    }

    iStatus = xwork__ensure_directory_tree(sRunsRoot);
    free(sRunsRoot);
    if ( iStatus != XWORK_OK ) {
        xwork_file_persistence_reset(pStore);
        return iStatus;
    }

    sTaskGraphsRoot = xwork__dup_printf("%s/task_graphs", pStore->sRootPath);
    if ( !sTaskGraphsRoot ) {
        xwork_file_persistence_reset(pStore);
        return XWORK_ERROR_NO_MEMORY;
    }
    iStatus = xwork__ensure_directory_tree(sTaskGraphsRoot);
    free(sTaskGraphsRoot);
    if ( iStatus != XWORK_OK ) {
        xwork_file_persistence_reset(pStore);
        return iStatus;
    }

    sAgentPoolsRoot = xwork__dup_printf("%s/agent_pools", pStore->sRootPath);
    if ( !sAgentPoolsRoot ) {
        xwork_file_persistence_reset(pStore);
        return XWORK_ERROR_NO_MEMORY;
    }
    iStatus = xwork__ensure_directory_tree(sAgentPoolsRoot);
    free(sAgentPoolsRoot);
    if ( iStatus != XWORK_OK ) {
        xwork_file_persistence_reset(pStore);
        return iStatus;
    }

    sControlPlanesRoot = xwork__dup_printf("%s/control_planes", pStore->sRootPath);
    if ( !sControlPlanesRoot ) {
        xwork_file_persistence_reset(pStore);
        return XWORK_ERROR_NO_MEMORY;
    }
    iStatus = xwork__ensure_directory_tree(sControlPlanesRoot);
    free(sControlPlanesRoot);
    if ( iStatus != XWORK_OK ) {
        xwork_file_persistence_reset(pStore);
        return iStatus;
    }

    pBackend->pfnStoreEvent = xwork__file_persistence_store_event_cb;
    pBackend->pfnStoreCheckpoint = xwork__file_persistence_store_checkpoint_cb;
    pBackend->pfnStoreRunSnapshot = xwork__file_persistence_store_run_snapshot_cb;
    pBackend->pfnStoreArtifact = xwork__file_persistence_store_artifact_cb;
    pBackend->pfnLoadRunSnapshot = xwork__file_persistence_load_run_snapshot_cb;
    pBackend->pfnLoadCheckpointSnapshot = xwork__file_persistence_load_checkpoint_snapshot_cb;
    pBackend->pfnListRuns = xwork__file_persistence_list_runs_cb;
    pBackend->pfnListCheckpoints = xwork__file_persistence_list_checkpoints_cb;
    pBackend->pfnListEvents = xwork__file_persistence_list_events_cb;
    pBackend->pfnListArtifacts = xwork__file_persistence_list_artifacts_cb;
    pBackend->pfnQueryRunIndex = xwork__file_persistence_query_run_index_cb;
    pBackend->pfnLoadRunSummary = xwork__file_persistence_load_run_summary_cb;
    pBackend->pfnLoadLastEvent = xwork__file_persistence_load_last_event_cb;
    pBackend->pfnLoadLastApprovalRequest = xwork__file_persistence_load_last_approval_request_cb;
    pBackend->pfnLoadLastCheckpoint = xwork__file_persistence_load_last_checkpoint_cb;
    pBackend->pfnLoadLastArtifact = xwork__file_persistence_load_last_artifact_cb;
    pBackend->pfnLoadEvent = xwork__file_persistence_load_event_cb;
    pBackend->pfnLoadCheckpoint = xwork__file_persistence_load_checkpoint_cb;
    pBackend->pfnLoadArtifact = xwork__file_persistence_load_artifact_cb;
    pBackend->pUserData = pStore;
    return XWORK_OK;
}

xwork_status xwork_file_persistence_list_runs(
    const xwork_file_persistence *pStore,
    xwork_string_list *pList
)
{
    char *sRunsRoot;
    xwork_status iStatus;

    if ( !pStore || !pStore->sRootPath || !pList ) {
        return XWORK_ERROR_INVALID_ARGUMENT;
    }

    sRunsRoot = xwork__dup_printf("%s/runs", pStore->sRootPath);
    if ( !sRunsRoot ) {
        return XWORK_ERROR_NO_MEMORY;
    }

    iStatus = xwork__list_encoded_entries(sRunsRoot, true, NULL, pList);
    free(sRunsRoot);
    return iStatus;
}

xwork_status xwork_file_persistence_list_run_summaries(
    const xwork_file_persistence *pStore,
    xwork_run_summary_list *pList
)
{
    xwork_string_list tRunIds;
    xwork_run_summary *pItems = NULL;
    xwork_status iStatus = XWORK_OK;
    size_t i;

    if ( !pStore || !pStore->sRootPath || !pList ) {
        return XWORK_ERROR_INVALID_ARGUMENT;
    }

    xwork_run_summary_list_reset(pList);
    xwork_string_list_init(&tRunIds);

    iStatus = xwork_file_persistence_list_runs(pStore, &tRunIds);
    if ( iStatus != XWORK_OK ) {
        goto done;
    }
    if ( tRunIds.iCount == 0u ) {
        goto done;
    }

    pItems = (xwork_run_summary *)calloc(tRunIds.iCount, sizeof(*pItems));
    if ( !pItems ) {
        iStatus = XWORK_ERROR_NO_MEMORY;
        goto done;
    }

    pList->pItems = pItems;
    pList->iCount = tRunIds.iCount;
    for ( i = 0u; i < pList->iCount; ++i ) {
        xwork_run_summary_init(&pItems[i]);
        iStatus = xwork_file_persistence_load_run_summary(
            pStore,
            tRunIds.psItems[i],
            &pItems[i]
        );
        if ( iStatus != XWORK_OK ) {
            goto done;
        }
    }

done:
    xwork_string_list_reset(&tRunIds);
    if ( iStatus != XWORK_OK ) {
        xwork_run_summary_list_reset(pList);
    }
    return iStatus;
}

xwork_status xwork_file_persistence_list_run_index(
    const xwork_file_persistence *pStore,
    xwork_run_index_list *pList
)
{
    xwork_string_list tRunIds;
    xwork_run_index_entry *pItems = NULL;
    xwork_status iStatus = XWORK_OK;
    size_t i;

    if ( !pStore || !pStore->sRootPath || !pList ) {
        return XWORK_ERROR_INVALID_ARGUMENT;
    }

    xwork_run_index_list_reset(pList);
    xwork_string_list_init(&tRunIds);

    iStatus = xwork_file_persistence_list_runs(pStore, &tRunIds);
    if ( iStatus != XWORK_OK ) {
        goto done;
    }
    if ( tRunIds.iCount == 0u ) {
        goto done;
    }

    pItems = (xwork_run_index_entry *)calloc(tRunIds.iCount, sizeof(*pItems));
    if ( !pItems ) {
        iStatus = XWORK_ERROR_NO_MEMORY;
        goto done;
    }

    pList->pItems = pItems;
    pList->iCount = tRunIds.iCount;
    for ( i = 0u; i < pList->iCount; ++i ) {
        xwork_status iLoadStatus;
        xwork_string_list tArtifactIds;
        xwork_string_list tEventIds;
        xwork_string_list tCheckpointIds;

        xwork_run_index_entry_init(&pItems[i]);
        xwork_string_list_init(&tArtifactIds);
        xwork_string_list_init(&tEventIds);
        xwork_string_list_init(&tCheckpointIds);
        iStatus = xwork_file_persistence_load_run_summary(
            pStore,
            tRunIds.psItems[i],
            &pItems[i].tSummary
        );
        if ( iStatus != XWORK_OK ) {
            xwork_string_list_reset(&tCheckpointIds);
            xwork_string_list_reset(&tEventIds);
            xwork_string_list_reset(&tArtifactIds);
            goto done;
        }

        iLoadStatus = xwork_file_persistence_load_last_approval_request(
            pStore,
            tRunIds.psItems[i],
            &pItems[i].tLastApprovalRequest
        );
        if ( iLoadStatus == XWORK_OK ) {
            pItems[i].bHasLastApprovalRequest = true;
        } else if ( iLoadStatus != XWORK_ERROR_NOT_FOUND ) {
            iStatus = iLoadStatus;
            xwork_string_list_reset(&tCheckpointIds);
            xwork_string_list_reset(&tEventIds);
            xwork_string_list_reset(&tArtifactIds);
            goto done;
        }

        iLoadStatus = xwork_file_persistence_list_events(
            pStore,
            tRunIds.psItems[i],
            &tEventIds
        );
        if ( iLoadStatus != XWORK_OK ) {
            iStatus = iLoadStatus;
            xwork_string_list_reset(&tCheckpointIds);
            xwork_string_list_reset(&tEventIds);
            xwork_string_list_reset(&tArtifactIds);
            goto done;
        }
        pItems[i].iEventCount = tEventIds.iCount;

        iLoadStatus = xwork_file_persistence_load_last_event(
            pStore,
            tRunIds.psItems[i],
            &pItems[i].tLastEvent
        );
        if ( iLoadStatus == XWORK_OK ) {
            pItems[i].bHasLastEvent = true;
        } else if ( iLoadStatus != XWORK_ERROR_NOT_FOUND ) {
            iStatus = iLoadStatus;
            xwork_string_list_reset(&tCheckpointIds);
            xwork_string_list_reset(&tEventIds);
            xwork_string_list_reset(&tArtifactIds);
            goto done;
        }

        iLoadStatus = xwork_file_persistence_list_checkpoints(
            pStore,
            tRunIds.psItems[i],
            &tCheckpointIds
        );
        if ( iLoadStatus != XWORK_OK ) {
            iStatus = iLoadStatus;
            xwork_string_list_reset(&tCheckpointIds);
            xwork_string_list_reset(&tEventIds);
            xwork_string_list_reset(&tArtifactIds);
            goto done;
        }
        pItems[i].iCheckpointCount = tCheckpointIds.iCount;

        iLoadStatus = xwork_file_persistence_load_last_checkpoint(
            pStore,
            tRunIds.psItems[i],
            &pItems[i].tLastCheckpoint
        );
        if ( iLoadStatus == XWORK_OK ) {
            pItems[i].bHasLastCheckpoint = true;
        } else if ( iLoadStatus != XWORK_ERROR_NOT_FOUND ) {
            iStatus = iLoadStatus;
            xwork_string_list_reset(&tCheckpointIds);
            xwork_string_list_reset(&tEventIds);
            xwork_string_list_reset(&tArtifactIds);
            goto done;
        }

        iLoadStatus = xwork_file_persistence_list_artifacts(
            pStore,
            tRunIds.psItems[i],
            &tArtifactIds
        );
        if ( iLoadStatus != XWORK_OK ) {
            iStatus = iLoadStatus;
            xwork_string_list_reset(&tCheckpointIds);
            xwork_string_list_reset(&tEventIds);
            xwork_string_list_reset(&tArtifactIds);
            goto done;
        }
        pItems[i].iArtifactCount = tArtifactIds.iCount;

        iLoadStatus = xwork_file_persistence_load_last_artifact(
            pStore,
            tRunIds.psItems[i],
            &pItems[i].tLastArtifact
        );
        if ( iLoadStatus == XWORK_OK ) {
            pItems[i].bHasLastArtifact = true;
        } else if ( iLoadStatus != XWORK_ERROR_NOT_FOUND ) {
            iStatus = iLoadStatus;
            xwork_string_list_reset(&tCheckpointIds);
            xwork_string_list_reset(&tEventIds);
            xwork_string_list_reset(&tArtifactIds);
            goto done;
        }

        xwork_string_list_reset(&tCheckpointIds);
        xwork_string_list_reset(&tEventIds);
        xwork_string_list_reset(&tArtifactIds);
    }

done:
    xwork_string_list_reset(&tRunIds);
    if ( iStatus != XWORK_OK ) {
        xwork_run_index_list_reset(pList);
    }
    return iStatus;
}

xwork_status xwork_file_persistence_query_run_index(
    const xwork_file_persistence *pStore,
    const xwork_run_index_query *pQuery,
    xwork_run_index_list *pList
)
{
    xwork_run_index_entry *pItems;
    size_t i;
    size_t iWriteIndex = 0u;
    size_t iOriginalCount;
    size_t iStartIndex = 0u;
    size_t iReturnedCount;
    xwork_status iStatus;

    if ( !pStore || !pStore->sRootPath || !pList ) {
        return XWORK_ERROR_INVALID_ARGUMENT;
    }

    iStatus = xwork_file_persistence_list_run_index(pStore, pList);
    if ( iStatus != XWORK_OK || !pQuery || pList->iCount == 0u ) {
        return iStatus;
    }

    pItems = (xwork_run_index_entry *)pList->pItems;
    iOriginalCount = pList->iCount;
    for ( i = 0u; i < iOriginalCount; ++i ) {
        if ( !xwork__run_index_matches_query(&pItems[i], pQuery) ) {
            xwork_run_index_entry_reset(&pItems[i]);
            continue;
        }

        if ( iWriteIndex != i ) {
            pItems[iWriteIndex] = pItems[i];
            xwork_run_index_entry_init(&pItems[i]);
        }
        ++iWriteIndex;
    }

    for ( i = iWriteIndex; i < iOriginalCount; ++i ) {
        xwork_run_index_entry_reset(&pItems[i]);
    }
    pList->iCount = iWriteIndex;

    if ( iWriteIndex == 0u ) {
        free(pItems);
        pList->pItems = NULL;
        return XWORK_OK;
    }

    pItems = (xwork_run_index_entry *)realloc(
        pItems,
        iWriteIndex * sizeof(*pItems)
    );
    if ( pItems ) {
        pList->pItems = pItems;
    }

    xwork__run_index_sort_items(
        (xwork_run_index_entry *)pList->pItems,
        pList->iCount,
        pQuery->eSort
    );

    pItems = (xwork_run_index_entry *)pList->pItems;
    if ( pQuery->sAfterRunId && pQuery->sAfterRunId[0] ) {
        bool bFoundAfter = false;

        for ( i = 0u; i < pList->iCount; ++i ) {
            if ( pItems[i].tSummary.sRunId &&
                 strcmp(pItems[i].tSummary.sRunId, pQuery->sAfterRunId) == 0 ) {
                iStartIndex = i + 1u;
                bFoundAfter = true;
                break;
            }
        }
        if ( !bFoundAfter ) {
            iStartIndex = pList->iCount;
        }
    }

    if ( iStartIndex >= pList->iCount ) {
        for ( i = 0u; i < pList->iCount; ++i ) {
            xwork_run_index_entry_reset(&pItems[i]);
        }
        free(pItems);
        pList->pItems = NULL;
        pList->iCount = 0u;
        pList->bHasMore = false;
        return XWORK_OK;
    }

    iReturnedCount = pList->iCount - iStartIndex;
    if ( pQuery->iLimit > 0u && iReturnedCount > pQuery->iLimit ) {
        iReturnedCount = pQuery->iLimit;
        pList->bHasMore = true;
    }
    if ( pList->bHasMore ) {
        const char *sNextAfterRunId =
            pItems[iStartIndex + iReturnedCount - 1u].tSummary.sRunId;

        pList->sNextAfterRunId = xwork__dup_cstr(sNextAfterRunId);
        if ( !pList->sNextAfterRunId ) {
            xwork_run_index_list_reset(pList);
            return XWORK_ERROR_NO_MEMORY;
        }
    }

    if ( iStartIndex > 0u || iReturnedCount < pList->iCount ) {
        for ( i = 0u; i < iReturnedCount; ++i ) {
            size_t iSourceIndex = iStartIndex + i;

            if ( i != iSourceIndex ) {
                xwork_run_index_entry_reset(&pItems[i]);
                pItems[i] = pItems[iSourceIndex];
                xwork_run_index_entry_init(&pItems[iSourceIndex]);
            }
        }
        for ( i = iReturnedCount; i < pList->iCount; ++i ) {
            xwork_run_index_entry_reset(&pItems[i]);
        }
        pList->iCount = iReturnedCount;
        pItems = (xwork_run_index_entry *)realloc(
            pItems,
            iReturnedCount * sizeof(*pItems)
        );
        if ( pItems ) {
            pList->pItems = pItems;
        }
    }
    return XWORK_OK;
}

xwork_status xwork_file_persistence_list_checkpoints(
    const xwork_file_persistence *pStore,
    const char *sRunId,
    xwork_string_list *pList
)
{
    char *sRunDir;
    char *sCheckpointsDir;
    xwork_status iStatus;

    if ( !pStore || !pStore->sRootPath || !sRunId || !sRunId[0] || !pList ) {
        return XWORK_ERROR_INVALID_ARGUMENT;
    }

    sRunDir = xwork__build_run_dir_path(pStore, sRunId);
    if ( !sRunDir ) {
        return XWORK_ERROR_NO_MEMORY;
    }

    sCheckpointsDir = xwork__dup_printf("%s/checkpoints", sRunDir);
    free(sRunDir);
    if ( !sCheckpointsDir ) {
        return XWORK_ERROR_NO_MEMORY;
    }

    iStatus = xwork__list_encoded_entries(
        sCheckpointsDir,
        false,
        ".snapshot",
        pList
    );
    free(sCheckpointsDir);
    return iStatus;
}

xwork_status xwork_file_persistence_list_events(
    const xwork_file_persistence *pStore,
    const char *sRunId,
    xwork_string_list *pList
)
{
    char *sRunDir;
    char *sEventsPath;
    FILE *pFile = NULL;
    char *sLine = NULL;
    xwork_event tEvent;
    xwork_status iStatus = XWORK_OK;

    if ( !pStore || !pStore->sRootPath || !sRunId || !sRunId[0] || !pList ) {
        return XWORK_ERROR_INVALID_ARGUMENT;
    }

    xwork_string_list_reset(pList);
    xwork_event_init(&tEvent);

    sRunDir = xwork__build_run_dir_path(pStore, sRunId);
    if ( !sRunDir ) {
        return XWORK_ERROR_NO_MEMORY;
    }

    sEventsPath = xwork__build_events_log_path(sRunDir);
    free(sRunDir);
    if ( !sEventsPath ) {
        return XWORK_ERROR_NO_MEMORY;
    }

    pFile = fopen(sEventsPath, "rb");
    free(sEventsPath);
    if ( !pFile ) {
        return errno == ENOENT ? XWORK_OK : XWORK_ERROR_EXTERNAL_FAILURE;
    }

    for ( ;; ) {
        bool bIsHeader = false;

        iStatus = xwork__read_line_owned(pFile, &sLine);
        if ( iStatus == XWORK_ERROR_NOT_FOUND ) {
            iStatus = XWORK_OK;
            break;
        }
        if ( iStatus != XWORK_OK ) {
            break;
        }
        if ( sLine[0] == '\0' ) {
            free(sLine);
            sLine = NULL;
            continue;
        }

        iStatus = xwork__check_event_log_line_header(sLine, &bIsHeader);
        if ( iStatus != XWORK_OK ) {
            free(sLine);
            sLine = NULL;
            break;
        }
        if ( bIsHeader ) {
            free(sLine);
            sLine = NULL;
            continue;
        }

        iStatus = xwork__parse_event_line(sRunId, sLine, &tEvent);
        free(sLine);
        sLine = NULL;
        if ( iStatus != XWORK_OK ) {
            break;
        }
        if ( !tEvent.sEventId ) {
            iStatus = XWORK_ERROR_EXTERNAL_FAILURE;
            break;
        }
        {
            char *sEventIdCopy = xwork__dup_cstr(tEvent.sEventId);
            if ( !sEventIdCopy ) {
                iStatus = XWORK_ERROR_NO_MEMORY;
            } else {
                iStatus = xwork__string_list_append_owned(pList, sEventIdCopy);
            }
        }
        xwork_event_reset(&tEvent);
        if ( iStatus != XWORK_OK ) {
            break;
        }
    }

    free(sLine);
    xwork_event_reset(&tEvent);
    if ( fclose(pFile) != 0 && iStatus == XWORK_OK ) {
        iStatus = XWORK_ERROR_EXTERNAL_FAILURE;
    }
    if ( iStatus != XWORK_OK ) {
        xwork_string_list_reset(pList);
    }
    return iStatus;
}

xwork_status xwork_file_persistence_list_artifacts(
    const xwork_file_persistence *pStore,
    const char *sRunId,
    xwork_string_list *pList
)
{
    char *sRunDir;
    char *sArtifactsDir;
    xwork_status iStatus;

    if ( !pStore || !pStore->sRootPath || !sRunId || !sRunId[0] || !pList ) {
        return XWORK_ERROR_INVALID_ARGUMENT;
    }

    sRunDir = xwork__build_run_dir_path(pStore, sRunId);
    if ( !sRunDir ) {
        return XWORK_ERROR_NO_MEMORY;
    }

    sArtifactsDir = xwork__dup_printf("%s/artifacts", sRunDir);
    free(sRunDir);
    if ( !sArtifactsDir ) {
        return XWORK_ERROR_NO_MEMORY;
    }

    iStatus = xwork__list_encoded_entries(
        sArtifactsDir,
        false,
        ".meta",
        pList
    );
    free(sArtifactsDir);
    return iStatus;
}

static bool xwork__persistence_artifact_summary_has_prefix(const char *sText, const char *sPrefix)
{
    size_t iPrefixLen;

    if ( !sPrefix || !sPrefix[0] ) {
        return true;
    }
    if ( !sText ) {
        return false;
    }
    iPrefixLen = strlen(sPrefix);
    return strncmp(sText, sPrefix, iPrefixLen) == 0;
}

static bool xwork__persistence_artifact_summary_matches_query(
    const xwork_artifact_summary *pSummary,
    const xwork_artifact_summary_query *pQuery
)
{
    if ( !pSummary || !pQuery ) {
        return true;
    }
    if ( pQuery->bHasKind && pSummary->eKind != pQuery->eKind ) {
        return false;
    }
    if ( pQuery->bHasOutputClass && pSummary->eOutputClass != pQuery->eOutputClass ) {
        return false;
    }
    if ( pQuery->sOutputRole && pQuery->sOutputRole[0] ) {
        if ( !pSummary->sOutputRole || strcmp(pSummary->sOutputRole, pQuery->sOutputRole) != 0 ) {
            return false;
        }
    }
    if ( !xwork__persistence_artifact_summary_has_prefix(pSummary->sOutputRole, pQuery->sOutputRolePrefix) ) {
        return false;
    }
    if ( pQuery->bHasReportClass && pSummary->eReportClass != pQuery->eReportClass ) {
        return false;
    }
    if ( pQuery->sReportSubjectRef && pQuery->sReportSubjectRef[0] ) {
        if ( !pSummary->sReportSubjectRef ||
             strcmp(pSummary->sReportSubjectRef, pQuery->sReportSubjectRef) != 0 ) {
            return false;
        }
    }
    if ( !xwork__persistence_artifact_summary_has_prefix(
             pSummary->sReportSubjectRef,
             pQuery->sReportSubjectRefPrefix
         ) ) {
        return false;
    }
    if ( pQuery->sArtifactName && pQuery->sArtifactName[0] ) {
        if ( !pSummary->sName || strcmp(pSummary->sName, pQuery->sArtifactName) != 0 ) {
            return false;
        }
    }
    if ( !xwork__persistence_artifact_summary_has_prefix(pSummary->sName, pQuery->sNamePrefix) ) {
        return false;
    }
    if ( pQuery->sMimeType && pQuery->sMimeType[0] ) {
        if ( !pSummary->sMimeType || strcmp(pSummary->sMimeType, pQuery->sMimeType) != 0 ) {
            return false;
        }
    }
    if ( !xwork__persistence_artifact_summary_has_prefix(pSummary->sMimeType, pQuery->sMimeTypePrefix) ) {
        return false;
    }
    if ( pQuery->sStorageRef && pQuery->sStorageRef[0] ) {
        if ( !pSummary->sStorageRef || strcmp(pSummary->sStorageRef, pQuery->sStorageRef) != 0 ) {
            return false;
        }
    }
    if ( !xwork__persistence_artifact_summary_has_prefix(pSummary->sStorageRef, pQuery->sStorageRefPrefix) ) {
        return false;
    }
    if ( pQuery->bRequireExitCode && !pSummary->bHasExitCode ) {
        return false;
    }
    if ( pQuery->bHasExitCodeValue ) {
        if ( !pSummary->bHasExitCode || pSummary->iExitCode != pQuery->iExitCode ) {
            return false;
        }
    }
    if ( pQuery->bHasAfterSequence && pSummary->iSequence <= pQuery->iAfterSequence ) {
        return false;
    }
    if ( pQuery->bHasMinSequence && pSummary->iSequence < pQuery->iMinSequence ) {
        return false;
    }
    if ( pQuery->bHasMaxSequence && pSummary->iSequence > pQuery->iMaxSequence ) {
        return false;
    }
    return true;
}

static xwork_status xwork__persistence_artifact_summary_copy(
    xwork_artifact_summary *pDst,
    const xwork_artifact_summary *pSrc
)
{
    xwork_status iStatus;

    if ( !pDst || !pSrc ) {
        return XWORK_ERROR_INVALID_ARGUMENT;
    }

    xwork_artifact_summary_reset(pDst);
    iStatus = xwork__replace_cstr((char **)&pDst->sArtifactId, pSrc->sArtifactId);
    if ( iStatus != XWORK_OK ) return iStatus;
    iStatus = xwork__replace_cstr((char **)&pDst->sName, pSrc->sName);
    if ( iStatus != XWORK_OK ) return iStatus;
    iStatus = xwork__replace_cstr((char **)&pDst->sMimeType, pSrc->sMimeType);
    if ( iStatus != XWORK_OK ) return iStatus;
    iStatus = xwork__replace_cstr((char **)&pDst->sStorageRef, pSrc->sStorageRef);
    if ( iStatus != XWORK_OK ) return iStatus;
    iStatus = xwork__replace_cstr((char **)&pDst->sSummary, pSrc->sSummary);
    if ( iStatus != XWORK_OK ) return iStatus;
    iStatus = xwork__replace_cstr((char **)&pDst->sOutputRole, pSrc->sOutputRole);
    if ( iStatus != XWORK_OK ) return iStatus;
    iStatus = xwork__replace_cstr((char **)&pDst->sReportSubjectRef, pSrc->sReportSubjectRef);
    if ( iStatus != XWORK_OK ) return iStatus;
    iStatus = xwork__replace_cstr((char **)&pDst->sPatchApplyResultJson, pSrc->sPatchApplyResultJson);
    if ( iStatus != XWORK_OK ) return iStatus;
    iStatus = xwork__replace_cstr((char **)&pDst->sPatchFileSummaryJson, pSrc->sPatchFileSummaryJson);
    if ( iStatus != XWORK_OK ) return iStatus;
    pDst->eKind = pSrc->eKind;
    pDst->eOutputClass = pSrc->eOutputClass;
    pDst->eReportClass = pSrc->eReportClass;
    pDst->bHasContentStats = pSrc->bHasContentStats;
    pDst->iContentByteCount = pSrc->iContentByteCount;
    pDst->iContentLineCount = pSrc->iContentLineCount;
    pDst->bHasPatchStats = pSrc->bHasPatchStats;
    pDst->iPatchFileCount = pSrc->iPatchFileCount;
    pDst->iPatchHunkCount = pSrc->iPatchHunkCount;
    pDst->iPatchAddedLineCount = pSrc->iPatchAddedLineCount;
    pDst->iPatchDeletedLineCount = pSrc->iPatchDeletedLineCount;
    pDst->bHasCommandIoStats = pSrc->bHasCommandIoStats;
    pDst->iStdoutByteCount = pSrc->iStdoutByteCount;
    pDst->iStderrByteCount = pSrc->iStderrByteCount;
    pDst->bStdoutTruncated = pSrc->bStdoutTruncated;
    pDst->bStderrTruncated = pSrc->bStderrTruncated;
    pDst->bHasExitCode = pSrc->bHasExitCode;
    pDst->iExitCode = pSrc->iExitCode;
    pDst->iSequence = pSrc->iSequence;
    return XWORK_OK;
}

static void xwork__persistence_artifact_summary_sort_by_sequence(
    xwork_artifact_summary *pItems,
    size_t iCount
)
{
    size_t i;

    if ( !pItems || iCount < 2u ) {
        return;
    }

    for ( i = 1u; i < iCount; ++i ) {
        size_t j = i;
        xwork_artifact_summary tValue = pItems[i];

        while ( j > 0u && tValue.iSequence < pItems[j - 1u].iSequence ) {
            pItems[j] = pItems[j - 1u];
            --j;
        }
        pItems[j] = tValue;
    }
}

xwork_status xwork_file_persistence_list_artifact_summaries(
    const xwork_file_persistence *pStore,
    const char *sRunId,
    xwork_artifact_summary_list *pList
)
{
    xwork_string_list tArtifactIds;
    xwork_artifact_summary *pItems = NULL;
    xwork_artifact tArtifact;
    xwork_status iStatus = XWORK_OK;
    size_t i;

    if ( !pStore || !pStore->sRootPath || !sRunId || !sRunId[0] || !pList ) {
        return XWORK_ERROR_INVALID_ARGUMENT;
    }

    xwork_artifact_summary_list_reset(pList);
    xwork_string_list_init(&tArtifactIds);
    xwork_artifact_init(&tArtifact);

    iStatus = xwork_file_persistence_list_artifacts(pStore, sRunId, &tArtifactIds);
    if ( iStatus != XWORK_OK ) {
        goto done;
    }
    if ( tArtifactIds.iCount == 0u ) {
        goto done;
    }

    pItems = (xwork_artifact_summary *)calloc(tArtifactIds.iCount, sizeof(*pItems));
    if ( !pItems ) {
        iStatus = XWORK_ERROR_NO_MEMORY;
        goto done;
    }

    pList->pItems = pItems;
    pList->iCount = tArtifactIds.iCount;
    for ( i = 0u; i < pList->iCount; ++i ) {
        xwork_artifact_summary_init(&pItems[i]);
        iStatus = xwork_file_persistence_load_artifact(
            pStore,
            sRunId,
            tArtifactIds.psItems[i],
            &tArtifact
        );
        if ( iStatus != XWORK_OK ) {
            goto done;
        }

        iStatus = xwork__replace_cstr((char **)&pItems[i].sArtifactId, tArtifact.sArtifactId);
        if ( iStatus != XWORK_OK ) goto done;
        iStatus = xwork__replace_cstr((char **)&pItems[i].sName, tArtifact.sName);
        if ( iStatus != XWORK_OK ) goto done;
        iStatus = xwork__replace_cstr((char **)&pItems[i].sMimeType, tArtifact.sMimeType);
        if ( iStatus != XWORK_OK ) goto done;
        iStatus = xwork__replace_cstr((char **)&pItems[i].sStorageRef, tArtifact.sStorageRef);
        if ( iStatus != XWORK_OK ) goto done;
        iStatus = xwork__replace_cstr((char **)&pItems[i].sSummary, tArtifact.sSummary);
        if ( iStatus != XWORK_OK ) goto done;
        iStatus = xwork__replace_cstr((char **)&pItems[i].sOutputRole, tArtifact.sOutputRole);
        if ( iStatus != XWORK_OK ) goto done;
        iStatus = xwork__replace_cstr((char **)&pItems[i].sReportSubjectRef, tArtifact.sReportSubjectRef);
        if ( iStatus != XWORK_OK ) goto done;
        iStatus = xwork__replace_cstr(
            (char **)&pItems[i].sPatchApplyResultJson,
            tArtifact.sPatchApplyResultJson
        );
        if ( iStatus != XWORK_OK ) goto done;
        iStatus = xwork__replace_cstr(
            (char **)&pItems[i].sPatchFileSummaryJson,
            tArtifact.sPatchFileSummaryJson
        );
        if ( iStatus != XWORK_OK ) goto done;
        pItems[i].eKind = tArtifact.eKind;
        pItems[i].eOutputClass = tArtifact.eOutputClass;
        pItems[i].eReportClass = tArtifact.eReportClass;
        pItems[i].bHasContentStats = tArtifact.bHasContentStats;
        pItems[i].iContentByteCount = tArtifact.iContentByteCount;
        pItems[i].iContentLineCount = tArtifact.iContentLineCount;
        pItems[i].bHasPatchStats = tArtifact.bHasPatchStats;
        pItems[i].iPatchFileCount = tArtifact.iPatchFileCount;
        pItems[i].iPatchHunkCount = tArtifact.iPatchHunkCount;
        pItems[i].iPatchAddedLineCount = tArtifact.iPatchAddedLineCount;
        pItems[i].iPatchDeletedLineCount = tArtifact.iPatchDeletedLineCount;
        pItems[i].bHasCommandIoStats = tArtifact.bHasCommandIoStats;
        pItems[i].iStdoutByteCount = tArtifact.iStdoutByteCount;
        pItems[i].iStderrByteCount = tArtifact.iStderrByteCount;
        pItems[i].bStdoutTruncated = tArtifact.bStdoutTruncated;
        pItems[i].bStderrTruncated = tArtifact.bStderrTruncated;
        pItems[i].bHasExitCode = tArtifact.bHasExitCode;
        pItems[i].iExitCode = tArtifact.iExitCode;
        pItems[i].iSequence = tArtifact.iSequence;
        xwork_artifact_reset(&tArtifact);
    }

done:
    xwork_artifact_reset(&tArtifact);
    xwork_string_list_reset(&tArtifactIds);
    if ( iStatus != XWORK_OK ) {
        xwork_artifact_summary_list_reset(pList);
    }
    return iStatus;
}

xwork_status xwork_file_persistence_query_artifact_summaries(
    const xwork_file_persistence *pStore,
    const char *sRunId,
    const xwork_artifact_summary_query *pQuery,
    xwork_artifact_summary_list *pList
)
{
    xwork_artifact_summary_list tAllSummaries;
    xwork_artifact_summary *pItems = NULL;
    xwork_status iStatus = XWORK_OK;
    size_t iMatchCount = 0u;
    size_t iReturnedCount = 0u;
    size_t iWriteIndex = 0u;
    size_t i;

    if ( !pStore || !pStore->sRootPath || !sRunId || !sRunId[0] || !pList ) {
        return XWORK_ERROR_INVALID_ARGUMENT;
    }
    if ( !pQuery ) {
        return xwork_file_persistence_list_artifact_summaries(pStore, sRunId, pList);
    }

    xwork_artifact_summary_list_init(&tAllSummaries);
    xwork_artifact_summary_list_reset(pList);

    iStatus = xwork_file_persistence_list_artifact_summaries(
        pStore,
        sRunId,
        &tAllSummaries
    );
    if ( iStatus != XWORK_OK ) {
        goto done;
    }
    xwork__persistence_artifact_summary_sort_by_sequence(
        (xwork_artifact_summary *)tAllSummaries.pItems,
        tAllSummaries.iCount
    );

    for ( i = 0u; i < tAllSummaries.iCount; ++i ) {
        if ( xwork__persistence_artifact_summary_matches_query(&tAllSummaries.pItems[i], pQuery) ) {
            iMatchCount++;
        }
    }
    if ( iMatchCount == 0u ) {
        goto done;
    }

    iReturnedCount = iMatchCount;
    if ( pQuery->iLimit > 0u && iReturnedCount > pQuery->iLimit ) {
        iReturnedCount = pQuery->iLimit;
        pList->bHasMore = true;
    }

    pItems = (xwork_artifact_summary *)calloc(iReturnedCount, sizeof(*pItems));
    if ( !pItems ) {
        iStatus = XWORK_ERROR_NO_MEMORY;
        goto done;
    }

    pList->pItems = pItems;
    pList->iCount = iReturnedCount;
    for ( i = 0u; i < pList->iCount; ++i ) {
        xwork_artifact_summary_init(&pItems[i]);
    }

    for ( i = 0u; i < tAllSummaries.iCount; ++i ) {
        if ( !xwork__persistence_artifact_summary_matches_query(&tAllSummaries.pItems[i], pQuery) ) {
            continue;
        }
        iStatus = xwork__persistence_artifact_summary_copy(
            &pItems[iWriteIndex],
            &tAllSummaries.pItems[i]
        );
        if ( iStatus != XWORK_OK ) {
            goto done;
        }
        iWriteIndex++;
        if ( iWriteIndex >= iReturnedCount ) {
            break;
        }
    }
    if ( pList->iCount > 0u ) {
        pList->iNextAfterSequence = pItems[pList->iCount - 1u].iSequence;
    }

done:
    xwork_artifact_summary_list_reset(&tAllSummaries);
    if ( iStatus != XWORK_OK ) {
        xwork_artifact_summary_list_reset(pList);
    }
    return iStatus;
}

xwork_status xwork_file_persistence_query_run_steps(
    const xwork_file_persistence *pStore,
    const char *sRunId,
    const xwork_run_step_query *pQuery,
    xwork_run_step_list *pList
)
{
    xwork_string_list tEventIds;
    xwork_event tEvent;
    xwork_checkpoint tCheckpoint;
    xwork_run_step tStep;
    xwork_status iStatus = XWORK_OK;
    size_t i;

    if ( !pStore || !pStore->sRootPath || !sRunId || !sRunId[0] || !pList ) {
        return XWORK_ERROR_INVALID_ARGUMENT;
    }

    xwork_string_list_init(&tEventIds);
    xwork_event_init(&tEvent);
    xwork_checkpoint_init(&tCheckpoint);
    xwork_run_step_init(&tStep);
    xwork_run_step_list_reset(pList);

    iStatus = xwork_file_persistence_list_events(pStore, sRunId, &tEventIds);
    if ( iStatus != XWORK_OK ) {
        goto done;
    }

    for ( i = 0u; i < tEventIds.iCount; ++i ) {
        const xwork_checkpoint *pCheckpoint = NULL;

        xwork_event_reset(&tEvent);
        iStatus = xwork_file_persistence_load_event(
            pStore,
            sRunId,
            tEventIds.psItems[i],
            &tEvent
        );
        if ( iStatus != XWORK_OK ) {
            goto done;
        }
        if ( tEvent.sCheckpointId && tEvent.sCheckpointId[0] ) {
            xwork_checkpoint_reset(&tCheckpoint);
            if ( xwork_file_persistence_load_checkpoint(
                     pStore,
                     sRunId,
                     tEvent.sCheckpointId,
                     &tCheckpoint
                 ) == XWORK_OK ) {
                pCheckpoint = &tCheckpoint;
            }
        }

        xwork_run_step_reset(&tStep);
        iStatus = xwork__run_step_from_event(&tStep, &tEvent, pCheckpoint);
        if ( iStatus != XWORK_OK ) {
            goto done;
        }
        if ( !xwork__run_step_matches_query(&tStep, pQuery) ) {
            continue;
        }
        if ( pQuery && pQuery->iLimit > 0u && pList->iCount >= pQuery->iLimit ) {
            pList->bHasMore = true;
            break;
        }
        iStatus = xwork__run_step_list_append(pList, &tStep);
        if ( iStatus != XWORK_OK ) {
            goto done;
        }
    }

done:
    xwork_run_step_reset(&tStep);
    xwork_checkpoint_reset(&tCheckpoint);
    xwork_event_reset(&tEvent);
    xwork_string_list_reset(&tEventIds);
    if ( iStatus != XWORK_OK ) {
        xwork_run_step_list_reset(pList);
    }
    return iStatus;
}

xwork_status xwork_file_persistence_load_artifact(
    const xwork_file_persistence *pStore,
    const char *sRunId,
    const char *sArtifactId,
    xwork_artifact *pArtifact
)
{
    char *sRunDir;
    char *sArtifactsDir;
    char *sArtifactPath;
    xwork_status iStatus;

    if ( !pStore || !pStore->sRootPath || !sRunId || !sRunId[0] ||
         !sArtifactId || !sArtifactId[0] || !pArtifact ) {
        return XWORK_ERROR_INVALID_ARGUMENT;
    }

    sRunDir = xwork__build_run_dir_path(pStore, sRunId);
    if ( !sRunDir ) {
        return XWORK_ERROR_NO_MEMORY;
    }

    sArtifactsDir = xwork__dup_printf("%s/artifacts", sRunDir);
    free(sRunDir);
    if ( !sArtifactsDir ) {
        return XWORK_ERROR_NO_MEMORY;
    }

    sArtifactPath = xwork__build_artifact_file_path(sArtifactsDir, sArtifactId);
    free(sArtifactsDir);
    if ( !sArtifactPath ) {
        return XWORK_ERROR_NO_MEMORY;
    }

    iStatus = xwork__read_artifact_file(sArtifactPath, pArtifact);
    free(sArtifactPath);
    if ( iStatus == XWORK_OK &&
         pArtifact->sRunId &&
         strcmp(pArtifact->sRunId, sRunId) != 0 ) {
        xwork_artifact_reset(pArtifact);
        return XWORK_ERROR_EXTERNAL_FAILURE;
    }
    return iStatus;
}

xwork_status xwork_file_persistence_load_last_artifact(
    const xwork_file_persistence *pStore,
    const char *sRunId,
    xwork_artifact *pArtifact
)
{
    xwork_string_list tArtifactIds;
    xwork_artifact tCurrentArtifact;
    xwork_status iStatus = XWORK_OK;
    bool bHasArtifact = false;
    size_t i;

    if ( !pStore || !pStore->sRootPath || !sRunId || !sRunId[0] || !pArtifact ) {
        return XWORK_ERROR_INVALID_ARGUMENT;
    }

    xwork_string_list_init(&tArtifactIds);
    xwork_artifact_init(&tCurrentArtifact);
    xwork_artifact_reset(pArtifact);

    iStatus = xwork_file_persistence_list_artifacts(pStore, sRunId, &tArtifactIds);
    if ( iStatus != XWORK_OK ) {
        goto done;
    }
    if ( tArtifactIds.iCount == 0u ) {
        iStatus = XWORK_ERROR_NOT_FOUND;
        goto done;
    }

    for ( i = 0u; i < tArtifactIds.iCount; ++i ) {
        iStatus = xwork_file_persistence_load_artifact(
            pStore,
            sRunId,
            tArtifactIds.psItems[i],
            &tCurrentArtifact
        );
        if ( iStatus != XWORK_OK ) {
            goto done;
        }

        if ( !bHasArtifact || tCurrentArtifact.iSequence > pArtifact->iSequence ) {
            xwork_artifact_reset(pArtifact);
            *pArtifact = tCurrentArtifact;
            xwork_artifact_init(&tCurrentArtifact);
            bHasArtifact = true;
        } else {
            xwork_artifact_reset(&tCurrentArtifact);
        }
    }

    if ( !bHasArtifact ) {
        iStatus = XWORK_ERROR_NOT_FOUND;
    }

done:
    xwork_artifact_reset(&tCurrentArtifact);
    xwork_string_list_reset(&tArtifactIds);
    if ( iStatus != XWORK_OK ) {
        xwork_artifact_reset(pArtifact);
    }
    return iStatus;
}

xwork_status xwork_file_persistence_find_artifact_by_name(
    const xwork_file_persistence *pStore,
    const char *sRunId,
    const char *sArtifactName,
    xwork_artifact *pArtifact
)
{
    xwork_string_list tArtifactIds;
    xwork_artifact tCurrentArtifact;
    xwork_status iStatus = XWORK_OK;
    size_t i;

    if ( !pStore || !pStore->sRootPath || !sRunId || !sRunId[0] ||
         !sArtifactName || !sArtifactName[0] || !pArtifact ) {
        return XWORK_ERROR_INVALID_ARGUMENT;
    }

    xwork_string_list_init(&tArtifactIds);
    xwork_artifact_init(&tCurrentArtifact);
    xwork_artifact_reset(pArtifact);

    iStatus = xwork_file_persistence_list_artifacts(pStore, sRunId, &tArtifactIds);
    if ( iStatus != XWORK_OK ) {
        goto done;
    }

    for ( i = 0u; i < tArtifactIds.iCount; ++i ) {
        iStatus = xwork_file_persistence_load_artifact(
            pStore,
            sRunId,
            tArtifactIds.psItems[i],
            &tCurrentArtifact
        );
        if ( iStatus != XWORK_OK ) {
            goto done;
        }

        if ( tCurrentArtifact.sName &&
             strcmp(tCurrentArtifact.sName, sArtifactName) == 0 ) {
            *pArtifact = tCurrentArtifact;
            xwork_artifact_init(&tCurrentArtifact);
            iStatus = XWORK_OK;
            goto done;
        }

        xwork_artifact_reset(&tCurrentArtifact);
    }

    iStatus = XWORK_ERROR_NOT_FOUND;

done:
    xwork_artifact_reset(&tCurrentArtifact);
    xwork_string_list_reset(&tArtifactIds);
    if ( iStatus != XWORK_OK ) {
        xwork_artifact_reset(pArtifact);
    }
    return iStatus;
}

xwork_status xwork_file_persistence_load_event(
    const xwork_file_persistence *pStore,
    const char *sRunId,
    const char *sEventId,
    xwork_event *pEvent
)
{
    char *sRunDir;
    char *sEventsPath;
    FILE *pFile = NULL;
    char *sLine = NULL;
    xwork_status iStatus = XWORK_OK;

    if ( !pStore || !pStore->sRootPath || !sRunId || !sRunId[0] ||
         !sEventId || !sEventId[0] || !pEvent ) {
        return XWORK_ERROR_INVALID_ARGUMENT;
    }

    xwork_event_reset(pEvent);

    sRunDir = xwork__build_run_dir_path(pStore, sRunId);
    if ( !sRunDir ) {
        return XWORK_ERROR_NO_MEMORY;
    }

    sEventsPath = xwork__build_events_log_path(sRunDir);
    free(sRunDir);
    if ( !sEventsPath ) {
        return XWORK_ERROR_NO_MEMORY;
    }

    pFile = fopen(sEventsPath, "rb");
    free(sEventsPath);
    if ( !pFile ) {
        return errno == ENOENT ? XWORK_ERROR_NOT_FOUND : XWORK_ERROR_EXTERNAL_FAILURE;
    }

    for ( ;; ) {
        bool bIsHeader = false;

        iStatus = xwork__read_line_owned(pFile, &sLine);
        if ( iStatus == XWORK_ERROR_NOT_FOUND ) {
            iStatus = XWORK_ERROR_NOT_FOUND;
            break;
        }
        if ( iStatus != XWORK_OK ) {
            break;
        }
        if ( sLine[0] == '\0' ) {
            free(sLine);
            sLine = NULL;
            continue;
        }

        iStatus = xwork__check_event_log_line_header(sLine, &bIsHeader);
        if ( iStatus != XWORK_OK ) {
            free(sLine);
            sLine = NULL;
            break;
        }
        if ( bIsHeader ) {
            free(sLine);
            sLine = NULL;
            continue;
        }

        iStatus = xwork__parse_event_line(sRunId, sLine, pEvent);
        free(sLine);
        sLine = NULL;
        if ( iStatus != XWORK_OK ) {
            break;
        }
        if ( pEvent->sEventId && strcmp(pEvent->sEventId, sEventId) == 0 ) {
            iStatus = XWORK_OK;
            break;
        }

        xwork_event_reset(pEvent);
    }

    free(sLine);
    if ( fclose(pFile) != 0 && iStatus == XWORK_OK ) {
        iStatus = XWORK_ERROR_EXTERNAL_FAILURE;
    }
    if ( iStatus != XWORK_OK ) {
        xwork_event_reset(pEvent);
    }
    return iStatus;
}

xwork_status xwork_file_persistence_load_last_event(
    const xwork_file_persistence *pStore,
    const char *sRunId,
    xwork_event *pEvent
)
{
    char *sRunDir;
    char *sEventsPath;
    FILE *pFile = NULL;
    char *sLine = NULL;
    xwork_status iStatus = XWORK_OK;
    bool bHasEvent = false;

    if ( !pStore || !pStore->sRootPath || !sRunId || !sRunId[0] || !pEvent ) {
        return XWORK_ERROR_INVALID_ARGUMENT;
    }

    xwork_event_reset(pEvent);

    sRunDir = xwork__build_run_dir_path(pStore, sRunId);
    if ( !sRunDir ) {
        return XWORK_ERROR_NO_MEMORY;
    }

    sEventsPath = xwork__build_events_log_path(sRunDir);
    free(sRunDir);
    if ( !sEventsPath ) {
        return XWORK_ERROR_NO_MEMORY;
    }

    pFile = fopen(sEventsPath, "rb");
    free(sEventsPath);
    if ( !pFile ) {
        return errno == ENOENT ? XWORK_ERROR_NOT_FOUND : XWORK_ERROR_EXTERNAL_FAILURE;
    }

    for ( ;; ) {
        bool bIsHeader = false;

        iStatus = xwork__read_line_owned(pFile, &sLine);
        if ( iStatus == XWORK_ERROR_NOT_FOUND ) {
            iStatus = bHasEvent ? XWORK_OK : XWORK_ERROR_NOT_FOUND;
            break;
        }
        if ( iStatus != XWORK_OK ) {
            break;
        }
        if ( sLine[0] == '\0' ) {
            free(sLine);
            sLine = NULL;
            continue;
        }

        iStatus = xwork__check_event_log_line_header(sLine, &bIsHeader);
        if ( iStatus != XWORK_OK ) {
            free(sLine);
            sLine = NULL;
            break;
        }
        if ( bIsHeader ) {
            free(sLine);
            sLine = NULL;
            continue;
        }

        xwork_event_reset(pEvent);
        iStatus = xwork__parse_event_line(sRunId, sLine, pEvent);
        free(sLine);
        sLine = NULL;
        if ( iStatus != XWORK_OK ) {
            break;
        }
        bHasEvent = true;
    }

    free(sLine);
    if ( fclose(pFile) != 0 && iStatus == XWORK_OK ) {
        iStatus = XWORK_ERROR_EXTERNAL_FAILURE;
    }
    if ( iStatus != XWORK_OK ) {
        xwork_event_reset(pEvent);
    }
    return iStatus;
}

xwork_status xwork_file_persistence_load_run_snapshot(
    const xwork_file_persistence *pStore,
    const char *sRunId,
    xwork_run_snapshot *pSnapshot
)
{
    return xwork__file_persistence_load_run_snapshot_cb(
        sRunId,
        pSnapshot,
        (void *)pStore
    );
}

xwork_status xwork_file_persistence_load_checkpoint_snapshot(
    const xwork_file_persistence *pStore,
    const char *sRunId,
    const char *sCheckpointId,
    xwork_run_snapshot *pSnapshot
)
{
    return xwork__file_persistence_load_checkpoint_snapshot_cb(
        sRunId,
        sCheckpointId,
        pSnapshot,
        (void *)pStore
    );
}

xwork_status xwork_file_persistence_store_task_graph_snapshot(
    const xwork_file_persistence *pStore,
    const xwork_task_graph_snapshot *pSnapshot
)
{
    char *sTaskGraphsDir;
    char *sGraphPath;
    xwork_status iStatus;

    if ( !pStore || !pStore->sRootPath || !pSnapshot ||
         !pSnapshot->sGraphId || !pSnapshot->sGraphId[0] ) {
        return XWORK_ERROR_INVALID_ARGUMENT;
    }

    sTaskGraphsDir = xwork__dup_printf("%s/task_graphs", pStore->sRootPath);
    if ( !sTaskGraphsDir ) {
        return XWORK_ERROR_NO_MEMORY;
    }
    iStatus = xwork__ensure_directory_tree(sTaskGraphsDir);
    if ( iStatus != XWORK_OK ) {
        free(sTaskGraphsDir);
        return iStatus;
    }

    sGraphPath = xwork__build_task_graph_file_path(
        sTaskGraphsDir,
        pSnapshot->sGraphId
    );
    free(sTaskGraphsDir);
    if ( !sGraphPath ) {
        return XWORK_ERROR_NO_MEMORY;
    }

    iStatus = xwork__write_task_graph_snapshot_file(sGraphPath, pSnapshot);
    free(sGraphPath);
    return iStatus;
}

xwork_status xwork_file_persistence_load_task_graph_snapshot(
    const xwork_file_persistence *pStore,
    const char *sGraphId,
    xwork_task_graph_snapshot *pSnapshot
)
{
    char *sTaskGraphsDir;
    char *sGraphPath;
    xwork_status iStatus;

    if ( !pStore || !pStore->sRootPath || !sGraphId || !sGraphId[0] || !pSnapshot ) {
        return XWORK_ERROR_INVALID_ARGUMENT;
    }

    sTaskGraphsDir = xwork__dup_printf("%s/task_graphs", pStore->sRootPath);
    if ( !sTaskGraphsDir ) {
        return XWORK_ERROR_NO_MEMORY;
    }
    sGraphPath = xwork__build_task_graph_file_path(sTaskGraphsDir, sGraphId);
    free(sTaskGraphsDir);
    if ( !sGraphPath ) {
        return XWORK_ERROR_NO_MEMORY;
    }

    iStatus = xwork__read_task_graph_snapshot_file(sGraphPath, pSnapshot);
    free(sGraphPath);
    return iStatus;
}

xwork_status xwork_file_persistence_store_agent_pool_snapshot(
    const xwork_file_persistence *pStore,
    const xwork_agent_pool_snapshot *pSnapshot
)
{
    char *sAgentPoolsDir;
    char *sPoolPath;
    xwork_status iStatus;

    if ( !pStore || !pStore->sRootPath || !pSnapshot ||
         !pSnapshot->sPoolId || !pSnapshot->sPoolId[0] ) {
        return XWORK_ERROR_INVALID_ARGUMENT;
    }

    sAgentPoolsDir = xwork__dup_printf("%s/agent_pools", pStore->sRootPath);
    if ( !sAgentPoolsDir ) {
        return XWORK_ERROR_NO_MEMORY;
    }
    iStatus = xwork__ensure_directory_tree(sAgentPoolsDir);
    if ( iStatus != XWORK_OK ) {
        free(sAgentPoolsDir);
        return iStatus;
    }

    sPoolPath = xwork__build_agent_pool_file_path(sAgentPoolsDir, pSnapshot->sPoolId);
    free(sAgentPoolsDir);
    if ( !sPoolPath ) {
        return XWORK_ERROR_NO_MEMORY;
    }

    iStatus = xwork__write_agent_pool_snapshot_file(sPoolPath, pSnapshot);
    free(sPoolPath);
    return iStatus;
}

xwork_status xwork_file_persistence_load_agent_pool_snapshot(
    const xwork_file_persistence *pStore,
    const char *sPoolId,
    xwork_agent_pool_snapshot *pSnapshot
)
{
    char *sAgentPoolsDir;
    char *sPoolPath;
    xwork_status iStatus;

    if ( !pStore || !pStore->sRootPath || !sPoolId || !sPoolId[0] || !pSnapshot ) {
        return XWORK_ERROR_INVALID_ARGUMENT;
    }

    sAgentPoolsDir = xwork__dup_printf("%s/agent_pools", pStore->sRootPath);
    if ( !sAgentPoolsDir ) {
        return XWORK_ERROR_NO_MEMORY;
    }
    sPoolPath = xwork__build_agent_pool_file_path(sAgentPoolsDir, sPoolId);
    free(sAgentPoolsDir);
    if ( !sPoolPath ) {
        return XWORK_ERROR_NO_MEMORY;
    }

    iStatus = xwork__read_agent_pool_snapshot_file(sPoolPath, pSnapshot);
    free(sPoolPath);
    return iStatus;
}

xwork_status xwork_file_persistence_store_control_plane_snapshot(
    const xwork_file_persistence *pStore,
    const xwork_control_plane_snapshot *pSnapshot
)
{
    char *sControlPlanesDir;
    char *sPlanePath;
    xwork_status iStatus;

    if ( !pStore || !pStore->sRootPath || !pSnapshot ||
         !pSnapshot->sPlaneId || !pSnapshot->sPlaneId[0] ) {
        return XWORK_ERROR_INVALID_ARGUMENT;
    }

    sControlPlanesDir = xwork__dup_printf("%s/control_planes", pStore->sRootPath);
    if ( !sControlPlanesDir ) {
        return XWORK_ERROR_NO_MEMORY;
    }
    iStatus = xwork__ensure_directory_tree(sControlPlanesDir);
    if ( iStatus != XWORK_OK ) {
        free(sControlPlanesDir);
        return iStatus;
    }

    sPlanePath = xwork__build_control_plane_file_path(
        sControlPlanesDir,
        pSnapshot->sPlaneId
    );
    free(sControlPlanesDir);
    if ( !sPlanePath ) {
        return XWORK_ERROR_NO_MEMORY;
    }

    iStatus = xwork__write_control_plane_snapshot_file(sPlanePath, pSnapshot);
    free(sPlanePath);
    return iStatus;
}

xwork_status xwork_file_persistence_load_control_plane_snapshot(
    const xwork_file_persistence *pStore,
    const char *sPlaneId,
    xwork_control_plane_snapshot *pSnapshot
)
{
    char *sControlPlanesDir;
    char *sPlanePath;
    xwork_status iStatus;

    if ( !pStore || !pStore->sRootPath || !sPlaneId || !sPlaneId[0] || !pSnapshot ) {
        return XWORK_ERROR_INVALID_ARGUMENT;
    }

    sControlPlanesDir = xwork__dup_printf("%s/control_planes", pStore->sRootPath);
    if ( !sControlPlanesDir ) {
        return XWORK_ERROR_NO_MEMORY;
    }
    sPlanePath = xwork__build_control_plane_file_path(sControlPlanesDir, sPlaneId);
    free(sControlPlanesDir);
    if ( !sPlanePath ) {
        return XWORK_ERROR_NO_MEMORY;
    }

    iStatus = xwork__read_control_plane_snapshot_file(sPlanePath, pSnapshot);
    free(sPlanePath);
    return iStatus;
}

xwork_status xwork_file_persistence_store_replay(
    const xwork_file_persistence *pStore,
    const xwork_replay_engine *pEngine
)
{
    xwork_replay_manifest tManifest;
    xwork_replay_entry_summary_list tEntries;
    xwork_replay_result tResult;
    char *sReplaysDir = NULL;
    char *sReplayPath = NULL;
    xwork_status iStatus;

    if ( !pStore || !pStore->sRootPath || !pEngine ) {
        return XWORK_ERROR_INVALID_ARGUMENT;
    }

    xwork_replay_manifest_init(&tManifest);
    xwork_replay_entry_summary_list_init(&tEntries);
    xwork_replay_result_init(&tResult);

    iStatus = xwork_replay_engine_get_manifest(pEngine, &tManifest);
    if ( iStatus != XWORK_OK ) goto done;
    if ( !tManifest.sReplayId || !tManifest.sReplayId[0] ) {
        iStatus = XWORK_ERROR_INVALID_ARGUMENT;
        goto done;
    }
    iStatus = xwork_replay_engine_list_entries(pEngine, &tEntries);
    if ( iStatus != XWORK_OK ) goto done;
    iStatus = xwork_replay_engine_get_result(pEngine, &tResult);
    if ( iStatus != XWORK_OK ) goto done;

    sReplaysDir = xwork__dup_printf("%s/replays", pStore->sRootPath);
    if ( !sReplaysDir ) {
        iStatus = XWORK_ERROR_NO_MEMORY;
        goto done;
    }
    iStatus = xwork__ensure_directory_tree(sReplaysDir);
    if ( iStatus != XWORK_OK ) goto done;

    sReplayPath = xwork__build_replay_file_path(sReplaysDir, tManifest.sReplayId);
    if ( !sReplayPath ) {
        iStatus = XWORK_ERROR_NO_MEMORY;
        goto done;
    }
    iStatus = xwork__write_replay_file(
        sReplayPath,
        &tManifest,
        &tEntries,
        &tResult
    );

done:
    free(sReplayPath);
    free(sReplaysDir);
    xwork_replay_result_reset(&tResult);
    xwork_replay_entry_summary_list_reset(&tEntries);
    xwork_replay_manifest_reset(&tManifest);
    return iStatus;
}

xwork_status xwork_file_persistence_list_replays(
    const xwork_file_persistence *pStore,
    xwork_string_list *pList
)
{
    char *sReplaysDir;
    xwork_status iStatus;

    if ( !pStore || !pStore->sRootPath || !pList ) {
        return XWORK_ERROR_INVALID_ARGUMENT;
    }

    sReplaysDir = xwork__dup_printf("%s/replays", pStore->sRootPath);
    if ( !sReplaysDir ) {
        return XWORK_ERROR_NO_MEMORY;
    }
    iStatus = xwork__list_encoded_entries(
        sReplaysDir,
        false,
        ".replay",
        pList
    );
    free(sReplaysDir);
    return iStatus;
}

xwork_status xwork_file_persistence_load_replay_manifest(
    const xwork_file_persistence *pStore,
    const char *sReplayId,
    xwork_replay_manifest *pManifest
)
{
    char *sReplaysDir;
    char *sReplayPath;
    xwork_status iStatus;

    if ( !pStore || !pStore->sRootPath || !sReplayId || !sReplayId[0] || !pManifest ) {
        return XWORK_ERROR_INVALID_ARGUMENT;
    }
    sReplaysDir = xwork__dup_printf("%s/replays", pStore->sRootPath);
    if ( !sReplaysDir ) {
        return XWORK_ERROR_NO_MEMORY;
    }
    sReplayPath = xwork__build_replay_file_path(sReplaysDir, sReplayId);
    free(sReplaysDir);
    if ( !sReplayPath ) {
        return XWORK_ERROR_NO_MEMORY;
    }
    iStatus = xwork__read_replay_file(sReplayPath, pManifest, NULL, NULL);
    free(sReplayPath);
    return iStatus;
}

xwork_status xwork_file_persistence_load_replay_entries(
    const xwork_file_persistence *pStore,
    const char *sReplayId,
    xwork_replay_entry_summary_list *pList
)
{
    char *sReplaysDir;
    char *sReplayPath;
    xwork_status iStatus;

    if ( !pStore || !pStore->sRootPath || !sReplayId || !sReplayId[0] || !pList ) {
        return XWORK_ERROR_INVALID_ARGUMENT;
    }
    sReplaysDir = xwork__dup_printf("%s/replays", pStore->sRootPath);
    if ( !sReplaysDir ) {
        return XWORK_ERROR_NO_MEMORY;
    }
    sReplayPath = xwork__build_replay_file_path(sReplaysDir, sReplayId);
    free(sReplaysDir);
    if ( !sReplayPath ) {
        return XWORK_ERROR_NO_MEMORY;
    }
    iStatus = xwork__read_replay_file(sReplayPath, NULL, pList, NULL);
    free(sReplayPath);
    return iStatus;
}

xwork_status xwork_file_persistence_load_replay_result(
    const xwork_file_persistence *pStore,
    const char *sReplayId,
    xwork_replay_result *pResult
)
{
    char *sReplaysDir;
    char *sReplayPath;
    xwork_status iStatus;

    if ( !pStore || !pStore->sRootPath || !sReplayId || !sReplayId[0] || !pResult ) {
        return XWORK_ERROR_INVALID_ARGUMENT;
    }
    sReplaysDir = xwork__dup_printf("%s/replays", pStore->sRootPath);
    if ( !sReplaysDir ) {
        return XWORK_ERROR_NO_MEMORY;
    }
    sReplayPath = xwork__build_replay_file_path(sReplaysDir, sReplayId);
    free(sReplaysDir);
    if ( !sReplayPath ) {
        return XWORK_ERROR_NO_MEMORY;
    }
    iStatus = xwork__read_replay_file(sReplayPath, NULL, NULL, pResult);
    free(sReplayPath);
    return iStatus;
}

xwork_status xwork_file_persistence_load_replay_engine(
    const xwork_file_persistence *pStore,
    const char *sReplayId,
    const xwork_replay_options *pOptions,
    xwork_replay_engine **ppEngine
)
{
    xwork_replay_manifest tManifest;
    xwork_replay_entry_summary_list tEntries;
    xwork_replay_options tOptions;
    xwork_replay_engine *pEngine = NULL;
    char *sReplaysDir = NULL;
    char *sReplayPath = NULL;
    xwork_status iStatus;
    size_t i;

    if ( !pStore || !pStore->sRootPath || !sReplayId || !sReplayId[0] || !ppEngine ) {
        return XWORK_ERROR_INVALID_ARGUMENT;
    }
    *ppEngine = NULL;
    xwork_replay_manifest_init(&tManifest);
    xwork_replay_entry_summary_list_init(&tEntries);

    sReplaysDir = xwork__dup_printf("%s/replays", pStore->sRootPath);
    if ( !sReplaysDir ) {
        iStatus = XWORK_ERROR_NO_MEMORY;
        goto done;
    }
    sReplayPath = xwork__build_replay_file_path(sReplaysDir, sReplayId);
    if ( !sReplayPath ) {
        iStatus = XWORK_ERROR_NO_MEMORY;
        goto done;
    }
    iStatus = xwork__read_replay_file(sReplayPath, &tManifest, &tEntries, NULL);
    if ( iStatus != XWORK_OK ) goto done;

    if ( pOptions ) {
        tOptions = *pOptions;
        if ( !tOptions.sReplayId || !tOptions.sReplayId[0] ) {
            tOptions.sReplayId = tManifest.sReplayId;
        }
    } else {
        xwork_replay_options_init(&tOptions);
        tOptions.sReplayId = tManifest.sReplayId;
        tOptions.eMode = XWORK_REPLAY_MODE_STRICT;
    }
    iStatus = xwork_replay_engine_create(&tOptions, &pEngine);
    if ( iStatus != XWORK_OK ) goto done;

    for ( i = 0u; i < tEntries.iCount; ++i ) {
        xwork_replay_entry_options tEntry;
        xwork_replay_entry_options_init(&tEntry);
        tEntry.eKind = tEntries.pItems[i].eKind;
        tEntry.sKey = tEntries.pItems[i].sKey;
        tEntry.sOperationId = tEntries.pItems[i].sOperationId;
        tEntry.sRequestJson = tEntries.pItems[i].sRequestJson;
        tEntry.sResponseJson = tEntries.pItems[i].sResponseJson;
        tEntry.sArgumentsJson = tEntries.pItems[i].sArgumentsJson;
        tEntry.sResultJson = tEntries.pItems[i].sResultJson;
        tEntry.sRequestHash = tEntries.pItems[i].sRequestHash;
        tEntry.sResponseHash = tEntries.pItems[i].sResponseHash;
        tEntry.sArgumentsHash = tEntries.pItems[i].sArgumentsHash;
        tEntry.sResultHash = tEntries.pItems[i].sResultHash;
        tEntry.sContentHash = tEntries.pItems[i].sContentHash;
        tEntry.iStatus = tEntries.pItems[i].iStatus;
        iStatus = xwork_replay_engine_load_entry(pEngine, &tEntry);
        if ( iStatus != XWORK_OK ) goto done;
    }

    *ppEngine = pEngine;
    pEngine = NULL;

done:
    xwork_replay_engine_destroy(pEngine);
    free(sReplayPath);
    free(sReplaysDir);
    xwork_replay_entry_summary_list_reset(&tEntries);
    xwork_replay_manifest_reset(&tManifest);
    return iStatus;
}

static void xwork__task_graph_snapshot_prepare_for_recovery(
    xwork_task_graph_snapshot *pSnapshot
)
{
    size_t i;

    if ( !pSnapshot ) {
        return;
    }
    for ( i = 0u; i < pSnapshot->tNodes.iCount; ++i ) {
        xwork_task_node_snapshot *pNode = &pSnapshot->tNodes.pItems[i];

        if ( pNode->eState == XWORK_TASK_READY ||
             pNode->eState == XWORK_TASK_RUNNING ||
             pNode->eState == XWORK_TASK_BLOCKED ) {
            pNode->eState = XWORK_TASK_PENDING;
            pNode->iStatus = XWORK_OK;
        }
    }
}

xwork_status xwork_file_persistence_recover_task_graph(
    const xwork_file_persistence *pStore,
    xwork_runtime *pRuntime,
    const char *sPoolId,
    const char *sGraphId,
    const xwork_task_graph_options *pExecutionOptions,
    xwork_agent_pool **ppPool,
    xwork_task_graph **ppGraph
)
{
    xwork_agent_pool_snapshot tPoolSnapshot;
    xwork_task_graph_snapshot tGraphSnapshot;
    xwork_task_graph_options tGraphOptions;
    xwork_agent_pool *pPool = NULL;
    xwork_task_graph *pGraph = NULL;
    xwork_status iStatus;

    if ( !pStore || !pRuntime || !sPoolId || !sPoolId[0] ||
         !sGraphId || !sGraphId[0] || !ppPool || !ppGraph ) {
        return XWORK_ERROR_INVALID_ARGUMENT;
    }
    *ppPool = NULL;
    *ppGraph = NULL;
    xwork_agent_pool_snapshot_init(&tPoolSnapshot);
    xwork_task_graph_snapshot_init(&tGraphSnapshot);

    iStatus = xwork_file_persistence_load_agent_pool_snapshot(
        pStore,
        sPoolId,
        &tPoolSnapshot
    );
    if ( iStatus != XWORK_OK ) {
        goto done;
    }
    iStatus = xwork_agent_pool_create_from_snapshot(
        pRuntime,
        &tPoolSnapshot,
        &pPool
    );
    if ( iStatus != XWORK_OK ) {
        goto done;
    }
    iStatus = xwork_file_persistence_load_task_graph_snapshot(
        pStore,
        sGraphId,
        &tGraphSnapshot
    );
    if ( iStatus != XWORK_OK ) {
        goto done;
    }
    xwork__task_graph_snapshot_prepare_for_recovery(&tGraphSnapshot);

    xwork_task_graph_options_init(&tGraphOptions);
    tGraphOptions.sGraphId =
        pExecutionOptions && pExecutionOptions->sGraphId && pExecutionOptions->sGraphId[0]
            ? pExecutionOptions->sGraphId
            : tGraphSnapshot.sGraphId;
    tGraphOptions.pAgentPool = pPool;
    tGraphOptions.iMaxConcurrency = tGraphSnapshot.iMaxConcurrency;
    tGraphOptions.eFailurePolicy = tGraphSnapshot.eFailurePolicy;
    if ( pExecutionOptions ) {
        tGraphOptions.pCancelToken = pExecutionOptions->pCancelToken;
        tGraphOptions.pfnExecute = pExecutionOptions->pfnExecute;
        tGraphOptions.pUserData = pExecutionOptions->pUserData;
    }

    iStatus = xwork_task_graph_create_from_snapshot(
        &tGraphOptions,
        &tGraphSnapshot,
        &pGraph
    );
    if ( iStatus != XWORK_OK ) {
        goto done;
    }

    *ppPool = pPool;
    *ppGraph = pGraph;
    pPool = NULL;
    pGraph = NULL;

done:
    xwork_task_graph_destroy(pGraph);
    xwork_agent_pool_destroy(pPool);
    xwork_task_graph_snapshot_reset(&tGraphSnapshot);
    xwork_agent_pool_snapshot_reset(&tPoolSnapshot);
    return iStatus;
}

xwork_status xwork_file_persistence_recover_control_plane(
    const xwork_file_persistence *pStore,
    xwork_runtime *pRuntime,
    const char *sPlaneId,
    const xwork_control_plane_options *pOptions,
    xwork_control_plane **ppPlane
)
{
    xwork_control_plane_snapshot tSnapshot;
    xwork_control_plane_options tOptions;
    xwork_control_plane *pPlane = NULL;
    xwork_status iStatus;

    if ( !pStore || !pRuntime || !sPlaneId || !sPlaneId[0] || !ppPlane ) {
        return XWORK_ERROR_INVALID_ARGUMENT;
    }
    *ppPlane = NULL;
    xwork_control_plane_snapshot_init(&tSnapshot);

    iStatus = xwork_file_persistence_load_control_plane_snapshot(
        pStore,
        sPlaneId,
        &tSnapshot
    );
    if ( iStatus != XWORK_OK ) {
        goto done;
    }

    xwork_control_plane_options_init(&tOptions);
    tOptions.sPlaneId =
        pOptions && pOptions->sPlaneId && pOptions->sPlaneId[0]
            ? pOptions->sPlaneId
            : tSnapshot.sPlaneId;
    tOptions.pRuntime = pRuntime;
    tOptions.eTransport = pOptions ? pOptions->eTransport : tSnapshot.eTransport;
    tOptions.iDefaultLeaseTimeoutMs =
        pOptions && pOptions->iDefaultLeaseTimeoutMs
            ? pOptions->iDefaultLeaseTimeoutMs
            : tSnapshot.iDefaultLeaseTimeoutMs;
    tOptions.iNowMs = pOptions ? pOptions->iNowMs : tSnapshot.iNowMs;

    iStatus = xwork_control_plane_create_from_snapshot(
        &tOptions,
        &tSnapshot,
        &pPlane
    );
    if ( iStatus != XWORK_OK ) {
        goto done;
    }

    *ppPlane = pPlane;
    pPlane = NULL;

done:
    xwork_control_plane_destroy(pPlane);
    xwork_control_plane_snapshot_reset(&tSnapshot);
    return iStatus;
}

xwork_status xwork_file_persistence_load_last_approval_request(
    const xwork_file_persistence *pStore,
    const char *sRunId,
    xwork_approval_request *pRequest
)
{
    xwork_run_snapshot tSnapshot;
    xwork_status iStatus;

    if ( !pStore || !sRunId || !sRunId[0] || !pRequest ) {
        return XWORK_ERROR_INVALID_ARGUMENT;
    }

    xwork_run_snapshot_init(&tSnapshot);
    iStatus = xwork__file_persistence_load_run_snapshot_cb(
        sRunId,
        &tSnapshot,
        (void *)pStore
    );
    if ( iStatus != XWORK_OK ) {
        xwork_run_snapshot_reset(&tSnapshot);
        return iStatus;
    }
    if ( !tSnapshot.bHasApprovalRequest || !tSnapshot.sLastApprovalRequestId ) {
        xwork_run_snapshot_reset(&tSnapshot);
        return XWORK_ERROR_NOT_FOUND;
    }

    xwork_approval_request_reset(pRequest);
    iStatus = xwork__replace_cstr((char **)&pRequest->sRequestId, tSnapshot.sLastApprovalRequestId);
    if ( iStatus != XWORK_OK ) goto done;
    iStatus = xwork__replace_cstr((char **)&pRequest->sRunId, tSnapshot.sRunId);
    if ( iStatus != XWORK_OK ) goto done;
    iStatus = xwork__replace_cstr((char **)&pRequest->sToolId, tSnapshot.sLastApprovalToolId);
    if ( iStatus != XWORK_OK ) goto done;
    iStatus = xwork__replace_cstr((char **)&pRequest->sReason, tSnapshot.sLastApprovalReason);
    if ( iStatus != XWORK_OK ) goto done;
    iStatus = xwork__replace_cstr((char **)&pRequest->sScope, tSnapshot.sLastApprovalScope);
    if ( iStatus != XWORK_OK ) goto done;
    iStatus = xwork__replace_cstr(
        (char **)&pRequest->sActionSummary,
        tSnapshot.sLastApprovalActionSummary
    );
    if ( iStatus != XWORK_OK ) goto done;

    pRequest->eRiskLevel = tSnapshot.eLastApprovalRiskLevel;
    pRequest->eState = tSnapshot.eLastApprovalState;

done:
    if ( iStatus != XWORK_OK ) {
        xwork_approval_request_reset(pRequest);
    }
    xwork_run_snapshot_reset(&tSnapshot);
    return iStatus;
}

xwork_status xwork_file_persistence_load_checkpoint(
    const xwork_file_persistence *pStore,
    const char *sRunId,
    const char *sCheckpointId,
    xwork_checkpoint *pCheckpoint
)
{
    xwork_run_snapshot tSnapshot;
    xwork_status iStatus;

    if ( !pStore || !sRunId || !sRunId[0] ||
         !sCheckpointId || !sCheckpointId[0] || !pCheckpoint ) {
        return XWORK_ERROR_INVALID_ARGUMENT;
    }

    xwork_run_snapshot_init(&tSnapshot);
    iStatus = xwork__file_persistence_load_checkpoint_snapshot_cb(
        sRunId,
        sCheckpointId,
        &tSnapshot,
        (void *)pStore
    );
    if ( iStatus != XWORK_OK ) {
        xwork_run_snapshot_reset(&tSnapshot);
        return iStatus;
    }
    if ( !tSnapshot.bHasCheckpoint || !tSnapshot.sLastCheckpointId ||
         strcmp(tSnapshot.sLastCheckpointId, sCheckpointId) != 0 ) {
        xwork_run_snapshot_reset(&tSnapshot);
        return XWORK_ERROR_EXTERNAL_FAILURE;
    }

    xwork_checkpoint_reset(pCheckpoint);
    iStatus = xwork__replace_cstr(
        (char **)&pCheckpoint->sCheckpointId,
        tSnapshot.sLastCheckpointId
    );
    if ( iStatus != XWORK_OK ) goto done;
    iStatus = xwork__replace_cstr((char **)&pCheckpoint->sRunId, tSnapshot.sRunId);
    if ( iStatus != XWORK_OK ) goto done;
    iStatus = xwork__replace_cstr(
        (char **)&pCheckpoint->sPendingStep,
        tSnapshot.sLastCheckpointPendingStep
    );
    if ( iStatus != XWORK_OK ) goto done;
    iStatus = xwork__replace_cstr(
        (char **)&pCheckpoint->sSessionStateRef,
        tSnapshot.sLastCheckpointSessionStateRef
    );
    if ( iStatus != XWORK_OK ) goto done;
    iStatus = xwork__replace_cstr(
        (char **)&pCheckpoint->sToolOutputsRef,
        tSnapshot.sLastCheckpointToolOutputsRef
    );
    if ( iStatus != XWORK_OK ) goto done;
    iStatus = xwork__replace_cstr(
        (char **)&pCheckpoint->sWorkspaceSnapshotRef,
        tSnapshot.sLastCheckpointWorkspaceSnapshotRef
    );
    if ( iStatus != XWORK_OK ) goto done;
    iStatus = xwork__replace_cstr(
        (char **)&pCheckpoint->sArtifactRefs,
        tSnapshot.sLastCheckpointArtifactRefs
    );
    if ( iStatus != XWORK_OK ) goto done;

    pCheckpoint->eKind = tSnapshot.eLastCheckpointKind;
    pCheckpoint->eRunState = tSnapshot.eLastCheckpointRunState;
    pCheckpoint->iSequence = tSnapshot.iLastCheckpointSequence;

done:
    if ( iStatus != XWORK_OK ) {
        xwork_checkpoint_reset(pCheckpoint);
    }
    xwork_run_snapshot_reset(&tSnapshot);
    return iStatus;
}

xwork_status xwork_file_persistence_load_run_summary(
    const xwork_file_persistence *pStore,
    const char *sRunId,
    xwork_run_summary *pSummary
)
{
    xwork_run_snapshot tSnapshot;
    xwork_status iStatus;

    if ( !pStore || !sRunId || !sRunId[0] || !pSummary ) {
        return XWORK_ERROR_INVALID_ARGUMENT;
    }

    xwork_run_snapshot_init(&tSnapshot);
    iStatus = xwork__file_persistence_load_run_snapshot_cb(
        sRunId,
        &tSnapshot,
        (void *)pStore
    );
    if ( iStatus != XWORK_OK ) {
        xwork_run_snapshot_reset(&tSnapshot);
        return iStatus;
    }

    xwork_run_summary_reset(pSummary);
    iStatus = xwork__replace_cstr((char **)&pSummary->sRunId, tSnapshot.sRunId);
    if ( iStatus != XWORK_OK ) goto done;
    iStatus = xwork__replace_cstr((char **)&pSummary->sParentRunId, tSnapshot.sParentRunId);
    if ( iStatus != XWORK_OK ) goto done;
    iStatus = xwork__replace_cstr((char **)&pSummary->sAgentId, tSnapshot.sAgentId);
    if ( iStatus != XWORK_OK ) goto done;
    iStatus = xwork__replace_cstr((char **)&pSummary->sTaskId, tSnapshot.sTaskId);
    if ( iStatus != XWORK_OK ) goto done;
    iStatus = xwork__replace_cstr((char **)&pSummary->sInstruction, tSnapshot.sInstruction);
    if ( iStatus != XWORK_OK ) goto done;

    pSummary->eAutonomy = tSnapshot.eAutonomy;
    pSummary->eState = tSnapshot.eState;
    pSummary->iWorkspaceCount = tSnapshot.iWorkspaceCount;

done:
    if ( iStatus != XWORK_OK ) {
        xwork_run_summary_reset(pSummary);
    }
    xwork_run_snapshot_reset(&tSnapshot);
    return iStatus;
}

xwork_status xwork_file_persistence_load_last_checkpoint(
    const xwork_file_persistence *pStore,
    const char *sRunId,
    xwork_checkpoint *pCheckpoint
)
{
    xwork_run_snapshot tSnapshot;
    xwork_status iStatus;

    if ( !pStore || !sRunId || !sRunId[0] || !pCheckpoint ) {
        return XWORK_ERROR_INVALID_ARGUMENT;
    }

    xwork_run_snapshot_init(&tSnapshot);
    iStatus = xwork__file_persistence_load_run_snapshot_cb(
        sRunId,
        &tSnapshot,
        (void *)pStore
    );
    if ( iStatus != XWORK_OK ) {
        xwork_run_snapshot_reset(&tSnapshot);
        return iStatus;
    }
    if ( !tSnapshot.bHasCheckpoint || !tSnapshot.sLastCheckpointId ) {
        xwork_run_snapshot_reset(&tSnapshot);
        return XWORK_ERROR_NOT_FOUND;
    }

    xwork_checkpoint_reset(pCheckpoint);
    iStatus = xwork__replace_cstr(
        (char **)&pCheckpoint->sCheckpointId,
        tSnapshot.sLastCheckpointId
    );
    if ( iStatus != XWORK_OK ) goto done;
    iStatus = xwork__replace_cstr((char **)&pCheckpoint->sRunId, tSnapshot.sRunId);
    if ( iStatus != XWORK_OK ) goto done;
    iStatus = xwork__replace_cstr(
        (char **)&pCheckpoint->sPendingStep,
        tSnapshot.sLastCheckpointPendingStep
    );
    if ( iStatus != XWORK_OK ) goto done;
    iStatus = xwork__replace_cstr(
        (char **)&pCheckpoint->sSessionStateRef,
        tSnapshot.sLastCheckpointSessionStateRef
    );
    if ( iStatus != XWORK_OK ) goto done;
    iStatus = xwork__replace_cstr(
        (char **)&pCheckpoint->sToolOutputsRef,
        tSnapshot.sLastCheckpointToolOutputsRef
    );
    if ( iStatus != XWORK_OK ) goto done;
    iStatus = xwork__replace_cstr(
        (char **)&pCheckpoint->sWorkspaceSnapshotRef,
        tSnapshot.sLastCheckpointWorkspaceSnapshotRef
    );
    if ( iStatus != XWORK_OK ) goto done;
    iStatus = xwork__replace_cstr(
        (char **)&pCheckpoint->sArtifactRefs,
        tSnapshot.sLastCheckpointArtifactRefs
    );
    if ( iStatus != XWORK_OK ) goto done;

    pCheckpoint->eKind = tSnapshot.eLastCheckpointKind;
    pCheckpoint->eRunState = tSnapshot.eLastCheckpointRunState;
    pCheckpoint->iSequence = tSnapshot.iLastCheckpointSequence;

done:
    if ( iStatus != XWORK_OK ) {
        xwork_checkpoint_reset(pCheckpoint);
    }
    xwork_run_snapshot_reset(&tSnapshot);
    return iStatus;
}

xwork_status xwork__runtime_store_event(
    const xwork_runtime *pRuntime,
    const xwork_event *pEvent
)
{
    if ( !pRuntime || !pEvent ) {
        return XWORK_ERROR_INVALID_ARGUMENT;
    }
    if ( !pRuntime->tPersistenceBackend.pfnStoreEvent ) {
        return XWORK_OK;
    }
    return pRuntime->tPersistenceBackend.pfnStoreEvent(
        pEvent,
        pRuntime->tPersistenceBackend.pUserData
    );
}

xwork_status xwork__runtime_store_checkpoint(
    const xwork_runtime *pRuntime,
    const xwork_checkpoint *pCheckpoint,
    const xwork_run_snapshot *pSnapshot
)
{
    if ( !pRuntime || !pCheckpoint || !pSnapshot ) {
        return XWORK_ERROR_INVALID_ARGUMENT;
    }
    if ( !pRuntime->tPersistenceBackend.pfnStoreCheckpoint ) {
        return XWORK_OK;
    }
    return pRuntime->tPersistenceBackend.pfnStoreCheckpoint(
        pCheckpoint,
        pSnapshot,
        pRuntime->tPersistenceBackend.pUserData
    );
}

xwork_status xwork__runtime_store_run_snapshot(
    const xwork_runtime *pRuntime,
    const xwork_run_snapshot *pSnapshot
)
{
    if ( !pRuntime || !pSnapshot ) {
        return XWORK_ERROR_INVALID_ARGUMENT;
    }
    if ( pRuntime->tPersistenceBackend.pfnStoreRunSnapshot ) {
        return pRuntime->tPersistenceBackend.pfnStoreRunSnapshot(
            pSnapshot,
            pRuntime->tPersistenceBackend.pUserData
        );
    }
    if ( !pRuntime->tPersistenceBackend.pfnStoreCheckpoint ) {
        return XWORK_OK;
    }
    if ( !pSnapshot->bHasCheckpoint ) {
        return XWORK_OK;
    }
    {
        xwork_checkpoint tCheckpoint;

        xwork_checkpoint_init(&tCheckpoint);
        tCheckpoint.sCheckpointId = pSnapshot->sLastCheckpointId;
        tCheckpoint.sRunId = pSnapshot->sRunId;
        tCheckpoint.sPendingStep = pSnapshot->sLastCheckpointPendingStep;
        tCheckpoint.sSessionStateRef = pSnapshot->sLastCheckpointSessionStateRef;
        tCheckpoint.sToolOutputsRef = pSnapshot->sLastCheckpointToolOutputsRef;
        tCheckpoint.sWorkspaceSnapshotRef = pSnapshot->sLastCheckpointWorkspaceSnapshotRef;
        tCheckpoint.sArtifactRefs = pSnapshot->sLastCheckpointArtifactRefs;
        tCheckpoint.eKind = pSnapshot->eLastCheckpointKind;
        tCheckpoint.eRunState = pSnapshot->eLastCheckpointRunState;
        tCheckpoint.iSequence = pSnapshot->iLastCheckpointSequence;
        return pRuntime->tPersistenceBackend.pfnStoreCheckpoint(
            &tCheckpoint,
            pSnapshot,
            pRuntime->tPersistenceBackend.pUserData
        );
    }
}

xwork_status xwork__runtime_store_artifact(
    const xwork_runtime *pRuntime,
    const xwork_artifact *pArtifact
)
{
    if ( !pRuntime || !pArtifact ) {
        return XWORK_ERROR_INVALID_ARGUMENT;
    }
    if ( !pRuntime->tPersistenceBackend.pfnStoreArtifact ) {
        return XWORK_OK;
    }
    return pRuntime->tPersistenceBackend.pfnStoreArtifact(
        pArtifact,
        pRuntime->tPersistenceBackend.pUserData
    );
}

xwork_status xwork__runtime_load_run_snapshot(
    const xwork_runtime *pRuntime,
    const char *sRunId,
    xwork_run_snapshot *pSnapshot
)
{
    if ( !pRuntime || !sRunId || !sRunId[0] || !pSnapshot ) {
        return XWORK_ERROR_INVALID_ARGUMENT;
    }
    if ( !pRuntime->tPersistenceBackend.pfnLoadRunSnapshot ) {
        return XWORK_ERROR_UNSUPPORTED;
    }
    return pRuntime->tPersistenceBackend.pfnLoadRunSnapshot(
        sRunId,
        pSnapshot,
        pRuntime->tPersistenceBackend.pUserData
    );
}

xwork_status xwork__runtime_load_checkpoint_snapshot(
    const xwork_runtime *pRuntime,
    const char *sRunId,
    const char *sCheckpointId,
    xwork_run_snapshot *pSnapshot
)
{
    if ( !pRuntime || !sRunId || !sRunId[0] ||
         !sCheckpointId || !sCheckpointId[0] || !pSnapshot ) {
        return XWORK_ERROR_INVALID_ARGUMENT;
    }
    if ( !pRuntime->tPersistenceBackend.pfnLoadCheckpointSnapshot ) {
        return XWORK_ERROR_UNSUPPORTED;
    }
    return pRuntime->tPersistenceBackend.pfnLoadCheckpointSnapshot(
        sRunId,
        sCheckpointId,
        pSnapshot,
        pRuntime->tPersistenceBackend.pUserData
    );
}

xwork_status xwork__runtime_list_runs(
    const xwork_runtime *pRuntime,
    xwork_string_list *pList
)
{
    if ( !pRuntime || !pList ) {
        return XWORK_ERROR_INVALID_ARGUMENT;
    }
    if ( !pRuntime->tPersistenceBackend.pfnListRuns ) {
        return XWORK_ERROR_UNSUPPORTED;
    }
    return pRuntime->tPersistenceBackend.pfnListRuns(
        pList,
        pRuntime->tPersistenceBackend.pUserData
    );
}

xwork_status xwork__runtime_list_checkpoints(
    const xwork_runtime *pRuntime,
    const char *sRunId,
    xwork_string_list *pList
)
{
    if ( !pRuntime || !sRunId || !sRunId[0] || !pList ) {
        return XWORK_ERROR_INVALID_ARGUMENT;
    }
    if ( !pRuntime->tPersistenceBackend.pfnListCheckpoints ) {
        return XWORK_ERROR_UNSUPPORTED;
    }
    return pRuntime->tPersistenceBackend.pfnListCheckpoints(
        sRunId,
        pList,
        pRuntime->tPersistenceBackend.pUserData
    );
}

xwork_status xwork__runtime_list_events(
    const xwork_runtime *pRuntime,
    const char *sRunId,
    xwork_string_list *pList
)
{
    if ( !pRuntime || !sRunId || !sRunId[0] || !pList ) {
        return XWORK_ERROR_INVALID_ARGUMENT;
    }
    if ( !pRuntime->tPersistenceBackend.pfnListEvents ) {
        return XWORK_ERROR_UNSUPPORTED;
    }
    return pRuntime->tPersistenceBackend.pfnListEvents(
        sRunId,
        pList,
        pRuntime->tPersistenceBackend.pUserData
    );
}

xwork_status xwork__runtime_list_artifacts(
    const xwork_runtime *pRuntime,
    const char *sRunId,
    xwork_string_list *pList
)
{
    if ( !pRuntime || !sRunId || !sRunId[0] || !pList ) {
        return XWORK_ERROR_INVALID_ARGUMENT;
    }
    if ( !pRuntime->tPersistenceBackend.pfnListArtifacts ) {
        return XWORK_ERROR_UNSUPPORTED;
    }
    return pRuntime->tPersistenceBackend.pfnListArtifacts(
        sRunId,
        pList,
        pRuntime->tPersistenceBackend.pUserData
    );
}

xwork_status xwork__runtime_list_run_summaries(
    const xwork_runtime *pRuntime,
    xwork_run_summary_list *pList
)
{
    xwork_string_list tRunIds;
    xwork_run_summary *pItems = NULL;
    xwork_status iStatus = XWORK_OK;
    size_t i;

    if ( !pRuntime || !pList ) {
        return XWORK_ERROR_INVALID_ARGUMENT;
    }

    xwork_run_summary_list_reset(pList);
    xwork_string_list_init(&tRunIds);

    iStatus = xwork__runtime_list_runs(pRuntime, &tRunIds);
    if ( iStatus != XWORK_OK ) {
        goto done;
    }
    if ( tRunIds.iCount == 0u ) {
        goto done;
    }

    pItems = (xwork_run_summary *)calloc(tRunIds.iCount, sizeof(*pItems));
    if ( !pItems ) {
        iStatus = XWORK_ERROR_NO_MEMORY;
        goto done;
    }

    pList->pItems = pItems;
    pList->iCount = tRunIds.iCount;
    for ( i = 0u; i < pList->iCount; ++i ) {
        xwork_run_summary_init(&pItems[i]);
        iStatus = xwork__runtime_load_run_summary(
            pRuntime,
            tRunIds.psItems[i],
            &pItems[i]
        );
        if ( iStatus != XWORK_OK ) {
            goto done;
        }
    }

done:
    xwork_string_list_reset(&tRunIds);
    if ( iStatus != XWORK_OK ) {
        xwork_run_summary_list_reset(pList);
    }
    return iStatus;
}

xwork_status xwork__runtime_query_run_index(
    const xwork_runtime *pRuntime,
    const xwork_run_index_query *pQuery,
    xwork_run_index_list *pList
)
{
    if ( !pRuntime || !pList ) {
        return XWORK_ERROR_INVALID_ARGUMENT;
    }
    if ( !pRuntime->tPersistenceBackend.pfnQueryRunIndex ) {
        return XWORK_ERROR_UNSUPPORTED;
    }
    return pRuntime->tPersistenceBackend.pfnQueryRunIndex(
        pQuery,
        pList,
        pRuntime->tPersistenceBackend.pUserData
    );
}

xwork_status xwork__runtime_load_run_summary(
    const xwork_runtime *pRuntime,
    const char *sRunId,
    xwork_run_summary *pSummary
)
{
    if ( !pRuntime || !sRunId || !sRunId[0] || !pSummary ) {
        return XWORK_ERROR_INVALID_ARGUMENT;
    }
    if ( !pRuntime->tPersistenceBackend.pfnLoadRunSummary ) {
        return XWORK_ERROR_UNSUPPORTED;
    }
    return pRuntime->tPersistenceBackend.pfnLoadRunSummary(
        sRunId,
        pSummary,
        pRuntime->tPersistenceBackend.pUserData
    );
}

xwork_status xwork__runtime_load_last_event(
    const xwork_runtime *pRuntime,
    const char *sRunId,
    xwork_event *pEvent
)
{
    if ( !pRuntime || !sRunId || !sRunId[0] || !pEvent ) {
        return XWORK_ERROR_INVALID_ARGUMENT;
    }
    if ( !pRuntime->tPersistenceBackend.pfnLoadLastEvent ) {
        return XWORK_ERROR_UNSUPPORTED;
    }
    return pRuntime->tPersistenceBackend.pfnLoadLastEvent(
        sRunId,
        pEvent,
        pRuntime->tPersistenceBackend.pUserData
    );
}

xwork_status xwork__runtime_load_last_approval_request(
    const xwork_runtime *pRuntime,
    const char *sRunId,
    xwork_approval_request *pRequest
)
{
    if ( !pRuntime || !sRunId || !sRunId[0] || !pRequest ) {
        return XWORK_ERROR_INVALID_ARGUMENT;
    }
    if ( !pRuntime->tPersistenceBackend.pfnLoadLastApprovalRequest ) {
        return XWORK_ERROR_UNSUPPORTED;
    }
    return pRuntime->tPersistenceBackend.pfnLoadLastApprovalRequest(
        sRunId,
        pRequest,
        pRuntime->tPersistenceBackend.pUserData
    );
}

xwork_status xwork__runtime_load_last_checkpoint(
    const xwork_runtime *pRuntime,
    const char *sRunId,
    xwork_checkpoint *pCheckpoint
)
{
    if ( !pRuntime || !sRunId || !sRunId[0] || !pCheckpoint ) {
        return XWORK_ERROR_INVALID_ARGUMENT;
    }
    if ( !pRuntime->tPersistenceBackend.pfnLoadLastCheckpoint ) {
        return XWORK_ERROR_UNSUPPORTED;
    }
    return pRuntime->tPersistenceBackend.pfnLoadLastCheckpoint(
        sRunId,
        pCheckpoint,
        pRuntime->tPersistenceBackend.pUserData
    );
}

xwork_status xwork__runtime_load_last_artifact(
    const xwork_runtime *pRuntime,
    const char *sRunId,
    xwork_artifact *pArtifact
)
{
    if ( !pRuntime || !sRunId || !sRunId[0] || !pArtifact ) {
        return XWORK_ERROR_INVALID_ARGUMENT;
    }
    if ( !pRuntime->tPersistenceBackend.pfnLoadLastArtifact ) {
        return XWORK_ERROR_UNSUPPORTED;
    }
    return pRuntime->tPersistenceBackend.pfnLoadLastArtifact(
        sRunId,
        pArtifact,
        pRuntime->tPersistenceBackend.pUserData
    );
}

xwork_status xwork__runtime_load_event(
    const xwork_runtime *pRuntime,
    const char *sRunId,
    const char *sEventId,
    xwork_event *pEvent
)
{
    if ( !pRuntime || !sRunId || !sRunId[0] ||
         !sEventId || !sEventId[0] || !pEvent ) {
        return XWORK_ERROR_INVALID_ARGUMENT;
    }
    if ( !pRuntime->tPersistenceBackend.pfnLoadEvent ) {
        return XWORK_ERROR_UNSUPPORTED;
    }
    return pRuntime->tPersistenceBackend.pfnLoadEvent(
        sRunId,
        sEventId,
        pEvent,
        pRuntime->tPersistenceBackend.pUserData
    );
}

xwork_status xwork__runtime_load_checkpoint(
    const xwork_runtime *pRuntime,
    const char *sRunId,
    const char *sCheckpointId,
    xwork_checkpoint *pCheckpoint
)
{
    if ( !pRuntime || !sRunId || !sRunId[0] ||
         !sCheckpointId || !sCheckpointId[0] || !pCheckpoint ) {
        return XWORK_ERROR_INVALID_ARGUMENT;
    }
    if ( !pRuntime->tPersistenceBackend.pfnLoadCheckpoint ) {
        return XWORK_ERROR_UNSUPPORTED;
    }
    return pRuntime->tPersistenceBackend.pfnLoadCheckpoint(
        sRunId,
        sCheckpointId,
        pCheckpoint,
        pRuntime->tPersistenceBackend.pUserData
    );
}

xwork_status xwork__runtime_load_artifact(
    const xwork_runtime *pRuntime,
    const char *sRunId,
    const char *sArtifactId,
    xwork_artifact *pArtifact
)
{
    if ( !pRuntime || !sRunId || !sRunId[0] ||
         !sArtifactId || !sArtifactId[0] || !pArtifact ) {
        return XWORK_ERROR_INVALID_ARGUMENT;
    }
    if ( !pRuntime->tPersistenceBackend.pfnLoadArtifact ) {
        return XWORK_ERROR_UNSUPPORTED;
    }
    return pRuntime->tPersistenceBackend.pfnLoadArtifact(
        sRunId,
        sArtifactId,
        pArtifact,
        pRuntime->tPersistenceBackend.pUserData
    );
}

xwork_status xwork__runtime_find_artifact_by_name(
    const xwork_runtime *pRuntime,
    const char *sRunId,
    const char *sArtifactName,
    xwork_artifact *pArtifact
)
{
    xwork_string_list tArtifactIds;
    xwork_artifact tCurrentArtifact;
    xwork_status iStatus = XWORK_OK;
    size_t i;

    if ( !pRuntime || !sRunId || !sRunId[0] ||
         !sArtifactName || !sArtifactName[0] || !pArtifact ) {
        return XWORK_ERROR_INVALID_ARGUMENT;
    }

    xwork_string_list_init(&tArtifactIds);
    xwork_artifact_init(&tCurrentArtifact);
    xwork_artifact_reset(pArtifact);

    iStatus = xwork__runtime_list_artifacts(pRuntime, sRunId, &tArtifactIds);
    if ( iStatus != XWORK_OK ) {
        goto done;
    }

    for ( i = 0u; i < tArtifactIds.iCount; ++i ) {
        iStatus = xwork__runtime_load_artifact(
            pRuntime,
            sRunId,
            tArtifactIds.psItems[i],
            &tCurrentArtifact
        );
        if ( iStatus != XWORK_OK ) {
            goto done;
        }

        if ( tCurrentArtifact.sName &&
             strcmp(tCurrentArtifact.sName, sArtifactName) == 0 ) {
            *pArtifact = tCurrentArtifact;
            xwork_artifact_init(&tCurrentArtifact);
            iStatus = XWORK_OK;
            goto done;
        }

        xwork_artifact_reset(&tCurrentArtifact);
    }

    iStatus = XWORK_ERROR_NOT_FOUND;

done:
    xwork_artifact_reset(&tCurrentArtifact);
    xwork_string_list_reset(&tArtifactIds);
    if ( iStatus != XWORK_OK ) {
        xwork_artifact_reset(pArtifact);
    }
    return iStatus;
}

static xwork_status xwork__file_persistence_list_runs_cb(
    xwork_string_list *pList,
    void *pUserData
)
{
    const xwork_file_persistence *pStore = (const xwork_file_persistence *)pUserData;

    return xwork_file_persistence_list_runs(pStore, pList);
}

static xwork_status xwork__file_persistence_list_checkpoints_cb(
    const char *sRunId,
    xwork_string_list *pList,
    void *pUserData
)
{
    const xwork_file_persistence *pStore = (const xwork_file_persistence *)pUserData;

    return xwork_file_persistence_list_checkpoints(pStore, sRunId, pList);
}

static xwork_status xwork__file_persistence_list_events_cb(
    const char *sRunId,
    xwork_string_list *pList,
    void *pUserData
)
{
    const xwork_file_persistence *pStore = (const xwork_file_persistence *)pUserData;

    return xwork_file_persistence_list_events(pStore, sRunId, pList);
}

static xwork_status xwork__file_persistence_list_artifacts_cb(
    const char *sRunId,
    xwork_string_list *pList,
    void *pUserData
)
{
    const xwork_file_persistence *pStore = (const xwork_file_persistence *)pUserData;

    return xwork_file_persistence_list_artifacts(pStore, sRunId, pList);
}

static xwork_status xwork__file_persistence_query_run_index_cb(
    const xwork_run_index_query *pQuery,
    xwork_run_index_list *pList,
    void *pUserData
)
{
    const xwork_file_persistence *pStore = (const xwork_file_persistence *)pUserData;

    return xwork_file_persistence_query_run_index(pStore, pQuery, pList);
}

static xwork_status xwork__file_persistence_load_run_summary_cb(
    const char *sRunId,
    xwork_run_summary *pSummary,
    void *pUserData
)
{
    const xwork_file_persistence *pStore = (const xwork_file_persistence *)pUserData;

    return xwork_file_persistence_load_run_summary(pStore, sRunId, pSummary);
}

static xwork_status xwork__file_persistence_load_last_event_cb(
    const char *sRunId,
    xwork_event *pEvent,
    void *pUserData
)
{
    const xwork_file_persistence *pStore = (const xwork_file_persistence *)pUserData;

    return xwork_file_persistence_load_last_event(pStore, sRunId, pEvent);
}

static xwork_status xwork__file_persistence_load_last_approval_request_cb(
    const char *sRunId,
    xwork_approval_request *pRequest,
    void *pUserData
)
{
    const xwork_file_persistence *pStore = (const xwork_file_persistence *)pUserData;

    return xwork_file_persistence_load_last_approval_request(pStore, sRunId, pRequest);
}

static xwork_status xwork__file_persistence_load_last_checkpoint_cb(
    const char *sRunId,
    xwork_checkpoint *pCheckpoint,
    void *pUserData
)
{
    const xwork_file_persistence *pStore = (const xwork_file_persistence *)pUserData;

    return xwork_file_persistence_load_last_checkpoint(pStore, sRunId, pCheckpoint);
}

static xwork_status xwork__file_persistence_load_last_artifact_cb(
    const char *sRunId,
    xwork_artifact *pArtifact,
    void *pUserData
)
{
    const xwork_file_persistence *pStore = (const xwork_file_persistence *)pUserData;

    return xwork_file_persistence_load_last_artifact(pStore, sRunId, pArtifact);
}

static xwork_status xwork__file_persistence_load_event_cb(
    const char *sRunId,
    const char *sEventId,
    xwork_event *pEvent,
    void *pUserData
)
{
    const xwork_file_persistence *pStore = (const xwork_file_persistence *)pUserData;

    return xwork_file_persistence_load_event(pStore, sRunId, sEventId, pEvent);
}

static xwork_status xwork__file_persistence_load_checkpoint_cb(
    const char *sRunId,
    const char *sCheckpointId,
    xwork_checkpoint *pCheckpoint,
    void *pUserData
)
{
    const xwork_file_persistence *pStore = (const xwork_file_persistence *)pUserData;

    return xwork_file_persistence_load_checkpoint(
        pStore,
        sRunId,
        sCheckpointId,
        pCheckpoint
    );
}

static xwork_status xwork__file_persistence_load_artifact_cb(
    const char *sRunId,
    const char *sArtifactId,
    xwork_artifact *pArtifact,
    void *pUserData
)
{
    const xwork_file_persistence *pStore = (const xwork_file_persistence *)pUserData;

    return xwork_file_persistence_load_artifact(
        pStore,
        sRunId,
        sArtifactId,
        pArtifact
    );
}
