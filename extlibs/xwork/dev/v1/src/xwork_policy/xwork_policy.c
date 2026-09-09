#include "../xwork_core/xwork_internal.h"

static xwork_risk_level xwork__policy_risk_level(xwork_tool_side_effect eSideEffect)
{
    switch ( eSideEffect ) {
        case XWORK_SIDE_EFFECT_READ_ONLY:
            return XWORK_RISK_LOW;
        case XWORK_SIDE_EFFECT_WORKSPACE_WRITE:
            return XWORK_RISK_MEDIUM;
        case XWORK_SIDE_EFFECT_PROCESS_EXEC:
        case XWORK_SIDE_EFFECT_NETWORK_ACCESS:
            return XWORK_RISK_HIGH;
        case XWORK_SIDE_EFFECT_EXTERNAL_MUTATION:
            return XWORK_RISK_CRITICAL;
        default:
            return XWORK_RISK_MEDIUM;
    }
}

static const char *xwork__policy_scope(xwork_tool_side_effect eSideEffect)
{
    switch ( eSideEffect ) {
        case XWORK_SIDE_EFFECT_READ_ONLY:
            return "read_only";
        case XWORK_SIDE_EFFECT_WORKSPACE_WRITE:
            return "workspace_write";
        case XWORK_SIDE_EFFECT_PROCESS_EXEC:
            return "process_exec";
        case XWORK_SIDE_EFFECT_NETWORK_ACCESS:
            return "network_access";
        case XWORK_SIDE_EFFECT_EXTERNAL_MUTATION:
            return "external_mutation";
        default:
            return "unknown_scope";
    }
}

static bool xwork__policy_ascii_contains_ci(const char *sText, const char *sPattern)
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

static bool xwork__policy_host_matches_any(
    const char *sHost,
    const char **psPatterns,
    size_t iPatternCount
)
{
    size_t i;

    if ( !sHost || !psPatterns || iPatternCount == 0u ) {
        return false;
    }

    for ( i = 0u; i < iPatternCount; ++i ) {
        if ( psPatterns[i] &&
             psPatterns[i][0] &&
             xwork__policy_ascii_contains_ci(sHost, psPatterns[i]) ) {
            return true;
        }
    }
    return false;
}

void xwork_policy_options_init(xwork_policy_options *pOptions)
{
    if ( pOptions ) {
        memset(pOptions, 0, sizeof(*pOptions));
        pOptions->eAutoApproveRiskLimit = XWORK_RISK_CRITICAL;
    }
}

void xwork_approval_eval_input_init(xwork_approval_eval_input *pInput)
{
    if ( pInput ) {
        memset(pInput, 0, sizeof(*pInput));
        pInput->eAutonomy = XWORK_AUTONOMY_SEMI_AUTO;
        pInput->eApprovalMode = XWORK_APPROVAL_DEFAULT;
        pInput->eSideEffect = XWORK_SIDE_EFFECT_READ_ONLY;
    }
}

void xwork_approval_decision_init(xwork_approval_decision *pDecision)
{
    if ( pDecision ) {
        memset(pDecision, 0, sizeof(*pDecision));
        pDecision->eRiskLevel = XWORK_RISK_LOW;
        pDecision->sScope = "read_only";
    }
}

void xwork_network_policy_eval_input_init(xwork_network_policy_eval_input *pInput)
{
    if ( pInput ) {
        memset(pInput, 0, sizeof(*pInput));
        pInput->bNetworkAccessRequested = true;
    }
}

void xwork_network_policy_decision_init(xwork_network_policy_decision *pDecision)
{
    if ( pDecision ) {
        memset(pDecision, 0, sizeof(*pDecision));
        pDecision->bAllowed = true;
        pDecision->eRiskLevel = XWORK_RISK_HIGH;
        pDecision->sScope = "network_access";
    }
}

xwork_status xwork_policy_evaluate_approval(
    const xwork_policy_options *pPolicy,
    const xwork_approval_eval_input *pInput,
    xwork_approval_decision *pDecision
)
{
    xwork_policy_options tDefaultPolicy;

    if ( !pInput || !pDecision ) {
        return XWORK_ERROR_INVALID_ARGUMENT;
    }

    if ( !pPolicy ) {
        xwork_policy_options_init(&tDefaultPolicy);
        pPolicy = &tDefaultPolicy;
    }

    xwork_approval_decision_init(pDecision);
    pDecision->eRiskLevel = xwork__policy_risk_level(pInput->eSideEffect);
    pDecision->sScope = xwork__policy_scope(pInput->eSideEffect);
    if ( pInput->bHasRiskLevelOverride ) {
        pDecision->eRiskLevel = pInput->eRiskLevelOverride;
        if ( pInput->sRiskScopeOverride && pInput->sRiskScopeOverride[0] ) {
            pDecision->sScope = pInput->sRiskScopeOverride;
        }
    }

    switch ( pInput->eApprovalMode ) {
        case XWORK_APPROVAL_NEVER:
            pDecision->bRequiresApproval = false;
            pDecision->sReason = "Approval is disabled for this tool.";
            break;
        case XWORK_APPROVAL_ALWAYS:
            pDecision->bRequiresApproval = true;
            pDecision->sReason = "Tool approval mode is set to always.";
            break;
        case XWORK_APPROVAL_ON_DEMAND:
            pDecision->bRequiresApproval = true;
            pDecision->sReason = "Tool approval mode is set to on_demand.";
            break;
        case XWORK_APPROVAL_DEFAULT:
        default:
            if ( pInput->eSideEffect == XWORK_SIDE_EFFECT_READ_ONLY ) {
                pDecision->bRequiresApproval = false;
                pDecision->sReason = "Read-only tools do not require approval.";
            } else if ( pInput->eAutonomy == XWORK_AUTONOMY_AUTO ) {
                pDecision->bRequiresApproval = false;
                pDecision->sReason = "Auto autonomy allows this tool without approval.";
            } else {
                pDecision->bRequiresApproval = true;
                pDecision->sReason = "Current autonomy requires approval for side-effecting tools.";
            }
            break;
    }

    if ( pInput->bHasRiskLevelOverride &&
         pInput->sRiskReasonOverride &&
         pInput->sRiskReasonOverride[0] ) {
        pDecision->sReason = pInput->sRiskReasonOverride;
    }

    pDecision->bAutoApproved =
        pDecision->bRequiresApproval &&
        pInput->bAutoApproveRequested &&
        pDecision->eRiskLevel <= pPolicy->eAutoApproveRiskLimit;

    return XWORK_OK;
}

xwork_status xwork_policy_evaluate_network_access(
    const xwork_policy_options *pPolicy,
    const xwork_network_policy_eval_input *pInput,
    xwork_network_policy_decision *pDecision
)
{
    xwork_policy_options tDefaultPolicy;
    const char *sHost;
    bool bAllowedByAllowList;

    if ( !pInput || !pDecision ) {
        return XWORK_ERROR_INVALID_ARGUMENT;
    }

    if ( !pPolicy ) {
        xwork_policy_options_init(&tDefaultPolicy);
        pPolicy = &tDefaultPolicy;
    }

    xwork_network_policy_decision_init(pDecision);
    pDecision->eRiskLevel = XWORK_RISK_HIGH;
    pDecision->sScope = "network_access";

    if ( !pInput->bNetworkAccessRequested ) {
        pDecision->bAllowed = true;
        pDecision->sReason = "No network access was requested.";
        return XWORK_OK;
    }

    sHost = (pInput->sHost && pInput->sHost[0]) ? pInput->sHost : pInput->sUrl;
    if ( !sHost || !sHost[0] ) {
        pDecision->bAllowed = false;
        pDecision->sReason = "Network host or URL is required.";
        return XWORK_OK;
    }

    if ( xwork__policy_host_matches_any(
             sHost,
             pPolicy->psNetworkDenyHostPatterns,
             pPolicy->iNetworkDenyHostPatternCount
         ) ) {
        pDecision->bAllowed = false;
        pDecision->sReason = "Network host matches denied host patterns.";
        return XWORK_OK;
    }

    bAllowedByAllowList = pPolicy->iNetworkAllowHostPatternCount == 0u ||
        xwork__policy_host_matches_any(
            sHost,
            pPolicy->psNetworkAllowHostPatterns,
            pPolicy->iNetworkAllowHostPatternCount
        );
    if ( !bAllowedByAllowList ||
         (pPolicy->bDenyNetworkByDefault &&
          pPolicy->iNetworkAllowHostPatternCount == 0u) ) {
        pDecision->bAllowed = false;
        pDecision->sReason = pPolicy->bDenyNetworkByDefault
            ? "Network access is denied by default."
            : "Network host is not in allowed host patterns.";
        return XWORK_OK;
    }

    pDecision->bAllowed = true;
    pDecision->sReason = "Network host is allowed by policy.";
    return XWORK_OK;
}
