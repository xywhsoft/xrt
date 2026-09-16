/* xoauth2 核心：PKCE + state + URL 构造 + provider 预设 + 错误便捷读取。 */
#include "xoauth2_internal.h"

/* ------------------------------------------------------------------ */
/* 错误                                                                 */
/* ------------------------------------------------------------------ */
void xoauth2__error(int iCode, const char* sMessage)
{
	xrtSetErrorInfo(XERR_STATE, "xrt.oauth2", iCode, sMessage);
}

int xoauth2LastError(void)
{
	const xerror* pErr = xrtGetError();
	if ( pErr == NULL ) return 0;
	cstr sDomain = xrtErrorDomain(pErr);
	if ( sDomain == NULL || strcmp(sDomain, "xrt.oauth2") != 0 ) return 0;
	return xrtErrorCode(pErr);
}

/* ------------------------------------------------------------------ */
/* Base64URL（PKCE challenge 需要）                                     */
/* ------------------------------------------------------------------ */
static const char s_B64url[] =
	"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_";

char* xoauth2__base64url(const void* pData, size_t iSize)
{
	const unsigned char* p = (const unsigned char*)pData;
	size_t iOut = (iSize + 2) / 3 * 4;
	if ( iSize % 3 == 1 ) iOut -= 2;
	else if ( iSize % 3 == 2 ) iOut -= 1;
	char* s = (char*)xrtMalloc(iOut + 1);
	if ( s == NULL ) return NULL;
	size_t j = 0;
	for ( size_t i = 0; i < iSize; i += 3 ) {
		uint32_t v = (uint32_t)p[i] << 16;
		if ( i+1 < iSize ) v |= (uint32_t)p[i+1] << 8;
		if ( i+2 < iSize ) v |= p[i+2];
		s[j++] = s_B64url[(v>>18)&63];
		s[j++] = s_B64url[(v>>12)&63];
		if ( i+1 < iSize ) s[j++] = s_B64url[(v>>6)&63];
		if ( i+2 < iSize ) s[j++] = s_B64url[v&63];
	}
	s[j] = 0;
	return s;
}

/* ------------------------------------------------------------------ */
/* PKCE (RFC 7636)                                                      */
/* ------------------------------------------------------------------ */

bool xoauth2PkceGenerate(char* sVerifier, size_t iVerifierSize,
                         char* sChallenge, size_t iChallengeSize)
{
	if ( sVerifier == NULL || iVerifierSize < 64 ||
	     sChallenge == NULL || iChallengeSize < 64 ) {
		xoauth2__error(XOAUTH2_ERROR_ARGUMENT, "PKCE buffers too small");
		return false;
	}
	/* code_verifier = 32 字节随机 → Base64URL（43 字符） */
	unsigned char aRandom[32];
	if ( !xrtSecureRandom(aRandom, 32) ) {
		xoauth2__error(XOAUTH2_ERROR_ARGUMENT, "secure random failed");
		return false;
	}
	char* sB64 = xoauth2__base64url(aRandom, 32);
	if ( sB64 == NULL ) return false;
	strncpy(sVerifier, sB64, iVerifierSize - 1);
	sVerifier[iVerifierSize - 1] = 0;
	xrtFree(sB64);

	/* code_challenge = BASE64URL(SHA256(code_verifier)) */
	unsigned char aDigest[32];
	if ( !xrtSha256(sVerifier, strlen(sVerifier), aDigest) ) return false;
	sB64 = xoauth2__base64url(aDigest, 32);
	if ( sB64 == NULL ) return false;
	strncpy(sChallenge, sB64, iChallengeSize - 1);
	sChallenge[iChallengeSize - 1] = 0;
	xrtFree(sB64);
	return true;
}

/* ------------------------------------------------------------------ */
/* State                                                                */
/* ------------------------------------------------------------------ */

bool xoauth2StateGenerate(char* sState, size_t iStateSize)
{
	if ( sState == NULL || iStateSize < 32 ) {
		xoauth2__error(XOAUTH2_ERROR_ARGUMENT, "state buffer too small");
		return false;
	}
	unsigned char aRandom[24];
	if ( !xrtSecureRandom(aRandom, 24) ) return false;
	char* sB64 = xoauth2__base64url(aRandom, 24);
	if ( sB64 == NULL ) return false;
	strncpy(sState, sB64, iStateSize - 1);
	sState[iStateSize - 1] = 0;
	xrtFree(sB64);
	return true;
}

/* ------------------------------------------------------------------ */
/* URL 编码                                                             */
/* ------------------------------------------------------------------ */

bool xoauth2__url_safe(char c)
{
	return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
	       (c >= '0' && c <= '9') || c == '-' || c == '.' ||
	       c == '_' || c == '~';
}

char* xoauth2UrlEncode(const char* sText)
{
	if ( sText == NULL ) return NULL;
	size_t n = strlen(sText);
	/* 最坏情况：每字符 3 字节 */
	char* sOut = (char*)xrtMalloc(n * 3 + 1);
	if ( sOut == NULL ) return NULL;
	size_t j = 0;
	for ( size_t i = 0; i < n; i++ ) {
		if ( xoauth2__url_safe(sText[i]) ) {
			sOut[j++] = sText[i];
		} else {
			j += snprintf(sOut + j, 4, "%%%02X", (unsigned char)sText[i]);
		}
	}
	sOut[j] = 0;
	return sOut;
}

/* ------------------------------------------------------------------ */
/* 授权 URL 构造                                                        */
/* ------------------------------------------------------------------ */

char* xoauth2BeginLogin(xoauth2client* pClient)
{
	if ( pClient == NULL || pClient->Config.ClientId == NULL ||
	     pClient->Config.AuthorizeUrl == NULL ) {
		xoauth2__error(XOAUTH2_ERROR_ARGUMENT, "xoauth2BeginLogin: not configured");
		return NULL;
	}

	/* 生成 state */
	if ( !xoauth2StateGenerate(pClient->sState, sizeof(pClient->sState)) ) {
		return NULL;
	}

	/* 生成 PKCE（尊重预设：UsePkce=false 的 provider 不带 PKCE 参数） */
	if ( pClient->Config.UsePkce ) {
		if ( !xoauth2PkceGenerate(pClient->sVerifier, sizeof(pClient->sVerifier),
		                          pClient->sChallenge, sizeof(pClient->sChallenge)) ) {
			return NULL;
		}
	}
	else {
		pClient->sVerifier[0] = 0;
		pClient->sChallenge[0] = 0;
	}

	/* OIDC nonce（provider 原样回显进 id_token，NonceConsume 常时比对焚毁） */
	if ( pClient->Config.UseNonce ) {
		if ( !xoauth2StateGenerate(pClient->sNonce, sizeof(pClient->sNonce)) )
			return NULL;
	}
	else {
		pClient->sNonce[0] = 0;
	}

	/* 拼授权 URL：动态算缓冲区（各编码后长度 + 固定参数开销） */
	size_t nBase = strlen(pClient->Config.AuthorizeUrl) +
	               strlen(pClient->Config.ClientId) +
	               strlen(pClient->sState) + 128;
	if ( pClient->Config.RedirectUri )
		nBase += strlen(pClient->Config.RedirectUri) * 3;
	if ( pClient->Config.Scope )
		nBase += strlen(pClient->Config.Scope) * 3;
	if ( pClient->Config.UsePkce )
		nBase += 128;
	size_t nCap = nBase + 64;
	char* sUrl = (char*)xrtMalloc(nCap);
	if ( sUrl == NULL ) return NULL;

	char* sRedir = xoauth2UrlEncode(pClient->Config.RedirectUri);
	char* sScope = pClient->Config.Scope ?
		xoauth2UrlEncode(pClient->Config.Scope) : NULL;

	int n = snprintf(sUrl, nCap, "%s?response_type=code&client_id=%s&redirect_uri=%s&state=%s",
		pClient->Config.AuthorizeUrl,
		pClient->Config.ClientId,
		sRedir ? sRedir : "",
		pClient->sState);

	if ( sScope != NULL && n > 0 && (size_t)n < nCap ) {
		n += snprintf(sUrl + n, nCap - n, "&scope=%s", sScope);
	}
	if ( pClient->Config.UsePkce && n > 0 && (size_t)n < nCap ) {
		n += snprintf(sUrl + n, nCap - n,
			"&code_challenge=%s&code_challenge_method=S256",
			pClient->sChallenge);
	}
	if ( pClient->Config.UseNonce && n > 0 && (size_t)n < nCap ) {
		n += snprintf(sUrl + n, nCap - n, "&nonce=%s", pClient->sNonce);
	}

	if ( sRedir ) xrtFree(sRedir);
	if ( sScope ) xrtFree(sScope);

	if ( n <= 0 || (size_t)n >= nCap ) {
		xrtFree(sUrl);
		xoauth2__error(XOAUTH2_ERROR_ARGUMENT, "auth URL too long");
		return NULL;
	}
	return sUrl;
}

/* ------------------------------------------------------------------ */
/* 预设持有的端点 URL                                                    */
/* ------------------------------------------------------------------ */

void xoauth2__client_clear_owned(xoauth2client* pClient)
{
	if ( pClient == NULL ) return;
	if ( pClient->pOwnedAuthUrl ) xrtFree(pClient->pOwnedAuthUrl);
	if ( pClient->pOwnedTokenUrl ) xrtFree(pClient->pOwnedTokenUrl);
	if ( pClient->pOwnedIssuerUrl ) xrtFree(pClient->pOwnedIssuerUrl);
	pClient->pOwnedAuthUrl = NULL;
	pClient->pOwnedTokenUrl = NULL;
	pClient->pOwnedIssuerUrl = NULL;
}

/* ------------------------------------------------------------------ */
/* Provider 预设                                                        */
/* ------------------------------------------------------------------ */

void xoauth2ConfigInit(xoauth2config* pConfig)
{
	if ( pConfig == NULL ) return;
	memset(pConfig, 0, sizeof(*pConfig));
	pConfig->UsePkce = true;
	pConfig->AuthStyle = XOAUTH2_AUTH_AUTO;
}

void xoauth2UseGithub(xoauth2client* pClient, const char* id, const char* secret, const char* redirect)
{
	if ( pClient == NULL ) return;
	xoauth2__client_clear_owned(pClient);
	memset(pClient, 0, sizeof(*pClient));
	xoauth2ConfigInit(&pClient->Config);
	pClient->Config.AuthorizeUrl = "https://github.com/login/oauth/authorize";
	pClient->Config.TokenUrl = "https://github.com/login/oauth/access_token";
	pClient->Config.UserInfoUrl = "https://api.github.com/user";
	pClient->Config.ClientId = id;
	pClient->Config.ClientSecret = secret;
	pClient->Config.RedirectUri = redirect;
	pClient->Config.Scope = "read:user user:email";
	pClient->Config.AuthStyle = XOAUTH2_AUTH_BODY;
}

void xoauth2UseGoogle(xoauth2client* pClient, const char* id, const char* secret, const char* redirect)
{
	if ( pClient == NULL ) return;
	xoauth2__client_clear_owned(pClient);
	memset(pClient, 0, sizeof(*pClient));
	xoauth2ConfigInit(&pClient->Config);
	pClient->Config.AuthorizeUrl = "https://accounts.google.com/o/oauth2/v2/auth";
	pClient->Config.TokenUrl = "https://oauth2.googleapis.com/token";
	pClient->Config.UserInfoUrl = "https://openidconnect.googleapis.com/v1/userinfo";
	pClient->Config.ClientId = id;
	pClient->Config.ClientSecret = secret;
	pClient->Config.RedirectUri = redirect;
	/* Google 授权端点历史上不支持 PKCE（2024 起支持 S256，保守关闭） */
	pClient->Config.UsePkce = false;
	pClient->Config.Scope = "openid email profile";
	pClient->Config.AuthStyle = XOAUTH2_AUTH_BASIC;
	/* OIDC：issuer/JWKS 由应用层喂给 xjwt（数据耦合） */
	pClient->Config.Issuer = "https://accounts.google.com";
	pClient->Config.JwksUrl = "https://www.googleapis.com/oauth2/v3/certs";
	pClient->Config.UseNonce = true;
}

void xoauth2UseWechat(xoauth2client* pClient, const char* appid, const char* appSecret, const char* redirect)
{
	if ( pClient == NULL ) return;
	xoauth2__client_clear_owned(pClient);
	memset(pClient, 0, sizeof(*pClient));
	xoauth2ConfigInit(&pClient->Config);
	pClient->Config.AuthorizeUrl = "https://open.weixin.qq.com/connect/qrconnect";
	pClient->Config.TokenUrl = "https://api.weixin.qq.com/sns/oauth2/access_token";
	pClient->Config.ClientId = appid;        /* WeChat 用 appid */
	pClient->Config.ClientSecret = appSecret;
	pClient->Config.RedirectUri = redirect;
	pClient->Config.Scope = "snsapi_login";
	pClient->Config.UsePkce = false;
	pClient->Config.AuthStyle = XOAUTH2_AUTH_BODY;
}

void xoauth2UseMicrosoft(xoauth2client* pClient, const char* id, const char* secret,
                         const char* redirect, const char* tenant)
{
	if ( pClient == NULL ) return;
	xoauth2__client_clear_owned(pClient);
	memset(pClient, 0, sizeof(*pClient));
	xoauth2ConfigInit(&pClient->Config);
	/* 按 tenant 动态生成端点；所有权归客户端（clear_owned/ClientUnit 释放） */
	const char* sTenant = tenant ? tenant : "common";
	if ( strlen(sTenant) > 256 ) {
		/* 预设是 void 返回：置错误并把端点留空，BeginLogin 会拒绝 */
		xoauth2__error(XOAUTH2_ERROR_ARGUMENT, "tenant too long (max 256)");
		pClient->Config.ClientId = id;
		pClient->Config.ClientSecret = secret;
		pClient->Config.RedirectUri = redirect;
		return;
	}
	char* sAuth = (char*)xrtMalloc(256);
	char* sToken = (char*)xrtMalloc(256);
	char* sIssuer = (char*)xrtMalloc(256);
	if ( sAuth == NULL || sToken == NULL || sIssuer == NULL ) {
		xrtFree(sAuth); xrtFree(sToken); xrtFree(sIssuer);
		return;
	}
	snprintf(sAuth, 256,
		"https://login.microsoftonline.com/%s/oauth2/v2.0/authorize", sTenant);
	snprintf(sToken, 256,
		"https://login.microsoftonline.com/%s/oauth2/v2.0/token", sTenant);
	snprintf(sIssuer, 256,
		"https://login.microsoftonline.com/%s/v2.0", sTenant);
	pClient->pOwnedAuthUrl = sAuth;
	pClient->pOwnedTokenUrl = sToken;
	pClient->pOwnedIssuerUrl = sIssuer;
	pClient->Config.AuthorizeUrl = sAuth;
	pClient->Config.TokenUrl = sToken;
	pClient->Config.Issuer = sIssuer;
	pClient->Config.JwksUrl = "https://login.microsoftonline.com/common/discovery/v2.0/keys";
	pClient->Config.ClientId = id;
	pClient->Config.ClientSecret = secret;
	pClient->Config.RedirectUri = redirect;
	pClient->Config.Scope = "openid profile email";
	pClient->Config.UsePkce = true;
	pClient->Config.AuthStyle = XOAUTH2_AUTH_BASIC;
	pClient->Config.UseNonce = true;
}

void xoauth2UseCustom(xoauth2client* pClient, const xoauth2config* pConfig)
{
	if ( pClient == NULL || pConfig == NULL ) return;
	xoauth2__client_clear_owned(pClient);
	memset(pClient, 0, sizeof(*pClient));
	pClient->Config = *pConfig;
}

void xoauth2ClientUnit(xoauth2client* pClient)
{
	if ( pClient == NULL ) return;
	/* 敏感会话状态先显式清零（memset 兜底） */
	memset(pClient->sState, 0, sizeof(pClient->sState));
	memset(pClient->sVerifier, 0, sizeof(pClient->sVerifier));
	memset(pClient->sChallenge, 0, sizeof(pClient->sChallenge));
	memset(pClient->sNonce, 0, sizeof(pClient->sNonce));
	xoauth2__client_clear_owned(pClient);
	/* 端点指针已失效；配置一并清零，调用方须重新预设 */
	memset(&pClient->Config, 0, sizeof(pClient->Config));
}

void xoauth2TokenFree(xoauth2token* pToken)
{
	if ( pToken == NULL ) return;
	/* 令牌是敏感凭据：释放前清零，避免明文残留堆内存 */
	if ( pToken->AccessToken ) {
		memset(pToken->AccessToken, 0, strlen(pToken->AccessToken));
		xrtFree(pToken->AccessToken);
	}
	if ( pToken->RefreshToken ) {
		memset(pToken->RefreshToken, 0, strlen(pToken->RefreshToken));
		xrtFree(pToken->RefreshToken);
	}
	if ( pToken->IdToken ) {
		memset(pToken->IdToken, 0, strlen(pToken->IdToken));
		xrtFree(pToken->IdToken);
	}
	if ( pToken->TokenType ) xrtFree(pToken->TokenType);
	if ( pToken->Scope ) xrtFree(pToken->Scope);
	memset(pToken, 0, sizeof(*pToken));
	xrtFree(pToken);
}

bool xoauth2TokenExpiring(const xoauth2token* pToken, int leewaySeconds)
{
	if ( pToken == NULL ) return true;
	/* provider 未告知有效期（如 GitHub）：未知 ≠ 即将过期。
	 * 若当"过期"处理，无 refresh_token 的令牌会陷入每请求刷新
	 * 且刷新必然 DENIED 的死循环 */
	if ( pToken->ExpiresIn == 0 ) return false;
	/* 剩余时间 = ExpiresAt - 当前；ExpiresIn 是签发时的总有效期，
	 * 不反映流逝，必须用获取时刻换算 */
	int64_t now = (int64_t)(xrtNow() / 1000000);
	return pToken->ExpiresAt - now <= (int64_t)leewaySeconds;
}

/* ------------------------------------------------------------------ */
/* OIDC nonce 消费（常时比对 + 一次性焚毁）                              */
/* ------------------------------------------------------------------ */
bool xoauth2NonceConsume(xoauth2client* pClient, const char* sNonce)
{
	if ( pClient == NULL || sNonce == NULL ) {
		xoauth2__error(XOAUTH2_ERROR_ARGUMENT, "xoauth2NonceConsume: null argument");
		return false;
	}
	size_t nMine = strlen(pClient->sNonce);
	size_t nTheirs = strlen(sNonce);
	if ( nMine == 0 || nMine != nTheirs ||
	     !xrtConstTimeEqual(pClient->sNonce, sNonce, nMine) ) {
		xoauth2__error(XOAUTH2_ERROR_NONCE_MISMATCH,
			"nonce mismatch (possible replay)");
		return false;
	}
	/* 匹配即焚毁：nonce 与 state 同为一次性会话凭证 */
	memset(pClient->sNonce, 0, sizeof(pClient->sNonce));
	return true;
}
