/*
 * Kimi 会话对话范例（Anthropic 兼容）
 *
 * 演示使用 xllm_session 通过 Anthropic 原生协议连接 Kimi（Moonshot AI）进行多轮对话。
 * 两轮关联对话 + 一轮需要上下文感知的追问。
 *
 * 编译:
 *   gcc -std=c11 -Wall -Wextra -Isinglehead -Ilib ^
 *       -DXRT_IMPLEMENTATION -DXLLM_IMPLEMENTATION ^
 *       examples\anthropic\kimi\kimi_session.c ^
 *       -o kimi_session.exe -lws2_32 -liphlpapi -lshell32 -lcrypt32
 */

#define KIMI_API_KEY getenv("KIMI_API_KEY")
#define KIMI_BASE_URL    "https://api.moonshot.cn/anthropic/v1"
#define KIMI_MODEL       "kimi-k2.5"

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

static int demo_print_response(const xllm_response *pResponse, int iTurnNum)
{
    const char *sText;

    if ( !pResponse ) {
        fprintf(stderr, "  第%d轮: 无响应\n", iTurnNum);
        return -1;
    }

    if ( pResponse->bHasError ) {
        fprintf(stderr, "  第%d轮: 出错 (code=%d, msg=%s)\n",
                iTurnNum,
                (int)pResponse->tError.eCode,
                pResponse->tError.sMessage ? pResponse->tError.sMessage : "(空)");
        return -2;
    }

    if ( pResponse->tUsage.uInputTokens > 0 || pResponse->tUsage.uOutputTokens > 0 ) {
        printf("  [用量] 输入=%u 输出=%u\n",
               pResponse->tUsage.uInputTokens,
               pResponse->tUsage.uOutputTokens);
    }

    sText = xllm_response_get_text(pResponse);
    if ( sText ) {
        printf("  [回复]  %s\n", sText);
    } else {
        printf("  [回复]  (空)\n");
    }

    return 0;
}

static void demo_print_error(const char *sPrefix, int iStatus, const xllm_error *pError)
{
    fprintf(stderr, "%s (status=%d", sPrefix, iStatus);
    if ( pError ) {
        fprintf(stderr,
                ", code=%d, http=%d, msg=%s",
                (int)pError->eCode,
                (int)pError->iHttpStatus,
                pError->sMessage ? pError->sMessage : "(空)");
        if ( pError->sRequestId && pError->sRequestId[0] ) {
            fprintf(stderr, ", request_id=%s", pError->sRequestId);
        }
    }
    fprintf(stderr, ")\n");
}

int main(void)
{
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);

    xrtInit();

    xllm_runtime *pRuntime = NULL;
    xllm_session *pSession = NULL;
    xllm_response *pResponse = NULL;
    xllm_profile tProfile;
    xllm_session_options tSessionOpts;
    xllm_turn tTurn;
    xllm_call_options tCallOpts;
    xllm_error tError;
    int iStatus;

    printf("=== Kimi 会话对话范例 (Anthropic) ===\n\n");

    if ( xllm_runtime_create(NULL, &pRuntime) != XRT_NET_OK || !pRuntime ) {
        fprintf(stderr, "创建运行时失败\n");
        return 1;
    }

    if ( xllm_register_anthropic_native_adapter(pRuntime) != XRT_NET_OK ) {
        fprintf(stderr, "注册 Anthropic 原生适配器失败\n");
        xllm_runtime_destroy(pRuntime);
        return 2;
    }

    xllm_profile_init(&tProfile);
    tProfile.sId            = "kimi";
    tProfile.sProvider      = "moonshot";
    tProfile.sAdapter       = XLLM_ADAPTER_ANTHROPIC_NATIVE;
    tProfile.sBaseUrl       = KIMI_BASE_URL;
    tProfile.tAuth.eKind    = XLLM_AUTH_BEARER;
    tProfile.tAuth.sSecret  = KIMI_API_KEY;
    tProfile.tModels.tText.sModelId = KIMI_MODEL;
    tProfile.tModels.tText.tCaps.uFlags =
        XLLM_CAP_TEXT_IN | XLLM_CAP_TEXT_OUT | XLLM_CAP_STREAM;

    if ( xllm_register_profile(pRuntime, &tProfile) != XRT_NET_OK ) {
        fprintf(stderr, "注册配置失败\n");
        xllm_runtime_destroy(pRuntime);
        return 3;
    }

    xllm_session_options_init(&tSessionOpts);
    tSessionOpts.sProfileId     = "kimi";
    tSessionOpts.sSystemPrompt  =
        "你是一个有用的助手，使用中文与用户对话。"
        "回答简洁自然，注意保持对话上下文的连贯性。";
    tSessionOpts.bEnableAutoCompact = true;
    tSessionOpts.uCompactTriggerTurns = 20u;

    if ( xllm_session_create(pRuntime, &tSessionOpts, &pSession) != XRT_NET_OK || !pSession ) {
        fprintf(stderr, "创建会话失败\n");
        xllm_runtime_destroy(pRuntime);
        return 4;
    }

    xllm_call_options_init(&tCallOpts);
    tCallOpts.eStreamMode = XLLM_STREAM_PREFER;
    tCallOpts.pfnOnEvent  = demo_on_event;
    tCallOpts.uTimeoutMs  = 120000u;
    xllm_error_init(&tError);

    printf("--- 第一轮 ---\n");
    xllm_turn_init(&tTurn);
    xllm_turn_add_user_text(&tTurn, "我有一只金毛寻回犬，叫 Luna，今年3岁了。");
    printf("[user]  我有一只金毛寻回犬，叫 Luna，今年3岁了。\n");

    iStatus = xllm_session_chat_ex(pSession, &tTurn, &tCallOpts, &pResponse, &tError);
    if ( iStatus != XRT_NET_OK || !pResponse ) {
        demo_print_error("第一轮对话失败", iStatus, &tError);
        xllm_turn_reset(&tTurn);
        xllm_error_free(&tError);
        xllm_session_destroy(pSession);
        xllm_runtime_destroy(pRuntime);
        return 5;
    }
    demo_print_response(pResponse, 1);
    printf("\n");

    xllm_response_free(pResponse);
    pResponse = NULL;
    xllm_turn_reset(&tTurn);

    printf("--- 第二轮 ---\n");
    xllm_turn_init(&tTurn);
    xllm_turn_add_user_text(&tTurn, "有小孩的家庭适合养什么类型的狗？");
    printf("[user]  有小孩的家庭适合养什么类型的狗？\n");

    iStatus = xllm_session_chat_ex(pSession, &tTurn, &tCallOpts, &pResponse, &tError);
    if ( iStatus != XRT_NET_OK || !pResponse ) {
        demo_print_error("第二轮对话失败", iStatus, &tError);
        xllm_turn_reset(&tTurn);
        xllm_error_free(&tError);
        xllm_session_destroy(pSession);
        xllm_runtime_destroy(pRuntime);
        return 6;
    }
    demo_print_response(pResponse, 2);
    printf("\n");

    xllm_response_free(pResponse);
    pResponse = NULL;
    xllm_turn_reset(&tTurn);

    printf("--- 第三轮（追问） ---\n");
    xllm_turn_init(&tTurn);
    xllm_turn_add_user_text(&tTurn, "结合你的回答，我的 Luna 适合吗？她很喜欢小孩。");
    printf("[user]  结合你的回答，我的 Luna 适合吗？她很喜欢小孩。\n");

    iStatus = xllm_session_chat_ex(pSession, &tTurn, &tCallOpts, &pResponse, &tError);
    if ( iStatus != XRT_NET_OK || !pResponse ) {
        demo_print_error("第三轮对话失败", iStatus, &tError);
        xllm_turn_reset(&tTurn);
        xllm_error_free(&tError);
        xllm_session_destroy(pSession);
        xllm_runtime_destroy(pRuntime);
        return 7;
    }
    demo_print_response(pResponse, 3);
    printf("\n");

    xllm_response_free(pResponse);
    pResponse = NULL;
    xllm_turn_reset(&tTurn);

    printf("=== 完成 ===\n");

    xllm_session_destroy(pSession);
    xllm_error_free(&tError);
    xllm_runtime_destroy(pRuntime);
    return 0;
}
