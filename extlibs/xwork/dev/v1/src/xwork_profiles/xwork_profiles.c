#include "../xwork_core/xwork_internal.h"

static const xwork_profile xwork__builtin_xcode_profile = {
    XWORK_PROFILE_XCODE,
    "xcode interactive",
    "Interactive coding profile for user-driven runs.",
    XWORK_PROFILE_XCODE,
    XWORK_PROFILE_XCODE,
    XWORK_AUTONOMY_SEMI_AUTO,
    {
        XWORK_RISK_LOW,
        NULL,
        0u,
        NULL,
        0u,
        true
    },
    { true, 0.75, 8u, 1024u, 8u, true, XWORK_SESSION_COMPACT_SUMMARIZE },
    8u,
    false,
    false,
    XWORK_PLANNER_OFF
};

static const xwork_profile xwork__builtin_xclaw_profile = {
    XWORK_PROFILE_XCLAW,
    "xclaw autonomous",
    "Autonomous task profile for longer-running background work.",
    XWORK_PROFILE_XCLAW,
    XWORK_PROFILE_XCLAW,
    XWORK_AUTONOMY_AUTO,
    {
        XWORK_RISK_HIGH,
        NULL,
        0u,
        NULL,
        0u,
        true
    },
    { true, 0.90, 24u, 2048u, 12u, true, XWORK_SESSION_COMPACT_SUMMARIZE },
    32u,
    true,
    true,
    XWORK_PLANNER_BOUNDARY
};

static const xwork_profile *xwork__find_builtin_profile(const char *sProfileId)
{
    if ( !sProfileId || !sProfileId[0] ) {
        return NULL;
    }
    if ( strcmp(sProfileId, XWORK_PROFILE_XCODE) == 0 ) {
        return &xwork__builtin_xcode_profile;
    }
    if ( strcmp(sProfileId, XWORK_PROFILE_XCLAW) == 0 ) {
        return &xwork__builtin_xclaw_profile;
    }
    return NULL;
}

void xwork_profile_init(xwork_profile *pProfile)
{
    if ( pProfile ) {
        memset(pProfile, 0, sizeof(*pProfile));
        pProfile->eAutonomy = XWORK_AUTONOMY_SEMI_AUTO;
        xwork_policy_options_init(&pProfile->tPolicy);
        xwork_session_policy_init(&pProfile->tSessionPolicy);
        pProfile->iDefaultMaxTurns = 4u;
        pProfile->bDefaultAutoApprove = true;
        pProfile->ePlannerMode = XWORK_PLANNER_OFF;
    }
}

xwork_status xwork_profile_get_builtin(const char *sProfileId, xwork_profile *pProfile)
{
    const xwork_profile *pBuiltin;

    if ( !pProfile ) {
        return XWORK_ERROR_INVALID_ARGUMENT;
    }

    xwork_profile_init(pProfile);
    pBuiltin = xwork__find_builtin_profile(sProfileId);
    if ( !pBuiltin ) {
        return XWORK_ERROR_NOT_FOUND;
    }

    *pProfile = *pBuiltin;
    return XWORK_OK;
}

xwork_status xwork_profile_apply_runtime_options(
    const xwork_profile *pProfile,
    xwork_runtime_options *pOptions
)
{
    if ( !pProfile || !pOptions ) {
        return XWORK_ERROR_INVALID_ARGUMENT;
    }

    pOptions->tPolicy = pProfile->tPolicy;
    return XWORK_OK;
}

xwork_status xwork_profile_apply_xllm_profile_options(
    const xwork_profile *pProfile,
    xwork_xllm_profile_options *pOptions
)
{
    if ( !pProfile || !pOptions ) {
        return XWORK_ERROR_INVALID_ARGUMENT;
    }

    if ( (!pOptions->sProfileId || !pOptions->sProfileId[0]) &&
         pProfile->sDefaultLlmProfileId && pProfile->sDefaultLlmProfileId[0] ) {
        pOptions->sProfileId = pProfile->sDefaultLlmProfileId;
    }
    if ( (!pOptions->sDisplayName || !pOptions->sDisplayName[0]) &&
         pProfile->sDisplayName && pProfile->sDisplayName[0] ) {
        pOptions->sDisplayName = pProfile->sDisplayName;
    }

    return XWORK_OK;
}

xwork_status xwork_profile_apply_xllm_bootstrap_options(
    const xwork_profile *pProfile,
    xwork_xllm_profile_options *pProfileOptions,
    xwork_xllm_bootstrap_options *pBootstrapOptions
)
{
    xwork_status iStatus;

    if ( !pProfile || !pProfileOptions || !pBootstrapOptions ) {
        return XWORK_ERROR_INVALID_ARGUMENT;
    }

    iStatus = xwork_profile_apply_xllm_profile_options(pProfile, pProfileOptions);
    if ( iStatus != XWORK_OK ) {
        return iStatus;
    }

    if ( !pBootstrapOptions->pProfiles ) {
        pBootstrapOptions->pProfiles = pProfileOptions;
    }
    if ( pBootstrapOptions->iProfileCount == 0u ) {
        pBootstrapOptions->iProfileCount = 1u;
    }

    return XWORK_OK;
}

xwork_status xwork_profile_apply_workspace_options(
    const xwork_profile *pProfile,
    xwork_workspace_options *pOptions
)
{
    if ( !pProfile || !pOptions ) {
        return XWORK_ERROR_INVALID_ARGUMENT;
    }

    pOptions->bEnableMemory = pProfile->bEnableWorkspaceMemory;
    return XWORK_OK;
}

xwork_status xwork_profile_apply_run_options(
    const xwork_profile *pProfile,
    xwork_run_options *pOptions
)
{
    if ( !pProfile || !pOptions ) {
        return XWORK_ERROR_INVALID_ARGUMENT;
    }

    if ( (!pOptions->sLlmProfileId || !pOptions->sLlmProfileId[0]) &&
         pProfile->sDefaultLlmProfileId && pProfile->sDefaultLlmProfileId[0] ) {
        pOptions->sLlmProfileId = pProfile->sDefaultLlmProfileId;
    }
    if ( (!pOptions->sSessionProfileId || !pOptions->sSessionProfileId[0]) &&
         pProfile->sDefaultSessionProfileId && pProfile->sDefaultSessionProfileId[0] ) {
        pOptions->sSessionProfileId = pProfile->sDefaultSessionProfileId;
    }
    pOptions->eAutonomy = pProfile->eAutonomy;
    pOptions->tSessionPolicy = pProfile->tSessionPolicy;
    return XWORK_OK;
}

xwork_status xwork_profile_apply_orchestrator_options(
    const xwork_profile *pProfile,
    xwork_orchestrator_options *pOptions
)
{
    if ( !pProfile || !pOptions ) {
        return XWORK_ERROR_INVALID_ARGUMENT;
    }

    if ( pProfile->iDefaultMaxTurns > 0u ) {
        pOptions->iMaxTurns = pProfile->iDefaultMaxTurns;
    }
    pOptions->ePlannerMode = pProfile->ePlannerMode;
    pOptions->bAutoApprove = pProfile->bDefaultAutoApprove;
    return XWORK_OK;
}
