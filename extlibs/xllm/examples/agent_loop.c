/* agent_loop — 纯 xllm 裸写 agent 的标准形（离线确定性演示）。
 *
 * 生命周期钩子在这里干三件事：
 *   pOnResponseBody  注入 canned 响应 = 一个假模型（本示例零网络运行；
 *                    接真端点时摘掉这对钩子即可，循环代码一字不改）
 *   pOnRequestBody   打印每轮出向 token 面（抓包/计费的最小形态）
 *   pOnToolCall      流中改写工具参数（演示数据干涉，此处仅日志）
 * xllm_history 承担全部消息账本：深拷贝、工具配对入账、整灌请求。
 *
 * 构建（库目录内；先编 unity 目标文件）：
 *   gcc -std=c11 -Wall -Wextra -Werror -O2 -I. -I../../single -c xllm.c -o release/xllm.o
 *   gcc -std=c11 -Wall -Wextra -Werror -I. examples/agent_loop.c release/xllm.o \
 *       release/xllm-xrt.o -lWs2_32 -lIPHLPAPI -lBcrypt -lCrypt32 \
 *       -lSecur32 -lAdvapi32 -o build/agent_loop.exe
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../xllm.h"

/* ---- 假模型：按调用序号回放 canned 响应 ---- */

static int g_iCall = 0;

static const char* g_sScript[] = {
    /* 第 1 轮：模型要读文件 */
    "{\"id\":\"c1\",\"model\":\"fake\",\"choices\":[{\"index\":0,\"message\":"
    "{\"role\":\"assistant\",\"content\":\"我先看看配置文件。\",\"tool_calls\":"
    "[{\"id\":\"call_1\",\"type\":\"function\",\"function\":{\"name\":\"read_file\","
    "\"arguments\":\"{\\\"path\\\":\\\"config.md\\\"}\"}}]},\"finish_reason\":\"tool_calls\"}],"
    "\"usage\":{\"prompt_tokens\":20,\"completion_tokens\":10,\"total_tokens\":30}}",
    /* 第 2 轮：读完了，收口作答 */
    "{\"id\":\"c2\",\"model\":\"fake\",\"choices\":[{\"index\":0,\"message\":"
    "{\"role\":\"assistant\",\"content\":\"配置读取完毕：项目代号是墨斗。\"},"
    "\"finish_reason\":\"stop\"}],"
    "\"usage\":{\"prompt_tokens\":60,\"completion_tokens\":15,\"total_tokens\":75}}",
};

static bool fake_on_raw(xllm_client* pClient, xllm_wire* pWire, void* pUserData)
{
    size_t iLen;
    (void)pClient; (void)pUserData;
    if ( g_iCall < 1 || g_iCall > (int)(sizeof(g_sScript) / sizeof(g_sScript[0])) ) { return false; }
    iLen = strlen(g_sScript[g_iCall - 1]);
    pWire->sBody = (char*)malloc(iLen + 1u);
    if ( !pWire->sBody ) { return false; }
    memcpy(pWire->sBody, g_sScript[g_iCall - 1], iLen + 1u);
    pWire->iBodySize = iLen;
    return true;
}

static bool fake_on_body(xllm_client* pClient, xllm_wire* pWire, void* pUserData)
{
    (void)pClient; (void)pUserData;
    printf("[wire] call #%u body=%zu bytes\n", (unsigned)pWire->uAttempt, pWire->iBodySize);
    return true;
}

static bool fake_on_tool(xllm_client* pClient, xllm_tool_call* pCall,
    uint32_t uIndex, void* pUserData)
{
    (void)pClient; (void)pUserData;
    printf("[tool-seam] #%u %s(%s)\n", (unsigned)uIndex, pCall->sName, pCall->sArgumentsJson);
    return true; /* 返回 false 可在此剔除该调用 */
}

/* ---- 宿主工具执行 ---- */

static char* run_tool(const xllm_tool_call* pCall)
{
    const char* sMock =
        "# mdo config\n- codename: modou (墨斗)\n- tools: 14\n";
    if ( strcmp(pCall->sName, "read_file") == 0 ) {
        char* sOut = (char*)malloc(strlen(sMock) + 1u);
        if ( sOut ) { strcpy(sOut, sMock); }
        return sOut;
    }
    return NULL;
}

int main(void)
{
    xllm_client_config tConfig;
    xllm_client* pClient;
    xllm_history* pHistory;
    xllm_hooks tHooks;
    xllm_error tError;
    int iRounds = 0;

    xllmClientConfigInit(&tConfig);
    tConfig.sBaseUrl = "http://127.0.0.1:9/v1"; /* 假模型下永不触网 */
    tConfig.sApiKey = "offline";
    tConfig.sModel = "fake";
    tConfig.eProvider = XLLM_PROVIDER_OPENAI_COMPAT;
    tConfig.uMaxAttempts = 1u;
    pClient = xllmClientCreate(&tConfig, &tError);
    if ( !pClient ) {
        printf("client failed: %s\n", tError.sMessage);
        return 1;
    }

    memset(&tHooks, 0, sizeof(tHooks));
    tHooks.pOnRequestBody = fake_on_body;
    tHooks.pOnResponseBody = fake_on_raw;
    tHooks.pOnToolCall = fake_on_tool;
    xllmClientSetHooks(pClient, &tHooks);

    pHistory = xllmHistoryCreate();
    (void)xllmHistoryAddText(pHistory, XLLM_ROLE_USER, "项目代号是什么？读一下配置。");

    for ( ;; ) {
        xllm_request tRequest;
        xllm_response* pResponse = NULL;
        size_t i;
        ++g_iCall;
        ++iRounds;
        xllmRequestInit(&tRequest);
        tRequest.bStream = false;
        if ( !xllmHistoryAppendInto(pHistory, &tRequest) ||
             !xllmRequestAddTool(&tRequest, "read_file",
                 "Read one text file", "{\"type\":\"object\",\"properties\":"
                 "{\"path\":{\"type\":\"string\"}},\"required\":[\"path\"]}", false) ) {
            printf("request build failed\n");
            xllmRequestUnit(&tRequest);
            break;
        }
        if ( xllmClientComplete(pClient, &tRequest, NULL, &pResponse, &tError) != XLLM_RESULT_OK ||
             !pResponse ) {
            printf("call failed: %s\n", tError.sMessage);
            xllmRequestUnit(&tRequest);
            break;
        }
        xllmRequestUnit(&tRequest);
        (void)xllmHistoryAddFromResponse(pHistory, pResponse);
        if ( pResponse->iToolCallCount == 0u ) {
            printf("[agent] final: %s\n", pResponse->sContent ? pResponse->sContent : "");
            xllmResponseDestroy(pResponse);
            break;
        }
        for ( i = 0u; i < pResponse->iToolCallCount; ++i ) {
            char* sOut = run_tool(&pResponse->pToolCalls[i]);
            (void)xllmHistoryAddToolResult(pHistory,
                pResponse->pToolCalls[i].sId, sOut ? sOut : "(tool failed)");
            printf("[agent] executed %s -> %zu bytes\n",
                pResponse->pToolCalls[i].sName, sOut ? strlen(sOut) : 0u);
            free(sOut);
        }
        xllmResponseDestroy(pResponse);
        if ( iRounds > 5 ) { printf("[agent] round guard\n"); break; }
    }

    printf("history entries=%zu rounds=%d\n", xllmHistoryCount(pHistory), iRounds);
    xllmHistoryDestroy(pHistory);
    xllmClientDestroy(pClient);
    return 0;
}
