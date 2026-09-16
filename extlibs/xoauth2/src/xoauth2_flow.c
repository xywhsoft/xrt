/* xoauth2 token 交换 + 刷新。
 * 请求构造与响应解析为完整实现（离线可测）；
 * 网络发送为 Phase 2 —— 经传输回调注入（见 README 路线图）。 */
#include "xoauth2_internal.h"

/* ------------------------------------------------------------------ */
/* Token 响应 JSON 解析                                                  */
/* ------------------------------------------------------------------ */

xoauth2token* xoauth2__parse_token_response(const char* sJson, size_t iSize)
{
	if ( sJson == NULL || iSize == 0 ) {
		xoauth2__error(XOAUTH2_ERROR_ARGUMENT, "empty token response");
		return NULL;
	}

	xvalue* p = xrtJsonParse(xrtStrViewN((cstr)sJson, iSize));
	if ( p == NULL ) {
		xoauth2__error(XOAUTH2_ERROR_TOKEN_RESPONSE, "JSON parse failed");
		return NULL;
	}

	/* 检查 error（RFC 6749 §5.2：失败响应只有 error 系字段） */
	{
		xstrview sv;
		if ( xrtValueGetString(xrtValueObjectGet(p, xrtStrView("error")), &sv) ) {
			xoauth2__error(XOAUTH2_ERROR_TOKEN_DENIED, "provider returned error");
			xrtValueRelease(p);
			return NULL;
		}
	}

	xoauth2token* pToken = (xoauth2token*)xrtMalloc(sizeof(xoauth2token));
	if ( pToken == NULL ) { xrtValueRelease(p); return NULL; }
	memset(pToken, 0, sizeof(*pToken));

	/* 提取字段 */
	xstrview sv;
	if ( xrtValueGetString(xrtValueObjectGet(p, xrtStrView("access_token")), &sv) ) {
		pToken->AccessToken = (char*)xrtMalloc(sv.Size + 1);
		if ( pToken->AccessToken ) {
			memcpy(pToken->AccessToken, sv.Data, sv.Size);
			pToken->AccessToken[sv.Size] = 0;
		}
	}
	if ( xrtValueGetString(xrtValueObjectGet(p, xrtStrView("refresh_token")), &sv) ) {
		pToken->RefreshToken = (char*)xrtMalloc(sv.Size + 1);
		if ( pToken->RefreshToken ) {
			memcpy(pToken->RefreshToken, sv.Data, sv.Size);
			pToken->RefreshToken[sv.Size] = 0;
		}
	}
	if ( xrtValueGetString(xrtValueObjectGet(p, xrtStrView("id_token")), &sv) ) {
		pToken->IdToken = (char*)xrtMalloc(sv.Size + 1);
		if ( pToken->IdToken ) {
			memcpy(pToken->IdToken, sv.Data, sv.Size);
			pToken->IdToken[sv.Size] = 0;
		}
	}
	if ( xrtValueGetString(xrtValueObjectGet(p, xrtStrView("token_type")), &sv) ) {
		/* RFC 6749 §5.1：token_type 大小写不敏感；统一小写存储 */
		pToken->TokenType = (char*)xrtMalloc(sv.Size + 1);
		if ( pToken->TokenType ) {
			for ( size_t i = 0; i < sv.Size; i++ ) {
				char c = ((const char*)sv.Data)[i];
				pToken->TokenType[i] = (c >= 'A' && c <= 'Z') ? (char)(c + 32) : c;
			}
			pToken->TokenType[sv.Size] = 0;
		}
	}
	if ( xrtValueGetString(xrtValueObjectGet(p, xrtStrView("scope")), &sv) ) {
		pToken->Scope = (char*)xrtMalloc(sv.Size + 1);
		if ( pToken->Scope ) {
			memcpy(pToken->Scope, sv.Data, sv.Size);
			pToken->Scope[sv.Size] = 0;
		}
	}
	{
		xvalue* pExp = xrtValueObjectGet(p, xrtStrView("expires_in"));
		if ( pExp != NULL ) {
			int64 v;
			/* RFC 6749 §4.2.2：expires_in 是十进制整数秒；
			 * 存在但类型错误显式拒绝（字符串时间戳曾是 xjwt 的 H1 教训） */
			if ( !xrtValueIs(pExp, XVALUE_INT) || !xrtValueGetInt(pExp, &v) ) {
				xrtValueRelease(p);
				xoauth2TokenFree(pToken);
				xoauth2__error(XOAUTH2_ERROR_TOKEN_RESPONSE,
					"expires_in must be an integer");
				return NULL;
			}
			pToken->ExpiresIn = v;
		}
	}
	/* 时间戳：获取时刻 + 过期时刻（TokenExpiring 的依据） */
	pToken->ObtainedAt = (int64_t)(xrtNow() / 1000000);
	pToken->ExpiresAt = pToken->ObtainedAt + pToken->ExpiresIn;

	xrtValueRelease(p);

	if ( pToken->AccessToken == NULL ) {
		xoauth2__error(XOAUTH2_ERROR_TOKEN_RESPONSE, "no access_token in response");
		xoauth2TokenFree(pToken);
		return NULL;
	}
	return pToken;
}

/* ------------------------------------------------------------------ */
/* 请求构造                                                              */
/* ------------------------------------------------------------------ */

/* AuthStyle=BASIC：Authorization: Basic base64(urlenc(id):urlenc(secret))
 * RFC 6749 §2.3.1 要求 id/secret 先做 form 编码再拼接 */
char* xoauth2__build_auth_header(const xoauth2client* pClient)
{
	if ( pClient == NULL || pClient->Config.ClientId == NULL ) return NULL;
	if ( pClient->Config.AuthStyle != XOAUTH2_AUTH_BASIC ) return NULL;
	char* sId = xoauth2UrlEncode(pClient->Config.ClientId);
	if ( sId == NULL ) return NULL;
	char* sSecret = xoauth2UrlEncode(
		pClient->Config.ClientSecret ? pClient->Config.ClientSecret : "");
	if ( sSecret == NULL ) { xrtFree(sId); return NULL; }

	size_t n = strlen(sId) + 1 + strlen(sSecret);
	char* sJoined = (char*)xrtMalloc(n + 1);
	if ( sJoined != NULL ) {
		strcpy(sJoined, sId);
		strcat(sJoined, ":");
		strcat(sJoined, sSecret);
	}
	xrtFree(sId);
	xrtFree(sSecret);
	if ( sJoined == NULL ) return NULL;

	str sB64 = xrtBase64EncodeNew(sJoined, strlen(sJoined), NULL);
	xrtFree(sJoined);
	if ( sB64 == NULL ) return NULL;

	size_t nOut = strlen(sB64) + 16;
	char* sHeader = (char*)xrtMalloc(nOut);
	if ( sHeader != NULL )
		snprintf(sHeader, nOut, "Basic %s", sB64);
	xrtFree(sB64);
	return sHeader;
}

/* 两遍法：先测量总长，再写入。 */
static char* build_form(const char* const* keys,
                        const char* const* values, int nFields)
{
	size_t nNeed = 1;
	for ( int i = 0; i < nFields; i++ ) {
		char* sEnc = xoauth2UrlEncode(values[i]);
		if ( sEnc == NULL ) return NULL;
		nNeed += strlen(keys[i]) + strlen(sEnc) + 2;  /* '=' + '&'/'\0' */
		xrtFree(sEnc);
	}
	char* sBody = (char*)xrtMalloc(nNeed);
	if ( sBody == NULL ) return NULL;
	sBody[0] = 0;
	size_t j = 0;
	for ( int i = 0; i < nFields; i++ ) {
		char* sEnc = xoauth2UrlEncode(values[i]);
		if ( sEnc == NULL ) { xrtFree(sBody); return NULL; }
		j += snprintf(sBody + j, nNeed - j, "%s%s=%s",
		              j > 0 ? "&" : "", keys[i], sEnc);
		xrtFree(sEnc);
	}
	return sBody;
}

char* xoauth2__build_token_request(xoauth2client* pClient, const char* sCode)
{
	if ( pClient == NULL || sCode == NULL ||
	     pClient->Config.ClientId == NULL ||
	     pClient->Config.RedirectUri == NULL ) {
		xoauth2__error(XOAUTH2_ERROR_ARGUMENT, "token request: not configured");
		return NULL;
	}

	/* BASIC 风格：凭据走 Authorization 头，body 不带 client_id/secret */
	bool bBasic = pClient->Config.AuthStyle == XOAUTH2_AUTH_BASIC;

	const char* keys[8];
	const char* values[8];
	int nFields = 0;
	keys[nFields] = "grant_type"; values[nFields++] = "authorization_code";
	keys[nFields] = "code"; values[nFields++] = sCode;
	keys[nFields] = "redirect_uri"; values[nFields++] = pClient->Config.RedirectUri;
	if ( !bBasic ) {
		keys[nFields] = "client_id"; values[nFields++] = pClient->Config.ClientId;
		if ( pClient->Config.ClientSecret != NULL ) {
			keys[nFields] = "client_secret";
			values[nFields++] = pClient->Config.ClientSecret;
		}
	}
	if ( pClient->Config.UsePkce && pClient->sVerifier[0] != 0 ) {
		keys[nFields] = "code_verifier"; values[nFields++] = pClient->sVerifier;
	}

	char* sBody = build_form(keys, values, nFields);
	if ( sBody == NULL ) {
		xoauth2__error(XOAUTH2_ERROR_ARGUMENT, "token request build failed");
		return NULL;
	}
	return sBody;
}

char* xoauth2__build_refresh_request(const xoauth2client* pClient,
                                     const char* sRefreshToken)
{
	if ( pClient == NULL || sRefreshToken == NULL ||
	     pClient->Config.ClientId == NULL ) {
		xoauth2__error(XOAUTH2_ERROR_ARGUMENT, "refresh request: not configured");
		return NULL;
	}

	bool bBasic = pClient->Config.AuthStyle == XOAUTH2_AUTH_BASIC;

	const char* keys[8];
	const char* values[8];
	int nFields = 0;
	keys[nFields] = "grant_type"; values[nFields++] = "refresh_token";
	keys[nFields] = "refresh_token"; values[nFields++] = sRefreshToken;
	if ( !bBasic ) {
		keys[nFields] = "client_id"; values[nFields++] = pClient->Config.ClientId;
		if ( pClient->Config.ClientSecret != NULL ) {
			keys[nFields] = "client_secret";
			values[nFields++] = pClient->Config.ClientSecret;
		}
	}

	char* sBody = build_form(keys, values, nFields);
	if ( sBody == NULL ) {
		xoauth2__error(XOAUTH2_ERROR_ARGUMENT, "refresh request build failed");
		return NULL;
	}
	return sBody;
}

/* ------------------------------------------------------------------ */
/* 会话消费（state 常时比较 + 一次性焚毁）                                */
/* ------------------------------------------------------------------ */

static bool state_consume(xoauth2client* pClient, const char* sState)
{
	size_t nMine = strlen(pClient->sState);
	size_t nTheirs = strlen(sState);
	if ( nMine == 0 || nMine != nTheirs ||
	     !xrtConstTimeEqual(pClient->sState, sState, nMine) ) {
		xoauth2__error(XOAUTH2_ERROR_STATE_MISMATCH,
			"state mismatch (possible CSRF)");
		return false;
	}
	/* state 一次性：校验通过即焚毁（无论后续交换成败，会话已消费） */
	memset(pClient->sState, 0, sizeof(pClient->sState));
	return true;
}

/* verifier 焚毁：请求构造完成后一次性消费（RFC 7636 防重放） */
static void verifier_burn(xoauth2client* pClient)
{
	memset(pClient->sVerifier, 0, sizeof(pClient->sVerifier));
	memset(pClient->sChallenge, 0, sizeof(pClient->sChallenge));
}

/* ------------------------------------------------------------------ */
/* 交换（经传输回调）与状态码分级                                         */
/* ------------------------------------------------------------------ */

/*
	响应分级：
	- 回调返回 false → NETWORK（连接/超时/TLS）
	- 2xx → 解析 JSON（解析失败 TOKEN_RESPONSE；带 error 字段 TOKEN_DENIED）
	- 非 2xx 且 body 是带 error 的 JSON → TOKEN_DENIED
	- 其他非 2xx → TOKEN_ENDPOINT
*/
static xoauth2token* exchange(const xoauth2client* pClient,
                              const char* sBody, const char* sAuth)
{
	xoauth2token* pToken = NULL;
	char* sResp = NULL;
	int iStatus = 0;

	if ( pClient->Config.Http == NULL ) {
		xoauth2__error(XOAUTH2_ERROR_NETWORK, "no transport configured");
		return NULL;
	}
	if ( !pClient->Config.Http("POST", pClient->Config.TokenUrl, sBody, sAuth,
	                           &sResp, &iStatus, pClient->Config.HttpContext) ) {
		xoauth2__error(XOAUTH2_ERROR_NETWORK, "transport failed");
		return NULL;
	}
	if ( iStatus >= 200 && iStatus < 300 ) {
		pToken = xoauth2__parse_token_response(
			sResp ? sResp : "", sResp ? strlen(sResp) : 0);
	}
	else if ( sResp != NULL && sResp[0] != 0 ) {
		/* 非 2xx：能提取 provider error 就报 DENIED。
		 * 响应体可能同时含完整令牌结构（恶意端点）——探测结果必须释放 */
		xoauth2token* pProbe;
		xrtClearError();
		pProbe = xoauth2__parse_token_response(sResp, strlen(sResp));
		if ( pProbe != NULL )
			xoauth2TokenFree(pProbe);
		if ( xoauth2LastError() != XOAUTH2_ERROR_TOKEN_DENIED )
			xoauth2__error(XOAUTH2_ERROR_TOKEN_ENDPOINT,
				"token endpoint returned HTTP error status");
	}
	else {
		xoauth2__error(XOAUTH2_ERROR_TOKEN_ENDPOINT,
			"token endpoint returned HTTP error status");
	}
	xrtFree(sResp);
	return pToken;
}

/* ------------------------------------------------------------------ */
/* 登录回调 / 刷新                                                       */
/* ------------------------------------------------------------------ */

xoauth2token* xoauth2CompleteLogin(xoauth2client* pClient,
                                   const char* sCode, const char* sState)
{
	if ( pClient == NULL || sCode == NULL || sState == NULL ) {
		xoauth2__error(XOAUTH2_ERROR_ARGUMENT, "xoauth2CompleteLogin: null argument");
		return NULL;
	}
	if ( pClient->Config.TokenUrl == NULL ) {
		xoauth2__error(XOAUTH2_ERROR_ARGUMENT, "xoauth2CompleteLogin: not configured");
		return NULL;
	}

	/* 验 state（常时比较；通过即焚毁会话） */
	if ( !state_consume(pClient, sState) ) return NULL;

	/* 构造请求体与认证头，随即焚毁 verifier */
	char* sBody = xoauth2__build_token_request(pClient, sCode);
	if ( sBody == NULL ) return NULL;
	char* sAuth = xoauth2__build_auth_header(pClient);
	verifier_burn(pClient);

	xoauth2token* pToken = exchange(pClient, sBody, sAuth);
	xrtFree(sAuth);
	xrtFree(sBody);
	return pToken;
}

xoauth2token* xoauth2Refresh(xoauth2client* pClient, const char* sRefreshToken)
{
	if ( pClient == NULL || sRefreshToken == NULL ) {
		xoauth2__error(XOAUTH2_ERROR_ARGUMENT, "xoauth2Refresh: null argument");
		return NULL;
	}
	if ( pClient->Config.TokenUrl == NULL ) {
		xoauth2__error(XOAUTH2_ERROR_ARGUMENT, "xoauth2Refresh: not configured");
		return NULL;
	}
	char* sBody = xoauth2__build_refresh_request(pClient, sRefreshToken);
	if ( sBody == NULL ) return NULL;
	char* sAuth = xoauth2__build_auth_header(pClient);

	xoauth2token* pToken = exchange(pClient, sBody, sAuth);
	xrtFree(sAuth);
	xrtFree(sBody);
	return pToken;
}

/* ------------------------------------------------------------------ */
/* OIDC / 通用 HTTP 辅助（数据耦合：不依赖 xjwt）                        */
/* ------------------------------------------------------------------ */

char* xoauth2HttpGet(xoauth2client* pClient, const char* sUrl,
                     const char* sAuthHeader, int* piStatus)
{
	char* sResp = NULL;
	int iStatus = 0;

	if ( pClient == NULL || sUrl == NULL || piStatus == NULL ) {
		xoauth2__error(XOAUTH2_ERROR_ARGUMENT, "xoauth2HttpGet: null argument");
		return NULL;
	}
	*piStatus = 0;
	if ( pClient->Config.Http == NULL ) {
		xoauth2__error(XOAUTH2_ERROR_NETWORK, "no transport configured");
		return NULL;
	}
	if ( !pClient->Config.Http("GET", sUrl, NULL, sAuthHeader,
	                           &sResp, &iStatus, pClient->Config.HttpContext) ) {
		xoauth2__error(XOAUTH2_ERROR_NETWORK, "transport failed");
		return NULL;
	}
	*piStatus = iStatus;
	if ( iStatus < 200 || iStatus >= 300 ) {
		xrtFree(sResp);
		xoauth2__error(XOAUTH2_ERROR_TOKEN_ENDPOINT,
			"HTTP GET returned error status");
		return NULL;
	}
	if ( sResp == NULL || sResp[0] == '\0' ) {
		xrtFree(sResp);
		xoauth2__error(XOAUTH2_ERROR_TOKEN_RESPONSE, "empty response body");
		return NULL;
	}
	return sResp;
}

xvalue* xoauth2GetUserInfo(xoauth2client* pClient, const char* sAccessToken)
{
	char* sBearer = NULL;
	char* sBody = NULL;
	int iStatus = 0;
	xvalue* pClaims = NULL;

	if ( pClient == NULL || sAccessToken == NULL ) {
		xoauth2__error(XOAUTH2_ERROR_ARGUMENT, "xoauth2GetUserInfo: null argument");
		return NULL;
	}
	if ( pClient->Config.UserInfoUrl == NULL ) {
		xoauth2__error(XOAUTH2_ERROR_ARGUMENT,
			"xoauth2GetUserInfo: no userinfo url configured");
		return NULL;
	}
	/* Bearer 头动态分配：JWT 形态的 access token 常超 512 字符，
	 * 定长缓冲会静默截断导致 userinfo 永远 401 */
	sBearer = (char*)xrtMalloc(strlen(sAccessToken) + 8);
	if ( sBearer == NULL ) {
		xoauth2__error(XOAUTH2_ERROR_ARGUMENT, "bearer header alloc failed");
		return NULL;
	}
	snprintf(sBearer, strlen(sAccessToken) + 8, "Bearer %s", sAccessToken);
	sBody = xoauth2HttpGet(pClient, pClient->Config.UserInfoUrl,
	                       sBearer, &iStatus);
	xrtFree(sBearer);
	if ( sBody == NULL ) return NULL;

	pClaims = xrtJsonParse(xrtStrView(sBody));
	xrtFree(sBody);
	if ( pClaims == NULL ) {
		xoauth2__error(XOAUTH2_ERROR_TOKEN_RESPONSE, "userinfo JSON parse failed");
		return NULL;
	}
	/* userinfo 必须是 JSON 对象（OIDC Core §5.3）；非对象 fail-closed */
	if ( !xrtValueIs(pClaims, XVALUE_OBJECT) ) {
		xrtValueRelease(pClaims);
		xoauth2__error(XOAUTH2_ERROR_TOKEN_RESPONSE,
			"user info must be a JSON object");
		return NULL;
	}
	return pClaims;
}
