/* 微信网站登录：离线 mock 展示 appid 授权、GET 换票、openid userinfo 和刷新。 */
#if !defined(XOAUTH2_FEATURE_XOAUTH2)
#define XRT_IMPLEMENTATION
#include "../tests/support/runtime.h"
#endif
#include <xoauth2.h>

#include <stdio.h>
#include <string.h>

static bool mock_http(const char* method, const char* url, const char* body,
	const char* auth, char** response, int* status, void* context)
{
	const char* json;
	(void)auth;
	(void)context;
	if ( strcmp(method, "GET") != 0 || body != NULL ) return false;
	if ( strstr(url, "/sns/oauth2/access_token?appid=") != NULL )
		json = "{\"access_token\":\"wx-access\",\"refresh_token\":\"wx-refresh\","
			"\"openid\":\"wx-user\",\"expires_in\":7200}";
	else if ( strstr(url, "/sns/userinfo?access_token=wx-access&openid=wx-user") != NULL )
		json = "{\"openid\":\"wx-user\",\"nickname\":\"Example\"}";
	else if ( strstr(url, "/sns/oauth2/refresh_token?appid=") != NULL )
		json = "{\"access_token\":\"wx-new\",\"openid\":\"wx-user\"}";
	else
		return false;
	*response = xrtStrDup(json);
	*status = 200;
	return *response != NULL;
}

int main(void)
{
	xoauth2client client = {0};
	xoauth2token* token = NULL;
	xoauth2token* refreshed = NULL;
	xvalue* user = NULL;
	char state[128];
	char* authorize = NULL;
	bool ok = false;

	xoauth2UseWechat(&client, "example-appid", "example-secret",
		"https://app.example/callback");
	client.Config.Http = mock_http;
	authorize = xoauth2BeginLogin(&client);
	if ( authorize == NULL || strstr(authorize, "?appid=example-appid") == NULL ||
		strstr(authorize, "#wechat_redirect") == NULL ) goto Done;
	printf("authorize: %.85s...\n", authorize);
	snprintf(state, sizeof(state), "%s", client.sState);
	token = xoauth2CompleteLogin(&client, "one-time-code", state);
	if ( token == NULL || token->OpenId == NULL ||
		strcmp(token->TokenType, "bearer") != 0 ) goto Done;
	user = xoauth2GetWechatUserInfo(&client, token);
	if ( user == NULL ) goto Done;
	refreshed = xoauth2Refresh(&client, token->RefreshToken);
	if ( refreshed == NULL || strcmp(refreshed->AccessToken, "wx-new") != 0 )
		goto Done;
	puts("WeChat login, userinfo and refresh: ok");
	ok = true;
Done:
	xrtFree(authorize);
	xrtValueRelease(user);
	xoauth2TokenFree(refreshed);
	xoauth2TokenFree(token);
	xoauth2ClientUnit(&client);
	return ok ? 0 : 1;
}
