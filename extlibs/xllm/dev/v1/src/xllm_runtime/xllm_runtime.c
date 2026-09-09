#include "xllm_runtime.h"

static void *xllm__default_malloc(void *pCtx, size_t iSize)
{
    (void)pCtx;
    return xrtCalloc(1, iSize);
}

static void *xllm__default_realloc(void *pCtx, void *pPtr, size_t iSize)
{
    (void)pCtx;
    return xrtRealloc(pPtr, iSize);
}

static void xllm__default_free(void *pCtx, void *pPtr)
{
    (void)pCtx;
    xrtFree(pPtr);
}

static void xllm__transport_options_reset(xllm_transport_options *pOptions);

static int xllm__string_array_clone(const char ***ppsOut, size_t *piOutCount, const char **psIn, size_t iInCount)
{
    char **psCopy;
    size_t i;

    if ( !ppsOut || !piOutCount ) {
        return XRT_NET_ERROR;
    }

    *ppsOut = NULL;
    *piOutCount = 0;
    if ( !psIn || iInCount == 0 ) {
        return XRT_NET_OK;
    }

    psCopy = (char **)xrtCalloc(iInCount, sizeof(char *));
    if ( !psCopy ) {
        return XRT_NET_ERROR;
    }

    for ( i = 0; i < iInCount; ++i ) {
        psCopy[i] = xllm__dup_cstr(psIn[i]);
        if ( psIn[i] && !psCopy[i] ) {
            size_t j;
            for ( j = 0; j < i; ++j ) {
                xllm__free_cstr(&psCopy[j]);
            }
            xrtFree(psCopy);
            return XRT_NET_ERROR;
        }
    }

    *ppsOut = (const char **)psCopy;
    *piOutCount = iInCount;
    return XRT_NET_OK;
}

static void xllm__string_array_free(const char ***ppsArray, size_t *piCount)
{
    size_t i;
    char **psArray;

    if ( !ppsArray || !*ppsArray ) {
        if ( piCount ) {
            *piCount = 0;
        }
        return;
    }

    psArray = (char **)*ppsArray;
    if ( piCount ) {
        for ( i = 0; i < *piCount; ++i ) {
            xllm__free_cstr(&psArray[i]);
        }
        *piCount = 0;
    }
    xrtFree(psArray);
    *ppsArray = NULL;
}

static int xllm__header_array_clone(xllm_header **ppOut, size_t *piOutCount, const xllm_header *pIn, size_t iInCount)
{
    xllm_header *pHeaders;
    size_t i;

    if ( !ppOut || !piOutCount ) {
        return XRT_NET_ERROR;
    }

    *ppOut = NULL;
    *piOutCount = 0;
    if ( !pIn || iInCount == 0 ) {
        return XRT_NET_OK;
    }

    pHeaders = (xllm_header *)xrtCalloc(iInCount, sizeof(xllm_header));
    if ( !pHeaders ) {
        return XRT_NET_ERROR;
    }

    for ( i = 0; i < iInCount; ++i ) {
        pHeaders[i].sName = xllm__dup_cstr(pIn[i].sName);
        pHeaders[i].sValue = xllm__dup_cstr(pIn[i].sValue);
        if ( (pIn[i].sName && !pHeaders[i].sName) || (pIn[i].sValue && !pHeaders[i].sValue) ) {
            size_t j;
            for ( j = 0; j <= i; ++j ) {
                xllm__free_cstr((char **)&pHeaders[j].sName);
                xllm__free_cstr((char **)&pHeaders[j].sValue);
            }
            xrtFree(pHeaders);
            return XRT_NET_ERROR;
        }
    }

    *ppOut = pHeaders;
    *piOutCount = iInCount;
    return XRT_NET_OK;
}

static void xllm__header_array_free(xllm_header **ppHeaders, size_t *piCount)
{
    size_t i;

    if ( !ppHeaders || !*ppHeaders ) {
        if ( piCount ) {
            *piCount = 0;
        }
        return;
    }

    if ( piCount ) {
        for ( i = 0; i < *piCount; ++i ) {
            xllm__free_cstr((char **)&(*ppHeaders)[i].sName);
            xllm__free_cstr((char **)&(*ppHeaders)[i].sValue);
        }
        *piCount = 0;
    }
    xrtFree(*ppHeaders);
    *ppHeaders = NULL;
}

static int xllm__transport_options_clone(xllm_transport_options *pOut, const xllm_transport_options *pIn)
{
    if ( !pOut || !pIn ) {
        return XRT_NET_ERROR;
    }

    memset(pOut, 0, sizeof(*pOut));
    *pOut = *pIn;
    pOut->sProxyHost = xllm__dup_cstr(pIn->sProxyHost);
    pOut->sProxyUser = xllm__dup_cstr(pIn->sProxyUser);
    pOut->sProxyPass = xllm__dup_cstr(pIn->sProxyPass);
    pOut->sCaBundlePath = xllm__dup_cstr(pIn->sCaBundlePath);
    pOut->sClientCertPath = xllm__dup_cstr(pIn->sClientCertPath);
    pOut->sClientKeyPath = xllm__dup_cstr(pIn->sClientKeyPath);
    pOut->tVendorExtra = pIn->tVendorExtra;
    xllm__xvalue_addref(pOut->tVendorExtra);
    if ( (pIn->sProxyHost && !pOut->sProxyHost) ||
         (pIn->sProxyUser && !pOut->sProxyUser) ||
         (pIn->sProxyPass && !pOut->sProxyPass) ||
         (pIn->sCaBundlePath && !pOut->sCaBundlePath) ||
         (pIn->sClientCertPath && !pOut->sClientCertPath) ||
         (pIn->sClientKeyPath && !pOut->sClientKeyPath) ) {
        xllm__transport_options_reset(pOut);
        return XRT_NET_ERROR;
    }
    return XRT_NET_OK;
}

static void xllm__transport_options_reset(xllm_transport_options *pOptions)
{
    if ( !pOptions ) {
        return;
    }

    xllm__free_cstr((char **)&pOptions->sProxyHost);
    xllm__free_cstr((char **)&pOptions->sProxyUser);
    xllm__free_cstr((char **)&pOptions->sProxyPass);
    xllm__free_cstr((char **)&pOptions->sCaBundlePath);
    xllm__free_cstr((char **)&pOptions->sClientCertPath);
    xllm__free_cstr((char **)&pOptions->sClientKeyPath);
    xllm__xvalue_release(&pOptions->tVendorExtra);
    memset(pOptions, 0, sizeof(*pOptions));
}

static int xllm__provider_options_clone(xllm_provider_options *pOut, const xllm_provider_options *pIn)
{
    if ( !pOut || !pIn ) {
        return XRT_NET_ERROR;
    }

    memset(pOut, 0, sizeof(*pOut));
    pOut->sOpenAIOrganizationId = xllm__dup_cstr(pIn->sOpenAIOrganizationId);
    pOut->sOpenAIProjectId = xllm__dup_cstr(pIn->sOpenAIProjectId);
    pOut->sAnthropicApiVersion = xllm__dup_cstr(pIn->sAnthropicApiVersion);
    pOut->tVendorExtra = pIn->tVendorExtra;
    xllm__xvalue_addref(pOut->tVendorExtra);
    if ( xllm__string_array_clone(&pOut->psAnthropicBetaHeaders, &pOut->iAnthropicBetaHeaderCount, pIn->psAnthropicBetaHeaders, pIn->iAnthropicBetaHeaderCount) != XRT_NET_OK ) {
        return XRT_NET_ERROR;
    }
    return XRT_NET_OK;
}

static void xllm__provider_options_reset(xllm_provider_options *pOptions)
{
    if ( !pOptions ) {
        return;
    }

    xllm__free_cstr((char **)&pOptions->sOpenAIOrganizationId);
    xllm__free_cstr((char **)&pOptions->sOpenAIProjectId);
    xllm__free_cstr((char **)&pOptions->sAnthropicApiVersion);
    xllm__string_array_free(&pOptions->psAnthropicBetaHeaders, &pOptions->iAnthropicBetaHeaderCount);
    xllm__xvalue_release(&pOptions->tVendorExtra);
    memset(pOptions, 0, sizeof(*pOptions));
}

static int xllm__model_caps_clone(xllm_model_caps *pOut, const xllm_model_caps *pIn)
{
    if ( !pOut || !pIn ) {
        return XRT_NET_ERROR;
    }

    memset(pOut, 0, sizeof(*pOut));
    *pOut = *pIn;
    pOut->sTokenizerId = xllm__dup_cstr(pIn->sTokenizerId);
    pOut->tVendorExtra = pIn->tVendorExtra;
    xllm__xvalue_addref(pOut->tVendorExtra);
    if ( xllm__string_array_clone(&pOut->psSupportedMimeTypes, &pOut->iSupportedMimeTypeCount, pIn->psSupportedMimeTypes, pIn->iSupportedMimeTypeCount) != XRT_NET_OK ) {
        xllm__xvalue_release(&pOut->tVendorExtra);
        xllm__free_cstr((char **)&pOut->sTokenizerId);
        memset(pOut, 0, sizeof(*pOut));
        return XRT_NET_ERROR;
    }
    return XRT_NET_OK;
}

static void xllm__model_caps_reset(xllm_model_caps *pCaps)
{
    if ( !pCaps ) {
        return;
    }

    xllm__string_array_free(&pCaps->psSupportedMimeTypes, &pCaps->iSupportedMimeTypeCount);
    xllm__free_cstr((char **)&pCaps->sTokenizerId);
    xllm__xvalue_release(&pCaps->tVendorExtra);
    memset(pCaps, 0, sizeof(*pCaps));
}

static int xllm__model_binding_clone(xllm_model_binding *pOut, const xllm_model_binding *pIn)
{
    if ( !pOut || !pIn ) {
        return XRT_NET_ERROR;
    }

    memset(pOut, 0, sizeof(*pOut));
    pOut->sModelId = xllm__dup_cstr(pIn->sModelId);
    pOut->sAliasOf = xllm__dup_cstr(pIn->sAliasOf);
    pOut->eCapMode = pIn->eCapMode;
    pOut->tVendorExtra = pIn->tVendorExtra;
    xllm__xvalue_addref(pOut->tVendorExtra);
    if ( xllm__model_caps_clone(&pOut->tCaps, &pIn->tCaps) != XRT_NET_OK ) {
        xllm__free_cstr((char **)&pOut->sModelId);
        xllm__free_cstr((char **)&pOut->sAliasOf);
        xllm__xvalue_release(&pOut->tVendorExtra);
        memset(pOut, 0, sizeof(*pOut));
        return XRT_NET_ERROR;
    }
    return XRT_NET_OK;
}

static void xllm__model_binding_reset(xllm_model_binding *pBinding)
{
    if ( !pBinding ) {
        return;
    }

    xllm__free_cstr((char **)&pBinding->sModelId);
    xllm__free_cstr((char **)&pBinding->sAliasOf);
    xllm__model_caps_reset(&pBinding->tCaps);
    xllm__xvalue_release(&pBinding->tVendorExtra);
    memset(pBinding, 0, sizeof(*pBinding));
}

static int xllm__profile_defaults_clone(xllm_profile_defaults *pOut, const xllm_profile_defaults *pIn)
{
    if ( !pOut || !pIn ) {
        return XRT_NET_ERROR;
    }

    memset(pOut, 0, sizeof(*pOut));
    if ( xllm__generation_params_clone(&pOut->tGeneration, &pIn->tGeneration) != XRT_NET_OK ) {
        return XRT_NET_ERROR;
    }
    if ( xllm__reasoning_options_clone(&pOut->tReasoning, &pIn->tReasoning) != XRT_NET_OK ) {
        xllm__generation_params_reset(&pOut->tGeneration);
        return XRT_NET_ERROR;
    }
    if ( xllm__response_format_clone(&pOut->tResponseFormat, &pIn->tResponseFormat) != XRT_NET_OK ) {
        xllm__generation_params_reset(&pOut->tGeneration);
        xllm__reasoning_options_reset(&pOut->tReasoning);
        return XRT_NET_ERROR;
    }
    pOut->tVendorExtra = pIn->tVendorExtra;
    xllm__xvalue_addref(pOut->tVendorExtra);
    return XRT_NET_OK;
}

static void xllm__profile_defaults_reset(xllm_profile_defaults *pDefaults)
{
    if ( !pDefaults ) {
        return;
    }

    xllm__generation_params_reset(&pDefaults->tGeneration);
    xllm__reasoning_options_reset(&pDefaults->tReasoning);
    xllm__response_format_reset(&pDefaults->tResponseFormat);
    xllm__xvalue_release(&pDefaults->tVendorExtra);
    memset(pDefaults, 0, sizeof(*pDefaults));
}

static int xllm__profile_clone(xllm_profile *pOut, const xllm_profile *pIn)
{
    if ( !pOut || !pIn ) {
        return XRT_NET_ERROR;
    }

    memset(pOut, 0, sizeof(*pOut));
    pOut->sId = xllm__dup_cstr(pIn->sId);
    pOut->sName = xllm__dup_cstr(pIn->sName);
    pOut->sProvider = xllm__dup_cstr(pIn->sProvider);
    pOut->sAdapter = xllm__dup_cstr(pIn->sAdapter);
    pOut->sBaseUrl = xllm__dup_cstr(pIn->sBaseUrl);
    pOut->tAuth.eKind = pIn->tAuth.eKind;
    pOut->tAuth.sSecret = xllm__dup_cstr(pIn->tAuth.sSecret);
    pOut->tAuth.sHeaderName = xllm__dup_cstr(pIn->tAuth.sHeaderName);
    pOut->tAuth.sScheme = xllm__dup_cstr(pIn->tAuth.sScheme);
    pOut->tVendorExtra = pIn->tVendorExtra;
    xllm__xvalue_addref(pOut->tVendorExtra);

    if ( xllm__header_array_clone(&pOut->pDefaultHeaders, &pOut->iDefaultHeaderCount, pIn->pDefaultHeaders, pIn->iDefaultHeaderCount) != XRT_NET_OK ||
         xllm__provider_options_clone(&pOut->tProviderOptions, &pIn->tProviderOptions) != XRT_NET_OK ||
         xllm__transport_options_clone(&pOut->tTransport, &pIn->tTransport) != XRT_NET_OK ||
         xllm__model_binding_clone(&pOut->tModels.tText, &pIn->tModels.tText) != XRT_NET_OK ||
         xllm__model_binding_clone(&pOut->tModels.tMultimodal, &pIn->tModels.tMultimodal) != XRT_NET_OK ||
         xllm__profile_defaults_clone(&pOut->tDefaults, &pIn->tDefaults) != XRT_NET_OK ) {
        xllm__header_array_free(&pOut->pDefaultHeaders, &pOut->iDefaultHeaderCount);
        xllm__provider_options_reset(&pOut->tProviderOptions);
        xllm__transport_options_reset(&pOut->tTransport);
        xllm__model_binding_reset(&pOut->tModels.tText);
        xllm__model_binding_reset(&pOut->tModels.tMultimodal);
        xllm__profile_defaults_reset(&pOut->tDefaults);
        xllm__free_cstr((char **)&pOut->sId);
        xllm__free_cstr((char **)&pOut->sName);
        xllm__free_cstr((char **)&pOut->sProvider);
        xllm__free_cstr((char **)&pOut->sAdapter);
        xllm__free_cstr((char **)&pOut->sBaseUrl);
        xllm__free_cstr((char **)&pOut->tAuth.sSecret);
        xllm__free_cstr((char **)&pOut->tAuth.sHeaderName);
        xllm__free_cstr((char **)&pOut->tAuth.sScheme);
        xllm__xvalue_release(&pOut->tVendorExtra);
        memset(pOut, 0, sizeof(*pOut));
        return XRT_NET_ERROR;
    }

    return XRT_NET_OK;
}

static void xllm__profile_release(xllm_profile *pProfile)
{
    if ( !pProfile ) {
        return;
    }

    xllm__free_cstr((char **)&pProfile->sId);
    xllm__free_cstr((char **)&pProfile->sName);
    xllm__free_cstr((char **)&pProfile->sProvider);
    xllm__free_cstr((char **)&pProfile->sAdapter);
    xllm__free_cstr((char **)&pProfile->sBaseUrl);
    xllm__free_cstr((char **)&pProfile->tAuth.sSecret);
    xllm__free_cstr((char **)&pProfile->tAuth.sHeaderName);
    xllm__free_cstr((char **)&pProfile->tAuth.sScheme);
    xllm__header_array_free(&pProfile->pDefaultHeaders, &pProfile->iDefaultHeaderCount);
    xllm__provider_options_reset(&pProfile->tProviderOptions);
    xllm__transport_options_reset(&pProfile->tTransport);
    xllm__model_binding_reset(&pProfile->tModels.tText);
    xllm__model_binding_reset(&pProfile->tModels.tMultimodal);
    xllm__profile_defaults_reset(&pProfile->tDefaults);
    xllm__xvalue_release(&pProfile->tVendorExtra);
    memset(pProfile, 0, sizeof(*pProfile));
}

static int xllm__adapter_clone(xllm_adapter *pOut, const xllm_adapter *pIn)
{
    if ( !pOut || !pIn ) {
        return XRT_NET_ERROR;
    }

    memset(pOut, 0, sizeof(*pOut));
    pOut->sName = xllm__dup_cstr(pIn->sName);
    pOut->pCtx = pIn->pCtx;
    pOut->pfnCountTokens = pIn->pfnCountTokens;
    pOut->pfnChat = pIn->pfnChat;
    pOut->tVendorExtra = pIn->tVendorExtra;
    xllm__xvalue_addref(pOut->tVendorExtra);
    if ( pIn->sName && !pOut->sName ) {
        xllm__xvalue_release(&pOut->tVendorExtra);
        return XRT_NET_ERROR;
    }
    return XRT_NET_OK;
}

static void xllm__adapter_release(xllm_adapter *pAdapter)
{
    if ( !pAdapter ) {
        return;
    }

    xllm__free_cstr((char **)&pAdapter->sName);
    xllm__xvalue_release(&pAdapter->tVendorExtra);
    memset(pAdapter, 0, sizeof(*pAdapter));
}

static int xllm__call_options_clone(xllm_call_options *pOut, const xllm_call_options *pIn)
{
    if ( !pOut || !pIn ) {
        return XRT_NET_ERROR;
    }

    memset(pOut, 0, sizeof(*pOut));
    *pOut = *pIn;
    pOut->tVendorExtra = pIn->tVendorExtra;
    xllm__xvalue_addref(pOut->tVendorExtra);
    return XRT_NET_OK;
}

static void xllm__call_options_reset(xllm_call_options *pOptions)
{
    if ( !pOptions ) {
        return;
    }

    xllm__xvalue_release(&pOptions->tVendorExtra);
    memset(pOptions, 0, sizeof(*pOptions));
}

static int xllm__runtime_options_clone(xllm_runtime_options *pOut, const xllm_runtime_options *pIn)
{
    if ( !pOut || !pIn ) {
        return XRT_NET_ERROR;
    }

    xllm_runtime_options_init(pOut);
    *pOut = *pIn;
    if ( xllm__transport_options_clone(&pOut->tTransportDefaults, &pIn->tTransportDefaults) != XRT_NET_OK ) {
        memset(pOut, 0, sizeof(*pOut));
        return XRT_NET_ERROR;
    }
    pOut->tVendorExtra = pIn->tVendorExtra;
    xllm__xvalue_addref(pOut->tVendorExtra);
    if ( !pOut->tAllocator.pfnMalloc ) {
        pOut->tAllocator.pfnMalloc = xllm__default_malloc;
    }
    if ( !pOut->tAllocator.pfnRealloc ) {
        pOut->tAllocator.pfnRealloc = xllm__default_realloc;
    }
    if ( !pOut->tAllocator.pfnFree ) {
        pOut->tAllocator.pfnFree = xllm__default_free;
    }
    return XRT_NET_OK;
}

static void xllm__runtime_options_reset(xllm_runtime_options *pOptions)
{
    if ( !pOptions ) {
        return;
    }

    xllm__transport_options_reset(&pOptions->tTransportDefaults);
    xllm__xvalue_release(&pOptions->tVendorExtra);
    memset(pOptions, 0, sizeof(*pOptions));
}

static xfuture *xllm__make_error_future(int32 iStatus, const char *sError)
{
    xfuture *pFuture;
    xpromise *pPromise;

    pFuture = xFutureCreate();
    if ( !pFuture ) {
        return NULL;
    }

    pPromise = xPromiseCreate(pFuture);
    if ( !pPromise ) {
        xFutureRelease(pFuture);
        return NULL;
    }

    (void)xPromiseReject(pPromise, iStatus, (str)(sError ? sError : "xllm async error"));
    xPromiseDestroy(pPromise);
    return pFuture;
}

static xllm_async_chat_task *xllm__async_chat_task_create(
    xllm_runtime *pRuntime,
    const xllm_request *pRequest,
    const xllm_call_options *pOptions
)
{
    xllm_async_chat_task *pTask;

    if ( !pRuntime || !pRequest ) {
        return NULL;
    }

    pTask = (xllm_async_chat_task *)xrtCalloc(1, sizeof(*pTask));
    if ( !pTask ) {
        return NULL;
    }

    pTask->pRuntime = pRuntime;
    if ( xllm__request_clone(&pTask->tRequest, pRequest) != XRT_NET_OK ) {
        xrtFree(pTask);
        return NULL;
    }

    if ( pOptions ) {
        if ( xllm__call_options_clone(&pTask->tOptions, pOptions) != XRT_NET_OK ) {
            xllm__request_release(&pTask->tRequest);
            xrtFree(pTask);
            return NULL;
        }
        pTask->bHasOptions = true;
    }

    return pTask;
}

static void xllm__async_chat_task_destroy(xllm_async_chat_task *pTask)
{
    if ( !pTask ) {
        return;
    }

    xllm__request_release(&pTask->tRequest);
    if ( pTask->bHasOptions ) {
        xllm__call_options_reset(&pTask->tOptions);
    }
    xrtFree(pTask);
}

static int32 xllm__async_chat_task_run(xllm_async_chat_task *pTask, xfuture_result *pOut)
{
    xllm_response *pResponse = NULL;
    int32 iStatus;

    if ( !pTask ) {
        return xllm__async_future_result_error(pOut, XRT_NET_ERROR, "xllm async task is null");
    }

    iStatus = xllm_chat(
        pTask->pRuntime,
        &pTask->tRequest,
        pTask->bHasOptions ? &pTask->tOptions : NULL,
        &pResponse
    );

    if ( iStatus == XRT_NET_OK ) {
        memset(pOut, 0, sizeof(*pOut));
        pOut->iStatus = XRT_NET_OK;
        pOut->pValue = pResponse;
    } else {
        if ( pResponse ) {
            xllm_response_free(pResponse);
        }
        (void)xllm__async_future_result_error(pOut, iStatus, "xllm async chat failed");
    }

    xllm__async_chat_task_destroy(pTask);
    return pOut ? pOut->iStatus : iStatus;
}

static int32 xllm__async_chat_task_thread_fn(ptr pArg, xfuture_result *pOut)
{
    return xllm__async_chat_task_run((xllm_async_chat_task *)pArg, pOut);
}

static int32 xllm__async_chat_task_engine_fn(xnetworker *pWorker, ptr pArg, xfuture_result *pOut)
{
    (void)pWorker;
    return xllm__async_chat_task_run((xllm_async_chat_task *)pArg, pOut);
}

static int32 xllm__async_chat_task_co_fn(ptr pArg, xfuture_result *pOut)
{
    return xllm__async_chat_task_run((xllm_async_chat_task *)pArg, pOut);
}

XLLM_API int xllm_runtime_create(const xllm_runtime_options *pOptions, xllm_runtime **ppRuntime)
{
    xllm_runtime *pRuntime;
    xnetengineconfig tNetEngineConfig;

    if ( !ppRuntime ) {
        return XRT_NET_ERROR;
    }

    *ppRuntime = NULL;
    pRuntime = (xllm_runtime *)xrtCalloc(1, sizeof(*pRuntime));
    if ( !pRuntime ) {
        return XRT_NET_ERROR;
    }

    if ( pOptions ) {
        if ( xllm__runtime_options_clone(&pRuntime->tOptions, pOptions) != XRT_NET_OK ) {
            xrtFree(pRuntime);
            return XRT_NET_ERROR;
        }
    } else {
        xllm_runtime_options_init(&pRuntime->tOptions);
        pRuntime->tOptions.tAllocator.pfnMalloc = xllm__default_malloc;
        pRuntime->tOptions.tAllocator.pfnRealloc = xllm__default_realloc;
        pRuntime->tOptions.tAllocator.pfnFree = xllm__default_free;
    }

    xrtNetEngineConfigInit(&tNetEngineConfig);
    tNetEngineConfig.iWorkerCount = 1u;
    pRuntime->pNetEngine = xrtNetEngineCreate(&tNetEngineConfig);
    if ( pRuntime->pNetEngine ) {
        if ( xrtNetEngineStart(pRuntime->pNetEngine) != XRT_NET_OK ) {
            xrtNetEngineDestroy(pRuntime->pNetEngine);
            pRuntime->pNetEngine = NULL;
        }
    }

    *ppRuntime = pRuntime;
    return XRT_NET_OK;
}

XLLM_API void xllm_runtime_destroy(xllm_runtime *pRuntime)
{
    size_t i;

    if ( !pRuntime ) {
        return;
    }

    for ( i = 0; i < pRuntime->iAdapterCount; ++i ) {
        xllm__adapter_release(&pRuntime->pAdapters[i]);
    }
    for ( i = 0; i < pRuntime->iProfileCount; ++i ) {
        xllm__profile_release(&pRuntime->pProfiles[i]);
    }

    if ( pRuntime->pAdapters ) {
        xrtFree(pRuntime->pAdapters);
    }
    if ( pRuntime->pProfiles ) {
        xrtFree(pRuntime->pProfiles);
    }
    if ( pRuntime->pNetEngine ) {
        xrtNetEngineDestroy(pRuntime->pNetEngine);
        pRuntime->pNetEngine = NULL;
    }
    xllm__runtime_options_reset(&pRuntime->tOptions);
    xrtFree(pRuntime);
}

XLLM_API const char *xllm_log_level_name(xllm_log_level eLevel)
{
    switch ( eLevel ) {
        case XLLM_LOG_ERROR:
            return "error";
        case XLLM_LOG_WARN:
            return "warn";
        case XLLM_LOG_INFO:
            return "info";
        case XLLM_LOG_DEBUG:
            return "debug";
        case XLLM_LOG_TRACE:
            return "trace";
        default:
            return "unknown";
    }
}

XLLM_API const char *xllm_log_event_name(xllm_log_event eEvent)
{
    switch ( eEvent ) {
        case XLLM_LOG_EVENT_RUNTIME_CREATE:
            return "runtime.create";
        case XLLM_LOG_EVENT_RUNTIME_DESTROY:
            return "runtime.destroy";
        case XLLM_LOG_EVENT_PROVIDER_REQUEST_START:
            return "provider.request_start";
        case XLLM_LOG_EVENT_PROVIDER_RESPONSE_COMPLETE:
            return "provider.response_complete";
        case XLLM_LOG_EVENT_PROVIDER_RESPONSE_FAILED:
            return "provider.response_failed";
        case XLLM_LOG_EVENT_PROVIDER_RETRY_SCHEDULED:
            return "provider.retry_scheduled";
        case XLLM_LOG_EVENT_STREAM_EVENT:
            return "stream.event";
        case XLLM_LOG_EVENT_SESSION_COMPACT_TRIGGERED:
            return "session.compact_triggered";
        case XLLM_LOG_EVENT_SESSION_COMPACT_RESULT:
            return "session.compact_result";
        case XLLM_LOG_EVENT_TOOL_LOOP_ROUND:
            return "tool_loop.round";
        case XLLM_LOG_EVENT_TOOL_LOOP_EXECUTE:
            return "tool_loop.execute";
        case XLLM_LOG_EVENT_TOOL_LOOP_STOP:
            return "tool_loop.stop";
        case XLLM_LOG_EVENT_MEMORY_INGEST:
            return "memory.ingest";
        case XLLM_LOG_EVENT_MEMORY_SEARCH:
            return "memory.search";
        case XLLM_LOG_EVENT_MEMORY_HEALTH_CHECK:
            return "memory.health_check";
        case XLLM_LOG_EVENT_WORKSPACE_SYNC:
            return "workspace.sync";
        case XLLM_LOG_EVENT_WATCHER_EVENT:
            return "watcher.event";
        case XLLM_LOG_EVENT_UNKNOWN:
        default:
            return "unknown";
    }
}

XLLM_API const char *xllm_trace_kind_name(xllm_trace_kind eKind)
{
    switch ( eKind ) {
        case XLLM_TRACE_EVENT:
            return "event";
        case XLLM_TRACE_REQUEST:
            return "request";
        case XLLM_TRACE_RESPONSE:
            return "response";
        case XLLM_TRACE_STREAM:
            return "stream";
        case XLLM_TRACE_COMPACT:
            return "compact";
        case XLLM_TRACE_TOOL_LOOP:
            return "tool_loop";
        default:
            return "unknown";
    }
}

XLLM_API int xllm_runtime_set_log_callback(
    xllm_runtime *pRuntime,
    xllm_log_callback pfnLog,
    void *pLogCtx
)
{
    if ( !pRuntime ) {
        return XRT_NET_ERROR;
    }

    pRuntime->tOptions.pfnLog = pfnLog;
    pRuntime->tOptions.pLogCtx = pLogCtx;
    return XRT_NET_OK;
}

XLLM_API int xllm_runtime_set_trace_callback(
    xllm_runtime *pRuntime,
    xllm_trace_callback pfnTrace,
    void *pTraceCtx
)
{
    if ( !pRuntime ) {
        return XRT_NET_ERROR;
    }

    pRuntime->tOptions.pfnTrace = pfnTrace;
    pRuntime->tOptions.pTraceCtx = pTraceCtx;
    return XRT_NET_OK;
}

XLLM_API int xllm_runtime_set_debug_mode(
    xllm_runtime *pRuntime,
    xllm_debug_mode eMode,
    xllm_redact_mode eRedactMode
)
{
    if ( !pRuntime ) {
        return XRT_NET_ERROR;
    }

    pRuntime->tOptions.eDebugMode = eMode;
    pRuntime->tOptions.eRedactMode = eRedactMode;
    return XRT_NET_OK;
}

XLLM_API int xllm_register_adapter(xllm_runtime *pRuntime, const xllm_adapter *pAdapter)
{
    xllm_adapter tCopy;

    if ( !pRuntime || !pAdapter || !pAdapter->sName ) {
        return XRT_NET_ERROR;
    }

    if ( xllm__runtime_find_adapter(pRuntime, pAdapter->sName) ) {
        return XRT_NET_ERROR;
    }

    if ( xllm__adapter_clone(&tCopy, pAdapter) != XRT_NET_OK ) {
        return XRT_NET_ERROR;
    }

    if ( xllm__append_buffer((void **)&pRuntime->pAdapters, sizeof(tCopy), &pRuntime->iAdapterCount, &pRuntime->iAdapterCapacity, &tCopy) != XRT_NET_OK ) {
        xllm__adapter_release(&tCopy);
        return XRT_NET_ERROR;
    }

    return XRT_NET_OK;
}

XLLM_API int xllm_register_profile(xllm_runtime *pRuntime, const xllm_profile *pProfile)
{
    xllm_profile tCopy;

    if ( !pRuntime || !pProfile || !pProfile->sId || !pProfile->sAdapter ) {
        return XRT_NET_ERROR;
    }

    if ( xllm__runtime_find_profile(pRuntime, pProfile->sId) ) {
        return XRT_NET_ERROR;
    }

    if ( xllm__profile_clone(&tCopy, pProfile) != XRT_NET_OK ) {
        return XRT_NET_ERROR;
    }

    if ( xllm__append_buffer((void **)&pRuntime->pProfiles, sizeof(tCopy), &pRuntime->iProfileCount, &pRuntime->iProfileCapacity, &tCopy) != XRT_NET_OK ) {
        xllm__profile_release(&tCopy);
        return XRT_NET_ERROR;
    }

    return XRT_NET_OK;
}

XLLM_API int xllm_chat_ex(
    xllm_runtime *pRuntime,
    const xllm_request *pRequest,
    const xllm_call_options *pOptions,
    xllm_response **ppResponse,
    xllm_error *pError
)
{
    const xllm_profile *pProfile;
    const xllm_adapter *pAdapter;
    xllm_error tLocalError;
    xllm_error *pWorkError = pError ? pError : &tLocalError;
    int32 iStatus;

    xllm_error_init(&tLocalError);
    xllm_error_reset(pWorkError);

    if ( !pRuntime || !pRequest || !ppResponse ) {
        xllm__error_set(pWorkError, XLLM_ERROR_INVALID_REQUEST, "chat arguments are invalid");
        iStatus = XRT_NET_ERROR;
        goto cleanup;
    }

    *ppResponse = NULL;
    iStatus = xllm_validate_request(pRuntime, pRequest, pOptions, pWorkError);
    if ( iStatus != XRT_NET_OK ) {
        goto cleanup;
    }

    if ( pOptions && pOptions->pCancelToken && xllm_cancel_token_is_cancelled(pOptions->pCancelToken) ) {
        xllm__error_set(pWorkError, XLLM_ERROR_CANCELLED, "chat cancelled before dispatch");
        iStatus = XRT_NET_CANCELLED;
        goto cleanup;
    }

    pProfile = xllm__runtime_find_profile(pRuntime, pRequest->sProfileId);
    pAdapter = pProfile ? xllm__runtime_find_adapter(pRuntime, pProfile->sAdapter) : NULL;
    if ( !pProfile || !pAdapter || !pAdapter->pfnChat ) {
        xllm__error_set(pWorkError, XLLM_ERROR_INTERNAL, "profile adapter is not available");
        iStatus = XRT_NET_ERROR;
        goto cleanup;
    }

    iStatus = pAdapter->pfnChat(
        pAdapter->pCtx,
        pProfile,
        pRequest,
        pOptions,
        ppResponse,
        pWorkError
    );
    if ( iStatus != XRT_NET_OK && pWorkError->eCode == XLLM_ERROR_NONE ) {
        xllm__error_set(pWorkError, XLLM_ERROR_INTERNAL, "adapter chat request failed");
    }

cleanup:
    xllm_error_free(&tLocalError);
    return iStatus;
}

XLLM_API int xllm_chat(
    xllm_runtime *pRuntime,
    const xllm_request *pRequest,
    const xllm_call_options *pOptions,
    xllm_response **ppResponse
)
{
    xllm_error tError;
    int32 iStatus;

    xllm_error_init(&tError);
    iStatus = xllm_chat_ex(pRuntime, pRequest, pOptions, ppResponse, &tError);
    xllm_error_free(&tError);
    return iStatus;
}

XLLM_API xfuture *xllm_chat_async_thread(
    xllm_runtime *pRuntime,
    const xllm_request *pRequest,
    const xllm_call_options *pOptions
)
{
    xllm_async_chat_task *pTask;

    pTask = xllm__async_chat_task_create(pRuntime, pRequest, pOptions);
    if ( !pTask ) {
        return xllm__make_error_future(XRT_NET_ERROR, "xllm async task create failed");
    }

    return xTaskRunThread(xllm__async_chat_task_thread_fn, pTask, 0);
}

XLLM_API xfuture *xllm_chat_async_engine(
    xllm_runtime *pRuntime,
    xnetengine *pEngine,
    uint32 uAffinityKey,
    const xllm_request *pRequest,
    const xllm_call_options *pOptions
)
{
    xllm_async_chat_task *pTask;

    if ( !pEngine ) {
        return xllm__make_error_future(XRT_NET_ERROR, "xllm engine is null");
    }

    pTask = xllm__async_chat_task_create(pRuntime, pRequest, pOptions);
    if ( !pTask ) {
        return xllm__make_error_future(XRT_NET_ERROR, "xllm async task create failed");
    }

    return xTaskRunEngine(pEngine, uAffinityKey, xllm__async_chat_task_engine_fn, pTask);
}

XLLM_API xfuture *xllm_chat_async_co(
    xllm_runtime *pRuntime,
    xcosched *pSched,
    const xllm_request *pRequest,
    const xllm_call_options *pOptions,
    size_t iStackSize
)
{
    xllm_async_chat_task *pTask;

    if ( !pSched ) {
        return xllm__make_error_future(XRT_NET_ERROR, "xllm coroutine scheduler is null");
    }

    pTask = xllm__async_chat_task_create(pRuntime, pRequest, pOptions);
    if ( !pTask ) {
        return xllm__make_error_future(XRT_NET_ERROR, "xllm async task create failed");
    }

#if !defined(XRT_NO_COROUTINE)
    return xTaskRunCo(pSched, xllm__async_chat_task_co_fn, pTask, iStackSize);
#else
    xllm__async_chat_task_destroy(pTask);
    return xllm__make_error_future(XRT_NET_ERROR, "xrt coroutine support is disabled");
#endif
}
