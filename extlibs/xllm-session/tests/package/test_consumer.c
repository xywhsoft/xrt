#define XLLM_SESSION_MODULE_XLLM_SESSION
#include <xllm-session.h>
#include <stdio.h>
#include <string.h>

int main(void)
{
    const char* path = "test_package_session.json";
    char text[] = "package-owned message";
    xllm_session_config config;
    xllm_session_stats stats;
    xllm_error error;
    xllm_session* session = NULL;
    xllm_session* restored = NULL;
    xllm_request owned, view;
    uint64_t turn;
    int result = 1;
    xllmRequestInit(&owned);
    xllmRequestInit(&view);
    xllmSessionConfigInit(&config);
    config.uContextWindowTokens = UINT64_MAX;
    session = xllmSessionCreate(&config, &error);
    if (!session || !xrtVersion()) goto done;
    turn = xllmSessionBeginTurn(session);
    if (!turn || !xllmSessionAddText(session, turn, XLLM_ROLE_USER, text, 0u)) goto done;
    memset(text, 'x', sizeof(text) - 1u);
    if (!xllmSessionBuildRequestView(session, &view, &error) || view.iMessageCount != 1u ||
        strcmp(view.pMessages[0].sContent, "package-owned message") != 0) goto done;
    xllmRequestUnit(&view);
    if (!xllmSessionSave(session, path, &error)) goto done;
    restored = xllmSessionLoad(path, &error);
    if (!restored || !xllmSessionGetStats(restored, &stats) ||
        stats.uContextWindowTokens != UINT64_MAX || stats.uEntryCount != 1u ||
        !xllmSessionBuildRequest(restored, &owned, &error)) goto done;
    xllmSessionDestroy(restored);
    restored = NULL;
    if (owned.iMessageCount != 1u ||
        strcmp(owned.pMessages[0].sContent, "package-owned message") != 0) goto done;
    result = 0;
done:
    xllmRequestUnit(&view);
    xllmRequestUnit(&owned);
    xllmSessionDestroy(restored);
    xllmSessionDestroy(session);
    (void)remove(path);
    return result;
}
