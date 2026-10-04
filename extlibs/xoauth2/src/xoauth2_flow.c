/* xoauth2 token 交换 + 刷新；传输由回调注入。 */
#include "xoauth2_internal.h"

/* ------------------------------------------------------------------ */
/* Token 响应 JSON 解析                                                  */
/* ------------------------------------------------------------------ */

static bool xoauth2__copy_token_field(const xvalue* p, const char* sKey,
	char** ppOut, bool bLowercase)
{
	xvalue* pField = xrtValueObjectGet(p, xrtStrView(sKey));
	xstrview Text;
	size_t i;
	if(pField == NULL)
		return true;
	if(!xrtValueGetString(pField, &Text) || Text.Size == SIZE_MAX) {
		xoauth2__error(XOAUTH2_ERROR_TOKEN_RESPONSE, "invalid token response field type");
		return false;
	}
	/* The public token stores C strings; accepting decoded NUL would lose credential bytes. */
	if(Text.Size != 0u && memchr(Text.Data, 0, Text.Size) != NULL) {
		xoauth2__error(XOAUTH2_ERROR_TOKEN_RESPONSE, "token response field contains NUL");
		return false;
	}
	*ppOut = (char*)xrtMalloc(Text.Size + 1u);
	if(*ppOut == NULL)
		return false;
	for(i = 0u; i < Text.Size; i++)
	{
		char c = ((const char*)Text.Data)[i];
		(*ppOut)[i] = (bLowercase && c >= 'A' && c <= 'Z') ?
			(char)(c + 32) : c;
	}
	(*ppOut)[Text.Size] = 0;
	return true;
}

static xoauth2token* xoauth2__parse_token_response_mode(
	const char* sJson, size_t iSize, bool bWechat)
{
	if ( sJson == NULL || iSize == 0 ) {
		xoauth2__error(XOAUTH2_ERROR_ARGUMENT, "empty token response");
		return NULL;
	}

	xvalue* p = xrtJsonParse(xrtStrViewN((cstr)sJson, iSize));
	if ( p == NULL ) {
		if ( xrtErrorKind(xrtGetError()) != XERR_MEMORY )
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
		if ( bWechat && xrtValueObjectHas(p, xrtStrView("errcode")) ) {
			xoauth2__error(XOAUTH2_ERROR_TOKEN_DENIED,
				"WeChat returned an error code");
			xrtValueRelease(p);
			return NULL;
		}
	}

	xoauth2token* pToken = (xoauth2token*)xrtMalloc(sizeof(xoauth2token));
	if ( pToken == NULL ) { xrtValueRelease(p); return NULL; }
	memset(pToken, 0, sizeof(*pToken));

	/* 已出现的字段若类型错误或复制失败，整份响应必须失败。 */
	if(!xoauth2__copy_token_field(p, "access_token", &pToken->AccessToken, false) ||
		!xoauth2__copy_token_field(p, "refresh_token", &pToken->RefreshToken, false) ||
		!xoauth2__copy_token_field(p, "id_token", &pToken->IdToken, false) ||
		!xoauth2__copy_token_field(p, "openid", &pToken->OpenId, false) ||
		!xoauth2__copy_token_field(p, "token_type", &pToken->TokenType, true) ||
		!xoauth2__copy_token_field(p, "scope", &pToken->Scope, false))
	{
		xrtValueRelease(p);
		xoauth2TokenFree(pToken);
		return NULL;
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
	if((pToken->ExpiresIn > 0 &&
		pToken->ObtainedAt > INT64_MAX - pToken->ExpiresIn) ||
		(pToken->ExpiresIn < 0 &&
		 pToken->ObtainedAt < INT64_MIN - pToken->ExpiresIn))
	{
		xrtValueRelease(p);
		xoauth2TokenFree(pToken);
		xoauth2__error(XOAUTH2_ERROR_TOKEN_RESPONSE,
			"expires_in overflows expiration timestamp");
		return NULL;
	}
	pToken->ExpiresAt = pToken->ObtainedAt + pToken->ExpiresIn;

	xrtValueRelease(p);
	if ( bWechat && pToken->TokenType == NULL ) {
		pToken->TokenType = (char*)xrtMalloc(sizeof("bearer"));
		if ( pToken->TokenType == NULL ) {
			xoauth2TokenFree(pToken);
			return NULL;
		}
		memcpy(pToken->TokenType, "bearer", sizeof("bearer"));
	}

	if ( pToken->AccessToken == NULL || pToken->AccessToken[0] == 0 ||
		 pToken->TokenType == NULL || pToken->TokenType[0] == 0 ||
		 (bWechat && (pToken->OpenId == NULL || pToken->OpenId[0] == 0)) ) {
		xoauth2__error(XOAUTH2_ERROR_TOKEN_RESPONSE,
			"token response is missing required fields");
		xoauth2TokenFree(pToken);
		return NULL;
	}
	return pToken;
}

xoauth2token* xoauth2__parse_token_response(const char* sJson, size_t iSize)
{
	return xoauth2__parse_token_response_mode(sJson, iSize, false);
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
		size_t nKey = strlen(keys[i]);
		size_t nValue = strlen(sEnc);
		if(nNeed > SIZE_MAX - 2u ||
			nKey > SIZE_MAX - nNeed - 2u ||
			nValue > SIZE_MAX - nNeed - nKey - 2u)
		{
			xrtFree(sEnc);
			xoauth2__error(XOAUTH2_ERROR_ARGUMENT, "token form too long");
			return NULL;
		}
		nNeed += nKey + nValue + 2u;  /* '=' + '&'/'\0' */
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
	     pClient->Config.RedirectUri == NULL ||
	     (pClient->Wechat && pClient->Config.ClientSecret == NULL) ) {
		xoauth2__error(XOAUTH2_ERROR_ARGUMENT, "token request: not configured");
		return NULL;
	}

	/* BASIC 风格：凭据走 Authorization 头，body 不带 client_id/secret */
	bool bBasic = pClient->Config.AuthStyle == XOAUTH2_AUTH_BASIC;

	const char* keys[8];
	const char* values[8];
	int nFields = 0;
	if ( pClient->Wechat ) {
		keys[nFields] = "appid"; values[nFields++] = pClient->Config.ClientId;
		keys[nFields] = "secret"; values[nFields++] = pClient->Config.ClientSecret;
		keys[nFields] = "code"; values[nFields++] = sCode;
		keys[nFields] = "grant_type"; values[nFields++] = "authorization_code";
		return build_form(keys, values, nFields);
	}
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

	return build_form(keys, values, nFields);
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
	if ( pClient->Wechat ) {
		keys[nFields] = "appid"; values[nFields++] = pClient->Config.ClientId;
		keys[nFields] = "grant_type"; values[nFields++] = "refresh_token";
		keys[nFields] = "refresh_token"; values[nFields++] = sRefreshToken;
		return build_form(keys, values, nFields);
	}
	keys[nFields] = "grant_type"; values[nFields++] = "refresh_token";
	keys[nFields] = "refresh_token"; values[nFields++] = sRefreshToken;
	if ( !bBasic ) {
		keys[nFields] = "client_id"; values[nFields++] = pClient->Config.ClientId;
		if ( pClient->Config.ClientSecret != NULL ) {
			keys[nFields] = "client_secret";
			values[nFields++] = pClient->Config.ClientSecret;
		}
	}

	return build_form(keys, values, nFields);
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
	xrtSecureZero(pClient->sState, sizeof(pClient->sState));
	return true;
}

/* verifier 焚毁：请求构造完成后一次性消费（RFC 7636 防重放） */
static void verifier_burn(xoauth2client* pClient)
{
	xrtSecureZero(pClient->sVerifier, sizeof(pClient->sVerifier));
	xrtSecureZero(pClient->sChallenge, sizeof(pClient->sChallenge));
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
	const char* sBody, const char* sAuth, bool bRefresh)
{
	xoauth2token* pToken = NULL;
	char* sResp = NULL;
	char* sRequestUrl = NULL;
	const char* sUrl = pClient->Config.TokenUrl;
	const char* sMethod = "POST";
	const char* sRequestBody = sBody;
	int iStatus = 0;

	if ( pClient->Config.Http == NULL ) {
		xoauth2__error(XOAUTH2_ERROR_NETWORK, "no transport configured");
		return NULL;
	}
	if ( pClient->Wechat ) {
		static const char sWechatRefresh[] =
			"https://api.weixin.qq.com/sns/oauth2/refresh_token";
		size_t iBase;
		size_t iQuery = strlen(sBody);
		sUrl = bRefresh ? sWechatRefresh : pClient->Config.TokenUrl;
		iBase = strlen(sUrl);
		if ( iQuery > SIZE_MAX - 2u ||
			iBase > SIZE_MAX - iQuery - 2u ) {
			xoauth2__error(XOAUTH2_ERROR_ARGUMENT,
				"WeChat token URL too long");
			return NULL;
		}
		sRequestUrl = (char*)xrtMalloc(iBase + iQuery + 2u);
		if ( sRequestUrl == NULL ) return NULL;
		memcpy(sRequestUrl, sUrl, iBase);
		sRequestUrl[iBase] = '?';
		memcpy(sRequestUrl + iBase + 1u, sBody, iQuery + 1u);
		sUrl = sRequestUrl;
		sMethod = "GET";
		sRequestBody = NULL;
	}
	/* A silent failed callback must not inherit a caller's stale error. */
	xrtClearError();
	if ( !pClient->Config.Http(sMethod, sUrl, sRequestBody, sAuth,
	                           &sResp, &iStatus, pClient->Config.HttpContext) ) {
		xerror* pError = xrtErrorRef(xrtGetError());
		xrtFree(sRequestUrl);
		xrtFree(sResp);
		if ( pError != NULL ) xrtSetErrorTake(pError);
		else xoauth2__error(XOAUTH2_ERROR_NETWORK, "transport failed");
		return NULL;
	}
	xrtFree(sRequestUrl);
	if ( iStatus >= 200 && iStatus < 300 ) {
		pToken = xoauth2__parse_token_response_mode(
			sResp ? sResp : "", sResp ? strlen(sResp) : 0,
			pClient->Wechat);
	}
	else if ( sResp != NULL && sResp[0] != 0 ) {
		/* 非 2xx：能提取 provider error 就报 DENIED。
		 * 响应体可能同时含完整令牌结构（恶意端点）——探测结果必须释放 */
		xoauth2token* pProbe;
		xrtClearError();
		pProbe = xoauth2__parse_token_response_mode(
			sResp, strlen(sResp), pClient->Wechat);
		if ( pProbe != NULL )
			xoauth2TokenFree(pProbe);
		if ( xoauth2LastError() != XOAUTH2_ERROR_TOKEN_DENIED &&
			xrtErrorKind(xrtGetError()) != XERR_MEMORY )
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
	if ( sBody == NULL ) {
		verifier_burn(pClient);
		return NULL;
	}
	char* sAuth = xoauth2__build_auth_header(pClient);
	verifier_burn(pClient);
	if(pClient->Config.AuthStyle == XOAUTH2_AUTH_BASIC && sAuth == NULL)
	{
		xrtFree(sBody);
		return NULL;
	}

	xoauth2token* pToken = exchange(pClient, sBody, sAuth, false);
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
	if(pClient->Config.AuthStyle == XOAUTH2_AUTH_BASIC && sAuth == NULL)
	{
		xrtFree(sBody);
		return NULL;
	}

	xoauth2token* pToken = exchange(pClient, sBody, sAuth, true);
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
	xrtClearError();
	if ( !pClient->Config.Http("GET", sUrl, NULL, sAuthHeader,
	                           &sResp, &iStatus, pClient->Config.HttpContext) ) {
		xerror* pError = xrtErrorRef(xrtGetError());
		xrtFree(sResp);
		if ( pError != NULL ) xrtSetErrorTake(pError);
		else xoauth2__error(XOAUTH2_ERROR_NETWORK, "transport failed");
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
	if ( pClient->Wechat ) {
		xoauth2__error(XOAUTH2_ERROR_ARGUMENT,
			"WeChat userinfo requires the token openid");
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
	if ( sBearer == NULL ) return NULL;
	snprintf(sBearer, strlen(sAccessToken) + 8, "Bearer %s", sAccessToken);
	sBody = xoauth2HttpGet(pClient, pClient->Config.UserInfoUrl,
	                       sBearer, &iStatus);
	xrtFree(sBearer);
	if ( sBody == NULL ) return NULL;

	pClaims = xrtJsonParse(xrtStrView(sBody));
	xrtFree(sBody);
	if ( pClaims == NULL ) {
		if ( xrtErrorKind(xrtGetError()) != XERR_MEMORY )
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

xvalue* xoauth2GetWechatUserInfo(xoauth2client* pClient,
	const xoauth2token* pToken)
{
	char* sAccess = NULL;
	char* sOpenId = NULL;
	char* sUrl = NULL;
	char* sBody = NULL;
	xvalue* pClaims = NULL;
	int iStatus = 0;
	size_t nCap, nExtra;
	int nWritten;
	if ( pClient == NULL || !pClient->Wechat ||
		pClient->Config.UserInfoUrl == NULL || pToken == NULL ||
		pToken->AccessToken == NULL || pToken->AccessToken[0] == 0 ||
		pToken->OpenId == NULL || pToken->OpenId[0] == 0 ) {
		xoauth2__error(XOAUTH2_ERROR_ARGUMENT,
			"WeChat userinfo requires a token with access_token and openid");
		return NULL;
	}
	sAccess = xoauth2UrlEncode(pToken->AccessToken);
	sOpenId = xoauth2UrlEncode(pToken->OpenId);
	if ( sAccess == NULL || sOpenId == NULL ) goto Done;
	nCap = strlen(pClient->Config.UserInfoUrl);
	nExtra = sizeof("?access_token=&openid=");
	if ( strlen(sAccess) > SIZE_MAX - nExtra ) {
		xoauth2__error(XOAUTH2_ERROR_ARGUMENT,
			"WeChat userinfo URL too long");
		goto Done;
	}
	nExtra += strlen(sAccess);
	if ( strlen(sOpenId) > SIZE_MAX - nExtra ) {
		xoauth2__error(XOAUTH2_ERROR_ARGUMENT,
			"WeChat userinfo URL too long");
		goto Done;
	}
	nExtra += strlen(sOpenId);
	if ( nCap > SIZE_MAX - nExtra ) {
		xoauth2__error(XOAUTH2_ERROR_ARGUMENT,
			"WeChat userinfo URL too long");
		goto Done;
	}
	nCap += nExtra;
	sUrl = (char*)xrtMalloc(nCap);
	if ( sUrl == NULL ) goto Done;
	nWritten = snprintf(sUrl, nCap, "%s?access_token=%s&openid=%s",
		pClient->Config.UserInfoUrl, sAccess, sOpenId);
	if ( nWritten < 0 || (size_t)nWritten >= nCap )
		goto Done;
	sBody = xoauth2HttpGet(pClient, sUrl, NULL, &iStatus);
	if ( sBody == NULL ) goto Done;
	pClaims = xrtJsonParse(xrtStrView(sBody));
	if ( pClaims == NULL ) {
		if ( xrtErrorKind(xrtGetError()) != XERR_MEMORY )
			xoauth2__error(XOAUTH2_ERROR_TOKEN_RESPONSE,
				"invalid WeChat userinfo response");
	}
	else if ( !xrtValueIs(pClaims, XVALUE_OBJECT) ) {
		xrtValueRelease(pClaims);
		pClaims = NULL;
		xoauth2__error(XOAUTH2_ERROR_TOKEN_RESPONSE,
			"invalid WeChat userinfo response");
	}
	else if ( xrtValueObjectHas(pClaims, xrtStrView("errcode")) ) {
		xrtValueRelease(pClaims);
		pClaims = NULL;
		xoauth2__error(XOAUTH2_ERROR_TOKEN_DENIED,
			"WeChat userinfo was denied");
	}
Done:
	xrtFree(sAccess);
	xrtFree(sOpenId);
	xrtFree(sUrl);
	xrtFree(sBody);
	return pClaims;
}
