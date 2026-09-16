/* xoauth2 测试：单 TU（XRT_IMPLEMENTATION 只定义一次）。
 * 覆盖：PKCE/state/URL 编码/预设/授权 URL/CSRF/请求构造（三种
 * AuthStyle）/响应解析/token 时间戳/错误码/泄漏实测/fuzz。 */
#define XRT_MODULE_ALL
#define XRT_IMPLEMENTATION
#include "../xoauth2.c"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ---- 计数分配器：泄漏回归实测（main 首行安装，须在首个 xrt 分配前） ---- */
long g_probeLiveBytes = 0;
static long s_probeLive = 0;
static ptr probe_alloc(ptr ctx, size_t size)
{
	(void)ctx;
	unsigned char* p = (unsigned char*)malloc(size + 16);
	if ( p == NULL ) return NULL;
	*(size_t*)p = size;
	s_probeLive++; g_probeLiveBytes += (long)size;
	return (ptr)(p + 16);
}
static void probe_free(ptr ctx, ptr mem)
{
	(void)ctx;
	if ( mem ) {
		unsigned char* p = (unsigned char*)mem - 16;
		s_probeLive--; g_probeLiveBytes -= (long)(*(size_t*)p);
		free(p);
	}
}
static ptr probe_realloc(ptr ctx, ptr mem, size_t size)
{
	(void)ctx;
	if ( mem == NULL ) return probe_alloc(ctx, size);
	unsigned char* q = (unsigned char*)realloc((unsigned char*)mem - 16, size + 16);
	if ( q == NULL ) return NULL;
	g_probeLiveBytes -= (long)(*(size_t*)q);
	*(size_t*)q = size;
	g_probeLiveBytes += (long)size;
	return (ptr)(q + 16);
}

static int s_pass = 0, s_fail = 0;

#define CHECK(expr, msg) do { \
	if ( !(expr) ) { printf("FAIL: %s\n", msg); s_fail++; } \
	else { s_pass++; } \
} while (0)


/* ---- Phase 2 测试基础设施：mock 传输回调 + 回环服务器（文件级） ---- */
static char g_MockUrl[512];
static char g_MockMethod[8];
static char g_MockBody[1024];
static char g_MockAuth[1024];
static const char* g_MockReply = "";
static int g_MockStatus = 200;
static bool g_MockFail = false;

static void mock_setup(int iStatus, const char* sReply)
{
	g_MockStatus = iStatus;
	g_MockReply = sReply;
	g_MockMethod[0] = 0;
	g_MockUrl[0] = 0;
	g_MockBody[0] = 0;
	g_MockAuth[0] = 0;
	g_MockFail = false;
}

static bool mock_http(const char* sMethod, const char* sUrl, const char* sBody,
                      const char* sAuthHeader, char** psBody, int* piStatus,
                      void* pContext)
{
	(void)pContext;
	snprintf(g_MockMethod, sizeof(g_MockMethod), "%s", sMethod ? sMethod : "");
	snprintf(g_MockUrl, sizeof(g_MockUrl), "%s", sUrl ? sUrl : "");
	snprintf(g_MockBody, sizeof(g_MockBody), "%s", sBody ? sBody : "");
	snprintf(g_MockAuth, sizeof(g_MockAuth), "%s", sAuthHeader ? sAuthHeader : "");
	if ( g_MockFail ) return false;
	*psBody = (char*)xrtMalloc(strlen(g_MockReply) + 1);
	if ( *psBody == NULL ) return false;
	strcpy(*psBody, g_MockReply);
	*piStatus = g_MockStatus;
	return true;
}

/* ---- P4 审计回归基础设施 ---- */
/* 三态循环回调：成功 / 404 带 token（探测泄漏路径）/ 传输失败 */
static int s_LeakCycle = 0;
static bool leakcycle_http(const char* sMethod, const char* sUrl,
                           const char* sBody, const char* sAuthHeader,
                           char** psBody, int* piStatus, void* pContext)
{
	(void)sMethod; (void)sUrl; (void)sBody; (void)sAuthHeader; (void)pContext;
	switch ( s_LeakCycle++ % 3 ) {
	case 0:
		*piStatus = 200;
		*psBody = (char*)xrtMalloc(40);
		strcpy(*psBody, "{\"access_token\":\"ok\",\"expires_in\":60}");
		return true;
	case 1:   /* 非 2xx 但含完整令牌结构：exchange 探测后必须释放 */
		*piStatus = 404;
		*psBody = (char*)xrtMalloc(64);
		strcpy(*psBody,
			"{\"access_token\":\"ghost\",\"token_type\":\"bearer\"}");
		return true;
	default:
		return false;
	}
}

/* fuzz 用：回显畸形响应 */
static bool fuzz_http(const char* sMethod, const char* sUrl,
                      const char* sBody, const char* sAuthHeader,
                      char** psBody, int* piStatus, void* pContext)
{
	(void)sUrl; (void)sBody; (void)sAuthHeader; (void)pContext;
	*piStatus = (sMethod[0] == 'G') ? 200 : 400;
	*psBody = (char*)xrtMalloc(2);
	strcpy(*psBody, "{}");
	return true;
}

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
static const char* g_ServerReply = "";
static char g_ServerSaw[4096];
static char g_ServerDone[4096];
static volatile LONG g_ServerStop = 0;
static SOCKET g_ListenSock = INVALID_SOCKET;

/* 单连接循环：accept → 读到 Content-Length 满足 → 回 canned 响应 */
static DWORD WINAPI loop_server_proc(LPVOID param)
{
	(void)param;
	while ( !g_ServerStop ) {
		SOCKET cli = accept(g_ListenSock, NULL, NULL);
		if ( cli == INVALID_SOCKET ) break;
		int total = 0;
		size_t want = (size_t)-1;
		memset(g_ServerSaw, 0, sizeof(g_ServerSaw));
		while ( total < (int)sizeof(g_ServerSaw) - 1 ) {
			int recvd = recv(cli, g_ServerSaw + total,
				(int)sizeof(g_ServerSaw) - 1 - total, 0);
			if ( recvd <= 0 ) break;
			total += recvd;
			g_ServerSaw[total] = 0;
			const char* pcl = strstr(g_ServerSaw, "Content-Length:");
			const char* pend = strstr(g_ServerSaw, "\r\n\r\n");
			if ( pend != NULL ) {
				if ( want == (size_t)-1 )
					want = pcl ? (size_t)atol(pcl + 15) : 0;
				if ( (size_t)(total - (pend - g_ServerSaw) - 4) >= want )
					break;
			}
		}
		memcpy(g_ServerDone, g_ServerSaw, sizeof(g_ServerSaw));
		send(cli, g_ServerReply, (int)strlen(g_ServerReply), 0);
		closesocket(cli);
	}
	return 0;
}
#endif

static const char* T_JSON_OK =
	"{\"access_token\":\"gho_abc123\",\"refresh_token\":\"rt_1\","
	"\"id_token\":\"eyJ...\",\"token_type\":\"Bearer\","
	"\"expires_in\":3600,\"scope\":\"read:user\"}";

int main(void)
{
	xallocator tProbeAlloc = { NULL, probe_alloc, probe_realloc, probe_free };
	xrtSetAllocator(&tProbeAlloc);

	/* ============ PKCE (RFC 7636) ============ */
	{
		char v[128], c[128];
		CHECK(xoauth2PkceGenerate(v, sizeof(v), c, sizeof(c)),
			"PKCE generate succeeds");
		CHECK(strlen(v) == 43, "verifier is 43 chars");
		CHECK(strlen(c) == 43, "challenge is 43 chars (S256)");
		char v2[128], c2[128];
		CHECK(xoauth2PkceGenerate(v2, sizeof(v2), c2, sizeof(c2)) &&
			strcmp(v, v2) != 0, "verifier differs each call");
		CHECK(!xoauth2PkceGenerate(v, 32, c, sizeof(c)),
			"small verifier buffer rejected");
		/* challenge 必须可复算：BASE64URL(SHA256(verifier)) */
		unsigned char dig[32];
		xrtSha256(v, strlen(v), dig);
		char* expect = xoauth2__base64url(dig, 32);
		CHECK(expect != NULL && strcmp(expect, c) == 0,
			"challenge = BASE64URL(SHA256(verifier))");
		xrtFree(expect);
	}

	/* ============ state ============ */
	{
		char s[64];
		CHECK(xoauth2StateGenerate(s, sizeof(s)), "state generate succeeds");
		CHECK(strlen(s) >= 30, "state length >= 30");
		char s2[64];
		xoauth2StateGenerate(s2, sizeof(s2));
		CHECK(strcmp(s, s2) != 0, "state differs each call");
		CHECK(!xoauth2StateGenerate(s, 16), "small state buffer rejected");
	}

	/* ============ URL 编码 ============ */
	{
		char* e = xoauth2UrlEncode("a b&c=d%e/f?g#h");
		CHECK(e != NULL && strcmp(e, "a%20b%26c%3Dd%25e%2Ff%3Fg%23h") == 0,
			"urlencode full coverage");
		xrtFree(e);
		e = xoauth2UrlEncode("AZaz09-._~");
		CHECK(e != NULL && strcmp(e, "AZaz09-._~") == 0, "url-safe chars untouched");
		xrtFree(e);
		e = xoauth2UrlEncode("");
		CHECK(e != NULL && e[0] == 0, "empty encodes to empty");
		xrtFree(e);
		CHECK(xoauth2UrlEncode(NULL) == NULL, "NULL rejected");
	}

	/* ============ 预设 ============ */
	{
		xoauth2client c;
		xoauth2UseGithub(&c, "id1", "sec1", "https://app/cb");
		CHECK(strcmp(c.Config.TokenUrl,
			"https://github.com/login/oauth/access_token") == 0, "github preset");
		CHECK(c.Config.AuthStyle == XOAUTH2_AUTH_BODY, "github uses body auth");
		CHECK(c.Config.UsePkce, "github pkce on");
		xoauth2ClientUnit(&c);
		CHECK(c.Config.ClientId == NULL, "ClientUnit clears config");

		xoauth2UseGoogle(&c, "id1", "sec1", "https://app/cb");
		CHECK(!c.Config.UsePkce, "google pkce off");
		CHECK(c.Config.AuthStyle == XOAUTH2_AUTH_BASIC, "google uses basic auth");
		xoauth2ClientUnit(&c);

		xoauth2UseWechat(&c, "wx1", "sec1", "https://app/cb");
		CHECK(!c.Config.UsePkce, "wechat pkce off");
		xoauth2ClientUnit(&c);

		/* Microsoft：tenant 进 URL、owned 指针非空、Unit 后释放 */
		xoauth2UseMicrosoft(&c, "id1", "sec1", "https://app/cb", "contoso.onmicrosoft.com");
		CHECK(c.Config.AuthorizeUrl != NULL &&
			strstr(c.Config.AuthorizeUrl, "contoso.onmicrosoft.com") != NULL,
			"microsoft tenant in authorize url");
		CHECK(c.pOwnedAuthUrl != NULL && c.pOwnedAuthUrl == c.Config.AuthorizeUrl,
			"microsoft owns auth url");
		xoauth2ClientUnit(&c);
		CHECK(c.pOwnedAuthUrl == NULL && c.Config.AuthorizeUrl == NULL,
			"ClientUnit frees owned urls");

		/* 超长 tenant → 端点 NULL + 错误 */
		char aLong[300];
		memset(aLong, 't', sizeof(aLong) - 1);
		aLong[sizeof(aLong) - 1] = 0;
		xoauth2UseMicrosoft(&c, "id1", "sec1", "https://app/cb", aLong);
		CHECK(c.Config.AuthorizeUrl == NULL, "oversized tenant rejected");
		CHECK(xoauth2LastError() == XOAUTH2_ERROR_ARGUMENT,
			"oversized tenant error code");
		xoauth2ClientUnit(&c);
	}

	/* ============ 授权 URL 构造 ============ */
	{
		xoauth2client c;
		xoauth2UseGithub(&c, "cid-1", "topsecret", "https://app/callback");
		char* url = xoauth2BeginLogin(&c);
		CHECK(url != NULL, "BeginLogin returns url");
		if ( url != NULL ) {
			CHECK(strstr(url, "response_type=code") != NULL, "response_type param");
			CHECK(strstr(url, "client_id=cid-1") != NULL, "client_id param");
			CHECK(strstr(url, "redirect_uri=https%3A%2F%2Fapp%2Fcallback") != NULL,
				"redirect_uri encoded");
			CHECK(strstr(url, "state=") != NULL, "state param");
			CHECK(strstr(url, "code_challenge=") != NULL &&
				strstr(url, "code_challenge_method=S256") != NULL,
				"github url has pkce");
			CHECK(strstr(url, "&scope=read%3Auser%20user%3Aemail") != NULL,
				"scope encoded");
			xrtFree(url);
		}
		/* Google：无 PKCE 参数 */
		xoauth2UseGoogle(&c, "g-1", "sec", "https://app/cb");
		url = xoauth2BeginLogin(&c);
		if ( url != NULL ) {
			CHECK(strstr(url, "code_challenge") == NULL,
				"H6 google preset: no pkce params");
			xrtFree(url);
		}
		/* WeChat：无 PKCE 参数 */
		xoauth2UseWechat(&c, "wx-1", "sec", "https://app/cb");
		url = xoauth2BeginLogin(&c);
		if ( url != NULL ) {
			CHECK(strstr(url, "code_challenge") == NULL,
				"H6 wechat preset: no pkce params");
			xrtFree(url);
		}
		/* custom 开 PKCE → 有参数；关 → 无 */
		xoauth2config cfg;
		xoauth2ConfigInit(&cfg);
		cfg.AuthorizeUrl = "https://idp.example/authorize";
		cfg.ClientId = "cu-1";
		cfg.RedirectUri = "https://app/cb";
		xoauth2UseCustom(&c, &cfg);
		url = xoauth2BeginLogin(&c);
		if ( url != NULL ) {
			CHECK(strstr(url, "code_challenge=") != NULL,
				"custom pkce on works");
			xrtFree(url);
		}
		cfg.UsePkce = false;
		xoauth2UseCustom(&c, &cfg);
		url = xoauth2BeginLogin(&c);
		if ( url != NULL ) {
			CHECK(strstr(url, "code_challenge") == NULL,
				"custom pkce off respected");
			xrtFree(url);
		}
		/* 未配置拒绝 */
		xoauth2client empty;
		memset(&empty, 0, sizeof(empty));
		CHECK(xoauth2BeginLogin(&empty) == NULL, "unconfigured rejected");
	}

	/* ============ 请求构造（H4/H5） ============ */
	{
		xoauth2client c;
		/* BODY 风格 + 特殊字符 secret：必须全部编码 */
		xoauth2config cfg;
		xoauth2ConfigInit(&cfg);
		cfg.AuthorizeUrl = "https://idp/a";
		cfg.TokenUrl = "https://idp/t";
		cfg.ClientId = "cid&1";
		cfg.ClientSecret = "sec&ret=x%y";
		cfg.RedirectUri = "https://app/cb";
		xoauth2UseCustom(&c, &cfg);
		c.sVerifier[0] = 0;   /* 跳过 verifier */
		char* body = xoauth2__build_token_request(&c, "code/1=2");
		CHECK(body != NULL, "body auth request built");
		if ( body != NULL ) {
			CHECK(strstr(body, "grant_type=authorization_code") != NULL,
				"grant_type present");
			CHECK(strstr(body, "code=code%2F1%3D2") != NULL, "H5 code encoded");
			CHECK(strstr(body, "client_id=cid%261") != NULL, "H5 client_id encoded");
			CHECK(strstr(body, "client_secret=sec%26ret%3Dx%25y") != NULL,
				"H5 client_secret encoded");
			xrtFree(body);
		}
		/* BASIC 风格：body 无凭据 + Authorization 头正确 */
		cfg.AuthStyle = XOAUTH2_AUTH_BASIC;
		xoauth2UseCustom(&c, &cfg);
		body = xoauth2__build_token_request(&c, "abc");
		if ( body != NULL ) {
			CHECK(strstr(body, "client_id") == NULL &&
				strstr(body, "client_secret") == NULL,
				"H4 basic style: no creds in body");
			xrtFree(body);
		}
		{
			xoauth2config b;
			xoauth2ConfigInit(&b);
			b.ClientId = "abc";
			b.ClientSecret = "def";
			b.AuthStyle = XOAUTH2_AUTH_BASIC;
			xoauth2UseCustom(&c, &b);
			char* h = xoauth2__build_auth_header(&c);
			CHECK(h != NULL && strcmp(h, "Basic YWJjOmRlZg==") == 0,
				"H4 basic auth header value");
			xrtFree(h);
			/* 非 BASIC 风格 → NULL */
			b.AuthStyle = XOAUTH2_AUTH_BODY;
			xoauth2UseCustom(&c, &b);
			CHECK(xoauth2__build_auth_header(&c) == NULL,
				"body style has no auth header");
		}
		/* refresh 请求体（M10） */
		xoauth2config r;
		xoauth2ConfigInit(&r);
		r.ClientId = "rid";
		r.ClientSecret = "rsec";
		r.TokenUrl = "https://idp/t";
		xoauth2UseCustom(&c, &r);
		char* rb = xoauth2__build_refresh_request(&c, "refresh/tok&e");
		CHECK(rb != NULL, "refresh request built");
		if ( rb != NULL ) {
			CHECK(strstr(rb, "grant_type=refresh_token") != NULL,
				"refresh grant_type");
			CHECK(strstr(rb, "refresh_token=refresh%2Ftok%26e") != NULL,
				"refresh token encoded");
			CHECK(strstr(rb, "client_id=rid") != NULL, "refresh client_id");
			xrtFree(rb);
		}
	}

	/* ============ 响应解析（M8/M9/M14） ============ */
	{
		xoauth2token* t = xoauth2__parse_token_response(T_JSON_OK, strlen(T_JSON_OK));
		CHECK(t != NULL, "full response parses");
		if ( t != NULL ) {
			CHECK(strcmp(t->AccessToken, "gho_abc123") == 0, "access_token");
			CHECK(strcmp(t->RefreshToken, "rt_1") == 0, "refresh_token");
			CHECK(strcmp(t->IdToken, "eyJ...") == 0, "id_token");
			CHECK(strcmp(t->TokenType, "bearer") == 0,
				"M14 token_type lowercased");
			CHECK(t->ExpiresIn == 3600, "expires_in");
			CHECK(strcmp(t->Scope, "read:user") == 0, "scope");
			CHECK(t->ObtainedAt > 1700000000, "M9 ObtainedAt stamped");
			CHECK(t->ExpiresAt == t->ObtainedAt + 3600, "M9 ExpiresAt computed");
			CHECK(!xoauth2TokenExpiring(t, 60), "M8 fresh token not expiring");
			CHECK(xoauth2TokenExpiring(t, 7200), "M8 expiring under big leeway");
			xoauth2TokenFree(t);
		}
		/* 短有效期 → expiring */
		{
			const char* j = "{\"access_token\":\"x\",\"expires_in\":30}";
			xoauth2token* t2 = xoauth2__parse_token_response(j, strlen(j));
			if ( t2 != NULL ) {
				CHECK(xoauth2TokenExpiring(t2, 60), "M8 short token expiring");
				xoauth2TokenFree(t2);
			}
		}
		/* 负有效期 → 立即过期 */
		{
			const char* j = "{\"access_token\":\"x\",\"expires_in\":-5}";
			xoauth2token* t3 = xoauth2__parse_token_response(j, strlen(j));
			if ( t3 != NULL ) {
				CHECK(xoauth2TokenExpiring(t3, 0), "negative expires_in expiring");
				xoauth2TokenFree(t3);
			}
		}
		/* error 响应拒绝 */
		CHECK(xoauth2__parse_token_response(
			"{\"error\":\"invalid_grant\"}", 26) == NULL, "error response rejected");
		/* 无 access_token 拒绝 */
		CHECK(xoauth2__parse_token_response(
			"{\"token_type\":\"bearer\"}", 24) == NULL, "no access_token rejected");
		/* 畸形 JSON 拒绝 */
		CHECK(xoauth2__parse_token_response("not json", 8) == NULL,
			"malformed json rejected");
		CHECK(xoauth2__parse_token_response(NULL, 0) == NULL, "null rejected");
	}

	/* ============ 登录回调（H2/H7/M12） ============ */
	{
		xoauth2client c;
		xoauth2config cfg;
		xoauth2ConfigInit(&cfg);
		cfg.AuthorizeUrl = "https://idp/a";
		cfg.TokenUrl = "https://idp/t";
		cfg.ClientId = "cid";
		cfg.RedirectUri = "https://app/cb";
		xoauth2UseCustom(&c, &cfg);
		char* url = xoauth2BeginLogin(&c);
		CHECK(url != NULL, "begin for callback test");
		if ( url != NULL ) xrtFree(url);

		/* 错误 state → 拒绝 + 错误码 + 会话不焚毁 */
		xrtClearError();
		xoauth2token* t = xoauth2CompleteLogin(&c, "code1", "wrong-state");
		CHECK(t == NULL, "wrong state rejected");
		CHECK(xoauth2LastError() == XOAUTH2_ERROR_STATE_MISMATCH,
			"M12 state mismatch error code");
		CHECK(c.sState[0] != 0, "H7 session kept on state mismatch");
		char aState[128];
		strcpy(aState, c.sState);

		/* 正确 state → 桩网络错误，但会话已焚毁（一次性） */
		xrtClearError();
		t = xoauth2CompleteLogin(&c, "code1", aState);
		CHECK(t == NULL, "complete login no-transport fails");
		CHECK(xoauth2LastError() == XOAUTH2_ERROR_NETWORK,
			"no-transport error code");
		CHECK(c.sState[0] == 0, "H7 state burned after use");
		CHECK(c.sVerifier[0] == 0, "H7 verifier burned after use");

		/* 重放同一 state → 拒绝 */
		xrtClearError();
		t = xoauth2CompleteLogin(&c, "code1", aState);
		CHECK(t == NULL &&
			xoauth2LastError() == XOAUTH2_ERROR_STATE_MISMATCH,
			"H7 replayed state rejected");

		/* Refresh 桩错误码 */
		xrtClearError();
		CHECK(xoauth2Refresh(&c, "rt1") == NULL, "refresh no-transport fails");
		CHECK(xoauth2LastError() == XOAUTH2_ERROR_NETWORK,
			"refresh no-transport error code");

		/* 空 state（未 BeginLogin）→ 拒绝 */
		xoauth2client c2;
		xoauth2UseCustom(&c2, &cfg);
		CHECK(xoauth2CompleteLogin(&c2, "code", "") == NULL,
			"empty state rejected");
		/* ClientUnit 后未预设 → CompleteLogin 拒绝 */
		xoauth2ClientUnit(&c2);
		CHECK(xoauth2CompleteLogin(&c2, "code", "x") == NULL,
			"unconfigured complete rejected");
		CHECK(xoauth2LastError() == XOAUTH2_ERROR_ARGUMENT,
			"unconfigured error code");
		/* LastError：非本域错误 → 0 */
		xrtClearError();
		CHECK(xoauth2LastError() == 0, "no xoauth2 error -> 0");
	}

	/* ============ 泄漏实测（H3 + 全路径，稳态法） ============ */
	{
		/* xrt 分配器池化高水位滞留：预热到稳态后测净增长 */
		for ( int i = 0; i < 300; i++ ) {
			xoauth2client c;
			xoauth2UseMicrosoft(&c, "id", "sec", "https://app/cb", "tenant-x");
			xoauth2ClientUnit(&c);
		}
		long base = g_probeLiveBytes;
		for ( int i = 0; i < 300; i++ ) {
			xoauth2client c;
			xoauth2UseMicrosoft(&c, "id", "sec", "https://app/cb", "tenant-x");
			xoauth2ClientUnit(&c);
		}
		CHECK(g_probeLiveBytes == base, "H3 microsoft preset 300x zero leak");

		/* BeginLogin / 请求构造 / ClientUnit 循环 */
		xoauth2config cfg;
		xoauth2ConfigInit(&cfg);
		cfg.AuthorizeUrl = "https://idp/a";
		cfg.TokenUrl = "https://idp/t";
		cfg.ClientId = "cid";
		cfg.ClientSecret = "sec";
		cfg.RedirectUri = "https://app/cb";
		for ( int i = 0; i < 300; i++ ) {
			xoauth2client c;
			xoauth2UseCustom(&c, &cfg);
			char* u = xoauth2BeginLogin(&c);
			if ( u ) xrtFree(u);
			char* b = xoauth2__build_token_request(&c, "code");
			if ( b ) xrtFree(b);
			char* h = xoauth2__build_auth_header(&c);
			if ( h ) xrtFree(h);
			xoauth2ClientUnit(&c);
		}
		base = g_probeLiveBytes;
		for ( int i = 0; i < 100; i++ ) {
			xoauth2client c;
			xoauth2UseCustom(&c, &cfg);
			char* u = xoauth2BeginLogin(&c);
			if ( u ) xrtFree(u);
			char* b = xoauth2__build_token_request(&c, "code");
			if ( b ) xrtFree(b);
			xoauth2ClientUnit(&c);
		}
		CHECK(g_probeLiveBytes == base, "begin/build/unit cycle zero leak");

		/* 解析循环（含失败路径） */
		for ( int i = 0; i < 300; i++ ) {
			xoauth2token* t = xoauth2__parse_token_response(
				T_JSON_OK, strlen(T_JSON_OK));
			if ( t ) xoauth2TokenFree(t);
		}
		base = g_probeLiveBytes;
		for ( int i = 0; i < 100; i++ ) {
			xoauth2token* t = xoauth2__parse_token_response(
				T_JSON_OK, strlen(T_JSON_OK));
			if ( t ) xoauth2TokenFree(t);
			xoauth2__parse_token_response("{\"error\":\"x\"}", 14);
			xoauth2__parse_token_response("bad", 3);
		}
		CHECK(g_probeLiveBytes == base, "parse cycle (ok+fail) zero leak");
	}

	/* ============ Phase 4 审计修复回归 ============ */
	/* ---- P4-M1/L2：exchange 全路径稳态零泄漏（含非 2xx 带令牌探测） ---- */
	{
		xoauth2client c;
		xoauth2config cfg;
		xoauth2ConfigInit(&cfg);
		cfg.AuthorizeUrl = "https://idp/a";
		cfg.TokenUrl = "https://idp/t";
		cfg.ClientId = "cid";
		cfg.RedirectUri = "https://app/cb";
		cfg.Http = leakcycle_http;
		xoauth2UseCustom(&c, &cfg);
		for ( int i = 0; i < 300; i++ ) {   /* 预热到稳态 */
			char* u = xoauth2BeginLogin(&c);
			if ( u ) xrtFree(u);
			if ( c.sState[0] ) {
				xoauth2token* t = xoauth2CompleteLogin(&c, "code", c.sState);
				if ( t ) xoauth2TokenFree(t);
			}
			xoauth2ClientUnit(&c);
			xoauth2UseCustom(&c, &cfg);
		}
		long base = g_probeLiveBytes;
		for ( int i = 0; i < 100; i++ ) {
			char* u = xoauth2BeginLogin(&c);
			if ( u ) xrtFree(u);
			if ( c.sState[0] ) {
				xoauth2token* t = xoauth2CompleteLogin(&c, "code", c.sState);
				if ( t ) xoauth2TokenFree(t);
			}
			xoauth2ClientUnit(&c);
			xoauth2UseCustom(&c, &cfg);
		}
		CHECK(g_probeLiveBytes == base,
			"P4-M1 exchange cycle (ok/404-with-token/fail) zero leak");
	}

	/* ---- P4-L5：expires_in 非整数显式拒绝 ---- */
	{
		xrtClearError();
		CHECK(xoauth2__parse_token_response(
			"{\"access_token\":\"x\",\"expires_in\":\"3600\"}", 42) == NULL,
			"P4-L5 string expires_in rejected");
		CHECK(xoauth2LastError() == XOAUTH2_ERROR_TOKEN_RESPONSE,
			"P4-L5 expires_in error code");
		xrtClearError();
		CHECK(xoauth2__parse_token_response(
			"{\"access_token\":\"x\",\"expires_in\":90.5}", 40) == NULL,
			"P4-L5 float expires_in rejected");
	}

	/* ---- P4-M2：超长 access_token 的 Bearer 头完整 ---- */
	{
		xoauth2client c;
		xoauth2config cfg;
		xoauth2ConfigInit(&cfg);
		cfg.AuthorizeUrl = "https://idp/a";
		cfg.TokenUrl = "https://idp/t";
		cfg.UserInfoUrl = "https://idp/ui";
		cfg.ClientId = "cid";
		cfg.RedirectUri = "https://app/cb";
		cfg.Http = mock_http;
		xoauth2UseCustom(&c, &cfg);
		mock_setup(200, "{\"sub\":\"u\"}");
		static char aBig[890 + 1];
		memset(aBig, 'T', sizeof(aBig) - 1);
		aBig[sizeof(aBig) - 1] = 0;
		xvalue* ui = xoauth2GetUserInfo(&c, aBig);
		CHECK(ui != NULL, "P4-M2 long-token userinfo works");
		if ( ui ) xrtValueRelease(ui);
		CHECK(strlen(g_MockAuth) == 897,
			"P4-M2 bearer header not truncated (897)");
	}

	/* ============ Phase 4b API 评审修复回归 ============ */

	/* ---- 评审②：GitHub 预设带 UserInfoUrl ---- */
	{
		xoauth2client c;
		xoauth2UseGithub(&c, "i", "s", "https://app/cb");
		CHECK(c.Config.UserInfoUrl != NULL &&
			strcmp(c.Config.UserInfoUrl, "https://api.github.com/user") == 0,
			"p4b github preset userinfo url");
		xoauth2ClientUnit(&c);
	}

	/* ---- 评审③：未知有效期（ExpiresIn==0）不判过期 ---- */
	{
		const char* j = "{\"access_token\":\"gh-no-exp\"}";
		xoauth2token* t = xoauth2__parse_token_response(j, strlen(j));
		CHECK(t != NULL, "p4b no-expires_in token parses");
		if ( t ) {
			CHECK(!xoauth2TokenExpiring(t, 0),
				"p4b unknown expiry not expiring (no refresh loop)");
			CHECK(t->RefreshToken == NULL, "p4b github-style token has no rt");
			xoauth2TokenFree(t);
		}
	}

	/* ============ fuzz：畸形输入不崩溃（确定性 LCG，2000 例） ============ */
	{
		unsigned int seed = 0xC0FFEE42u;
		const char* aAlphabet =
			"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789"
			"-._~&=%?#/\\ \"'{}[]<>";
		char aBuf[160];
		char aSmall[64], aChal[64];
		for ( int iter = 0; iter < 2000; iter++ ) {
			seed = seed * 1664525u + 1013904223u;
			size_t len = (size_t)(seed % 100) + 1;
			if ( len >= sizeof(aBuf) ) len = sizeof(aBuf) - 1;
			for ( size_t i = 0; i < len; i++ ) {
				seed = seed * 1664525u + 1013904223u;
				aBuf[i] = aAlphabet[seed % 85];
			}
			aBuf[len] = 0;
			char* e = xoauth2UrlEncode(aBuf);
			if ( e ) xrtFree(e);
			xoauth2token* t = xoauth2__parse_token_response(aBuf, len);
			if ( t ) xoauth2TokenFree(t);
			(void)xoauth2StateGenerate(aSmall, sizeof(aSmall));
			(void)xoauth2PkceGenerate(aSmall, sizeof(aSmall), aChal, sizeof(aChal));
			xoauth2client c;
			xoauth2config cfg;
			xoauth2ConfigInit(&cfg);
			cfg.AuthorizeUrl = aBuf;
			cfg.ClientId = aBuf;
			cfg.RedirectUri = aBuf;
			cfg.Scope = aBuf;
			xoauth2UseCustom(&c, &cfg);
			char* u = xoauth2BeginLogin(&c);
			if ( u ) xrtFree(u);
			if ( c.sState[0] ) {
				xoauth2token* r = xoauth2CompleteLogin(&c, aBuf, c.sState);
				if ( r ) xoauth2TokenFree(r);
				r = xoauth2Refresh(&c, aBuf);
				if ( r ) xoauth2TokenFree(r);
			}
			/* L-1：Phase 3 新 API 入 fuzz */
			{
				xoauth2client fc;
				xoauth2config fcfg;
				int fst = 0;
				xoauth2ConfigInit(&fcfg);
				fcfg.TokenUrl = aBuf;
				fcfg.UserInfoUrl = aBuf;
				fcfg.ClientId = aBuf;
				fcfg.Http = fuzz_http;
				xoauth2UseCustom(&fc, &fcfg);
				char* fb = xoauth2HttpGet(&fc, aBuf, aBuf, &fst);
				if ( fb ) xrtFree(fb);
				xvalue* fu = xoauth2GetUserInfo(&fc, aBuf);
				if ( fu ) xrtValueRelease(fu);
				(void)xoauth2NonceConsume(&fc, aBuf);
				xoauth2ClientUnit(&fc);
			}
			xoauth2ClientUnit(&c);
		}
		CHECK(1, "fuzz 2000 malformed inputs no crash");
	}

	/* ============ Phase 2：网络层（mock 回调 + 回环真实端到端） ============ */
	/* 网络测试放在泄漏实测之后：后台线程分配会干扰稳态测量 */

	/* ---- mock 回调全路径 ---- */
	{
		xoauth2client c;
		xoauth2config cfg;
		char* url;
		xoauth2token* t;

		xoauth2ConfigInit(&cfg);
		cfg.AuthorizeUrl = "https://idp.example/authorize";
		cfg.TokenUrl = "https://idp.example/token";
		cfg.ClientId = "mock-id";
		cfg.ClientSecret = "mock-secret";
		cfg.RedirectUri = "https://app/cb";
		cfg.Http = mock_http;

		/* 成功路径：mock 200 → token 完整返回，回调收到正确请求 */
		mock_setup(200, "{\"access_token\":\"mk_tok\",\"refresh_token\":\"mk_rt\","
			"\"token_type\":\"Bearer\",\"expires_in\":7200}");
		xoauth2UseCustom(&c, &cfg);
		url = xoauth2BeginLogin(&c);
		CHECK(url != NULL, "p2 mock: begin");
		if ( url ) xrtFree(url);
		t = xoauth2CompleteLogin(&c, "the-code", c.sState);
		CHECK(t != NULL, "p2 mock: complete returns token");
		if ( t ) {
			CHECK(strcmp(t->AccessToken, "mk_tok") == 0, "p2 mock token value");
			CHECK(t->ExpiresIn == 7200, "p2 mock expires_in");
			CHECK(t->ObtainedAt > 0 && t->ExpiresAt == t->ObtainedAt + 7200,
				"p2 mock timestamps");
			xoauth2TokenFree(t);
		}
		CHECK(strcmp(g_MockUrl, "https://idp.example/token") == 0,
			"p2 mock: callback url");
		CHECK(strstr(g_MockBody, "grant_type=authorization_code") != NULL &&
			strstr(g_MockBody, "code=the-code") != NULL,
			"p2 mock: callback body");
		CHECK(g_MockAuth[0] == 0, "p2 mock: body style no auth header");

		/* BASIC 风格：Authorization 头透传到回调 */
		mock_setup(200, "{\"access_token\":\"b2\"}");
		cfg.AuthStyle = XOAUTH2_AUTH_BASIC;
		xoauth2UseCustom(&c, &cfg);
		url = xoauth2BeginLogin(&c);
		if ( url ) xrtFree(url);
		t = xoauth2CompleteLogin(&c, "c2", c.sState);
		CHECK(t != NULL, "p2 mock basic: token");
		if ( t ) xoauth2TokenFree(t);
		CHECK(strncmp(g_MockAuth, "Basic ", 6) == 0,
			"p2 mock basic: auth header passed through");
		CHECK(strstr(g_MockBody, "client_secret") == NULL,
			"p2 mock basic: no creds in body");

		/* 4xx + provider error JSON → TOKEN_DENIED */
		mock_setup(400, "{\"error\":\"invalid_grant\","
			"\"error_description\":\"code expired\"}");
		xoauth2UseCustom(&c, &cfg);
		url = xoauth2BeginLogin(&c);
		if ( url ) xrtFree(url);
		xrtClearError();
		t = xoauth2CompleteLogin(&c, "c3", c.sState);
		CHECK(t == NULL, "p2 mock 400: rejected");
		CHECK(xoauth2LastError() == XOAUTH2_ERROR_TOKEN_DENIED,
			"p2 mock 400: DENIED code");

		/* 4xx/5xx + 非 JSON body → TOKEN_ENDPOINT */
		mock_setup(502, "<html>Bad Gateway</html>");
		url = xoauth2BeginLogin(&c);
		if ( url ) xrtFree(url);
		xrtClearError();
		t = xoauth2CompleteLogin(&c, "c4", c.sState);
		CHECK(t == NULL && xoauth2LastError() == XOAUTH2_ERROR_TOKEN_ENDPOINT,
			"p2 mock 502: ENDPOINT code");

		/* 5xx 空 body → TOKEN_ENDPOINT */
		mock_setup(500, "");
		url = xoauth2BeginLogin(&c);
		if ( url ) xrtFree(url);
		xrtClearError();
		t = xoauth2CompleteLogin(&c, "c5", c.sState);
		CHECK(t == NULL && xoauth2LastError() == XOAUTH2_ERROR_TOKEN_ENDPOINT,
			"p2 mock 500 empty: ENDPOINT code");

		/* 2xx 但响应畸形 → TOKEN_RESPONSE */
		mock_setup(200, "not json at all");
		url = xoauth2BeginLogin(&c);
		if ( url ) xrtFree(url);
		xrtClearError();
		t = xoauth2CompleteLogin(&c, "c6", c.sState);
		CHECK(t == NULL && xoauth2LastError() == XOAUTH2_ERROR_TOKEN_RESPONSE,
			"p2 mock 200 garbage: RESPONSE code");

		/* 传输失败（连接/超时语义）→ NETWORK */
		g_MockFail = true;
		url = xoauth2BeginLogin(&c);
		if ( url ) xrtFree(url);
		xrtClearError();
		t = xoauth2CompleteLogin(&c, "c7", c.sState);
		CHECK(t == NULL && xoauth2LastError() == XOAUTH2_ERROR_NETWORK,
			"p2 mock transport fail: NETWORK code");
		g_MockFail = false;

		/* 未配置传输 → NETWORK */
		cfg.Http = NULL;
		xoauth2UseCustom(&c, &cfg);
		url = xoauth2BeginLogin(&c);
		if ( url ) xrtFree(url);
		xrtClearError();
		t = xoauth2CompleteLogin(&c, "c8", c.sState);
		CHECK(t == NULL && xoauth2LastError() == XOAUTH2_ERROR_NETWORK,
			"p2 no transport: NETWORK code");

		/* Refresh 全路径：成功 / DENIED */
		cfg.Http = mock_http;
		mock_setup(200, "{\"access_token\":\"rf_tok\",\"expires_in\":1800}");
		cfg.AuthStyle = XOAUTH2_AUTH_BODY;
		xoauth2UseCustom(&c, &cfg);
		t = xoauth2Refresh(&c, "old-rt");
		CHECK(t != NULL && strcmp(t->AccessToken, "rf_tok") == 0,
			"p2 refresh success");
		if ( t ) xoauth2TokenFree(t);
		CHECK(strstr(g_MockBody, "grant_type=refresh_token") != NULL &&
			strstr(g_MockBody, "refresh_token=old-rt") != NULL,
			"p2 refresh: callback body");
		mock_setup(400, "{\"error\":\"invalid_grant\"}");
		xrtClearError();
		t = xoauth2Refresh(&c, "old-rt");
		CHECK(t == NULL && xoauth2LastError() == XOAUTH2_ERROR_TOKEN_DENIED,
			"p2 refresh denied");
	}

	/* ---- 回环真实服务器 + xoauth2HttpXrt 端到端（http://） ---- */
	{
#ifdef _WIN32
		xoauth2client c;
		xoauth2config cfg;
		char* url;
		xoauth2token* t;
		char aTokenUrl[128];

		/* 极简单连接回环服务器：accept → 读请求 → 回 canned 响应 */
		WSADATA wsa;
		CHECK(WSAStartup(MAKEWORD(2, 2), &wsa) == 0, "p2 loop: winsock init");
		g_ListenSock = socket(AF_INET, SOCK_STREAM, 0);
		struct sockaddr_in addr;
		memset(&addr, 0, sizeof(addr));
		addr.sin_family = AF_INET;
		addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
		addr.sin_port = 0;
		CHECK(bind(g_ListenSock, (struct sockaddr*)&addr, sizeof(addr)) == 0,
			"p2 loop: bind");
		CHECK(listen(g_ListenSock, 4) == 0, "p2 loop: listen");
		int addrLen = sizeof(addr);
		getsockname(g_ListenSock, (struct sockaddr*)&addr, &addrLen);
		int port = ntohs(addr.sin_port);
		CreateThread(NULL, 0, loop_server_proc, NULL, 0, NULL);

		/* xrt 传输初始化（自建 engine，3s 超时） */
		xoauth2httpxrt httpxrt;
		CHECK(xoauth2HttpXrtInit(&httpxrt, NULL, NULL, 3000000),
			"p2 xrt transport init");

		xoauth2ConfigInit(&cfg);
		cfg.AuthorizeUrl = "https://idp.example/authorize";
		snprintf(aTokenUrl, sizeof(aTokenUrl), "http://127.0.0.1:%d/token", port);
		cfg.TokenUrl = aTokenUrl;
		cfg.ClientId = "loop-id";
		cfg.ClientSecret = "loop-secret";
		cfg.RedirectUri = "https://app/cb";
		cfg.Http = xoauth2HttpXrt;
		cfg.HttpContext = &httpxrt;

		/* 端到端成功：200 JSON → token */
		g_ServerReply =
			"HTTP/1.1 200 OK\r\nContent-Type: application/json\r\n"
			"Content-Length: 66\r\nConnection: close\r\n\r\n"
			"{\"access_token\":\"loop_tok\",\"token_type\":\"bearer\","
			"\"expires_in\":900}";
		xoauth2UseCustom(&c, &cfg);
		url = xoauth2BeginLogin(&c);
		CHECK(url != NULL, "p2 loop: begin");
		if ( url ) xrtFree(url);
		t = xoauth2CompleteLogin(&c, "loop-code", c.sState);
		CHECK(t != NULL, "p2 loop: end-to-end token");
		if ( t ) {
			CHECK(strcmp(t->AccessToken, "loop_tok") == 0,
				"p2 loop token value");
			xoauth2TokenFree(t);
		}
		CHECK(strstr(g_ServerDone, "POST /token HTTP/1.1") != NULL,
			"p2 loop: request line");
		CHECK(strstr(g_ServerDone, "Host: 127.0.0.1:") != NULL,
			"p2 loop: host header");
		CHECK(strstr(g_ServerDone,
			"Content-Type: application/x-www-form-urlencoded") != NULL,
			"p2 loop: form content type");
		CHECK(strstr(g_ServerDone, "grant_type=authorization_code") != NULL &&
			strstr(g_ServerDone, "code=loop-code") != NULL,
			"p2 loop: body received");

		/* 端到端错误：400 + error JSON → DENIED */
		g_ServerReply =
			"HTTP/1.1 400 Bad Request\r\nContent-Length: 25\r\n"
			"Connection: close\r\n\r\n"
			"{\"error\":\"invalid_grant\"}";
		url = xoauth2BeginLogin(&c);
		if ( url ) xrtFree(url);
		xrtClearError();
		t = xoauth2CompleteLogin(&c, "x", c.sState);
		CHECK(t == NULL && xoauth2LastError() == XOAUTH2_ERROR_TOKEN_DENIED,
			"p2 loop: 400 denied end-to-end");

		/* Refresh 端到端 */
		g_ServerReply =
			"HTTP/1.1 200 OK\r\nContent-Length: 39\r\n"
			"Connection: close\r\n\r\n"
			"{\"access_token\":\"rf2\",\"expires_in\":600}";
		xrtClearError();
		t = xoauth2Refresh(&c, "rt-loop");
		CHECK(t != NULL && strcmp(t->AccessToken, "rf2") == 0,
			"p2 loop: refresh end-to-end");
		if ( t ) xoauth2TokenFree(t);
		CHECK(strstr(g_ServerDone, "grant_type=refresh_token") != NULL,
			"p2 loop: refresh body");

		/* 连接不可达 → NETWORK */
		snprintf(aTokenUrl, sizeof(aTokenUrl), "http://127.0.0.1:1/token");
		xrtClearError();
		t = xoauth2Refresh(&c, "rt");
		CHECK(t == NULL && xoauth2LastError() == XOAUTH2_ERROR_NETWORK,
			"p2 loop: unreachable -> NETWORK");

		/* P4-M4：超大 Content-Length 声明 → MaxBody 上限拒绝 → NETWORK */
		g_ServerReply =
			"HTTP/1.1 200 OK\r\nContent-Length: 99999999\r\n"
			"Connection: close\r\n\r\n{\"access_token\":\"x\"}";
		snprintf(aTokenUrl, sizeof(aTokenUrl), "http://127.0.0.1:%d/token", port);
		cfg.TokenUrl = aTokenUrl;
		xoauth2UseCustom(&c, &cfg);
		xrtClearError();
		t = xoauth2Refresh(&c, "rt");
		CHECK(t == NULL && xoauth2LastError() == XOAUTH2_ERROR_NETWORK,
			"P4-M4 oversized declared body -> NETWORK");

		/* P4-L4：IPv6 字面量 URL 解析被接受（区别于格式拒绝） */
		{
			char* pBody6 = NULL;
			int st6 = 0;
			bool ok6 = xoauth2HttpXrt("GET", "http://[::1]:1/jwks", NULL,
				NULL, &pBody6, &st6, &httpxrt);
			CHECK(!ok6 && strstr(
				xrtErrorMessage(xrtGetError()) ?
				xrtErrorMessage(xrtGetError()) : "",
				"url is not") == NULL,
				"P4-L4 ipv6 literal parsed (not url-reject)");
			xrtClearError();
			ok6 = xoauth2HttpXrt("GET", "http://[::1/x", NULL,
				NULL, &pBody6, &st6, &httpxrt);
			CHECK(!ok6, "P4-L4 malformed ipv6 rejected");
		}

		/* ---- 评审⑤：HttpXrtCreate/Destroy 堆版本端到端 ---- */
		{
			xoauth2httpxrt* pHeap = xoauth2HttpXrtCreate(NULL, NULL, 3000000);
			CHECK(pHeap != NULL, "p4b HttpXrtCreate");
			if ( pHeap != NULL ) {
				xoauth2client hc;
				memcpy(&hc, &c, sizeof(hc));   /* 借用回环段的配置 */
				hc.Config.Http = xoauth2HttpXrt;
				hc.Config.HttpContext = pHeap;
				g_ServerReply =
					"HTTP/1.1 200 OK\r\nContent-Length: 43\r\n"
					"Connection: close\r\n\r\n"
					"{\"access_token\":\"heap-rt\",\"expires_in\":600}";
				xrtClearError();
				xoauth2token* ht = xoauth2Refresh(&hc, "rt");
				CHECK(ht != NULL && strcmp(ht->AccessToken, "heap-rt") == 0,
					"p4b heap transport end-to-end");
				if ( ht ) xoauth2TokenFree(ht);
				xoauth2HttpXrtDestroy(pHeap);
			}
		}

		/* 畸形 scheme → 回调 false → NETWORK */
		cfg.TokenUrl = "ftp://bad";
		xoauth2UseCustom(&c, &cfg);
		xrtClearError();
		t = xoauth2Refresh(&c, "rt");
		CHECK(t == NULL && xoauth2LastError() == XOAUTH2_ERROR_NETWORK,
			"p2 loop: bad scheme -> NETWORK");

		xoauth2HttpXrtUnit(&httpxrt);
		InterlockedExchange(&g_ServerStop, 1);
		closesocket(g_ListenSock);
		WSACleanup();
#else
		/* 非 Windows：网络端到端留给 CI（xrt 自身有跨平台网络测试基建） */
		CHECK(1, "p2 loopback skipped on non-windows");
#endif
	}

	/* ============ Phase 3：OIDC 辅助（数据耦合层） ============ */

	/* ---- 预设 OIDC 字段 ---- */
	{
		xoauth2client c;
		xoauth2UseGoogle(&c, "g", "s", "https://app/cb");
		CHECK(c.Config.Issuer != NULL &&
			strcmp(c.Config.Issuer, "https://accounts.google.com") == 0,
			"p3 google issuer preset");
		CHECK(c.Config.JwksUrl != NULL &&
			strcmp(c.Config.JwksUrl,
				"https://www.googleapis.com/oauth2/v3/certs") == 0,
			"p3 google jwks preset");
		CHECK(c.Config.UseNonce, "p3 google nonce on");
		xoauth2ClientUnit(&c);

		xoauth2UseMicrosoft(&c, "id", "sec", "https://app/cb", "contoso.com");
		CHECK(c.Config.Issuer != NULL &&
			strcmp(c.Config.Issuer,
				"https://login.microsoftonline.com/contoso.com/v2.0") == 0,
			"p3 microsoft dynamic issuer (v2 iss domain)");
		CHECK(c.pOwnedIssuerUrl == c.Config.Issuer,
			"p3 microsoft owns issuer url");
		CHECK(c.Config.UseNonce, "p3 microsoft nonce on");
		xoauth2ClientUnit(&c);
		CHECK(c.pOwnedIssuerUrl == NULL, "p3 ClientUnit frees issuer url");

		/* 非 OIDC 预设不开 nonce */
		xoauth2UseGithub(&c, "i", "s", "https://app/cb");
		CHECK(!c.Config.UseNonce && c.Config.Issuer == NULL,
			"p3 github stays oauth-only");
		xoauth2ClientUnit(&c);
	}

	/* ---- nonce 全链路 ---- */
	{
		xoauth2client c;
		xoauth2config cfg;
		xoauth2ConfigInit(&cfg);
		cfg.AuthorizeUrl = "https://idp/a";
		cfg.TokenUrl = "https://idp/t";
		cfg.ClientId = "cid";
		cfg.RedirectUri = "https://app/cb";
		cfg.UseNonce = true;
		xoauth2UseCustom(&c, &cfg);
		char* url = xoauth2BeginLogin(&c);
		CHECK(url != NULL, "p3 nonce begin");
		if ( url != NULL ) {
			CHECK(strstr(url, "&nonce=") != NULL, "p3 nonce in url");
			xrtFree(url);
		}
		CHECK(c.sNonce[0] != 0, "p3 nonce generated");

		/* 消费：正确 → 焚毁；重放/错误 → NONCE_MISMATCH */
		char aNonce[128];
		strcpy(aNonce, c.sNonce);
		CHECK(xoauth2NonceConsume(&c, aNonce), "p3 nonce consume ok");
		CHECK(c.sNonce[0] == 0, "p3 nonce burned");
		xrtClearError();
		CHECK(!xoauth2NonceConsume(&c, aNonce) &&
			xoauth2LastError() == XOAUTH2_ERROR_NONCE_MISMATCH,
			"p3 nonce replay rejected");
		xrtClearError();
		CHECK(!xoauth2NonceConsume(&c, "wrong") &&
			xoauth2LastError() == XOAUTH2_ERROR_NONCE_MISMATCH,
			"p3 nonce mismatch rejected");

		/* UseNonce=false：URL 无参数、sNonce 空、消费拒绝 */
		cfg.UseNonce = false;
		xoauth2UseCustom(&c, &cfg);
		url = xoauth2BeginLogin(&c);
		if ( url != NULL ) {
			CHECK(strstr(url, "nonce=") == NULL, "p3 nonce off: no param");
			xrtFree(url);
		}
		xrtClearError();
		CHECK(!xoauth2NonceConsume(&c, "x") &&
			xoauth2LastError() == XOAUTH2_ERROR_NONCE_MISMATCH,
			"p3 empty nonce cannot consume");
	}

	/* ---- HttpGet / GetUserInfo（mock 传输） ---- */
	{
		xoauth2client c;
		xoauth2config cfg;
		xoauth2ConfigInit(&cfg);
		cfg.AuthorizeUrl = "https://idp/a";
		cfg.TokenUrl = "https://idp/t";
		cfg.UserInfoUrl = "https://idp/userinfo";
		cfg.ClientId = "cid";
		cfg.RedirectUri = "https://app/cb";
		cfg.Http = mock_http;
		xoauth2UseCustom(&c, &cfg);

		/* HttpGet 成功：method=GET、无 body、auth 透传 */
		mock_setup(200, "{\"keys\":[]}");
		int st = 0;
		char* body = xoauth2HttpGet(&c, "https://idp/jwks", "Bearer xx", &st);
		CHECK(body != NULL && strcmp(body, "{\"keys\":[]}") == 0,
			"p3 HttpGet body");
		CHECK(st == 200, "p3 HttpGet status");
		CHECK(strcmp(g_MockMethod, "GET") == 0, "p3 HttpGet method");
		CHECK(strcmp(g_MockUrl, "https://idp/jwks") == 0, "p3 HttpGet url");
		CHECK(g_MockAuth[0] != 0 && strncmp(g_MockAuth, "Bearer ", 7) == 0,
			"p3 HttpGet auth passthrough");
		xrtFree(body);

		/* 非 2xx → ENDPOINT；空 body → RESPONSE */
		mock_setup(404, "gone");
		xrtClearError();
		CHECK(xoauth2HttpGet(&c, "https://idp/jwks", NULL, &st) == NULL &&
			xoauth2LastError() == XOAUTH2_ERROR_TOKEN_ENDPOINT,
			"p3 HttpGet 404 -> ENDPOINT");
		mock_setup(200, "");
		xrtClearError();
		CHECK(xoauth2HttpGet(&c, "https://idp/jwks", NULL, &st) == NULL &&
			xoauth2LastError() == XOAUTH2_ERROR_TOKEN_RESPONSE,
			"p3 HttpGet empty -> RESPONSE");

		/* GetUserInfo：Bearer + 对象 claims；非对象拒绝 */
		mock_setup(200, "{\"sub\":\"u1\",\"email\":\"u@x.com\"}");
		xvalue* ui = xoauth2GetUserInfo(&c, "the-access-token");
		CHECK(ui != NULL, "p3 userinfo claims");
		if ( ui != NULL ) {
			xstrview svSub;
			CHECK(xrtValueGetString(xrtValueObjectGet(ui, xrtStrView("sub")), &svSub)
				&& svSub.Size == 2 && memcmp(svSub.Data, "u1", 2) == 0,
				"p3 userinfo sub");
			xrtValueRelease(ui);
		}
		CHECK(strncmp(g_MockAuth, "Bearer the-access-token", 23) == 0,
			"p3 userinfo bearer header");
		mock_setup(200, "[1,2]");
		xrtClearError();
		CHECK(xoauth2GetUserInfo(&c, "t") == NULL &&
			xoauth2LastError() == XOAUTH2_ERROR_TOKEN_RESPONSE,
			"p3 userinfo non-object rejected");
	}

	printf("\n%d pass, %d fail\n", s_pass, s_fail);
	return s_fail > 0 ? 1 : 0;
}
