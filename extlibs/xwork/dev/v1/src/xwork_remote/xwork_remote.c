#include "../xwork_core/xwork_internal.h"

#include <stdio.h>

#define XWORK__REMOTE_DEFAULT_LEASE_MS 30000u
#define XWORK__REMOTE_PROTOCOL_CURRENT XWORK_REMOTE_PROTOCOL_VERSION_CURRENT

typedef struct xwork__remote_worker_record xwork__remote_worker_record;
typedef struct xwork__remote_task_record xwork__remote_task_record;
typedef struct xwork__remote_blob_chunk_record xwork__remote_blob_chunk_record;

struct xwork_worker {
    xwork__remote_worker_record *pRecord;
};

struct xwork__remote_worker_record {
    xwork_worker tPublic;
    char *sWorkerId;
    char *sDisplayName;
    char *sEndpoint;
    size_t iProtocolVersion;
    char **psCapabilities;
    size_t iCapabilityCount;
    char **psLabels;
    size_t iLabelCount;
    size_t iLeaseTimeoutMs;
    size_t iLastHeartbeatMs;
    size_t iLeaseExpiresMs;
    size_t iClaimedCount;
    size_t iCompletedCount;
    size_t iFailedCount;
    xwork_worker_state eState;
    xwork_runtime *pRuntime;
    xwork__remote_worker_record *pNext;
};

struct xwork__remote_task_record {
    char *sTaskId;
    char *sAssignmentId;
    char *sWorkerId;
    char *sOperationId;
    char *sRequestJson;
    char *sRequiredCapability;
    char *sOutputText;
    char *sVisibleSummary;
    char *sErrorKind;
    char *sErrorMessage;
    xwork_artifact_summary *pArtifacts;
    size_t iArtifactCount;
    xwork_remote_output_chunk_summary *pOutputChunks;
    size_t iOutputChunkCount;
    xwork_remote_task_kind eKind;
    xwork_remote_task_state eState;
    xwork_host_service_kind eHostService;
    size_t iAttemptCount;
    size_t iAssignedAtMs;
    size_t iCompletedAtMs;
    size_t iTimeoutMs;
    size_t iProtocolVersion;
    xwork_status iStatus;
    bool bRetryable;
    void *pUserData;
    xwork__remote_task_record *pNext;
};

struct xwork__remote_blob_chunk_record {
    xwork_remote_blob_chunk_summary tSummary;
    xwork__remote_blob_chunk_record *pNext;
};

struct xwork_control_plane {
    char *sPlaneId;
    xwork_runtime *pRuntime;
    xwork_remote_transport_kind eTransport;
    size_t iProtocolVersion;
    size_t iDefaultLeaseTimeoutMs;
    size_t iNowMs;
    bool bStarted;
    char **psAllowedCapabilities;
    size_t iAllowedCapabilityCount;
    bool bEnforceCapabilityAllowlist;
    xwork_autonomy_mode eAutonomy;
    xwork_approval_mode eApprovalMode;
    bool bAutoApproveTasks;
    bool bEnforceTaskPolicy;
    bool bEnforceNetworkPolicy;
    bool bRedactTaskSecrets;
    size_t iNextAssignmentSequence;
    xwork__remote_worker_record *pWorkers;
    xwork__remote_task_record *pTasks;
    xwork__remote_blob_chunk_record *pBlobChunks;
};

static xwork_status xwork__remote_copy_string_array(
    const char **psItems,
    size_t iCount,
    char ***ppsCopy,
    size_t *piCopyCount
)
{
    char **psCopy = NULL;
    size_t i;

    if ( !ppsCopy || !piCopyCount ) {
        return XWORK_ERROR_INVALID_ARGUMENT;
    }
    *ppsCopy = NULL;
    *piCopyCount = 0u;
    if ( iCount == 0u ) {
        return XWORK_OK;
    }
    if ( !psItems ) {
        return XWORK_ERROR_INVALID_ARGUMENT;
    }
    psCopy = (char **)calloc(iCount, sizeof(*psCopy));
    if ( !psCopy ) {
        return XWORK_ERROR_NO_MEMORY;
    }
    for ( i = 0u; i < iCount; ++i ) {
        if ( !psItems[i] || !psItems[i][0] ) {
            xwork__free_str_array(&psCopy, &i);
            return XWORK_ERROR_INVALID_ARGUMENT;
        }
        psCopy[i] = xwork__dup_cstr(psItems[i]);
        if ( !psCopy[i] ) {
            size_t iCopied = i + 1u;
            xwork__free_str_array(&psCopy, &iCopied);
            return XWORK_ERROR_NO_MEMORY;
        }
    }
    *ppsCopy = psCopy;
    *piCopyCount = iCount;
    return XWORK_OK;
}

static bool xwork__remote_string_list_contains(
    char **psItems,
    size_t iCount,
    const char *sNeedle
)
{
    size_t i;

    if ( !sNeedle || !sNeedle[0] ) {
        return true;
    }
    for ( i = 0u; i < iCount; ++i ) {
        if ( psItems[i] && strcmp(psItems[i], sNeedle) == 0 ) {
            return true;
        }
    }
    return false;
}

static xwork_status xwork__remote_validate_protocol_version(size_t iProtocolVersion)
{
    if ( iProtocolVersion == 0u ||
         iProtocolVersion == XWORK__REMOTE_PROTOCOL_CURRENT ) {
        return XWORK_OK;
    }
    return XWORK_ERROR_UNSUPPORTED;
}

static size_t xwork__remote_resolve_protocol_version(size_t iProtocolVersion)
{
    return iProtocolVersion
        ? iProtocolVersion
        : XWORK__REMOTE_PROTOCOL_CURRENT;
}

static const char *xwork__remote_default_error_kind(xwork_status iStatus)
{
    switch ( iStatus ) {
    case XWORK_OK:
        return NULL;
    case XWORK_ERROR_INVALID_ARGUMENT:
        return "invalid_argument";
    case XWORK_ERROR_NO_MEMORY:
        return "no_memory";
    case XWORK_ERROR_ALREADY_EXISTS:
        return "already_exists";
    case XWORK_ERROR_NOT_FOUND:
        return "not_found";
    case XWORK_ERROR_INVALID_STATE:
        return "invalid_state";
    case XWORK_ERROR_EXTERNAL_FAILURE:
        return "external_failure";
    case XWORK_ERROR_UNSUPPORTED:
        return "unsupported";
    case XWORK_ERROR_CANCELLED:
        return "cancelled";
    case XWORK_ERROR_PAUSED:
        return "approval_required";
    default:
        return "unknown";
    }
}

static bool xwork__remote_ascii_contains_ci(const char *sText, const char *sPattern)
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
        bool bMatch = true;
        for ( j = 0u; j < iPatternLength; ++j ) {
            if ( tolower((unsigned char)sText[i + j]) !=
                 tolower((unsigned char)sPattern[j]) ) {
                bMatch = false;
                break;
            }
        }
        if ( bMatch ) {
            return true;
        }
    }
    return false;
}

static bool xwork__remote_key_looks_secret(const char *sKey, size_t iKeyLength)
{
    char sBuffer[64];
    size_t iCopyLength;

    if ( !sKey || iKeyLength == 0u ) {
        return false;
    }
    iCopyLength = iKeyLength < sizeof(sBuffer) - 1u
        ? iKeyLength
        : sizeof(sBuffer) - 1u;
    memcpy(sBuffer, sKey, iCopyLength);
    sBuffer[iCopyLength] = '\0';
    return xwork__remote_ascii_contains_ci(sBuffer, "password") ||
        xwork__remote_ascii_contains_ci(sBuffer, "passwd") ||
        xwork__remote_ascii_contains_ci(sBuffer, "secret") ||
        xwork__remote_ascii_contains_ci(sBuffer, "api_key") ||
        xwork__remote_ascii_contains_ci(sBuffer, "apikey") ||
        xwork__remote_ascii_contains_ci(sBuffer, "token") ||
        xwork__remote_ascii_contains_ci(sBuffer, "authorization");
}

static xwork_status xwork__remote_redact_json_secrets(
    const char *sText,
    char **psRedacted
)
{
    const char *s;
    char *sOut;
    size_t iLength;
    size_t iOut = 0u;

    if ( !psRedacted ) {
        return XWORK_ERROR_INVALID_ARGUMENT;
    }
    *psRedacted = NULL;
    if ( !sText ) {
        return XWORK_OK;
    }
    iLength = strlen(sText);
    sOut = (char *)calloc(iLength + 32u, 1u);
    if ( !sOut ) {
        return XWORK_ERROR_NO_MEMORY;
    }
    s = sText;
    while ( *s ) {
        const char *sKeyStart;
        const char *sKeyEnd;
        const char *sCursor;
        bool bSecretKey;

        if ( *s != '"' ) {
            sOut[iOut++] = *s++;
            continue;
        }
        sKeyStart = s + 1;
        sCursor = sKeyStart;
        while ( *sCursor && !(*sCursor == '"' && sCursor[-1] != '\\') ) {
            ++sCursor;
        }
        if ( *sCursor != '"' ) {
            sOut[iOut++] = *s++;
            continue;
        }
        sKeyEnd = sCursor;
        bSecretKey = xwork__remote_key_looks_secret(
            sKeyStart,
            (size_t)(sKeyEnd - sKeyStart)
        );
        while ( s <= sKeyEnd ) {
            sOut[iOut++] = *s++;
        }
        sCursor = s;
        while ( *sCursor == ' ' || *sCursor == '\t' ||
                *sCursor == '\r' || *sCursor == '\n' ) {
            ++sCursor;
        }
        if ( !bSecretKey || *sCursor != ':' ) {
            continue;
        }
        while ( s < sCursor ) {
            sOut[iOut++] = *s++;
        }
        sOut[iOut++] = *s++;
        while ( *s == ' ' || *s == '\t' || *s == '\r' || *s == '\n' ) {
            sOut[iOut++] = *s++;
        }
        memcpy(&sOut[iOut], "\"[REDACTED]\"", 12u);
        iOut += 12u;
        if ( *s == '"' ) {
            ++s;
            while ( *s && !(*s == '"' && s[-1] != '\\') ) {
                ++s;
            }
            if ( *s == '"' ) {
                ++s;
            }
        } else {
            while ( *s && *s != ',' && *s != '}' && *s != ']' &&
                    *s != '\r' && *s != '\n' ) {
                ++s;
            }
        }
    }
    sOut[iOut] = '\0';
    *psRedacted = sOut;
    return XWORK_OK;
}

static xwork_status xwork__remote_copy_observable_text(
    const xwork_control_plane *pPlane,
    char **psTarget,
    const char *sText
)
{
    char *sRedacted = NULL;
    xwork_status iStatus;

    if ( !pPlane || !pPlane->bRedactTaskSecrets ) {
        return xwork__replace_cstr(psTarget, sText);
    }
    iStatus = xwork__remote_redact_json_secrets(sText, &sRedacted);
    if ( iStatus != XWORK_OK ) {
        return iStatus;
    }
    iStatus = xwork__replace_cstr(psTarget, sRedacted);
    free(sRedacted);
    return iStatus;
}

static bool xwork__remote_capability_allowed(
    const xwork_control_plane *pPlane,
    const char *sCapability
)
{
    if ( !pPlane || !pPlane->bEnforceCapabilityAllowlist ||
         !sCapability || !sCapability[0] ) {
        return true;
    }
    return xwork__remote_string_list_contains(
        pPlane->psAllowedCapabilities,
        pPlane->iAllowedCapabilityCount,
        sCapability
    );
}

static xwork_status xwork__remote_validate_capabilities_allowed(
    const xwork_control_plane *pPlane,
    const char **psCapabilities,
    size_t iCapabilityCount
)
{
    size_t i;

    if ( !pPlane || !pPlane->bEnforceCapabilityAllowlist ) {
        return XWORK_OK;
    }
    if ( iCapabilityCount > 0u && !psCapabilities ) {
        return XWORK_ERROR_INVALID_ARGUMENT;
    }
    for ( i = 0u; i < iCapabilityCount; ++i ) {
        if ( !psCapabilities[i] || !psCapabilities[i][0] ) {
            return XWORK_ERROR_INVALID_ARGUMENT;
        }
        if ( !xwork__remote_capability_allowed(pPlane, psCapabilities[i]) ) {
            return XWORK_ERROR_UNSUPPORTED;
        }
    }
    return XWORK_OK;
}

static xwork_tool_side_effect xwork__remote_task_side_effect(
    xwork_remote_task_kind eKind,
    xwork_host_service_kind eHostService,
    const char *sOperationId
)
{
    if ( eKind == XWORK_REMOTE_TASK_PROCESS_EXEC ) {
        return XWORK_SIDE_EFFECT_PROCESS_EXEC;
    }
    switch ( eHostService ) {
        case XWORK_HOST_FILESYSTEM:
            if ( sOperationId &&
                 (strcmp(sOperationId, XWORK_HOST_FILESYSTEM_READ_TEXT) == 0 ||
                  strcmp(sOperationId, XWORK_HOST_FILESYSTEM_LIST) == 0 ||
                  strcmp(sOperationId, XWORK_HOST_FILESYSTEM_STAT) == 0 ||
                  strcmp(sOperationId, XWORK_HOST_FILESYSTEM_GLOB) == 0) ) {
                return XWORK_SIDE_EFFECT_READ_ONLY;
            }
            return XWORK_SIDE_EFFECT_WORKSPACE_WRITE;
        case XWORK_HOST_PROCESS:
            if ( sOperationId &&
                 (strcmp(sOperationId, XWORK_HOST_PROCESS_LIST_TERMINALS) == 0 ||
                  strcmp(sOperationId, XWORK_HOST_PROCESS_TERMINAL_READ) == 0 ||
                  strcmp(sOperationId, XWORK_HOST_PROCESS_TERMINAL_RESIZE) == 0) ) {
                return XWORK_SIDE_EFFECT_READ_ONLY;
            }
            return XWORK_SIDE_EFFECT_PROCESS_EXEC;
        case XWORK_HOST_VCS:
        case XWORK_HOST_DIAGNOSTICS:
            return XWORK_SIDE_EFFECT_READ_ONLY;
        case XWORK_HOST_NETWORK:
            return XWORK_SIDE_EFFECT_NETWORK_ACCESS;
        case XWORK_HOST_EDITOR:
            if ( sOperationId && strcmp(sOperationId, XWORK_HOST_EDITOR_OPEN_BUFFER) == 0 ) {
                return XWORK_SIDE_EFFECT_READ_ONLY;
            }
            return XWORK_SIDE_EFFECT_WORKSPACE_WRITE;
        default:
            return XWORK_SIDE_EFFECT_EXTERNAL_MUTATION;
    }
}

static xwork_approval_mode xwork__remote_task_approval_mode(
    xwork_tool_side_effect eSideEffect
)
{
    return eSideEffect == XWORK_SIDE_EFFECT_READ_ONLY
        ? XWORK_APPROVAL_DEFAULT
        : XWORK_APPROVAL_ALWAYS;
}

static xwork_status xwork__remote_json_copy_string_field(
    const char *sJson,
    const char *sFieldName,
    char **psValue
)
{
    char sPattern[64];
    const char *sCursor;
    const char *sStart;
    const char *sEnd;
    size_t iLength;

    if ( !psValue ) {
        return XWORK_ERROR_INVALID_ARGUMENT;
    }
    *psValue = NULL;
    if ( !sJson || !sFieldName || !sFieldName[0] ) {
        return XWORK_OK;
    }
    if ( snprintf(sPattern, sizeof(sPattern), "\"%s\"", sFieldName) >=
         (int)sizeof(sPattern) ) {
        return XWORK_ERROR_INVALID_ARGUMENT;
    }
    sCursor = strstr(sJson, sPattern);
    if ( !sCursor ) {
        return XWORK_OK;
    }
    sCursor += strlen(sPattern);
    while ( *sCursor == ' ' || *sCursor == '\t' || *sCursor == '\r' ||
            *sCursor == '\n' ) {
        ++sCursor;
    }
    if ( *sCursor != ':' ) {
        return XWORK_OK;
    }
    ++sCursor;
    while ( *sCursor == ' ' || *sCursor == '\t' || *sCursor == '\r' ||
            *sCursor == '\n' ) {
        ++sCursor;
    }
    if ( *sCursor != '"' ) {
        return XWORK_OK;
    }
    sStart = ++sCursor;
    while ( *sCursor ) {
        if ( *sCursor == '"' && (sCursor == sStart || sCursor[-1] != '\\') ) {
            break;
        }
        ++sCursor;
    }
    if ( *sCursor != '"' ) {
        return XWORK_ERROR_INVALID_ARGUMENT;
    }
    sEnd = sCursor;
    iLength = (size_t)(sEnd - sStart);
    *psValue = (char *)calloc(iLength + 1u, 1u);
    if ( !*psValue ) {
        return XWORK_ERROR_NO_MEMORY;
    }
    memcpy(*psValue, sStart, iLength);
    return XWORK_OK;
}

static xwork_status xwork__remote_enforce_network_policy(
    const xwork_control_plane *pPlane,
    const xwork_remote_task_options *pOptions
)
{
    xwork_network_policy_eval_input tInput;
    xwork_network_policy_decision tDecision;
    char *sHost = NULL;
    char *sUrl = NULL;
    xwork_status iStatus;

    if ( !pPlane || !pPlane->bEnforceNetworkPolicy || !pOptions ||
         pOptions->eKind != XWORK_REMOTE_TASK_HOST_TOOL ||
         pOptions->eHostService != XWORK_HOST_NETWORK ) {
        return XWORK_OK;
    }
    iStatus = xwork__remote_json_copy_string_field(pOptions->sRequestJson, "host", &sHost);
    if ( iStatus != XWORK_OK ) goto done;
    iStatus = xwork__remote_json_copy_string_field(pOptions->sRequestJson, "url", &sUrl);
    if ( iStatus != XWORK_OK ) goto done;
    xwork_network_policy_eval_input_init(&tInput);
    tInput.sHost = sHost;
    tInput.sUrl = sUrl;
    tInput.bNetworkAccessRequested = true;
    xwork_network_policy_decision_init(&tDecision);
    iStatus = xwork_policy_evaluate_network_access(
        pPlane->pRuntime ? &pPlane->pRuntime->tPolicy : NULL,
        &tInput,
        &tDecision
    );
    if ( iStatus == XWORK_OK && !tDecision.bAllowed ) {
        iStatus = XWORK_ERROR_UNSUPPORTED;
    }

done:
    free(sHost);
    free(sUrl);
    return iStatus;
}

static xwork_status xwork__remote_enforce_task_policy(
    const xwork_control_plane *pPlane,
    const xwork_remote_task_options *pOptions
)
{
    xwork_approval_eval_input tInput;
    xwork_approval_decision tDecision;
    xwork_tool_side_effect eSideEffect;
    xwork_approval_mode eApprovalMode;
    xwork_status iStatus;

    if ( !pPlane || !pPlane->bEnforceTaskPolicy || !pOptions ) {
        return XWORK_OK;
    }
    eSideEffect = xwork__remote_task_side_effect(
        pOptions->eKind,
        pOptions->eKind == XWORK_REMOTE_TASK_PROCESS_EXEC
            ? XWORK_HOST_PROCESS
            : pOptions->eHostService,
        pOptions->eKind == XWORK_REMOTE_TASK_PROCESS_EXEC
            ? XWORK_HOST_PROCESS_EXEC
            : pOptions->sOperationId
    );
    eApprovalMode = pPlane->eApprovalMode == XWORK_APPROVAL_DEFAULT
        ? xwork__remote_task_approval_mode(eSideEffect)
        : pPlane->eApprovalMode;
    xwork_approval_eval_input_init(&tInput);
    tInput.eAutonomy = pPlane->eAutonomy;
    tInput.eApprovalMode = eApprovalMode;
    tInput.eSideEffect = eSideEffect;
    tInput.bAutoApproveRequested = pPlane->bAutoApproveTasks;
    xwork_approval_decision_init(&tDecision);
    iStatus = xwork_policy_evaluate_approval(
        pPlane->pRuntime ? &pPlane->pRuntime->tPolicy : NULL,
        &tInput,
        &tDecision
    );
    if ( iStatus != XWORK_OK ) {
        return iStatus;
    }
    if ( tDecision.bRequiresApproval && !tDecision.bAutoApproved ) {
        return XWORK_ERROR_PAUSED;
    }
    return XWORK_OK;
}

static xwork__remote_worker_record *xwork__remote_find_worker(
    const xwork_control_plane *pPlane,
    const char *sWorkerId
)
{
    xwork__remote_worker_record *pWorker;

    if ( !pPlane || !sWorkerId || !sWorkerId[0] ) {
        return NULL;
    }
    for ( pWorker = pPlane->pWorkers; pWorker; pWorker = pWorker->pNext ) {
        if ( pWorker->sWorkerId && strcmp(pWorker->sWorkerId, sWorkerId) == 0 ) {
            return pWorker;
        }
    }
    return NULL;
}

static xwork__remote_task_record *xwork__remote_find_task(
    const xwork_control_plane *pPlane,
    const char *sTaskId
)
{
    xwork__remote_task_record *pTask;

    if ( !pPlane || !sTaskId || !sTaskId[0] ) {
        return NULL;
    }
    for ( pTask = pPlane->pTasks; pTask; pTask = pTask->pNext ) {
        if ( pTask->sTaskId && strcmp(pTask->sTaskId, sTaskId) == 0 ) {
            return pTask;
        }
    }
    return NULL;
}

static xwork__remote_task_record *xwork__remote_find_assignment(
    const xwork_control_plane *pPlane,
    const char *sAssignmentId
)
{
    xwork__remote_task_record *pTask;

    if ( !pPlane || !sAssignmentId || !sAssignmentId[0] ) {
        return NULL;
    }
    for ( pTask = pPlane->pTasks; pTask; pTask = pTask->pNext ) {
        if ( pTask->sAssignmentId &&
             strcmp(pTask->sAssignmentId, sAssignmentId) == 0 ) {
            return pTask;
        }
    }
    return NULL;
}

static void xwork__remote_free_worker(xwork__remote_worker_record *pWorker)
{
    if ( !pWorker ) {
        return;
    }
    free(pWorker->sWorkerId);
    free(pWorker->sDisplayName);
    free(pWorker->sEndpoint);
    xwork__free_str_array(&pWorker->psCapabilities, &pWorker->iCapabilityCount);
    xwork__free_str_array(&pWorker->psLabels, &pWorker->iLabelCount);
    free(pWorker);
}

static void xwork__remote_free_task(xwork__remote_task_record *pTask)
{
    if ( !pTask ) {
        return;
    }
    free(pTask->sTaskId);
    free(pTask->sAssignmentId);
    free(pTask->sWorkerId);
    free(pTask->sOperationId);
    free(pTask->sRequestJson);
    free(pTask->sRequiredCapability);
    free(pTask->sOutputText);
    free(pTask->sVisibleSummary);
    free(pTask->sErrorKind);
    free(pTask->sErrorMessage);
    {
        xwork_artifact_summary_list tList;
        tList.pItems = pTask->pArtifacts;
        tList.iCount = pTask->iArtifactCount;
        tList.bHasMore = false;
        tList.iNextAfterSequence = 0u;
        xwork_artifact_summary_list_reset(&tList);
    }
    {
        xwork_remote_output_chunk_summary_list tList;
        tList.pItems = pTask->pOutputChunks;
        tList.iCount = pTask->iOutputChunkCount;
        xwork_remote_output_chunk_summary_list_reset(&tList);
    }
    free(pTask);
}

void xwork_control_plane_options_init(xwork_control_plane_options *pOptions)
{
    if ( pOptions ) {
        memset(pOptions, 0, sizeof(*pOptions));
        pOptions->eTransport = XWORK_REMOTE_TRANSPORT_IN_PROCESS;
        pOptions->iProtocolVersion = XWORK__REMOTE_PROTOCOL_CURRENT;
        pOptions->iDefaultLeaseTimeoutMs = XWORK__REMOTE_DEFAULT_LEASE_MS;
        pOptions->eAutonomy = XWORK_AUTONOMY_SEMI_AUTO;
        pOptions->eApprovalMode = XWORK_APPROVAL_DEFAULT;
        pOptions->bAutoApproveTasks = true;
        pOptions->bEnforceTaskPolicy = true;
        pOptions->bEnforceNetworkPolicy = true;
        pOptions->bRedactTaskSecrets = true;
    }
}

void xwork_worker_options_init(xwork_worker_options *pOptions)
{
    if ( pOptions ) {
        memset(pOptions, 0, sizeof(*pOptions));
        pOptions->iProtocolVersion = XWORK__REMOTE_PROTOCOL_CURRENT;
    }
}

void xwork_worker_summary_init(xwork_worker_summary *pSummary)
{
    if ( pSummary ) {
        memset(pSummary, 0, sizeof(*pSummary));
        pSummary->iProtocolVersion = XWORK__REMOTE_PROTOCOL_CURRENT;
        pSummary->eState = XWORK_WORKER_REGISTERED;
    }
}

void xwork_worker_summary_reset(xwork_worker_summary *pSummary)
{
    if ( !pSummary ) {
        return;
    }
    free((char *)pSummary->sWorkerId);
    free((char *)pSummary->sDisplayName);
    free((char *)pSummary->sEndpoint);
    xwork_worker_summary_init(pSummary);
}

void xwork_worker_summary_list_init(xwork_worker_summary_list *pList)
{
    if ( pList ) {
        memset(pList, 0, sizeof(*pList));
    }
}

void xwork_worker_summary_list_reset(xwork_worker_summary_list *pList)
{
    xwork_worker_summary *pItems;
    size_t i;

    if ( !pList ) {
        return;
    }
    pItems = pList->pItems;
    if ( pItems ) {
        for ( i = 0u; i < pList->iCount; ++i ) {
            xwork_worker_summary_reset(&pItems[i]);
        }
        free(pItems);
    }
    xwork_worker_summary_list_init(pList);
}

void xwork_worker_snapshot_init(xwork_worker_snapshot *pSnapshot)
{
    if ( pSnapshot ) {
        memset(pSnapshot, 0, sizeof(*pSnapshot));
        pSnapshot->iProtocolVersion = XWORK__REMOTE_PROTOCOL_CURRENT;
        pSnapshot->eState = XWORK_WORKER_REGISTERED;
    }
}

void xwork_worker_snapshot_reset(xwork_worker_snapshot *pSnapshot)
{
    if ( !pSnapshot ) {
        return;
    }
    free((char *)pSnapshot->sWorkerId);
    free((char *)pSnapshot->sDisplayName);
    free((char *)pSnapshot->sEndpoint);
    xwork__free_str_array((char ***)&pSnapshot->psCapabilities, &pSnapshot->iCapabilityCount);
    xwork__free_str_array((char ***)&pSnapshot->psLabels, &pSnapshot->iLabelCount);
    xwork_worker_snapshot_init(pSnapshot);
}

void xwork_worker_snapshot_list_init(xwork_worker_snapshot_list *pList)
{
    if ( pList ) {
        memset(pList, 0, sizeof(*pList));
    }
}

void xwork_worker_snapshot_list_reset(xwork_worker_snapshot_list *pList)
{
    xwork_worker_snapshot *pItems;
    size_t i;

    if ( !pList ) {
        return;
    }
    pItems = pList->pItems;
    if ( pItems ) {
        for ( i = 0u; i < pList->iCount; ++i ) {
            xwork_worker_snapshot_reset(&pItems[i]);
        }
        free(pItems);
    }
    xwork_worker_snapshot_list_init(pList);
}

void xwork_remote_task_options_init(xwork_remote_task_options *pOptions)
{
    if ( pOptions ) {
        memset(pOptions, 0, sizeof(*pOptions));
        pOptions->eKind = XWORK_REMOTE_TASK_HOST_TOOL;
        pOptions->eHostService = XWORK_HOST_PROCESS;
    }
}

void xwork_remote_task_summary_init(xwork_remote_task_summary *pSummary)
{
    if ( pSummary ) {
        memset(pSummary, 0, sizeof(*pSummary));
        pSummary->iProtocolVersion = XWORK__REMOTE_PROTOCOL_CURRENT;
        pSummary->eKind = XWORK_REMOTE_TASK_HOST_TOOL;
        pSummary->eState = XWORK_REMOTE_TASK_QUEUED;
        pSummary->eHostService = XWORK_HOST_PROCESS;
    }
}

void xwork_remote_task_summary_reset(xwork_remote_task_summary *pSummary)
{
    if ( !pSummary ) {
        return;
    }
    free((char *)pSummary->sTaskId);
    free((char *)pSummary->sAssignmentId);
    free((char *)pSummary->sWorkerId);
    free((char *)pSummary->sOperationId);
    free((char *)pSummary->sRequestJson);
    free((char *)pSummary->sRequiredCapability);
    free((char *)pSummary->sOutputText);
    free((char *)pSummary->sVisibleSummary);
    free((char *)pSummary->sErrorKind);
    free((char *)pSummary->sErrorMessage);
    {
        xwork_artifact_summary_list tArtifacts;
        tArtifacts.pItems = pSummary->pArtifacts;
        tArtifacts.iCount = pSummary->iArtifactCount;
        tArtifacts.bHasMore = false;
        tArtifacts.iNextAfterSequence = 0u;
        xwork_artifact_summary_list_reset(&tArtifacts);
    }
    {
        xwork_remote_output_chunk_summary_list tChunks;
        tChunks.pItems = pSummary->pOutputChunks;
        tChunks.iCount = pSummary->iOutputChunkCount;
        xwork_remote_output_chunk_summary_list_reset(&tChunks);
    }
    xwork_remote_task_summary_init(pSummary);
}

void xwork_remote_task_summary_list_init(xwork_remote_task_summary_list *pList)
{
    if ( pList ) {
        memset(pList, 0, sizeof(*pList));
    }
}

void xwork_remote_task_summary_list_reset(xwork_remote_task_summary_list *pList)
{
    xwork_remote_task_summary *pItems;
    size_t i;

    if ( !pList ) {
        return;
    }
    pItems = pList->pItems;
    if ( pItems ) {
        for ( i = 0u; i < pList->iCount; ++i ) {
            xwork_remote_task_summary_reset(&pItems[i]);
        }
        free(pItems);
    }
    xwork_remote_task_summary_list_init(pList);
}

void xwork_remote_task_snapshot_init(xwork_remote_task_snapshot *pSnapshot)
{
    if ( pSnapshot ) {
        memset(pSnapshot, 0, sizeof(*pSnapshot));
        pSnapshot->iProtocolVersion = XWORK__REMOTE_PROTOCOL_CURRENT;
        pSnapshot->eKind = XWORK_REMOTE_TASK_HOST_TOOL;
        pSnapshot->eState = XWORK_REMOTE_TASK_QUEUED;
        pSnapshot->eHostService = XWORK_HOST_PROCESS;
    }
}

void xwork_remote_task_snapshot_reset(xwork_remote_task_snapshot *pSnapshot)
{
    if ( !pSnapshot ) {
        return;
    }
    free((char *)pSnapshot->sTaskId);
    free((char *)pSnapshot->sAssignmentId);
    free((char *)pSnapshot->sWorkerId);
    free((char *)pSnapshot->sOperationId);
    free((char *)pSnapshot->sRequestJson);
    free((char *)pSnapshot->sRequiredCapability);
    free((char *)pSnapshot->sOutputText);
    free((char *)pSnapshot->sVisibleSummary);
    free((char *)pSnapshot->sErrorKind);
    free((char *)pSnapshot->sErrorMessage);
    {
        xwork_artifact_summary_list tArtifacts;
        tArtifacts.pItems = pSnapshot->pArtifacts;
        tArtifacts.iCount = pSnapshot->iArtifactCount;
        tArtifacts.bHasMore = false;
        tArtifacts.iNextAfterSequence = 0u;
        xwork_artifact_summary_list_reset(&tArtifacts);
    }
    {
        xwork_remote_output_chunk_summary_list tChunks;
        tChunks.pItems = pSnapshot->pOutputChunks;
        tChunks.iCount = pSnapshot->iOutputChunkCount;
        xwork_remote_output_chunk_summary_list_reset(&tChunks);
    }
    xwork_remote_task_snapshot_init(pSnapshot);
}

void xwork_remote_task_snapshot_list_init(xwork_remote_task_snapshot_list *pList)
{
    if ( pList ) {
        memset(pList, 0, sizeof(*pList));
    }
}

void xwork_remote_task_snapshot_list_reset(xwork_remote_task_snapshot_list *pList)
{
    xwork_remote_task_snapshot *pItems;
    size_t i;

    if ( !pList ) {
        return;
    }
    pItems = pList->pItems;
    if ( pItems ) {
        for ( i = 0u; i < pList->iCount; ++i ) {
            xwork_remote_task_snapshot_reset(&pItems[i]);
        }
        free(pItems);
    }
    xwork_remote_task_snapshot_list_init(pList);
}

void xwork_remote_task_assignment_init(xwork_remote_task_assignment *pAssignment)
{
    if ( pAssignment ) {
        memset(pAssignment, 0, sizeof(*pAssignment));
        pAssignment->iProtocolVersion = XWORK__REMOTE_PROTOCOL_CURRENT;
        pAssignment->eKind = XWORK_REMOTE_TASK_HOST_TOOL;
        pAssignment->eHostService = XWORK_HOST_PROCESS;
    }
}

void xwork_remote_task_assignment_reset(xwork_remote_task_assignment *pAssignment)
{
    if ( !pAssignment ) {
        return;
    }
    free((char *)pAssignment->sTaskId);
    free((char *)pAssignment->sAssignmentId);
    free((char *)pAssignment->sWorkerId);
    free((char *)pAssignment->sOperationId);
    free((char *)pAssignment->sRequestJson);
    free((char *)pAssignment->sRequiredCapability);
    xwork_remote_task_assignment_init(pAssignment);
}

void xwork_remote_task_result_init(xwork_remote_task_result *pResult)
{
    if ( pResult ) {
        memset(pResult, 0, sizeof(*pResult));
        pResult->iStatus = XWORK_OK;
        pResult->iProtocolVersion = XWORK__REMOTE_PROTOCOL_CURRENT;
    }
}

void xwork_remote_output_chunk_init(xwork_remote_output_chunk *pChunk)
{
    if ( pChunk ) {
        memset(pChunk, 0, sizeof(*pChunk));
        pChunk->iProtocolVersion = XWORK__REMOTE_PROTOCOL_CURRENT;
        pChunk->eStream = XWORK_REMOTE_OUTPUT_STDOUT;
    }
}

void xwork_remote_output_chunk_summary_init(
    xwork_remote_output_chunk_summary *pSummary
)
{
    if ( pSummary ) {
        memset(pSummary, 0, sizeof(*pSummary));
        pSummary->eStream = XWORK_REMOTE_OUTPUT_STDOUT;
    }
}

void xwork_remote_output_chunk_summary_reset(
    xwork_remote_output_chunk_summary *pSummary
)
{
    if ( !pSummary ) {
        return;
    }
    free((char *)pSummary->sContentHash);
    free((char *)pSummary->sText);
    xwork_remote_output_chunk_summary_init(pSummary);
}

void xwork_remote_output_chunk_summary_list_init(
    xwork_remote_output_chunk_summary_list *pList
)
{
    if ( pList ) {
        memset(pList, 0, sizeof(*pList));
    }
}

void xwork_remote_output_chunk_summary_list_reset(
    xwork_remote_output_chunk_summary_list *pList
)
{
    xwork_remote_output_chunk_summary *pItems;
    size_t i;

    if ( !pList ) {
        return;
    }
    pItems = pList->pItems;
    if ( pItems ) {
        for ( i = 0u; i < pList->iCount; ++i ) {
            xwork_remote_output_chunk_summary_reset(&pItems[i]);
        }
        free(pItems);
    }
    xwork_remote_output_chunk_summary_list_init(pList);
}

void xwork_remote_blob_chunk_summary_init(
    xwork_remote_blob_chunk_summary *pSummary
)
{
    if ( pSummary ) {
        memset(pSummary, 0, sizeof(*pSummary));
    }
}

void xwork_remote_blob_chunk_summary_reset(
    xwork_remote_blob_chunk_summary *pSummary
)
{
    if ( !pSummary ) {
        return;
    }
    free((char *)pSummary->sTaskId);
    free((char *)pSummary->sAssignmentId);
    free((char *)pSummary->sWorkerId);
    free((char *)pSummary->sArtifactId);
    free((char *)pSummary->sBlobRef);
    free((char *)pSummary->sContentHash);
    free((void *)pSummary->pChunkData);
    xwork_remote_blob_chunk_summary_init(pSummary);
}

void xwork_remote_blob_chunk_summary_list_init(
    xwork_remote_blob_chunk_summary_list *pList
)
{
    if ( pList ) {
        memset(pList, 0, sizeof(*pList));
    }
}

void xwork_remote_blob_chunk_summary_list_reset(
    xwork_remote_blob_chunk_summary_list *pList
)
{
    xwork_remote_blob_chunk_summary *pItems;
    size_t i;

    if ( !pList ) {
        return;
    }
    pItems = pList->pItems;
    if ( pItems ) {
        for ( i = 0u; i < pList->iCount; ++i ) {
            xwork_remote_blob_chunk_summary_reset(&pItems[i]);
        }
        free(pItems);
    }
    xwork_remote_blob_chunk_summary_list_init(pList);
}

void xwork_remote_artifact_upload_init(xwork_remote_artifact_upload *pUpload)
{
    if ( pUpload ) {
        memset(pUpload, 0, sizeof(*pUpload));
        pUpload->iProtocolVersion = XWORK__REMOTE_PROTOCOL_CURRENT;
    }
}

static xwork_status xwork__remote_copy_artifact_summaries(
    const xwork_artifact_summary *pArtifacts,
    size_t iArtifactCount,
    xwork_artifact_summary **ppArtifacts,
    size_t *piArtifactCount
)
{
    xwork_artifact_summary *pCopy = NULL;
    size_t i;
    xwork_status iStatus;

    if ( !ppArtifacts || !piArtifactCount ) {
        return XWORK_ERROR_INVALID_ARGUMENT;
    }
    *ppArtifacts = NULL;
    *piArtifactCount = 0u;
    if ( iArtifactCount == 0u ) {
        return XWORK_OK;
    }
    if ( !pArtifacts ) {
        return XWORK_ERROR_INVALID_ARGUMENT;
    }
    pCopy = (xwork_artifact_summary *)calloc(iArtifactCount, sizeof(*pCopy));
    if ( !pCopy ) {
        return XWORK_ERROR_NO_MEMORY;
    }
    for ( i = 0u; i < iArtifactCount; ++i ) {
        xwork_artifact_summary_init(&pCopy[i]);
        iStatus = xwork__artifact_summary_copy(&pCopy[i], &pArtifacts[i]);
        if ( iStatus != XWORK_OK ) {
            xwork_artifact_summary_list tList;
            tList.pItems = pCopy;
            tList.iCount = iArtifactCount;
            tList.bHasMore = false;
            tList.iNextAfterSequence = 0u;
            xwork_artifact_summary_list_reset(&tList);
            return iStatus;
        }
    }
    *ppArtifacts = pCopy;
    *piArtifactCount = iArtifactCount;
    return XWORK_OK;
}

static xwork_status xwork__remote_upsert_artifact_summary(
    xwork__remote_task_record *pTask,
    const xwork_artifact_summary *pArtifact
)
{
    xwork_artifact_summary *pNewArtifacts;
    size_t i;
    xwork_status iStatus;

    if ( !pTask || !pArtifact || !pArtifact->sArtifactId || !pArtifact->sArtifactId[0] ) {
        return XWORK_ERROR_INVALID_ARGUMENT;
    }
    for ( i = 0u; i < pTask->iArtifactCount; ++i ) {
        if ( pTask->pArtifacts[i].sArtifactId &&
             strcmp(pTask->pArtifacts[i].sArtifactId, pArtifact->sArtifactId) == 0 ) {
            xwork_artifact_summary_reset(&pTask->pArtifacts[i]);
            return xwork__artifact_summary_copy(&pTask->pArtifacts[i], pArtifact);
        }
    }

    pNewArtifacts = (xwork_artifact_summary *)realloc(
        pTask->pArtifacts,
        (pTask->iArtifactCount + 1u) * sizeof(*pNewArtifacts)
    );
    if ( !pNewArtifacts ) {
        return XWORK_ERROR_NO_MEMORY;
    }
    pTask->pArtifacts = pNewArtifacts;
    xwork_artifact_summary_init(&pTask->pArtifacts[pTask->iArtifactCount]);
    iStatus = xwork__artifact_summary_copy(
        &pTask->pArtifacts[pTask->iArtifactCount],
        pArtifact
    );
    if ( iStatus != XWORK_OK ) {
        return iStatus;
    }
    ++pTask->iArtifactCount;
    return XWORK_OK;
}

static xwork_status xwork__remote_copy_output_chunks(
    const xwork_remote_output_chunk_summary *pChunks,
    size_t iChunkCount,
    xwork_remote_output_chunk_summary **ppChunks,
    size_t *piChunkCount
)
{
    xwork_remote_output_chunk_summary *pCopy = NULL;
    size_t i;
    xwork_status iStatus;

    if ( !ppChunks || !piChunkCount ) {
        return XWORK_ERROR_INVALID_ARGUMENT;
    }
    *ppChunks = NULL;
    *piChunkCount = 0u;
    if ( iChunkCount == 0u ) {
        return XWORK_OK;
    }
    if ( !pChunks ) {
        return XWORK_ERROR_INVALID_ARGUMENT;
    }
    pCopy = (xwork_remote_output_chunk_summary *)calloc(
        iChunkCount,
        sizeof(*pCopy)
    );
    if ( !pCopy ) {
        return XWORK_ERROR_NO_MEMORY;
    }
    for ( i = 0u; i < iChunkCount; ++i ) {
        xwork_remote_output_chunk_summary_init(&pCopy[i]);
        pCopy[i].eStream = pChunks[i].eStream;
        pCopy[i].iChunkIndex = pChunks[i].iChunkIndex;
        pCopy[i].iOffsetBytes = pChunks[i].iOffsetBytes;
        pCopy[i].iByteCount = pChunks[i].iByteCount;
        pCopy[i].bFinalChunk = pChunks[i].bFinalChunk;
        iStatus = xwork__replace_cstr(
            (char **)&pCopy[i].sContentHash,
            pChunks[i].sContentHash
        );
        if ( iStatus != XWORK_OK ) {
            xwork_remote_output_chunk_summary_list tList;
            tList.pItems = pCopy;
            tList.iCount = iChunkCount;
            xwork_remote_output_chunk_summary_list_reset(&tList);
            return iStatus;
        }
        iStatus = xwork__replace_cstr((char **)&pCopy[i].sText, pChunks[i].sText);
        if ( iStatus != XWORK_OK ) {
            xwork_remote_output_chunk_summary_list tList;
            tList.pItems = pCopy;
            tList.iCount = iChunkCount;
            xwork_remote_output_chunk_summary_list_reset(&tList);
            return iStatus;
        }
    }
    *ppChunks = pCopy;
    *piChunkCount = iChunkCount;
    return XWORK_OK;
}

static xwork_status xwork__remote_append_output_chunk(
    xwork__remote_task_record *pTask,
    const xwork_remote_output_chunk *pChunk
)
{
    xwork_remote_output_chunk_summary *pNewChunks;
    xwork_remote_output_chunk_summary *pSummary;
    size_t iByteCount;
    xwork_status iStatus;

    if ( !pTask || !pChunk || !pChunk->sText ) {
        return XWORK_ERROR_INVALID_ARGUMENT;
    }
    if ( pChunk->eStream != XWORK_REMOTE_OUTPUT_STDOUT &&
         pChunk->eStream != XWORK_REMOTE_OUTPUT_STDERR ) {
        return XWORK_ERROR_INVALID_ARGUMENT;
    }
    iByteCount = pChunk->iByteCount ? pChunk->iByteCount : strlen(pChunk->sText);
    if ( iByteCount > strlen(pChunk->sText) ) {
        return XWORK_ERROR_INVALID_ARGUMENT;
    }
    pNewChunks = (xwork_remote_output_chunk_summary *)realloc(
        pTask->pOutputChunks,
        (pTask->iOutputChunkCount + 1u) * sizeof(*pNewChunks)
    );
    if ( !pNewChunks ) {
        return XWORK_ERROR_NO_MEMORY;
    }
    pTask->pOutputChunks = pNewChunks;
    pSummary = &pTask->pOutputChunks[pTask->iOutputChunkCount];
    xwork_remote_output_chunk_summary_init(pSummary);
    pSummary->eStream = pChunk->eStream;
    pSummary->iChunkIndex = pChunk->iChunkIndex;
    pSummary->iOffsetBytes = pChunk->iOffsetBytes;
    pSummary->iByteCount = iByteCount;
    pSummary->bFinalChunk = pChunk->bFinalChunk;
    iStatus = xwork__replace_cstr((char **)&pSummary->sContentHash, pChunk->sContentHash);
    if ( iStatus != XWORK_OK ) {
        return iStatus;
    }
    iStatus = xwork__replace_cstr((char **)&pSummary->sText, pChunk->sText);
    if ( iStatus != XWORK_OK ) {
        return iStatus;
    }
    ++pTask->iOutputChunkCount;
    return XWORK_OK;
}

static bool xwork__remote_nullable_string_equal(const char *sA, const char *sB)
{
    if ( sA == sB ) {
        return true;
    }
    if ( !sA || !sB ) {
        return false;
    }
    return strcmp(sA, sB) == 0;
}

static xwork_status xwork__remote_copy_blob_chunk_summary(
    xwork_remote_blob_chunk_summary *pTarget,
    const xwork_remote_blob_chunk_summary *pSource
)
{
    unsigned char *pData = NULL;
    xwork_status iStatus;

    if ( !pTarget || !pSource ) {
        return XWORK_ERROR_INVALID_ARGUMENT;
    }
    xwork_remote_blob_chunk_summary_reset(pTarget);
    pTarget->iChunkIndex = pSource->iChunkIndex;
    pTarget->iChunkCount = pSource->iChunkCount;
    pTarget->iOffsetBytes = pSource->iOffsetBytes;
    pTarget->iChunkSize = pSource->iChunkSize;
    pTarget->bFinalChunk = pSource->bFinalChunk;
    iStatus = xwork__replace_cstr((char **)&pTarget->sTaskId, pSource->sTaskId);
    if ( iStatus != XWORK_OK ) return iStatus;
    iStatus = xwork__replace_cstr(
        (char **)&pTarget->sAssignmentId,
        pSource->sAssignmentId
    );
    if ( iStatus != XWORK_OK ) return iStatus;
    iStatus = xwork__replace_cstr((char **)&pTarget->sWorkerId, pSource->sWorkerId);
    if ( iStatus != XWORK_OK ) return iStatus;
    iStatus = xwork__replace_cstr(
        (char **)&pTarget->sArtifactId,
        pSource->sArtifactId
    );
    if ( iStatus != XWORK_OK ) return iStatus;
    iStatus = xwork__replace_cstr((char **)&pTarget->sBlobRef, pSource->sBlobRef);
    if ( iStatus != XWORK_OK ) return iStatus;
    iStatus = xwork__replace_cstr(
        (char **)&pTarget->sContentHash,
        pSource->sContentHash
    );
    if ( iStatus != XWORK_OK ) return iStatus;
    if ( pSource->iChunkSize > 0u && pSource->pChunkData ) {
        pData = (unsigned char *)malloc(pSource->iChunkSize);
        if ( !pData ) {
            return XWORK_ERROR_NO_MEMORY;
        }
        memcpy(pData, pSource->pChunkData, pSource->iChunkSize);
        pTarget->pChunkData = pData;
    }
    return XWORK_OK;
}

static xwork_status xwork__remote_append_blob_chunk_summary(
    xwork_remote_blob_chunk_summary_list *pList,
    const xwork_remote_blob_chunk_summary *pSource
)
{
    xwork_remote_blob_chunk_summary *pItems;
    xwork_status iStatus;

    if ( !pList || !pSource ) {
        return XWORK_ERROR_INVALID_ARGUMENT;
    }
    pItems = (xwork_remote_blob_chunk_summary *)realloc(
        pList->pItems,
        (pList->iCount + 1u) * sizeof(*pItems)
    );
    if ( !pItems ) {
        return XWORK_ERROR_NO_MEMORY;
    }
    pList->pItems = pItems;
    xwork_remote_blob_chunk_summary_init(&pList->pItems[pList->iCount]);
    iStatus = xwork__remote_copy_blob_chunk_summary(
        &pList->pItems[pList->iCount],
        pSource
    );
    if ( iStatus != XWORK_OK ) {
        return iStatus;
    }
    ++pList->iCount;
    return XWORK_OK;
}

static void xwork__remote_free_blob_chunk_record(
    xwork__remote_blob_chunk_record *pRecord
)
{
    if ( pRecord ) {
        xwork_remote_blob_chunk_summary_reset(&pRecord->tSummary);
        free(pRecord);
    }
}

static bool xwork__remote_blob_chunk_matches(
    const xwork_remote_blob_chunk_summary *pSummary,
    const char *sTaskId,
    const char *sArtifactId,
    const char *sBlobRef,
    size_t iChunkIndex
)
{
    return pSummary &&
           xwork__remote_nullable_string_equal(pSummary->sTaskId, sTaskId) &&
           xwork__remote_nullable_string_equal(pSummary->sArtifactId, sArtifactId) &&
           xwork__remote_nullable_string_equal(pSummary->sBlobRef, sBlobRef) &&
           pSummary->iChunkIndex == iChunkIndex;
}

static xwork_status xwork__remote_upsert_blob_chunk(
    xwork_control_plane *pPlane,
    const xwork_remote_artifact_upload *pUpload
)
{
    xwork_remote_blob_chunk_summary tSummary;
    xwork__remote_blob_chunk_record *pRecord;
    xwork_status iStatus;

    if ( !pPlane || !pUpload || !pUpload->pArtifact ) {
        return XWORK_ERROR_INVALID_ARGUMENT;
    }
    if ( !pUpload->sBlobRef && pUpload->iChunkSize == 0u ) {
        return XWORK_OK;
    }
    if ( pUpload->iChunkSize > 0u && !pUpload->pChunkData ) {
        return XWORK_ERROR_INVALID_ARGUMENT;
    }
    xwork_remote_blob_chunk_summary_init(&tSummary);
    tSummary.sTaskId = pUpload->sTaskId;
    tSummary.sAssignmentId = pUpload->sAssignmentId;
    tSummary.sWorkerId = pUpload->sWorkerId;
    tSummary.sArtifactId = pUpload->pArtifact->sArtifactId;
    tSummary.sBlobRef = pUpload->sBlobRef;
    tSummary.sContentHash = pUpload->sContentHash;
    tSummary.iChunkIndex = pUpload->iChunkIndex;
    tSummary.iChunkCount = pUpload->iChunkCount;
    tSummary.iOffsetBytes = pUpload->iOffsetBytes;
    tSummary.pChunkData = pUpload->pChunkData;
    tSummary.iChunkSize = pUpload->iChunkSize;
    tSummary.bFinalChunk = pUpload->bFinalChunk;

    for ( pRecord = pPlane->pBlobChunks; pRecord; pRecord = pRecord->pNext ) {
        if ( xwork__remote_blob_chunk_matches(
                 &pRecord->tSummary,
                 tSummary.sTaskId,
                 tSummary.sArtifactId,
                 tSummary.sBlobRef,
                 tSummary.iChunkIndex
             ) ) {
            return xwork__remote_copy_blob_chunk_summary(
                &pRecord->tSummary,
                &tSummary
            );
        }
    }
    pRecord = (xwork__remote_blob_chunk_record *)calloc(1u, sizeof(*pRecord));
    if ( !pRecord ) {
        return XWORK_ERROR_NO_MEMORY;
    }
    xwork_remote_blob_chunk_summary_init(&pRecord->tSummary);
    iStatus = xwork__remote_copy_blob_chunk_summary(&pRecord->tSummary, &tSummary);
    if ( iStatus != XWORK_OK ) {
        xwork__remote_free_blob_chunk_record(pRecord);
        return iStatus;
    }
    pRecord->pNext = pPlane->pBlobChunks;
    pPlane->pBlobChunks = pRecord;
    return XWORK_OK;
}

void xwork_control_plane_snapshot_init(xwork_control_plane_snapshot *pSnapshot)
{
    if ( pSnapshot ) {
        memset(pSnapshot, 0, sizeof(*pSnapshot));
        pSnapshot->eTransport = XWORK_REMOTE_TRANSPORT_IN_PROCESS;
        pSnapshot->iProtocolVersion = XWORK__REMOTE_PROTOCOL_CURRENT;
        xwork_worker_snapshot_list_init(&pSnapshot->tWorkers);
        xwork_remote_task_snapshot_list_init(&pSnapshot->tTasks);
        xwork_remote_blob_chunk_summary_list_init(&pSnapshot->tBlobChunks);
    }
}

void xwork_control_plane_snapshot_reset(xwork_control_plane_snapshot *pSnapshot)
{
    if ( !pSnapshot ) {
        return;
    }
    free((char *)pSnapshot->sPlaneId);
    xwork_worker_snapshot_list_reset(&pSnapshot->tWorkers);
    xwork_remote_task_snapshot_list_reset(&pSnapshot->tTasks);
    xwork_remote_blob_chunk_summary_list_reset(&pSnapshot->tBlobChunks);
    xwork_control_plane_snapshot_init(pSnapshot);
}

static xwork_status xwork__remote_fill_worker_summary(
    xwork_worker_summary *pSummary,
    const xwork__remote_worker_record *pWorker
)
{
    xwork_status iStatus;

    if ( !pSummary || !pWorker ) {
        return XWORK_ERROR_INVALID_ARGUMENT;
    }
    xwork_worker_summary_reset(pSummary);
    iStatus = xwork__replace_cstr((char **)&pSummary->sWorkerId, pWorker->sWorkerId);
    if ( iStatus != XWORK_OK ) return iStatus;
    iStatus = xwork__replace_cstr((char **)&pSummary->sDisplayName, pWorker->sDisplayName);
    if ( iStatus != XWORK_OK ) return iStatus;
    iStatus = xwork__replace_cstr((char **)&pSummary->sEndpoint, pWorker->sEndpoint);
    if ( iStatus != XWORK_OK ) return iStatus;
    pSummary->eState = pWorker->eState;
    pSummary->iProtocolVersion = pWorker->iProtocolVersion;
    pSummary->iCapabilityCount = pWorker->iCapabilityCount;
    pSummary->iLabelCount = pWorker->iLabelCount;
    pSummary->iLastHeartbeatMs = pWorker->iLastHeartbeatMs;
    pSummary->iLeaseExpiresMs = pWorker->iLeaseExpiresMs;
    pSummary->iClaimedCount = pWorker->iClaimedCount;
    pSummary->iCompletedCount = pWorker->iCompletedCount;
    pSummary->iFailedCount = pWorker->iFailedCount;
    return XWORK_OK;
}

static xwork_status xwork__remote_fill_worker_snapshot(
    xwork_worker_snapshot *pSnapshot,
    const xwork__remote_worker_record *pWorker
)
{
    xwork_status iStatus;

    if ( !pSnapshot || !pWorker ) {
        return XWORK_ERROR_INVALID_ARGUMENT;
    }
    xwork_worker_snapshot_reset(pSnapshot);
    iStatus = xwork__replace_cstr((char **)&pSnapshot->sWorkerId, pWorker->sWorkerId);
    if ( iStatus != XWORK_OK ) return iStatus;
    iStatus = xwork__replace_cstr((char **)&pSnapshot->sDisplayName, pWorker->sDisplayName);
    if ( iStatus != XWORK_OK ) return iStatus;
    iStatus = xwork__replace_cstr((char **)&pSnapshot->sEndpoint, pWorker->sEndpoint);
    if ( iStatus != XWORK_OK ) return iStatus;
    iStatus = xwork__remote_copy_string_array(
        (const char **)pWorker->psCapabilities,
        pWorker->iCapabilityCount,
        (char ***)&pSnapshot->psCapabilities,
        &pSnapshot->iCapabilityCount
    );
    if ( iStatus != XWORK_OK ) return iStatus;
    iStatus = xwork__remote_copy_string_array(
        (const char **)pWorker->psLabels,
        pWorker->iLabelCount,
        (char ***)&pSnapshot->psLabels,
        &pSnapshot->iLabelCount
    );
    if ( iStatus != XWORK_OK ) return iStatus;
    pSnapshot->iLeaseTimeoutMs = pWorker->iLeaseTimeoutMs;
    pSnapshot->iProtocolVersion = pWorker->iProtocolVersion;
    pSnapshot->iLastHeartbeatMs = pWorker->iLastHeartbeatMs;
    pSnapshot->iLeaseExpiresMs = pWorker->iLeaseExpiresMs;
    pSnapshot->iClaimedCount = pWorker->iClaimedCount;
    pSnapshot->iCompletedCount = pWorker->iCompletedCount;
    pSnapshot->iFailedCount = pWorker->iFailedCount;
    pSnapshot->eState = pWorker->eState;
    return XWORK_OK;
}

static xwork_status xwork__remote_fill_task_summary(
    xwork_remote_task_summary *pSummary,
    const xwork__remote_task_record *pTask
)
{
    xwork_status iStatus;

    if ( !pSummary || !pTask ) {
        return XWORK_ERROR_INVALID_ARGUMENT;
    }
    xwork_remote_task_summary_reset(pSummary);
    iStatus = xwork__replace_cstr((char **)&pSummary->sTaskId, pTask->sTaskId);
    if ( iStatus != XWORK_OK ) return iStatus;
    iStatus = xwork__replace_cstr((char **)&pSummary->sAssignmentId, pTask->sAssignmentId);
    if ( iStatus != XWORK_OK ) return iStatus;
    iStatus = xwork__replace_cstr((char **)&pSummary->sWorkerId, pTask->sWorkerId);
    if ( iStatus != XWORK_OK ) return iStatus;
    iStatus = xwork__replace_cstr((char **)&pSummary->sOperationId, pTask->sOperationId);
    if ( iStatus != XWORK_OK ) return iStatus;
    iStatus = xwork__replace_cstr((char **)&pSummary->sRequestJson, pTask->sRequestJson);
    if ( iStatus != XWORK_OK ) return iStatus;
    iStatus = xwork__replace_cstr((char **)&pSummary->sRequiredCapability, pTask->sRequiredCapability);
    if ( iStatus != XWORK_OK ) return iStatus;
    iStatus = xwork__replace_cstr((char **)&pSummary->sOutputText, pTask->sOutputText);
    if ( iStatus != XWORK_OK ) return iStatus;
    iStatus = xwork__replace_cstr((char **)&pSummary->sVisibleSummary, pTask->sVisibleSummary);
    if ( iStatus != XWORK_OK ) return iStatus;
    iStatus = xwork__replace_cstr((char **)&pSummary->sErrorKind, pTask->sErrorKind);
    if ( iStatus != XWORK_OK ) return iStatus;
    iStatus = xwork__replace_cstr((char **)&pSummary->sErrorMessage, pTask->sErrorMessage);
    if ( iStatus != XWORK_OK ) return iStatus;
    iStatus = xwork__remote_copy_artifact_summaries(
        pTask->pArtifacts,
        pTask->iArtifactCount,
        &pSummary->pArtifacts,
        &pSummary->iArtifactCount
    );
    if ( iStatus != XWORK_OK ) return iStatus;
    iStatus = xwork__remote_copy_output_chunks(
        pTask->pOutputChunks,
        pTask->iOutputChunkCount,
        &pSummary->pOutputChunks,
        &pSummary->iOutputChunkCount
    );
    if ( iStatus != XWORK_OK ) return iStatus;
    pSummary->eKind = pTask->eKind;
    pSummary->eState = pTask->eState;
    pSummary->eHostService = pTask->eHostService;
    pSummary->iAttemptCount = pTask->iAttemptCount;
    pSummary->iAssignedAtMs = pTask->iAssignedAtMs;
    pSummary->iCompletedAtMs = pTask->iCompletedAtMs;
    pSummary->iStatus = pTask->iStatus;
    pSummary->bRetryable = pTask->bRetryable;
    pSummary->iProtocolVersion = pTask->iProtocolVersion;
    return XWORK_OK;
}

static xwork_status xwork__remote_fill_task_snapshot(
    xwork_remote_task_snapshot *pSnapshot,
    const xwork__remote_task_record *pTask
)
{
    xwork_status iStatus;

    if ( !pSnapshot || !pTask ) {
        return XWORK_ERROR_INVALID_ARGUMENT;
    }
    xwork_remote_task_snapshot_reset(pSnapshot);
    iStatus = xwork__replace_cstr((char **)&pSnapshot->sTaskId, pTask->sTaskId);
    if ( iStatus != XWORK_OK ) return iStatus;
    iStatus = xwork__replace_cstr((char **)&pSnapshot->sAssignmentId, pTask->sAssignmentId);
    if ( iStatus != XWORK_OK ) return iStatus;
    iStatus = xwork__replace_cstr((char **)&pSnapshot->sWorkerId, pTask->sWorkerId);
    if ( iStatus != XWORK_OK ) return iStatus;
    iStatus = xwork__replace_cstr((char **)&pSnapshot->sOperationId, pTask->sOperationId);
    if ( iStatus != XWORK_OK ) return iStatus;
    iStatus = xwork__replace_cstr((char **)&pSnapshot->sRequestJson, pTask->sRequestJson);
    if ( iStatus != XWORK_OK ) return iStatus;
    iStatus = xwork__replace_cstr((char **)&pSnapshot->sRequiredCapability, pTask->sRequiredCapability);
    if ( iStatus != XWORK_OK ) return iStatus;
    iStatus = xwork__replace_cstr((char **)&pSnapshot->sOutputText, pTask->sOutputText);
    if ( iStatus != XWORK_OK ) return iStatus;
    iStatus = xwork__replace_cstr((char **)&pSnapshot->sVisibleSummary, pTask->sVisibleSummary);
    if ( iStatus != XWORK_OK ) return iStatus;
    iStatus = xwork__replace_cstr((char **)&pSnapshot->sErrorKind, pTask->sErrorKind);
    if ( iStatus != XWORK_OK ) return iStatus;
    iStatus = xwork__replace_cstr((char **)&pSnapshot->sErrorMessage, pTask->sErrorMessage);
    if ( iStatus != XWORK_OK ) return iStatus;
    iStatus = xwork__remote_copy_artifact_summaries(
        pTask->pArtifacts,
        pTask->iArtifactCount,
        &pSnapshot->pArtifacts,
        &pSnapshot->iArtifactCount
    );
    if ( iStatus != XWORK_OK ) return iStatus;
    iStatus = xwork__remote_copy_output_chunks(
        pTask->pOutputChunks,
        pTask->iOutputChunkCount,
        &pSnapshot->pOutputChunks,
        &pSnapshot->iOutputChunkCount
    );
    if ( iStatus != XWORK_OK ) return iStatus;
    pSnapshot->eKind = pTask->eKind;
    pSnapshot->eState = pTask->eState;
    pSnapshot->eHostService = pTask->eHostService;
    pSnapshot->iAttemptCount = pTask->iAttemptCount;
    pSnapshot->iAssignedAtMs = pTask->iAssignedAtMs;
    pSnapshot->iCompletedAtMs = pTask->iCompletedAtMs;
    pSnapshot->iTimeoutMs = pTask->iTimeoutMs;
    pSnapshot->iStatus = pTask->iStatus;
    pSnapshot->bRetryable = pTask->bRetryable;
    pSnapshot->iProtocolVersion = pTask->iProtocolVersion;
    return XWORK_OK;
}

static xwork_status xwork__remote_fill_assignment(
    xwork_remote_task_assignment *pAssignment,
    const xwork__remote_task_record *pTask
)
{
    xwork_status iStatus;

    if ( !pAssignment || !pTask ) {
        return XWORK_ERROR_INVALID_ARGUMENT;
    }
    xwork_remote_task_assignment_reset(pAssignment);
    iStatus = xwork__replace_cstr((char **)&pAssignment->sTaskId, pTask->sTaskId);
    if ( iStatus != XWORK_OK ) return iStatus;
    iStatus = xwork__replace_cstr((char **)&pAssignment->sAssignmentId, pTask->sAssignmentId);
    if ( iStatus != XWORK_OK ) return iStatus;
    iStatus = xwork__replace_cstr((char **)&pAssignment->sWorkerId, pTask->sWorkerId);
    if ( iStatus != XWORK_OK ) return iStatus;
    iStatus = xwork__replace_cstr((char **)&pAssignment->sOperationId, pTask->sOperationId);
    if ( iStatus != XWORK_OK ) return iStatus;
    iStatus = xwork__replace_cstr((char **)&pAssignment->sRequestJson, pTask->sRequestJson);
    if ( iStatus != XWORK_OK ) return iStatus;
    iStatus = xwork__replace_cstr((char **)&pAssignment->sRequiredCapability, pTask->sRequiredCapability);
    if ( iStatus != XWORK_OK ) return iStatus;
    pAssignment->eKind = pTask->eKind;
    pAssignment->eHostService = pTask->eHostService;
    pAssignment->iProtocolVersion = pTask->iProtocolVersion;
    pAssignment->iAttemptCount = pTask->iAttemptCount;
    pAssignment->bRetryable = pTask->bRetryable;
    return XWORK_OK;
}

xwork_status xwork_control_plane_create(
    const xwork_control_plane_options *pOptions,
    xwork_control_plane **ppPlane
)
{
    xwork_control_plane *pPlane;
    xwork_status iStatus;

    if ( !pOptions || !ppPlane || !pOptions->sPlaneId || !pOptions->sPlaneId[0] ) {
        return XWORK_ERROR_INVALID_ARGUMENT;
    }
    iStatus = xwork__remote_validate_protocol_version(pOptions->iProtocolVersion);
    if ( iStatus != XWORK_OK ) {
        return iStatus;
    }
    *ppPlane = NULL;
    pPlane = (xwork_control_plane *)calloc(1u, sizeof(*pPlane));
    if ( !pPlane ) {
        return XWORK_ERROR_NO_MEMORY;
    }
    pPlane->sPlaneId = xwork__dup_cstr(pOptions->sPlaneId);
    if ( !pPlane->sPlaneId ) {
        free(pPlane);
        return XWORK_ERROR_NO_MEMORY;
    }
    pPlane->pRuntime = pOptions->pRuntime;
    pPlane->eTransport = pOptions->eTransport;
    pPlane->iProtocolVersion = xwork__remote_resolve_protocol_version(
        pOptions->iProtocolVersion
    );
    pPlane->iDefaultLeaseTimeoutMs =
        pOptions->iDefaultLeaseTimeoutMs
            ? pOptions->iDefaultLeaseTimeoutMs
            : XWORK__REMOTE_DEFAULT_LEASE_MS;
    pPlane->iNowMs = pOptions->iNowMs;
    pPlane->bEnforceCapabilityAllowlist = pOptions->bEnforceCapabilityAllowlist;
    pPlane->eAutonomy = pOptions->eAutonomy;
    pPlane->eApprovalMode = pOptions->eApprovalMode;
    pPlane->bAutoApproveTasks = pOptions->bAutoApproveTasks;
    pPlane->bEnforceTaskPolicy = pOptions->bEnforceTaskPolicy;
    pPlane->bEnforceNetworkPolicy = pOptions->bEnforceNetworkPolicy;
    pPlane->bRedactTaskSecrets = pOptions->bRedactTaskSecrets;
    iStatus = xwork__remote_copy_string_array(
        pOptions->psAllowedCapabilities,
        pOptions->iAllowedCapabilityCount,
        &pPlane->psAllowedCapabilities,
        &pPlane->iAllowedCapabilityCount
    );
    if ( iStatus != XWORK_OK ) {
        xwork_control_plane_destroy(pPlane);
        return iStatus;
    }
    pPlane->iNextAssignmentSequence = 1u;
    *ppPlane = pPlane;
    return XWORK_OK;
}

xwork_status xwork_control_plane_create_from_snapshot(
    const xwork_control_plane_options *pOptions,
    const xwork_control_plane_snapshot *pSnapshot,
    xwork_control_plane **ppPlane
)
{
    xwork_control_plane_options tOptions;
    xwork_control_plane *pPlane = NULL;
    size_t i;
    xwork_status iStatus;

    if ( !pSnapshot || !ppPlane || !pSnapshot->sPlaneId || !pSnapshot->sPlaneId[0] ) {
        return XWORK_ERROR_INVALID_ARGUMENT;
    }
    *ppPlane = NULL;
    xwork_control_plane_options_init(&tOptions);
    tOptions.sPlaneId = pOptions && pOptions->sPlaneId
        ? pOptions->sPlaneId
        : pSnapshot->sPlaneId;
    tOptions.pRuntime = pOptions ? pOptions->pRuntime : NULL;
    tOptions.eTransport = pOptions ? pOptions->eTransport : pSnapshot->eTransport;
    tOptions.iProtocolVersion = pOptions && pOptions->iProtocolVersion
        ? pOptions->iProtocolVersion
        : pSnapshot->iProtocolVersion;
    tOptions.iDefaultLeaseTimeoutMs = pOptions && pOptions->iDefaultLeaseTimeoutMs
        ? pOptions->iDefaultLeaseTimeoutMs
        : pSnapshot->iDefaultLeaseTimeoutMs;
    tOptions.iNowMs = pOptions ? pOptions->iNowMs : pSnapshot->iNowMs;
    tOptions.psAllowedCapabilities = pOptions ? pOptions->psAllowedCapabilities : NULL;
    tOptions.iAllowedCapabilityCount = pOptions ? pOptions->iAllowedCapabilityCount : 0u;
    tOptions.bEnforceCapabilityAllowlist = pOptions
        ? pOptions->bEnforceCapabilityAllowlist
        : false;
    tOptions.eAutonomy = pOptions ? pOptions->eAutonomy : XWORK_AUTONOMY_SEMI_AUTO;
    tOptions.eApprovalMode = pOptions ? pOptions->eApprovalMode : XWORK_APPROVAL_DEFAULT;
    tOptions.bAutoApproveTasks = pOptions ? pOptions->bAutoApproveTasks : true;
    tOptions.bEnforceTaskPolicy = pOptions ? pOptions->bEnforceTaskPolicy : true;
    tOptions.bEnforceNetworkPolicy = pOptions ? pOptions->bEnforceNetworkPolicy : true;
    tOptions.bRedactTaskSecrets = pOptions ? pOptions->bRedactTaskSecrets : true;

    iStatus = xwork_control_plane_create(&tOptions, &pPlane);
    if ( iStatus != XWORK_OK ) {
        return iStatus;
    }
    pPlane->bStarted = pSnapshot->bStarted;
    pPlane->iNowMs = pSnapshot->iNowMs;
    pPlane->iNextAssignmentSequence = pSnapshot->iNextAssignmentSequence
        ? pSnapshot->iNextAssignmentSequence
        : 1u;

    for ( i = 0u; i < pSnapshot->tWorkers.iCount; ++i ) {
        const xwork_worker_snapshot *pWorkerSnapshot = &pSnapshot->tWorkers.pItems[i];
        xwork__remote_worker_record *pWorker;

        if ( !pWorkerSnapshot->sWorkerId || !pWorkerSnapshot->sWorkerId[0] ) {
            iStatus = XWORK_ERROR_INVALID_ARGUMENT;
            goto fail;
        }
        iStatus = xwork__remote_validate_protocol_version(
            pWorkerSnapshot->iProtocolVersion
        );
        if ( iStatus != XWORK_OK ) {
            goto fail;
        }
        if ( xwork__remote_find_worker(pPlane, pWorkerSnapshot->sWorkerId) ) {
            iStatus = XWORK_ERROR_ALREADY_EXISTS;
            goto fail;
        }
        pWorker = (xwork__remote_worker_record *)calloc(1u, sizeof(*pWorker));
        if ( !pWorker ) {
            iStatus = XWORK_ERROR_NO_MEMORY;
            goto fail;
        }
        pWorker->tPublic.pRecord = pWorker;
        pWorker->pRuntime = tOptions.pRuntime;
        pWorker->iProtocolVersion = xwork__remote_resolve_protocol_version(
            pWorkerSnapshot->iProtocolVersion
        );
        iStatus = xwork__replace_cstr(&pWorker->sWorkerId, pWorkerSnapshot->sWorkerId);
        if ( iStatus != XWORK_OK ) {
            xwork__remote_free_worker(pWorker);
            goto fail;
        }
        iStatus = xwork__replace_cstr(&pWorker->sDisplayName, pWorkerSnapshot->sDisplayName);
        if ( iStatus != XWORK_OK ) {
            xwork__remote_free_worker(pWorker);
            goto fail;
        }
        iStatus = xwork__replace_cstr(&pWorker->sEndpoint, pWorkerSnapshot->sEndpoint);
        if ( iStatus != XWORK_OK ) {
            xwork__remote_free_worker(pWorker);
            goto fail;
        }
        iStatus = xwork__remote_copy_string_array(
            pWorkerSnapshot->psCapabilities,
            pWorkerSnapshot->iCapabilityCount,
            &pWorker->psCapabilities,
            &pWorker->iCapabilityCount
        );
        if ( iStatus != XWORK_OK ) {
            xwork__remote_free_worker(pWorker);
            goto fail;
        }
        iStatus = xwork__remote_copy_string_array(
            pWorkerSnapshot->psLabels,
            pWorkerSnapshot->iLabelCount,
            &pWorker->psLabels,
            &pWorker->iLabelCount
        );
        if ( iStatus != XWORK_OK ) {
            xwork__remote_free_worker(pWorker);
            goto fail;
        }
        pWorker->iLeaseTimeoutMs = pWorkerSnapshot->iLeaseTimeoutMs
            ? pWorkerSnapshot->iLeaseTimeoutMs
            : pPlane->iDefaultLeaseTimeoutMs;
        pWorker->iLastHeartbeatMs = pWorkerSnapshot->iLastHeartbeatMs;
        pWorker->iLeaseExpiresMs = pWorkerSnapshot->iLeaseExpiresMs;
        pWorker->iClaimedCount = pWorkerSnapshot->iClaimedCount;
        pWorker->iCompletedCount = pWorkerSnapshot->iCompletedCount;
        pWorker->iFailedCount = pWorkerSnapshot->iFailedCount;
        pWorker->eState = pWorkerSnapshot->eState;
        pWorker->pNext = pPlane->pWorkers;
        pPlane->pWorkers = pWorker;
    }

    for ( i = 0u; i < pSnapshot->tTasks.iCount; ++i ) {
        const xwork_remote_task_snapshot *pTaskSnapshot = &pSnapshot->tTasks.pItems[i];
        xwork__remote_task_record *pTask;

        if ( !pTaskSnapshot->sTaskId || !pTaskSnapshot->sTaskId[0] ||
             !pTaskSnapshot->sRequestJson || !pTaskSnapshot->sRequestJson[0] ) {
            iStatus = XWORK_ERROR_INVALID_ARGUMENT;
            goto fail;
        }
        iStatus = xwork__remote_validate_protocol_version(
            pTaskSnapshot->iProtocolVersion
        );
        if ( iStatus != XWORK_OK ) {
            goto fail;
        }
        if ( xwork__remote_find_task(pPlane, pTaskSnapshot->sTaskId) ) {
            iStatus = XWORK_ERROR_ALREADY_EXISTS;
            goto fail;
        }
        pTask = (xwork__remote_task_record *)calloc(1u, sizeof(*pTask));
        if ( !pTask ) {
            iStatus = XWORK_ERROR_NO_MEMORY;
            goto fail;
        }
        iStatus = xwork__replace_cstr(&pTask->sTaskId, pTaskSnapshot->sTaskId);
        if ( iStatus != XWORK_OK ) {
            xwork__remote_free_task(pTask);
            goto fail;
        }
        iStatus = xwork__replace_cstr(&pTask->sAssignmentId, pTaskSnapshot->sAssignmentId);
        if ( iStatus != XWORK_OK ) {
            xwork__remote_free_task(pTask);
            goto fail;
        }
        iStatus = xwork__replace_cstr(&pTask->sWorkerId, pTaskSnapshot->sWorkerId);
        if ( iStatus != XWORK_OK ) {
            xwork__remote_free_task(pTask);
            goto fail;
        }
        iStatus = xwork__replace_cstr(&pTask->sOperationId, pTaskSnapshot->sOperationId);
        if ( iStatus != XWORK_OK ) {
            xwork__remote_free_task(pTask);
            goto fail;
        }
        iStatus = xwork__replace_cstr(&pTask->sRequestJson, pTaskSnapshot->sRequestJson);
        if ( iStatus != XWORK_OK ) {
            xwork__remote_free_task(pTask);
            goto fail;
        }
        iStatus = xwork__replace_cstr(&pTask->sRequiredCapability, pTaskSnapshot->sRequiredCapability);
        if ( iStatus != XWORK_OK ) {
            xwork__remote_free_task(pTask);
            goto fail;
        }
        iStatus = xwork__replace_cstr(&pTask->sOutputText, pTaskSnapshot->sOutputText);
        if ( iStatus != XWORK_OK ) {
            xwork__remote_free_task(pTask);
            goto fail;
        }
        iStatus = xwork__replace_cstr(&pTask->sVisibleSummary, pTaskSnapshot->sVisibleSummary);
        if ( iStatus != XWORK_OK ) {
            xwork__remote_free_task(pTask);
            goto fail;
        }
        iStatus = xwork__replace_cstr(&pTask->sErrorKind, pTaskSnapshot->sErrorKind);
        if ( iStatus != XWORK_OK ) {
            xwork__remote_free_task(pTask);
            goto fail;
        }
        iStatus = xwork__replace_cstr(&pTask->sErrorMessage, pTaskSnapshot->sErrorMessage);
        if ( iStatus != XWORK_OK ) {
            xwork__remote_free_task(pTask);
            goto fail;
        }
        iStatus = xwork__remote_copy_artifact_summaries(
            pTaskSnapshot->pArtifacts,
            pTaskSnapshot->iArtifactCount,
            &pTask->pArtifacts,
            &pTask->iArtifactCount
        );
        if ( iStatus != XWORK_OK ) {
            xwork__remote_free_task(pTask);
            goto fail;
        }
        iStatus = xwork__remote_copy_output_chunks(
            pTaskSnapshot->pOutputChunks,
            pTaskSnapshot->iOutputChunkCount,
            &pTask->pOutputChunks,
            &pTask->iOutputChunkCount
        );
        if ( iStatus != XWORK_OK ) {
            xwork__remote_free_task(pTask);
            goto fail;
        }
        pTask->eKind = pTaskSnapshot->eKind;
        pTask->eState = pTaskSnapshot->eState;
        pTask->eHostService = pTaskSnapshot->eHostService;
        pTask->iAttemptCount = pTaskSnapshot->iAttemptCount;
        pTask->iAssignedAtMs = pTaskSnapshot->iAssignedAtMs;
        pTask->iCompletedAtMs = pTaskSnapshot->iCompletedAtMs;
        pTask->iTimeoutMs = pTaskSnapshot->iTimeoutMs;
        pTask->iProtocolVersion = xwork__remote_resolve_protocol_version(
            pTaskSnapshot->iProtocolVersion
        );
        pTask->iStatus = pTaskSnapshot->iStatus;
        pTask->bRetryable = pTaskSnapshot->bRetryable;
        if ( pTask->eState == XWORK_REMOTE_TASK_ASSIGNED ||
             pTask->eState == XWORK_REMOTE_TASK_RUNNING ) {
            pTask->eState = XWORK_REMOTE_TASK_ORPHANED;
            pTask->iStatus = XWORK_ERROR_CANCELLED;
            iStatus = xwork__replace_cstr(&pTask->sErrorKind, "orphaned");
            if ( iStatus != XWORK_OK ) {
                xwork__remote_free_task(pTask);
                goto fail;
            }
            iStatus = xwork__replace_cstr(
                &pTask->sErrorMessage,
                "orphaned during control plane recovery"
            );
            if ( iStatus != XWORK_OK ) {
                xwork__remote_free_task(pTask);
                goto fail;
            }
            if ( !pTask->sVisibleSummary ) {
                iStatus = xwork__replace_cstr(
                    &pTask->sVisibleSummary,
                    "orphaned during control plane recovery"
                );
                if ( iStatus != XWORK_OK ) {
                    xwork__remote_free_task(pTask);
                    goto fail;
                }
            }
        }
        pTask->pNext = pPlane->pTasks;
        pPlane->pTasks = pTask;
    }

    for ( i = 0u; i < pSnapshot->tBlobChunks.iCount; ++i ) {
        xwork__remote_blob_chunk_record *pBlobChunk;

        pBlobChunk = (xwork__remote_blob_chunk_record *)calloc(
            1u,
            sizeof(*pBlobChunk)
        );
        if ( !pBlobChunk ) {
            iStatus = XWORK_ERROR_NO_MEMORY;
            goto fail;
        }
        xwork_remote_blob_chunk_summary_init(&pBlobChunk->tSummary);
        iStatus = xwork__remote_copy_blob_chunk_summary(
            &pBlobChunk->tSummary,
            &pSnapshot->tBlobChunks.pItems[i]
        );
        if ( iStatus != XWORK_OK ) {
            xwork__remote_free_blob_chunk_record(pBlobChunk);
            goto fail;
        }
        pBlobChunk->pNext = pPlane->pBlobChunks;
        pPlane->pBlobChunks = pBlobChunk;
    }

    *ppPlane = pPlane;
    return XWORK_OK;

fail:
    xwork_control_plane_destroy(pPlane);
    return iStatus;
}

void xwork_control_plane_destroy(xwork_control_plane *pPlane)
{
    xwork__remote_worker_record *pWorker;
    xwork__remote_task_record *pTask;
    xwork__remote_blob_chunk_record *pBlobChunk;

    if ( !pPlane ) {
        return;
    }
    while ( pPlane->pWorkers ) {
        pWorker = pPlane->pWorkers;
        pPlane->pWorkers = pWorker->pNext;
        xwork__remote_free_worker(pWorker);
    }
    while ( pPlane->pTasks ) {
        pTask = pPlane->pTasks;
        pPlane->pTasks = pTask->pNext;
        xwork__remote_free_task(pTask);
    }
    while ( pPlane->pBlobChunks ) {
        pBlobChunk = pPlane->pBlobChunks;
        pPlane->pBlobChunks = pBlobChunk->pNext;
        xwork__remote_free_blob_chunk_record(pBlobChunk);
    }
    xwork__free_str_array(
        &pPlane->psAllowedCapabilities,
        &pPlane->iAllowedCapabilityCount
    );
    free(pPlane->sPlaneId);
    free(pPlane);
}

xwork_status xwork_control_plane_start(xwork_control_plane *pPlane)
{
    if ( !pPlane ) {
        return XWORK_ERROR_INVALID_ARGUMENT;
    }
    pPlane->bStarted = true;
    return XWORK_OK;
}

xwork_status xwork_control_plane_stop(xwork_control_plane *pPlane)
{
    if ( !pPlane ) {
        return XWORK_ERROR_INVALID_ARGUMENT;
    }
    pPlane->bStarted = false;
    return XWORK_OK;
}

xwork_status xwork_control_plane_set_time(xwork_control_plane *pPlane, size_t iNowMs)
{
    if ( !pPlane ) {
        return XWORK_ERROR_INVALID_ARGUMENT;
    }
    pPlane->iNowMs = iNowMs;
    return XWORK_OK;
}

xwork_status xwork_control_plane_register_worker(
    xwork_control_plane *pPlane,
    const xwork_worker_options *pOptions,
    xwork_worker **ppWorker
)
{
    xwork__remote_worker_record *pWorker;
    xwork_status iStatus;

    if ( !pPlane || !pOptions || !pOptions->sWorkerId || !pOptions->sWorkerId[0] ) {
        return XWORK_ERROR_INVALID_ARGUMENT;
    }
    if ( ppWorker ) {
        *ppWorker = NULL;
    }
    if ( xwork__remote_find_worker(pPlane, pOptions->sWorkerId) ) {
        return XWORK_ERROR_ALREADY_EXISTS;
    }
    iStatus = xwork__remote_validate_protocol_version(pOptions->iProtocolVersion);
    if ( iStatus != XWORK_OK ) {
        return iStatus;
    }
    if ( xwork__remote_resolve_protocol_version(pOptions->iProtocolVersion) !=
         pPlane->iProtocolVersion ) {
        return XWORK_ERROR_UNSUPPORTED;
    }
    iStatus = xwork__remote_validate_capabilities_allowed(
        pPlane,
        pOptions->psCapabilities,
        pOptions->iCapabilityCount
    );
    if ( iStatus != XWORK_OK ) {
        return iStatus;
    }
    pWorker = (xwork__remote_worker_record *)calloc(1u, sizeof(*pWorker));
    if ( !pWorker ) {
        return XWORK_ERROR_NO_MEMORY;
    }
    pWorker->tPublic.pRecord = pWorker;
    pWorker->eState = XWORK_WORKER_ONLINE;
    pWorker->iLeaseTimeoutMs =
        pOptions->iLeaseTimeoutMs
            ? pOptions->iLeaseTimeoutMs
            : pPlane->iDefaultLeaseTimeoutMs;
    pWorker->iLastHeartbeatMs = pPlane->iNowMs;
    pWorker->iLeaseExpiresMs = pPlane->iNowMs + pWorker->iLeaseTimeoutMs;
    pWorker->iProtocolVersion = xwork__remote_resolve_protocol_version(
        pOptions->iProtocolVersion
    );
    pWorker->pRuntime = pOptions->pRuntime ? pOptions->pRuntime : pPlane->pRuntime;
    iStatus = xwork__replace_cstr(&pWorker->sWorkerId, pOptions->sWorkerId);
    if ( iStatus != XWORK_OK ) goto fail;
    iStatus = xwork__replace_cstr(&pWorker->sDisplayName, pOptions->sDisplayName);
    if ( iStatus != XWORK_OK ) goto fail;
    iStatus = xwork__replace_cstr(&pWorker->sEndpoint, pOptions->sEndpoint);
    if ( iStatus != XWORK_OK ) goto fail;
    iStatus = xwork__remote_copy_string_array(
        pOptions->psCapabilities,
        pOptions->iCapabilityCount,
        &pWorker->psCapabilities,
        &pWorker->iCapabilityCount
    );
    if ( iStatus != XWORK_OK ) goto fail;
    iStatus = xwork__remote_copy_string_array(
        pOptions->psLabels,
        pOptions->iLabelCount,
        &pWorker->psLabels,
        &pWorker->iLabelCount
    );
    if ( iStatus != XWORK_OK ) goto fail;

    pWorker->pNext = pPlane->pWorkers;
    pPlane->pWorkers = pWorker;
    if ( ppWorker ) {
        *ppWorker = &pWorker->tPublic;
    }
    return XWORK_OK;

fail:
    xwork__remote_free_worker(pWorker);
    return iStatus;
}

xwork_status xwork_control_plane_unregister_worker(
    xwork_control_plane *pPlane,
    const char *sWorkerId
)
{
    xwork__remote_worker_record *pWorker = xwork__remote_find_worker(pPlane, sWorkerId);

    if ( !pWorker ) {
        return XWORK_ERROR_NOT_FOUND;
    }
    pWorker->eState = XWORK_WORKER_UNREGISTERED;
    return XWORK_OK;
}

xwork_status xwork_control_plane_worker_heartbeat(
    xwork_control_plane *pPlane,
    const char *sWorkerId,
    size_t iNowMs
)
{
    xwork__remote_worker_record *pWorker = xwork__remote_find_worker(pPlane, sWorkerId);

    if ( !pPlane || !sWorkerId || !sWorkerId[0] ) {
        return XWORK_ERROR_INVALID_ARGUMENT;
    }
    if ( !pWorker ) {
        return XWORK_ERROR_NOT_FOUND;
    }
    if ( pWorker->eState == XWORK_WORKER_UNREGISTERED ) {
        return XWORK_ERROR_INVALID_STATE;
    }
    pPlane->iNowMs = iNowMs;
    pWorker->iLastHeartbeatMs = iNowMs;
    pWorker->iLeaseExpiresMs = iNowMs + pWorker->iLeaseTimeoutMs;
    pWorker->eState = XWORK_WORKER_ONLINE;
    return XWORK_OK;
}

xwork_status xwork_control_plane_sweep_stale(
    xwork_control_plane *pPlane,
    size_t iNowMs,
    size_t *piOrphanedCount
)
{
    xwork__remote_worker_record *pWorker;
    xwork__remote_task_record *pTask;
    size_t iOrphanedCount = 0u;

    if ( !pPlane ) {
        return XWORK_ERROR_INVALID_ARGUMENT;
    }
    pPlane->iNowMs = iNowMs;
    for ( pWorker = pPlane->pWorkers; pWorker; pWorker = pWorker->pNext ) {
        if ( pWorker->eState == XWORK_WORKER_ONLINE &&
             pWorker->iLeaseExpiresMs > 0u &&
             iNowMs > pWorker->iLeaseExpiresMs ) {
            pWorker->eState = XWORK_WORKER_STALE;
            for ( pTask = pPlane->pTasks; pTask; pTask = pTask->pNext ) {
                if ( pTask->sWorkerId &&
                     strcmp(pTask->sWorkerId, pWorker->sWorkerId) == 0 &&
                    (pTask->eState == XWORK_REMOTE_TASK_ASSIGNED ||
                     pTask->eState == XWORK_REMOTE_TASK_RUNNING) ) {
                    pTask->eState = XWORK_REMOTE_TASK_ORPHANED;
                    pTask->iStatus = XWORK_ERROR_CANCELLED;
                    (void)xwork__replace_cstr(&pTask->sErrorKind, "orphaned");
                    (void)xwork__replace_cstr(
                        &pTask->sErrorMessage,
                        "worker lease expired"
                    );
                    ++iOrphanedCount;
                }
            }
        }
    }
    if ( piOrphanedCount ) {
        *piOrphanedCount = iOrphanedCount;
    }
    return XWORK_OK;
}

xwork_status xwork_control_plane_list_workers(
    const xwork_control_plane *pPlane,
    xwork_worker_summary_list *pList
)
{
    xwork__remote_worker_record *pWorker;
    xwork_worker_summary *pItems;
    size_t iCount = 0u;
    size_t i = 0u;
    xwork_status iStatus = XWORK_OK;

    if ( !pPlane || !pList ) {
        return XWORK_ERROR_INVALID_ARGUMENT;
    }
    xwork_worker_summary_list_reset(pList);
    for ( pWorker = pPlane->pWorkers; pWorker; pWorker = pWorker->pNext ) {
        ++iCount;
    }
    if ( iCount == 0u ) {
        return XWORK_OK;
    }
    pItems = (xwork_worker_summary *)calloc(iCount, sizeof(*pItems));
    if ( !pItems ) {
        return XWORK_ERROR_NO_MEMORY;
    }
    for ( i = 0u; i < iCount; ++i ) {
        xwork_worker_summary_init(&pItems[i]);
    }
    i = 0u;
    for ( pWorker = pPlane->pWorkers; pWorker; pWorker = pWorker->pNext ) {
        iStatus = xwork__remote_fill_worker_summary(&pItems[i], pWorker);
        if ( iStatus != XWORK_OK ) {
            break;
        }
        ++i;
    }
    if ( iStatus != XWORK_OK ) {
        while ( i > 0u ) {
            --i;
            xwork_worker_summary_reset(&pItems[i]);
        }
        free(pItems);
        return iStatus;
    }
    pList->pItems = pItems;
    pList->iCount = iCount;
    return XWORK_OK;
}

xwork_status xwork_control_plane_enqueue_task(
    xwork_control_plane *pPlane,
    const xwork_remote_task_options *pOptions
)
{
    xwork__remote_task_record *pTask;
    xwork_status iStatus;

    if ( !pPlane || !pOptions || !pOptions->sTaskId || !pOptions->sTaskId[0] ||
         !pOptions->sRequestJson || !pOptions->sRequestJson[0] ) {
        return XWORK_ERROR_INVALID_ARGUMENT;
    }
    if ( pOptions->eKind == XWORK_REMOTE_TASK_HOST_TOOL &&
         (!pOptions->sOperationId || !pOptions->sOperationId[0]) ) {
        return XWORK_ERROR_INVALID_ARGUMENT;
    }
    if ( !xwork__remote_capability_allowed(pPlane, pOptions->sRequiredCapability) ) {
        return XWORK_ERROR_UNSUPPORTED;
    }
    iStatus = xwork__remote_enforce_network_policy(pPlane, pOptions);
    if ( iStatus != XWORK_OK ) {
        return iStatus;
    }
    iStatus = xwork__remote_enforce_task_policy(pPlane, pOptions);
    if ( iStatus != XWORK_OK ) {
        return iStatus;
    }
    if ( xwork__remote_find_task(pPlane, pOptions->sTaskId) ) {
        return XWORK_ERROR_ALREADY_EXISTS;
    }
    pTask = (xwork__remote_task_record *)calloc(1u, sizeof(*pTask));
    if ( !pTask ) {
        return XWORK_ERROR_NO_MEMORY;
    }
    pTask->eKind = pOptions->eKind;
    pTask->eState = XWORK_REMOTE_TASK_QUEUED;
    pTask->eHostService = pOptions->eHostService;
    pTask->iTimeoutMs = pOptions->iTimeoutMs;
    pTask->iProtocolVersion = pPlane->iProtocolVersion;
    pTask->bRetryable = pOptions->bRetryable;
    pTask->pUserData = pOptions->pUserData;
    if ( pTask->eKind == XWORK_REMOTE_TASK_PROCESS_EXEC ) {
        pTask->eHostService = XWORK_HOST_PROCESS;
        iStatus = xwork__replace_cstr(&pTask->sOperationId, XWORK_HOST_PROCESS_EXEC);
    } else {
        iStatus = xwork__replace_cstr(&pTask->sOperationId, pOptions->sOperationId);
    }
    if ( iStatus != XWORK_OK ) goto fail;
    iStatus = xwork__replace_cstr(&pTask->sTaskId, pOptions->sTaskId);
    if ( iStatus != XWORK_OK ) goto fail;
    iStatus = xwork__remote_copy_observable_text(
        pPlane,
        &pTask->sRequestJson,
        pOptions->sRequestJson
    );
    if ( iStatus != XWORK_OK ) goto fail;
    iStatus = xwork__replace_cstr(&pTask->sRequiredCapability, pOptions->sRequiredCapability);
    if ( iStatus != XWORK_OK ) goto fail;
    pTask->pNext = pPlane->pTasks;
    pPlane->pTasks = pTask;
    return XWORK_OK;

fail:
    xwork__remote_free_task(pTask);
    return iStatus;
}

xwork_status xwork_control_plane_claim_task(
    xwork_control_plane *pPlane,
    const char *sWorkerId,
    xwork_remote_task_assignment *pAssignment
)
{
    xwork__remote_worker_record *pWorker;
    xwork__remote_task_record *pTask;
    char *sAssignmentId;
    xwork_status iStatus;

    if ( !pPlane || !sWorkerId || !sWorkerId[0] || !pAssignment ) {
        return XWORK_ERROR_INVALID_ARGUMENT;
    }
    if ( !pPlane->bStarted ) {
        return XWORK_ERROR_INVALID_STATE;
    }
    pWorker = xwork__remote_find_worker(pPlane, sWorkerId);
    if ( !pWorker ) {
        return XWORK_ERROR_NOT_FOUND;
    }
    if ( pWorker->eState != XWORK_WORKER_ONLINE ) {
        return XWORK_ERROR_INVALID_STATE;
    }
    for ( pTask = pPlane->pTasks; pTask; pTask = pTask->pNext ) {
        if ( pTask->eState != XWORK_REMOTE_TASK_QUEUED ) {
            continue;
        }
        if ( !xwork__remote_string_list_contains(
                 pWorker->psCapabilities,
                 pWorker->iCapabilityCount,
                 pTask->sRequiredCapability
             ) ) {
            continue;
        }
        sAssignmentId = xwork__dup_printf(
            "%s:assignment:%zu",
            pTask->sTaskId,
            pPlane->iNextAssignmentSequence++
        );
        if ( !sAssignmentId ) {
            return XWORK_ERROR_NO_MEMORY;
        }
        free(pTask->sAssignmentId);
        pTask->sAssignmentId = sAssignmentId;
        iStatus = xwork__replace_cstr(&pTask->sWorkerId, pWorker->sWorkerId);
        if ( iStatus != XWORK_OK ) {
            return iStatus;
        }
        pTask->eState = XWORK_REMOTE_TASK_ASSIGNED;
        pTask->iAssignedAtMs = pPlane->iNowMs;
        ++pTask->iAttemptCount;
        ++pWorker->iClaimedCount;
        return xwork__remote_fill_assignment(pAssignment, pTask);
    }
    return XWORK_ERROR_NOT_FOUND;
}

xwork_status xwork_control_plane_complete_task(
    xwork_control_plane *pPlane,
    const char *sAssignmentId,
    const xwork_remote_task_result *pResult
)
{
    xwork__remote_task_record *pTask;
    xwork__remote_worker_record *pWorker;
    xwork_status iStatus;

    if ( !pPlane || !sAssignmentId || !sAssignmentId[0] || !pResult ) {
        return XWORK_ERROR_INVALID_ARGUMENT;
    }
    pTask = xwork__remote_find_assignment(pPlane, sAssignmentId);
    if ( !pTask ) {
        return XWORK_ERROR_NOT_FOUND;
    }
    if ( pTask->eState != XWORK_REMOTE_TASK_ASSIGNED &&
         pTask->eState != XWORK_REMOTE_TASK_RUNNING ) {
        return XWORK_ERROR_INVALID_STATE;
    }
    iStatus = xwork__remote_validate_protocol_version(pResult->iProtocolVersion);
    if ( iStatus != XWORK_OK ) {
        return iStatus;
    }
    if ( xwork__remote_resolve_protocol_version(pResult->iProtocolVersion) !=
         pTask->iProtocolVersion ) {
        return XWORK_ERROR_UNSUPPORTED;
    }
    pTask->eState = pResult->iStatus == XWORK_OK
        ? XWORK_REMOTE_TASK_COMPLETED
        : XWORK_REMOTE_TASK_FAILED;
    pTask->iStatus = pResult->iStatus;
    pTask->iCompletedAtMs = pPlane->iNowMs;
    pTask->bRetryable = pResult->bRetryable;
    iStatus = xwork__remote_copy_observable_text(
        pPlane,
        &pTask->sOutputText,
        pResult->sOutputText
    );
    if ( iStatus != XWORK_OK ) return iStatus;
    iStatus = xwork__remote_copy_observable_text(
        pPlane,
        &pTask->sVisibleSummary,
        pResult->sVisibleSummary
    );
    if ( iStatus != XWORK_OK ) return iStatus;
    if ( pResult->iStatus == XWORK_OK ) {
        free(pTask->sErrorKind);
        pTask->sErrorKind = NULL;
        free(pTask->sErrorMessage);
        pTask->sErrorMessage = NULL;
    } else {
        const char *sErrorKind = pResult->sErrorKind
            ? pResult->sErrorKind
            : xwork__remote_default_error_kind(pResult->iStatus);
        const char *sErrorMessage = pResult->sErrorMessage
            ? pResult->sErrorMessage
            : pResult->sVisibleSummary;
        iStatus = xwork__remote_copy_observable_text(
            pPlane,
            &pTask->sErrorKind,
            sErrorKind
        );
        if ( iStatus != XWORK_OK ) return iStatus;
        iStatus = xwork__remote_copy_observable_text(
            pPlane,
            &pTask->sErrorMessage,
            sErrorMessage
        );
        if ( iStatus != XWORK_OK ) return iStatus;
    }
    {
        xwork_artifact_summary_list tOldArtifacts;
        tOldArtifacts.pItems = pTask->pArtifacts;
        tOldArtifacts.iCount = pTask->iArtifactCount;
        tOldArtifacts.bHasMore = false;
        tOldArtifacts.iNextAfterSequence = 0u;
        xwork_artifact_summary_list_reset(&tOldArtifacts);
        pTask->pArtifacts = NULL;
        pTask->iArtifactCount = 0u;
    }
    iStatus = xwork__remote_copy_artifact_summaries(
        pResult->pArtifacts,
        pResult->iArtifactCount,
        &pTask->pArtifacts,
        &pTask->iArtifactCount
    );
    if ( iStatus != XWORK_OK ) return iStatus;
    pWorker = xwork__remote_find_worker(pPlane, pTask->sWorkerId);
    if ( pWorker ) {
        if ( pTask->eState == XWORK_REMOTE_TASK_COMPLETED ) {
            ++pWorker->iCompletedCount;
        } else {
            ++pWorker->iFailedCount;
        }
    }
    return XWORK_OK;
}

xwork_status xwork_control_plane_upload_artifact(
    xwork_control_plane *pPlane,
    const xwork_remote_artifact_upload *pUpload
)
{
    xwork__remote_task_record *pTask;
    xwork_status iStatus;

    if ( !pPlane ||
         !pUpload ||
         !pUpload->sTaskId ||
         !pUpload->sTaskId[0] ||
         !pUpload->sWorkerId ||
         !pUpload->sWorkerId[0] ||
         !pUpload->pArtifact ||
         !pUpload->pArtifact->sArtifactId ||
         !pUpload->pArtifact->sArtifactId[0] ) {
        return XWORK_ERROR_INVALID_ARGUMENT;
    }
    iStatus = xwork__remote_validate_protocol_version(pUpload->iProtocolVersion);
    if ( iStatus != XWORK_OK ) {
        return iStatus;
    }
    if ( pUpload->iChunkCount > 0u && pUpload->iChunkIndex >= pUpload->iChunkCount ) {
        return XWORK_ERROR_INVALID_ARGUMENT;
    }
    if ( pUpload->iChunkSize > 0u && !pUpload->pChunkData ) {
        return XWORK_ERROR_INVALID_ARGUMENT;
    }
    pTask = xwork__remote_find_task(pPlane, pUpload->sTaskId);
    if ( !pTask ) {
        return XWORK_ERROR_NOT_FOUND;
    }
    if ( pTask->iProtocolVersion !=
         xwork__remote_resolve_protocol_version(pUpload->iProtocolVersion) ) {
        return XWORK_ERROR_UNSUPPORTED;
    }
    if ( pTask->sWorkerId && strcmp(pTask->sWorkerId, pUpload->sWorkerId) != 0 ) {
        return XWORK_ERROR_INVALID_STATE;
    }
    if ( pUpload->sAssignmentId &&
         pUpload->sAssignmentId[0] &&
         (!pTask->sAssignmentId || strcmp(pTask->sAssignmentId, pUpload->sAssignmentId) != 0) ) {
        return XWORK_ERROR_INVALID_STATE;
    }
    if ( pTask->eState == XWORK_REMOTE_TASK_QUEUED ||
         pTask->eState == XWORK_REMOTE_TASK_CANCELLED ||
         pTask->eState == XWORK_REMOTE_TASK_ORPHANED ) {
        return XWORK_ERROR_INVALID_STATE;
    }
    iStatus = xwork__remote_upsert_artifact_summary(pTask, pUpload->pArtifact);
    if ( iStatus != XWORK_OK ) {
        return iStatus;
    }
    return xwork__remote_upsert_blob_chunk(pPlane, pUpload);
}

xwork_status xwork_control_plane_upload_output_chunk(
    xwork_control_plane *pPlane,
    const xwork_remote_output_chunk *pChunk
)
{
    xwork__remote_task_record *pTask;
    xwork_status iStatus;

    if ( !pPlane ||
         !pChunk ||
         !pChunk->sTaskId ||
         !pChunk->sTaskId[0] ||
         !pChunk->sWorkerId ||
         !pChunk->sWorkerId[0] ||
         !pChunk->sText ) {
        return XWORK_ERROR_INVALID_ARGUMENT;
    }
    iStatus = xwork__remote_validate_protocol_version(pChunk->iProtocolVersion);
    if ( iStatus != XWORK_OK ) {
        return iStatus;
    }
    pTask = xwork__remote_find_task(pPlane, pChunk->sTaskId);
    if ( !pTask ) {
        return XWORK_ERROR_NOT_FOUND;
    }
    if ( pTask->iProtocolVersion !=
         xwork__remote_resolve_protocol_version(pChunk->iProtocolVersion) ) {
        return XWORK_ERROR_UNSUPPORTED;
    }
    if ( pTask->sWorkerId && strcmp(pTask->sWorkerId, pChunk->sWorkerId) != 0 ) {
        return XWORK_ERROR_INVALID_STATE;
    }
    if ( pChunk->sAssignmentId &&
         pChunk->sAssignmentId[0] &&
         (!pTask->sAssignmentId || strcmp(pTask->sAssignmentId, pChunk->sAssignmentId) != 0) ) {
        return XWORK_ERROR_INVALID_STATE;
    }
    if ( pTask->eState == XWORK_REMOTE_TASK_QUEUED ||
         pTask->eState == XWORK_REMOTE_TASK_CANCELLED ||
         pTask->eState == XWORK_REMOTE_TASK_ORPHANED ) {
        return XWORK_ERROR_INVALID_STATE;
    }
    return xwork__remote_append_output_chunk(pTask, pChunk);
}

xwork_status xwork_control_plane_list_artifact_blobs(
    const xwork_control_plane *pPlane,
    const char *sTaskId,
    const char *sArtifactId,
    xwork_remote_blob_chunk_summary_list *pList
)
{
    const xwork__remote_blob_chunk_record *pBlobChunk;
    xwork_status iStatus;

    if ( !pPlane || !sTaskId || !sTaskId[0] || !pList ) {
        return XWORK_ERROR_INVALID_ARGUMENT;
    }
    xwork_remote_blob_chunk_summary_list_reset(pList);
    for ( pBlobChunk = pPlane->pBlobChunks;
          pBlobChunk;
          pBlobChunk = pBlobChunk->pNext ) {
        if ( !xwork__remote_nullable_string_equal(
                 pBlobChunk->tSummary.sTaskId,
                 sTaskId
             ) ) {
            continue;
        }
        if ( sArtifactId &&
             sArtifactId[0] &&
             !xwork__remote_nullable_string_equal(
                 pBlobChunk->tSummary.sArtifactId,
                 sArtifactId
             ) ) {
            continue;
        }
        iStatus = xwork__remote_append_blob_chunk_summary(
            pList,
            &pBlobChunk->tSummary
        );
        if ( iStatus != XWORK_OK ) {
            xwork_remote_blob_chunk_summary_list_reset(pList);
            return iStatus;
        }
    }
    return XWORK_OK;
}

xwork_status xwork_control_plane_fail_task(
    xwork_control_plane *pPlane,
    const char *sAssignmentId,
    const char *sErrorText,
    bool bRetryable
)
{
    xwork_remote_task_result tResult;

    xwork_remote_task_result_init(&tResult);
    tResult.iStatus = XWORK_ERROR_EXTERNAL_FAILURE;
    tResult.sOutputText = sErrorText;
    tResult.sVisibleSummary = sErrorText ? sErrorText : "remote task failed";
    tResult.sErrorKind = "remote_failure";
    tResult.sErrorMessage = tResult.sVisibleSummary;
    tResult.bRetryable = bRetryable;
    return xwork_control_plane_complete_task(pPlane, sAssignmentId, &tResult);
}

xwork_status xwork_control_plane_cancel_task(
    xwork_control_plane *pPlane,
    const char *sTaskId,
    const char *sReason
)
{
    xwork__remote_task_record *pTask = xwork__remote_find_task(pPlane, sTaskId);

    if ( !pTask ) {
        return XWORK_ERROR_NOT_FOUND;
    }
    if ( pTask->eState == XWORK_REMOTE_TASK_COMPLETED ||
         pTask->eState == XWORK_REMOTE_TASK_FAILED ||
         pTask->eState == XWORK_REMOTE_TASK_CANCELLED ) {
        return XWORK_ERROR_INVALID_STATE;
    }
    pTask->eState = XWORK_REMOTE_TASK_CANCELLED;
    pTask->iStatus = XWORK_ERROR_CANCELLED;
    pTask->iCompletedAtMs = pPlane ? pPlane->iNowMs : 0u;
    if ( xwork__replace_cstr(&pTask->sErrorKind, "cancelled") != XWORK_OK ) {
        return XWORK_ERROR_NO_MEMORY;
    }
    if ( xwork__replace_cstr(
             &pTask->sErrorMessage,
             sReason ? sReason : "remote task cancelled"
         ) != XWORK_OK ) {
        return XWORK_ERROR_NO_MEMORY;
    }
    return xwork__replace_cstr(&pTask->sVisibleSummary, sReason);
}

xwork_status xwork_control_plane_execute_next_local(
    xwork_control_plane *pPlane,
    const char *sWorkerId,
    xwork_remote_task_assignment *pAssignment
)
{
    xwork_remote_task_assignment tAssignment;
    xwork_remote_task_result tRemoteResult;
    xwork_tool_result tToolResult;
    xwork__remote_worker_record *pWorker;
    xwork_status iStatus;

    if ( !pPlane || !sWorkerId || !sWorkerId[0] ) {
        return XWORK_ERROR_INVALID_ARGUMENT;
    }
    pWorker = xwork__remote_find_worker(pPlane, sWorkerId);
    if ( !pWorker || !pWorker->pRuntime ) {
        return pWorker ? XWORK_ERROR_INVALID_STATE : XWORK_ERROR_NOT_FOUND;
    }

    xwork_remote_task_assignment_init(&tAssignment);
    xwork_remote_task_result_init(&tRemoteResult);
    xwork_tool_result_init(&tToolResult);
    iStatus = xwork_control_plane_claim_task(pPlane, sWorkerId, &tAssignment);
    if ( iStatus != XWORK_OK ) {
        return iStatus;
    }
    iStatus = xwork_runtime_invoke_host_service(
        pWorker->pRuntime,
        tAssignment.eHostService,
        tAssignment.sOperationId,
        tAssignment.sRequestJson,
        &tToolResult
    );
    tRemoteResult.iStatus = iStatus;
    tRemoteResult.sOutputText = tToolResult.sOutputText;
    tRemoteResult.sVisibleSummary = tToolResult.sVisibleSummary;
    tRemoteResult.bRetryable = tToolResult.bRetryable;
    if ( xwork_control_plane_complete_task(
             pPlane,
             tAssignment.sAssignmentId,
             &tRemoteResult
         ) != XWORK_OK ) {
        iStatus = XWORK_ERROR_EXTERNAL_FAILURE;
    }
    if ( pAssignment ) {
        xwork_remote_task_assignment_reset(pAssignment);
        if ( xwork__remote_fill_assignment(
                 pAssignment,
                 xwork__remote_find_assignment(pPlane, tAssignment.sAssignmentId)
             ) != XWORK_OK ) {
            iStatus = XWORK_ERROR_EXTERNAL_FAILURE;
        }
    }
    xwork_remote_task_assignment_reset(&tAssignment);
    return iStatus;
}

xwork_status xwork_control_plane_get_task_summary(
    const xwork_control_plane *pPlane,
    const char *sTaskId,
    xwork_remote_task_summary *pSummary
)
{
    xwork__remote_task_record *pTask = xwork__remote_find_task(pPlane, sTaskId);

    if ( !pPlane || !sTaskId || !sTaskId[0] || !pSummary ) {
        return XWORK_ERROR_INVALID_ARGUMENT;
    }
    if ( !pTask ) {
        return XWORK_ERROR_NOT_FOUND;
    }
    return xwork__remote_fill_task_summary(pSummary, pTask);
}

xwork_status xwork_control_plane_list_tasks(
    const xwork_control_plane *pPlane,
    xwork_remote_task_summary_list *pList
)
{
    xwork__remote_task_record *pTask;
    xwork_remote_task_summary *pItems;
    size_t iCount = 0u;
    size_t i = 0u;
    xwork_status iStatus = XWORK_OK;

    if ( !pPlane || !pList ) {
        return XWORK_ERROR_INVALID_ARGUMENT;
    }
    xwork_remote_task_summary_list_reset(pList);
    for ( pTask = pPlane->pTasks; pTask; pTask = pTask->pNext ) {
        ++iCount;
    }
    if ( iCount == 0u ) {
        return XWORK_OK;
    }
    pItems = (xwork_remote_task_summary *)calloc(iCount, sizeof(*pItems));
    if ( !pItems ) {
        return XWORK_ERROR_NO_MEMORY;
    }
    for ( i = 0u; i < iCount; ++i ) {
        xwork_remote_task_summary_init(&pItems[i]);
    }
    i = 0u;
    for ( pTask = pPlane->pTasks; pTask; pTask = pTask->pNext ) {
        iStatus = xwork__remote_fill_task_summary(&pItems[i], pTask);
        if ( iStatus != XWORK_OK ) {
            break;
        }
        ++i;
    }
    if ( iStatus != XWORK_OK ) {
        while ( i > 0u ) {
            --i;
            xwork_remote_task_summary_reset(&pItems[i]);
        }
        free(pItems);
        return iStatus;
    }
    pList->pItems = pItems;
    pList->iCount = iCount;
    return XWORK_OK;
}

xwork_status xwork_control_plane_get_snapshot(
    const xwork_control_plane *pPlane,
    xwork_control_plane_snapshot *pSnapshot
)
{
    xwork__remote_worker_record *pWorker;
    xwork__remote_task_record *pTask;
    xwork__remote_blob_chunk_record *pBlobChunk;
    size_t iCount;
    size_t i;
    xwork_status iStatus;

    if ( !pPlane || !pSnapshot ) {
        return XWORK_ERROR_INVALID_ARGUMENT;
    }
    xwork_control_plane_snapshot_reset(pSnapshot);
    iStatus = xwork__replace_cstr((char **)&pSnapshot->sPlaneId, pPlane->sPlaneId);
    if ( iStatus != XWORK_OK ) return iStatus;
    pSnapshot->eTransport = pPlane->eTransport;
    pSnapshot->iProtocolVersion = pPlane->iProtocolVersion;
    pSnapshot->iDefaultLeaseTimeoutMs = pPlane->iDefaultLeaseTimeoutMs;
    pSnapshot->iNowMs = pPlane->iNowMs;
    pSnapshot->bStarted = pPlane->bStarted;
    pSnapshot->iNextAssignmentSequence = pPlane->iNextAssignmentSequence;

    iCount = 0u;
    for ( pWorker = pPlane->pWorkers; pWorker; pWorker = pWorker->pNext ) {
        ++iCount;
    }
    if ( iCount > 0u ) {
        pSnapshot->tWorkers.pItems = (xwork_worker_snapshot *)calloc(
            iCount,
            sizeof(*pSnapshot->tWorkers.pItems)
        );
        if ( !pSnapshot->tWorkers.pItems ) {
            return XWORK_ERROR_NO_MEMORY;
        }
        pSnapshot->tWorkers.iCount = iCount;
        for ( i = 0u; i < iCount; ++i ) {
            xwork_worker_snapshot_init(&pSnapshot->tWorkers.pItems[i]);
        }
        i = 0u;
        for ( pWorker = pPlane->pWorkers; pWorker; pWorker = pWorker->pNext ) {
            iStatus = xwork__remote_fill_worker_snapshot(
                &pSnapshot->tWorkers.pItems[i],
                pWorker
            );
            if ( iStatus != XWORK_OK ) {
                return iStatus;
            }
            ++i;
        }
    }

    iCount = 0u;
    for ( pTask = pPlane->pTasks; pTask; pTask = pTask->pNext ) {
        ++iCount;
    }
    if ( iCount > 0u ) {
        pSnapshot->tTasks.pItems = (xwork_remote_task_snapshot *)calloc(
            iCount,
            sizeof(*pSnapshot->tTasks.pItems)
        );
        if ( !pSnapshot->tTasks.pItems ) {
            return XWORK_ERROR_NO_MEMORY;
        }
        pSnapshot->tTasks.iCount = iCount;
        for ( i = 0u; i < iCount; ++i ) {
            xwork_remote_task_snapshot_init(&pSnapshot->tTasks.pItems[i]);
        }
        i = 0u;
        for ( pTask = pPlane->pTasks; pTask; pTask = pTask->pNext ) {
            iStatus = xwork__remote_fill_task_snapshot(
                &pSnapshot->tTasks.pItems[i],
                pTask
            );
            if ( iStatus != XWORK_OK ) {
                return iStatus;
            }
            ++i;
        }
    }

    iCount = 0u;
    for ( pBlobChunk = pPlane->pBlobChunks; pBlobChunk; pBlobChunk = pBlobChunk->pNext ) {
        ++iCount;
    }
    if ( iCount > 0u ) {
        pSnapshot->tBlobChunks.pItems = (xwork_remote_blob_chunk_summary *)calloc(
            iCount,
            sizeof(*pSnapshot->tBlobChunks.pItems)
        );
        if ( !pSnapshot->tBlobChunks.pItems ) {
            return XWORK_ERROR_NO_MEMORY;
        }
        pSnapshot->tBlobChunks.iCount = iCount;
        for ( i = 0u; i < iCount; ++i ) {
            xwork_remote_blob_chunk_summary_init(&pSnapshot->tBlobChunks.pItems[i]);
        }
        i = 0u;
        for ( pBlobChunk = pPlane->pBlobChunks;
              pBlobChunk;
              pBlobChunk = pBlobChunk->pNext ) {
            iStatus = xwork__remote_copy_blob_chunk_summary(
                &pSnapshot->tBlobChunks.pItems[i],
                &pBlobChunk->tSummary
            );
            if ( iStatus != XWORK_OK ) {
                return iStatus;
            }
            ++i;
        }
    }

    return XWORK_OK;
}
