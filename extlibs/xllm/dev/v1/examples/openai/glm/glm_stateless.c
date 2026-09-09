/*
 * GLM 无状态对话范例
 *
 * 演示使用底层无状态 API（xllm_chat_ex）连接智谱 AI GLM 平台。
 * 单轮对话，无会话上下文。
 *
 * 注意：当前 xrt 运行时的内置 TLS 实现可能无法连接某些 HTTPS 站点。
 * 如果遇到 "request failed" 错误，可尝试将 GLM_BASE_URL 改为代理地址，
 * 或等待 xrt 更新 TLS 支持后再运行。
 *
 * 编译:
 *   gcc -std=c11 -Wall -Wextra -Isinglehead -Ilib ^
 *       -DXRT_IMPLEMENTATION -DXLLM_IMPLEMENTATION ^
 *       examples\openai\glm\glm_stateless.c ^
 *       -o build\glm_stateless.exe -lws2_32 -liphlpapi -lshell32
 */

#define GLM_API_KEY getenv("GLM_API_KEY")
#define GLM_BASE_URL   "https://open.bigmodel.cn/api/paas/v4"
#define GLM_MODEL      "glm-5-turbo"

#include "xllm-session.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>

static bool demo_on_event(const xllm_event *pEvent, void *pUserData)
{
    (void)pUserData;

    if ( !pEvent ) {
        return true;
    }

    if ( pEvent->eType == XLLM_EVENT_TEXT_DELTA ) {
        printf("%s", pEvent->as.tTextDelta.sText ? pEvent->as.tTextDelta.sText : "");
        fflush(stdout);
    }

    return true;
}

int main(void)
{
    xllm_runtime *pRuntime = NULL;
    xllm_response *pResponse = NULL;
    xllm_profile tProfile;
    xllm_request tRequest;
    xllm_error tError;
    xllm_call_options tCallOpts;
    xllm_message *pUserMsg;
    xllm_content_part *pParts;
    const char *sText;
    int iStatus;

    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);

    xrtInit();

    printf("=== GLM 无状态对话范例 ===\n\n");

    xllm_error_init(&tError);

    if ( xllm_runtime_create(NULL, &pRuntime) != XRT_NET_OK || !pRuntime ) {
        fprintf(stderr, "创建运行时失败\n");
        return 1;
    }

    if ( xllm_register_openai_compat_adapter(pRuntime) != XRT_NET_OK ) {
        fprintf(stderr, "注册 OpenAI 兼容适配器失败\n");
        xllm_runtime_destroy(pRuntime);
        return 2;
    }

    xllm_profile_init(&tProfile);
    tProfile.sId            = "glm";
    tProfile.sProvider      = "zhipu";
    tProfile.sAdapter       = XLLM_ADAPTER_OPENAI_COMPAT;
    tProfile.sBaseUrl       = GLM_BASE_URL;
    tProfile.tAuth.eKind    = XLLM_AUTH_BEARER;
    tProfile.tAuth.sSecret  = GLM_API_KEY;
    tProfile.tModels.tText.sModelId = GLM_MODEL;
    tProfile.tModels.tText.tCaps.uFlags =
        XLLM_CAP_TEXT_IN | XLLM_CAP_TEXT_OUT | XLLM_CAP_STREAM;

    if ( xllm_register_profile(pRuntime, &tProfile) != XRT_NET_OK ) {
        fprintf(stderr, "注册配置失败\n");
        xllm_runtime_destroy(pRuntime);
        return 3;
    }

    xllm_request_init(&tRequest);
    tRequest.sProfileId = (const char *)xrtCopyStr((str)"glm", 0u);
    if ( !tRequest.sProfileId ) {
        fprintf(stderr, "分配 profile id 失败\n");
        xllm_error_free(&tError);
        xllm_request_reset(&tRequest);
        xllm_runtime_destroy(pRuntime);
        return 4;
    }
    tRequest.iMessageCount = 2u;
    tRequest.pMessages = (xllm_message *)xrtCalloc(2u, sizeof(xllm_message));
    if ( !tRequest.pMessages ) {
        fprintf(stderr, "分配消息内存失败\n");
        xllm_runtime_destroy(pRuntime);
        return 4;
    }

    /* 系统提示词 */
    tRequest.pMessages[0].eRole     = XLLM_ROLE_SYSTEM;
    tRequest.pMessages[0].iPartCount = 1u;
    tRequest.pMessages[0].pParts = (xllm_content_part *)xrtCalloc(1u, sizeof(xllm_content_part));
    tRequest.pMessages[0].pParts[0].eKind = XLLM_PART_TEXT;
    tRequest.pMessages[0].pParts[0].as.tSource.eKind = XLLM_SOURCE_INLINE_TEXT;
    tRequest.pMessages[0].pParts[0].as.tSource.as.sText =
        (const char *)xrtCopyStr((str)"你是一个有帮助的助手，回答尽量简洁。", 0u);
    if ( !tRequest.pMessages[0].pParts[0].as.tSource.as.sText ) {
        fprintf(stderr, "分配系统提示词失败\n");
        xllm_error_free(&tError);
        xllm_request_reset(&tRequest);
        xllm_runtime_destroy(pRuntime);
        return 4;
    }

    /* 用户消息 */
    pUserMsg = &tRequest.pMessages[1];
    pUserMsg->eRole      = XLLM_ROLE_USER;
    pUserMsg->iPartCount = 1u;
    pParts = (xllm_content_part *)xrtCalloc(1u, sizeof(xllm_content_part));
    pParts[0].eKind = XLLM_PART_TEXT;
    pParts[0].as.tSource.eKind = XLLM_SOURCE_INLINE_TEXT;
    pParts[0].as.tSource.as.sText = (const char *)xrtCopyStr((str)"用一句话介绍 xllm 是什么。", 0u);
    if ( !pParts[0].as.tSource.as.sText ) {
        fprintf(stderr, "分配用户消息失败\n");
        xllm_error_free(&tError);
        xllm_request_reset(&tRequest);
        xllm_runtime_destroy(pRuntime);
        return 4;
    }
    pUserMsg->pParts = pParts;

    xllm_call_options_init(&tCallOpts);
    tCallOpts.eStreamMode = XLLM_STREAM_PREFER;
    tCallOpts.pfnOnEvent  = demo_on_event;
    tCallOpts.pUserData   = NULL;
    tCallOpts.uTimeoutMs  = 120000u;

    printf("[user]  用一句话介绍 xllm 是什么。\n");
    printf("[glm]   ");

    iStatus = xllm_chat_ex(pRuntime, &tRequest, &tCallOpts, &pResponse, &tError);

    if ( iStatus != XRT_NET_OK || !pResponse ) {
        fprintf(stderr, "\n对话失败 (status=%d", iStatus);
        if ( tError.eCode != XLLM_ERROR_NONE ) {
            fprintf(stderr, ", error=%d, msg=%s", (int)tError.eCode,
                    tError.sMessage ? tError.sMessage : "(空)");
            if ( tError.iHttpStatus > 0 ) {
                fprintf(stderr, ", http=%d", tError.iHttpStatus);
            }
        }
        fprintf(stderr, ")\n");
        xllm_error_free(&tError);
        xllm_request_reset(&tRequest);
        xllm_runtime_destroy(pRuntime);
        return 5;
    }

    printf("\n\n");

    sText = xllm_response_get_text(pResponse);
    if ( sText ) {
        printf("[glm]   %s\n", sText);
    } else {
        printf("[glm]   (空)\n");
    }

    if ( pResponse->tUsage.uInputTokens > 0 || pResponse->tUsage.uOutputTokens > 0 ) {
        printf("[用量] 输入=%u 输出=%u\n",
               pResponse->tUsage.uInputTokens,
               pResponse->tUsage.uOutputTokens);
    }
    if ( pResponse->sModel ) {
        printf("[模型] %s\n", pResponse->sModel);
    }

    printf("\n=== 完成 ===\n");

    xllm_response_free(pResponse);
    xllm_error_free(&tError);
    xllm_request_reset(&tRequest);
    xllm_runtime_destroy(pRuntime);
    return 0;
}
