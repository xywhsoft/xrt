#include <assert.h>
#include <stdio.h>
#include <string.h>

#define XRT_IMPLEMENTATION
#include "../lib/xrt.h"
#define XLLM_SESSION_IMPLEMENTATION
#include "../lib/xllm-session.h"
#define XLLM_MEMORY_IMPLEMENTATION
#include "../lib/xllm-memory.h"
#include "../xwork.c"

static void xwork_profile_smoke_assert_xcode(void)
{
    xwork_profile tProfile;
    xwork_runtime_options tRuntimeOptions;
    xwork_workspace_options tWorkspaceOptions;
    xwork_run_options tRunOptions;
    xwork_orchestrator_options tExecOptions;
    xwork_xllm_profile_options tLlmOptions;
    xwork_xllm_bootstrap_options tBootstrapOptions;
    xwork_approval_eval_input tApprovalInput;
    xwork_approval_decision tApprovalDecision;

    xwork_profile_init(&tProfile);
    assert(xwork_profile_get_builtin(XWORK_PROFILE_XCODE, &tProfile) == XWORK_OK);
    assert(strcmp(tProfile.sProfileId, XWORK_PROFILE_XCODE) == 0);
    assert(strcmp(tProfile.sDefaultLlmProfileId, XWORK_PROFILE_XCODE) == 0);
    assert(strcmp(tProfile.sDefaultSessionProfileId, XWORK_PROFILE_XCODE) == 0);
    assert(tProfile.eAutonomy == XWORK_AUTONOMY_SEMI_AUTO);
    assert(tProfile.tPolicy.eAutoApproveRiskLimit == XWORK_RISK_LOW);
    assert(tProfile.tPolicy.bDenyNetworkByDefault);
    assert(tProfile.tSessionPolicy.bEnableAutoCompact);
    assert(tProfile.tSessionPolicy.fCompactTriggerRatio == 0.75);
    assert(tProfile.tSessionPolicy.iCompactTriggerTurns == 8u);
    assert(tProfile.iDefaultMaxTurns == 8u);
    assert(!tProfile.bDefaultAutoApprove);
    assert(!tProfile.bEnableWorkspaceMemory);
    assert(tProfile.ePlannerMode == XWORK_PLANNER_OFF);

    xwork_approval_eval_input_init(&tApprovalInput);
    xwork_approval_decision_init(&tApprovalDecision);
    tApprovalInput.eAutonomy = tProfile.eAutonomy;
    tApprovalInput.eApprovalMode = XWORK_APPROVAL_DEFAULT;
    tApprovalInput.eSideEffect = XWORK_SIDE_EFFECT_PROCESS_EXEC;
    tApprovalInput.bAutoApproveRequested = tProfile.bDefaultAutoApprove;
    assert(xwork_policy_evaluate_approval(&tProfile.tPolicy, &tApprovalInput, &tApprovalDecision) == XWORK_OK);
    assert(tApprovalDecision.bRequiresApproval);
    assert(!tApprovalDecision.bAutoApproved);
    assert(tApprovalDecision.eRiskLevel == XWORK_RISK_HIGH);

    xwork_runtime_options_init(&tRuntimeOptions);
    assert(xwork_profile_apply_runtime_options(&tProfile, &tRuntimeOptions) == XWORK_OK);
    assert(tRuntimeOptions.tPolicy.eAutoApproveRiskLimit == XWORK_RISK_LOW);

    xwork_workspace_options_init(&tWorkspaceOptions);
    assert(xwork_profile_apply_workspace_options(&tProfile, &tWorkspaceOptions) == XWORK_OK);
    assert(!tWorkspaceOptions.bEnableMemory);

    xwork_xllm_profile_options_init(&tLlmOptions);
    xwork_xllm_bootstrap_options_init(&tBootstrapOptions);
    assert(xwork_profile_apply_xllm_bootstrap_options(&tProfile, &tLlmOptions, &tBootstrapOptions) == XWORK_OK);
    assert(strcmp(tLlmOptions.sProfileId, XWORK_PROFILE_XCODE) == 0);
    assert(strcmp(tLlmOptions.sDisplayName, "xcode interactive") == 0);
    assert(tBootstrapOptions.pProfiles == &tLlmOptions);
    assert(tBootstrapOptions.iProfileCount == 1u);

    xwork_xllm_profile_options_init(&tLlmOptions);
    xwork_xllm_bootstrap_options_init(&tBootstrapOptions);
    tLlmOptions.sProfileId = "override-xcode-profile";
    tLlmOptions.sDisplayName = "override-xcode-display";
    tBootstrapOptions.pProfiles = (const xwork_xllm_profile_options *)&tProfile;
    tBootstrapOptions.iProfileCount = 7u;
    assert(xwork_profile_apply_xllm_bootstrap_options(&tProfile, &tLlmOptions, &tBootstrapOptions) == XWORK_OK);
    assert(strcmp(tLlmOptions.sProfileId, "override-xcode-profile") == 0);
    assert(strcmp(tLlmOptions.sDisplayName, "override-xcode-display") == 0);
    assert(tBootstrapOptions.pProfiles == (const xwork_xllm_profile_options *)&tProfile);
    assert(tBootstrapOptions.iProfileCount == 7u);

    xwork_run_options_init(&tRunOptions);
    assert(xwork_profile_apply_run_options(&tProfile, &tRunOptions) == XWORK_OK);
    assert(strcmp(tRunOptions.sLlmProfileId, XWORK_PROFILE_XCODE) == 0);
    assert(strcmp(tRunOptions.sSessionProfileId, XWORK_PROFILE_XCODE) == 0);
    assert(tRunOptions.eAutonomy == XWORK_AUTONOMY_SEMI_AUTO);
    assert(tRunOptions.tSessionPolicy.bEnableAutoCompact);
    assert(tRunOptions.tSessionPolicy.fCompactTriggerRatio == 0.75);
    assert(tRunOptions.tSessionPolicy.iCompactTriggerTurns == 8u);

    xwork_run_options_init(&tRunOptions);
    tRunOptions.sLlmProfileId = "override-llm";
    tRunOptions.sSessionProfileId = "override-session";
    assert(xwork_profile_apply_run_options(&tProfile, &tRunOptions) == XWORK_OK);
    assert(strcmp(tRunOptions.sLlmProfileId, "override-llm") == 0);
    assert(strcmp(tRunOptions.sSessionProfileId, "override-session") == 0);
    assert(tRunOptions.eAutonomy == XWORK_AUTONOMY_SEMI_AUTO);

    xwork_orchestrator_options_init(&tExecOptions);
    assert(xwork_profile_apply_orchestrator_options(&tProfile, &tExecOptions) == XWORK_OK);
    assert(tExecOptions.iMaxTurns == 8u);
    assert(!tExecOptions.bAutoApprove);
    assert(tExecOptions.ePlannerMode == XWORK_PLANNER_OFF);
}

static void xwork_profile_smoke_assert_xclaw(void)
{
    xwork_profile tProfile;
    xwork_runtime_options tRuntimeOptions;
    xwork_workspace_options tWorkspaceOptions;
    xwork_run_options tRunOptions;
    xwork_orchestrator_options tExecOptions;
    xwork_xllm_profile_options tLlmOptions;
    xwork_xllm_bootstrap_options tBootstrapOptions;
    xwork_approval_eval_input tApprovalInput;
    xwork_approval_decision tApprovalDecision;

    xwork_profile_init(&tProfile);
    assert(xwork_profile_get_builtin(XWORK_PROFILE_XCLAW, &tProfile) == XWORK_OK);
    assert(strcmp(tProfile.sProfileId, XWORK_PROFILE_XCLAW) == 0);
    assert(strcmp(tProfile.sDefaultLlmProfileId, XWORK_PROFILE_XCLAW) == 0);
    assert(strcmp(tProfile.sDefaultSessionProfileId, XWORK_PROFILE_XCLAW) == 0);
    assert(tProfile.eAutonomy == XWORK_AUTONOMY_AUTO);
    assert(tProfile.tPolicy.eAutoApproveRiskLimit == XWORK_RISK_HIGH);
    assert(tProfile.tPolicy.bDenyNetworkByDefault);
    assert(tProfile.tSessionPolicy.bEnableAutoCompact);
    assert(tProfile.tSessionPolicy.fCompactTriggerRatio == 0.90);
    assert(tProfile.tSessionPolicy.iCompactTriggerTurns == 24u);
    assert(tProfile.iDefaultMaxTurns == 32u);
    assert(tProfile.bDefaultAutoApprove);
    assert(tProfile.bEnableWorkspaceMemory);
    assert(tProfile.ePlannerMode == XWORK_PLANNER_BOUNDARY);

    xwork_approval_eval_input_init(&tApprovalInput);
    xwork_approval_decision_init(&tApprovalDecision);
    tApprovalInput.eAutonomy = tProfile.eAutonomy;
    tApprovalInput.eApprovalMode = XWORK_APPROVAL_DEFAULT;
    tApprovalInput.eSideEffect = XWORK_SIDE_EFFECT_PROCESS_EXEC;
    tApprovalInput.bAutoApproveRequested = tProfile.bDefaultAutoApprove;
    assert(xwork_policy_evaluate_approval(&tProfile.tPolicy, &tApprovalInput, &tApprovalDecision) == XWORK_OK);
    assert(!tApprovalDecision.bRequiresApproval);
    assert(!tApprovalDecision.bAutoApproved);
    assert(tApprovalDecision.eRiskLevel == XWORK_RISK_HIGH);

    xwork_runtime_options_init(&tRuntimeOptions);
    assert(xwork_profile_apply_runtime_options(&tProfile, &tRuntimeOptions) == XWORK_OK);
    assert(tRuntimeOptions.tPolicy.eAutoApproveRiskLimit == XWORK_RISK_HIGH);

    xwork_workspace_options_init(&tWorkspaceOptions);
    assert(xwork_profile_apply_workspace_options(&tProfile, &tWorkspaceOptions) == XWORK_OK);
    assert(tWorkspaceOptions.bEnableMemory);

    xwork_xllm_profile_options_init(&tLlmOptions);
    xwork_xllm_bootstrap_options_init(&tBootstrapOptions);
    assert(xwork_profile_apply_xllm_bootstrap_options(&tProfile, &tLlmOptions, &tBootstrapOptions) == XWORK_OK);
    assert(strcmp(tLlmOptions.sProfileId, XWORK_PROFILE_XCLAW) == 0);
    assert(strcmp(tLlmOptions.sDisplayName, "xclaw autonomous") == 0);
    assert(tBootstrapOptions.pProfiles == &tLlmOptions);
    assert(tBootstrapOptions.iProfileCount == 1u);

    xwork_run_options_init(&tRunOptions);
    assert(xwork_profile_apply_run_options(&tProfile, &tRunOptions) == XWORK_OK);
    assert(strcmp(tRunOptions.sLlmProfileId, XWORK_PROFILE_XCLAW) == 0);
    assert(strcmp(tRunOptions.sSessionProfileId, XWORK_PROFILE_XCLAW) == 0);
    assert(tRunOptions.eAutonomy == XWORK_AUTONOMY_AUTO);
    assert(tRunOptions.tSessionPolicy.bEnableAutoCompact);
    assert(tRunOptions.tSessionPolicy.fCompactTriggerRatio == 0.90);
    assert(tRunOptions.tSessionPolicy.iCompactTriggerTurns == 24u);

    xwork_orchestrator_options_init(&tExecOptions);
    assert(xwork_profile_apply_orchestrator_options(&tProfile, &tExecOptions) == XWORK_OK);
    assert(tExecOptions.iMaxTurns == 32u);
    assert(tExecOptions.bAutoApprove);
    assert(tExecOptions.ePlannerMode == XWORK_PLANNER_BOUNDARY);
}

static void xwork_profile_smoke_assert_bootstrap_run(void)
{
    const char *asWorkspaceIds[1];
    xwork_profile tProfile;
    xwork_xllm_profile_options tLlmOptions;
    xwork_xllm_bootstrap_options tBootstrapOptions;
    xwork_runtime_options tRuntimeOptions;
    xwork_workspace_options tWorkspaceOptions;
    xwork_run_options tRunOptions;
    xwork_run_snapshot tSnapshot;
    xwork_runtime *pRuntime = NULL;
    xwork_workspace *pWorkspace = NULL;
    xwork_run *pRun = NULL;

    xwork_profile_init(&tProfile);
    xwork_xllm_profile_options_init(&tLlmOptions);
    xwork_xllm_bootstrap_options_init(&tBootstrapOptions);
    xwork_runtime_options_init(&tRuntimeOptions);
    xwork_workspace_options_init(&tWorkspaceOptions);
    xwork_run_options_init(&tRunOptions);
    xwork_run_snapshot_init(&tSnapshot);

    assert(xwork_profile_get_builtin(XWORK_PROFILE_XCODE, &tProfile) == XWORK_OK);
    assert(xwork_profile_apply_xllm_bootstrap_options(&tProfile, &tLlmOptions, &tBootstrapOptions) == XWORK_OK);
    tLlmOptions.sAdapter = XWORK_XLLM_ADAPTER_OPENAI_COMPAT;
    tLlmOptions.sBaseUrl = "http://127.0.0.1:1/v1";
    tLlmOptions.sModelId = "dummy-xcode-model";
    tLlmOptions.iMaxOutputTokens = 64u;

    assert(xwork_profile_apply_runtime_options(&tProfile, &tRuntimeOptions) == XWORK_OK);
    tRuntimeOptions.pLlmBootstrap = &tBootstrapOptions;
    assert(xwork_runtime_create(&tRuntimeOptions, &pRuntime) == XWORK_OK);

    tWorkspaceOptions.sWorkspaceId = "profile-main";
    tWorkspaceOptions.sRootPath = ".";
    assert(xwork_runtime_add_workspace(pRuntime, &tWorkspaceOptions, &pWorkspace) == XWORK_OK);
    assert(pWorkspace != NULL);

    asWorkspaceIds[0] = "profile-main";
    assert(xwork_profile_apply_run_options(&tProfile, &tRunOptions) == XWORK_OK);
    tRunOptions.sRunId = "profile-smoke-run";
    tRunOptions.sInstruction = "Profile smoke run.";
    tRunOptions.psWorkspaceIds = asWorkspaceIds;
    tRunOptions.iWorkspaceCount = 1u;
    assert(xwork_run_create(pRuntime, &tRunOptions, &pRun) == XWORK_OK);
    assert(xwork_run_get_snapshot(pRun, &tSnapshot) == XWORK_OK);
    assert(strcmp(tSnapshot.sLlmProfileId, XWORK_PROFILE_XCODE) == 0);
    assert(strcmp(tSnapshot.sSessionProfileId, XWORK_PROFILE_XCODE) == 0);

    xwork_run_snapshot_reset(&tSnapshot);
    xwork_run_destroy(pRun);
    xwork_runtime_destroy(pRuntime);
}

int main(void)
{
    xwork_profile tMissingProfile;

    xwork_profile_smoke_assert_xcode();
    xwork_profile_smoke_assert_xclaw();
    xwork_profile_smoke_assert_bootstrap_run();

    xwork_profile_init(&tMissingProfile);
    assert(xwork_profile_get_builtin("missing-profile", &tMissingProfile) == XWORK_ERROR_NOT_FOUND);
    assert(xwork_profile_get_builtin(NULL, &tMissingProfile) == XWORK_ERROR_NOT_FOUND);
    assert(xwork_profile_get_builtin(XWORK_PROFILE_XCODE, NULL) == XWORK_ERROR_INVALID_ARGUMENT);
    assert(xwork_profile_apply_runtime_options(NULL, NULL) == XWORK_ERROR_INVALID_ARGUMENT);

    puts("xwork profile smoke passed");
    return 0;
}
