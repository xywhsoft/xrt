#include "xllm_internal.h"

static const xllm_capability_flags XLLM_GLM_AGENT_CAPS =
    XLLM_CAP_TEXT_IN | XLLM_CAP_TOOL_RESULT_IN | XLLM_CAP_TEXT_OUT |
    XLLM_CAP_JSON_OUT | XLLM_CAP_TOOL_CALL_OUT | XLLM_CAP_REASONING_OUT |
    XLLM_CAP_STREAM | XLLM_CAP_REASONING_CONTROL | XLLM_CAP_PARALLEL_TOOL_CALL;

static const xllm_model_profile XLLM_BUILTIN_PROFILES[] = {
    {
        "glm-5.2-coding", "glm-5.2", XLLM_PROVIDER_GLM, XLLM_GLM_AGENT_CAPS,
        XLLM_WINDOW_SHARED_CONTEXT, 1000000ull, 999999ull, 131072u, 65536u, 32768u
    },
    {
        "glm-5.1-coding", "glm-5.1", XLLM_PROVIDER_GLM, XLLM_GLM_AGENT_CAPS,
        XLLM_WINDOW_SHARED_CONTEXT, 204800ull, 204799ull, 131072u, 32768u, 32768u
    },
    {
        "glm-5-coding", "glm-5", XLLM_PROVIDER_GLM, XLLM_GLM_AGENT_CAPS,
        XLLM_WINDOW_SHARED_CONTEXT, 204800ull, 204799ull, 131072u, 32768u, 32768u
    }
};

static bool xllm_profile__equal_ci(const char* a, const char* b)
{
    if ( !a || !b ) return false;
    while ( *a && *b ) {
        if ( tolower((unsigned char)*a) != tolower((unsigned char)*b) ) return false;
        ++a;
        ++b;
    }
    return *a == '\0' && *b == '\0';
}

void xllmModelProfileInit(xllm_model_profile* pProfile)
{
    if ( !pProfile ) return;
    memset(pProfile, 0, sizeof(*pProfile));
    pProfile->eWindowMode = XLLM_WINDOW_SHARED_CONTEXT;
}

const xllm_model_profile* xllmModelProfileBuiltin(const char* sIdOrModel)
{
    size_t i;
    if ( !sIdOrModel || !sIdOrModel[0] ) return NULL;
    for ( i = 0u; i < sizeof(XLLM_BUILTIN_PROFILES) / sizeof(XLLM_BUILTIN_PROFILES[0]); ++i ) {
        if ( xllm_profile__equal_ci(sIdOrModel, XLLM_BUILTIN_PROFILES[i].sId) ||
             xllm_profile__equal_ci(sIdOrModel, XLLM_BUILTIN_PROFILES[i].sModel) ) {
            return &XLLM_BUILTIN_PROFILES[i];
        }
    }
    return NULL;
}

bool xllmModelProfileValidate(const xllm_model_profile* pProfile, xllm_error* pError)
{
    const xllm_capability_flags uBasic = XLLM_CAP_TEXT_IN | XLLM_CAP_TEXT_OUT | XLLM_CAP_STREAM;
    if ( pError ) xllmErrorInit(pError);
    if ( !pProfile || !pProfile->sId || !pProfile->sId[0] ||
         !pProfile->sModel || !pProfile->sModel[0] ) {
        xllm__error_set(pError, XLLM_ERROR_INVALID_ARGUMENT, "model profile id and model are required");
        return false;
    }
    if ( (pProfile->uCapabilities & uBasic) != uBasic ||
         pProfile->eWindowMode == XLLM_WINDOW_UNSPECIFIED ||
         pProfile->uContextWindowTokens == 0u || pProfile->uMaxOutputTokens == 0u ||
         pProfile->uMaxInputTokens == 0u ||
         pProfile->uMaxInputTokens > pProfile->uContextWindowTokens ||
         pProfile->uMaxOutputTokens > pProfile->uContextWindowTokens ||
         pProfile->uRecommendedOutputReserveTokens > pProfile->uMaxOutputTokens ||
         pProfile->uRecommendedSummaryTokens > pProfile->uMaxOutputTokens ) {
        xllm__error_set(pError, XLLM_ERROR_INVALID_ARGUMENT, "model profile has invalid capabilities or token limits");
        return false;
    }
    if ( (pProfile->uCapabilities & XLLM_CAP_PARALLEL_TOOL_CALL) != 0u &&
         (pProfile->uCapabilities & XLLM_CAP_TOOL_CALL_OUT) == 0u ) {
        xllm__error_set(pError, XLLM_ERROR_INVALID_ARGUMENT, "parallel tools require tool-call output capability");
        return false;
    }
    return true;
}

bool xllmModelProfileSupports(const xllm_model_profile* pProfile, xllm_capability_flags uRequired)
{
    return pProfile && (pProfile->uCapabilities & uRequired) == uRequired;
}

bool xllmModelProfileValidateRequest(const xllm_model_profile* pProfile,
    const xllm_request* pRequest, xllm_error* pError)
{
    xllm_capability_flags uRequired = XLLM_CAP_TEXT_IN | XLLM_CAP_TEXT_OUT | XLLM_CAP_STREAM;
    uint32_t uOutput;
    size_t i;
    if ( pError ) xllmErrorInit(pError);
    if ( !xllmModelProfileValidate(pProfile, pError) || !pRequest ) {
        if ( pRequest == NULL ) xllm__error_set(pError, XLLM_ERROR_INVALID_ARGUMENT, "model request is required");
        return false;
    }
    if ( pRequest->sModel && pRequest->sModel[0] && !xllm_profile__equal_ci(pRequest->sModel, pProfile->sModel) ) {
        xllm__error_set(pError, XLLM_ERROR_MODEL_NOT_FOUND, "request model does not match the active model profile");
        return false;
    }
    if ( pRequest->iToolCount != 0u || pRequest->eToolChoice == XLLM_TOOL_CHOICE_REQUIRED ||
         pRequest->eToolChoice == XLLM_TOOL_CHOICE_NAMED ) uRequired |= XLLM_CAP_TOOL_CALL_OUT;
    if ( pRequest->bParallelToolCalls ) uRequired |= XLLM_CAP_PARALLEL_TOOL_CALL;
    if ( pRequest->sReasoningEffort && pRequest->sReasoningEffort[0] ) uRequired |= XLLM_CAP_REASONING_CONTROL;
    for ( i = 0u; i < pRequest->iMessageCount; ++i ) {
        const xllm_message* pMessage = &pRequest->pMessages[i];
        size_t j;
        if ( pMessage->eRole == XLLM_ROLE_TOOL ) uRequired |= XLLM_CAP_TOOL_RESULT_IN;
        if ( pMessage->iToolCallCount != 0u ) uRequired |= XLLM_CAP_TOOL_CALL_OUT;
        for ( j = 0u; j < pMessage->iPartCount; ++j ) {
            if ( pMessage->pParts[j].eKind == XLLM_PART_IMAGE ||
                 pMessage->pParts[j].eKind == XLLM_PART_AUDIO ||
                 pMessage->pParts[j].eKind == XLLM_PART_FILE ) {
                uRequired |= XLLM_CAP_IMAGE_IN;
            }
        }
    }
    if ( !xllmModelProfileSupports(pProfile, uRequired) ) {
        xllm__error_set(pError, XLLM_ERROR_INVALID_ARGUMENT, "request requires capabilities not declared by the model profile");
        return false;
    }
    uOutput = pRequest->uMaxOutputTokens ? pRequest->uMaxOutputTokens : pProfile->uMaxOutputTokens;
    if ( uOutput > pProfile->uMaxOutputTokens ) {
        xllm__error_set(pError, XLLM_ERROR_INVALID_ARGUMENT, "request output limit exceeds the model profile");
        return false;
    }
    return true;
}
