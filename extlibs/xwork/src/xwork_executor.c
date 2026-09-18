/* Executor adapter: the agent's tool machinery behind the xllm contract.
 *
 * Slice note: the binding currently wraps an xwork_agent because the registry,
 * permission gate, hooks and spill path live on it. The planned toolset /
 * process-table extraction rehomes that machinery into standalone objects;
 * this adapter then keeps its signature while the agent dependency shrinks
 * to the toolset. */

struct xwork_executor_state {
    xwork_agent* pAgent;
    char* sLastResult;   /* rolling storage: freed on the next execute or unbind */
};

static bool xwork__executor_list(void* pUserData, xllm_request* pRequest)
{
    xwork_executor_state* pState = (xwork_executor_state*)pUserData;
    size_t i;
    if ( !pState || !pState->pAgent || !pRequest ) { return false; }
    for ( i = 0u; i < pState->pAgent->iToolCount; ++i ) {
        const xwork_tool_entry* pTool = &pState->pAgent->pTools[i];
        if ( !xllmRequestAddTool(pRequest, pTool->sName, pTool->sDescription,
                pTool->sParametersJson, pTool->bStrict) ) {
            return false;
        }
    }
    return true;
}

static bool xwork__executor_execute(void* pUserData, const xllm_tool_call* pCall,
    const xllm_executor_ctx* pCtx, xllm_executor_result* pResult)
{
    xwork_executor_state* pState = (xwork_executor_state*)pUserData;
    xwork_error tError;
    char* sContent = NULL;
    bool bSuccess = false;
    xwork_result eResult;
    if ( pResult ) { memset(pResult, 0, sizeof(*pResult)); }
    if ( !pState || !pState->pAgent || !pCall || !pResult ) { return false; }
    if ( pCtx ) {
        if ( pCtx->pCancel && xrtCancelRequested(pCtx->pCancel) ) {
            xworkErrorInit(&tError);
            xwork__set_error(&tError, XWORK_ERROR_CANCELLED,
                "executor refused a tool call after cancellation");
            return false;
        }
        if ( pCtx->uDeadline != 0u && pCtx->uDeadline != XRT_DEADLINE_NEVER &&
             xrtDeadlineExpired(pCtx->uDeadline) ) {
            xworkErrorInit(&tError);
            xwork__set_error(&tError, XWORK_ERROR_TIMEOUT,
                "executor refused a tool call after the operation deadline");
            return false;
        }
    }
    xworkErrorInit(&tError);
    eResult = xwork__execute_tool(pState->pAgent, pCall, pCtx ? pCtx->uTurn : 0u,
        &sContent, &bSuccess, NULL, &tError);
    if ( eResult != XWORK_RESULT_OK ) {
        free(sContent);
        return false;   /* infrastructure failure: run aborts */
    }
    free(pState->sLastResult);
    pState->sLastResult = sContent;
    pResult->sContent = sContent;
    pResult->bSuccess = bSuccess;
    return true;
}

bool xworkExecutorBind(xllm_executor* pOut, xwork_agent* pAgent, xwork_error* pError)
{
    xwork_executor_state* pState;
    if ( pError ) { xworkErrorInit(pError); }
    if ( !pOut || !pAgent ) {
        xwork__set_error(pError, XWORK_ERROR_INVALID_ARGUMENT,
            "executor binding requires an output executor and an agent");
        return false;
    }
    pState = (xwork_executor_state*)calloc(1u, sizeof(*pState));
    if ( !pState ) {
        xwork__set_error(pError, XWORK_ERROR_OUT_OF_MEMORY,
            "failed to allocate the executor binding");
        return false;
    }
    pState->pAgent = pAgent;
    memset(pOut, 0, sizeof(*pOut));
    pOut->pListTools = xwork__executor_list;
    pOut->pExecute = xwork__executor_execute;
    pOut->pUserData = pState;
    return true;
}

void xworkExecutorUnbind(xllm_executor* pExecutor)
{
    xwork_executor_state* pState = NULL;
    if ( !pExecutor ) { return; }
    if ( pExecutor->pListTools == xwork__executor_list ) {
        pState = (xwork_executor_state*)pExecutor->pUserData;
    }
    if ( pState ) {
        free(pState->sLastResult);
        free(pState);
    }
    memset(pExecutor, 0, sizeof(*pExecutor));
}
