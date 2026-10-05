/*
	xjwt API 手写体验示例：一个 Web 服务的完整身份认证中间件。

	场景覆盖（真实工程里会遇到的全部路径）：
	  1. 登录：校验用户名密码 → 签发 HS256 令牌（iss/aud/sub/角色 claims）
	  2. 中间件：Bearer 解析 → 验签 + iss/aud + 过期 → 取角色做权限判断
	  3. RS256 密钥对：启动时从文件加载，签发访问令牌
	  4. 高频验证：xjwtKeyParse 公钥缓存（避免每次 PEM 解析）
	  5. 第三方 IdP：JWKS 验证 id_token，密钥轮换失败自动刷新重试
	  6. 错误诊断：区分过期 / 签名错误 / 未到期，返回不同 HTTP 状态码

 * 构建入口见本库 README；使用根目录 tools/build.py。
*/
#if defined(XJWT_FEATURE_XJWT)
#include <xjwt.h>
#else
#define XRT_IMPLEMENTATION
#include "../tests/support/runtime.h"
#include "../tests/support/implementation.c"
#endif
#include <stdio.h>
#include <string.h>

/* 复用 tests 的 openssl 夹具（python tests/gen_keys.py 生成） */
#include "../tests/test_keys.h"
#define FIX_RSA_PRIVATE K_RSA_PRIV
#define FIX_RSA_PUBLIC  K_RSA_PUB
#define FIX_JWKS        K_JWKS
#define FIX_IDP_TOKEN   K_TOKEN_IDP_OPENSSL

/* ================================================================== */
/* 服务启动时装载的全局状态（真实工程里放在 server context）             */
/* ================================================================== */
static const char* G_HS_SECRET = "server-secret-2026";
static const char* G_ISSUER    = "https://myapp.example.com";
static const char* G_AUDIENCE  = "myapp-web";

static xjwtkey* G_RSA_KEY;        /* 自家 RS256 公钥缓存 */
static xjwtjwks* G_IDP_JWKS;      /* 第三方 IdP 公钥集 */

static bool example_set_string(xvalue* claims, const char* key, const char* text)
{
	return text != NULL && xrtValueObjectSetNew(claims, xrtStrView(key),
		xrtValueString(xrtStrView(text)));
}

/* 身份/权限字段不可截断、为空或包含内嵌 NUL。输出只在验证完成后发布。 */
static bool example_claim_string(const xvalue* claims, const char* key,
                                char* output, size_t capacity)
{
	xstrview value;
	if ( output == NULL || capacity == 0 ||
		!xrtValueGetString(xrtValueObjectGet(claims, xrtStrView(key)), &value) ||
		value.Size == 0 || value.Size >= capacity ||
		memchr(value.Data, 0, value.Size) != NULL ) return false;
	memcpy(output, value.Data, value.Size);
	output[value.Size] = 0;
	return true;
}

/* ================================================================== */
/* 1. 登录处理器：签发令牌                                              */
/* ================================================================== */
static char* handle_login(const char* user, const char* role)
{
	/* 组装业务 claims —— 全程 xrt value API */
	xvalue* claims = xrtValueObject();
	if ( claims == NULL || !example_set_string(claims, "role", role) ||
		!example_set_string(claims, "pref", "dark-mode") ) {
		xrtValueRelease(claims);
		return NULL;
	}

	/* 中间件要校验 iss/aud → 必须用 config 式签发注入；
	 * 一步式 xjwtHs256 不注入 iss/aud，只适合无校验场景 */
	xjwtconfig cfg;
	xjwtConfigInit(&cfg);
	cfg.Alg = XJWT_ALG_HS256;
	cfg.KeyPem = G_HS_SECRET;
	cfg.Issuer = G_ISSUER;
	cfg.Audience = G_AUDIENCE;
	cfg.Subject = user;
	cfg.ExpireSeconds = 3600;
	char* token = xjwtSign(&cfg, claims);
	xrtValueRelease(claims);   /* Sign 借用 claims，调用方仍负责释放。 */
	return token;
}

/* ================================================================== */
/* 2. 认证中间件：每请求调用                                            */
/* ================================================================== */
typedef enum { AUTH_OK, AUTH_EXPIRED, AUTH_BAD_TOKEN, AUTH_NO_TOKEN } authresult;

static authresult auth_middleware(const char* authHeader,
                                  char* outUser, size_t userCap,
                                  char* outRole, size_t roleCap)
{
	if ( outUser != NULL && userCap != 0 ) outUser[0] = 0;
	if ( outRole != NULL && roleCap != 0 ) outRole[0] = 0;
	if ( outUser == NULL || outRole == NULL || userCap == 0 || roleCap == 0 )
		return AUTH_BAD_TOKEN;
	/* Bearer 解析（库不管，自己两行） */
	if ( authHeader == NULL || strncmp(authHeader, "Bearer ", 7) != 0 )
		return AUTH_NO_TOKEN;
	const char* token = authHeader + 7;

	/* 校验参数：中间件固定 iss/aud */
	xjwtcheck check;
	xjwtCheckInit(&check);
	check.Issuer = G_ISSUER;
	check.Audience = G_AUDIENCE;
	check.ClockLeeway = 30;   /* 30 秒时钟容差 */

	xvalue* claims = xjwtVerify(token, G_HS_SECRET, &check);
	if ( claims == NULL ) {
		/* 错误诊断：xjwtLastError 直接给本域错误码，映射到 HTTP 语义 */
		int err = xjwtLastError();
		if ( err == XJWT_ERROR_EXPIRED || err == XJWT_ERROR_NOT_YET )
			return AUTH_EXPIRED;
		return AUTH_BAD_TOKEN;
	}

	bool valid = example_claim_string(claims, "sub", outUser, userCap) &&
		example_claim_string(claims, "role", outRole, roleCap);
	xrtValueRelease(claims);
	if ( !valid ) {
		outUser[0] = outRole[0] = 0;
		return AUTH_BAD_TOKEN;
	}
	return AUTH_OK;
}

/* ================================================================== */
/* 3+4. RS256 签发 + 缓存公钥的高频验证                                 */
/* ================================================================== */
static char* issue_access_token(const char* user)
{
	xjwtconfig cfg;
	xjwtConfigInit(&cfg);
	cfg.Alg = XJWT_ALG_RS256;
	cfg.KeyPem = FIX_RSA_PRIVATE;      /* 启动时从文件读进的 PEM 字符串 */
	cfg.KeyId = "web-2026-09";
	cfg.Issuer = G_ISSUER;
	cfg.Audience = G_AUDIENCE;
	cfg.Subject = user;
	cfg.ExpireSeconds = 900;           /* 访问令牌 15 分钟 */
	cfg.Jti = "jti-4f2a";              /* 演示用；真实场景用随机数 */

	xvalue* claims = xrtValueObject();
	if ( claims == NULL || !example_set_string(claims, "scope", "profile orders") ) {
		xrtValueRelease(claims);
		return NULL;
	}
	char* token = xjwtSign(&cfg, claims);
	xrtValueRelease(claims);
	return token;
}

/* 每请求：缓存公钥验签（无 PEM 解析开销） */
static bool verify_access_token(const char* token)
{
	xjwtcheck check;
	xjwtCheckInit(&check);
	check.Issuer = G_ISSUER;
	check.Audience = G_AUDIENCE;
	xvalue* claims = xjwtVerifyKey(token, G_RSA_KEY, &check);
	if ( claims == NULL ) return false;
	xrtValueRelease(claims);
	return true;
}

/* ================================================================== */
/* 5. 第三方 IdP：JWKS 验证 + 密钥轮换自动刷新                          */
/* ================================================================== */
static bool refresh_idp_jwks(void)
{
	/* 真实场景：xhttp GET IdP 的 /.well-known/jwks.json；演示用夹具 */
	xjwtjwks* replacement = xjwtJwksParse(FIX_JWKS);
	if ( replacement == NULL ) return false;
	xjwtJwksFree(G_IDP_JWKS);
	G_IDP_JWKS = replacement;
	return true;
}

static bool verify_idp_token(const char* idToken)
{
	xjwtcheck check;
	xjwtCheckInit(&check);
	check.Issuer = "https://idp.example.com";
	check.Audience = G_AUDIENCE;

	xvalue* claims = xjwtVerifyJwks(idToken, G_IDP_JWKS, &check);
	if ( claims != NULL ) { xrtValueRelease(claims); return true; }

	/* 轮换场景：kid 找不到 → 刷新 JWKS 重试一次 */
	if ( xjwtLastError() == XJWT_ERROR_KEY_NOT_FOUND ) {
		if ( !refresh_idp_jwks() ) return false;
		claims = xjwtVerifyJwks(idToken, G_IDP_JWKS, &check);
		if ( claims != NULL ) { xrtValueRelease(claims); return true; }
	}
	return false;
}

/* ================================================================== */
/* 演示主流程                                                          */
/* ================================================================== */
int main(void)
{
	int result = 1;
	char* token = NULL;
	char* access = NULL;
	char* expired = NULL;
	xvalue* expiredClaims = NULL;
	const char* stage = "startup keys";
	char aUser[64], aRole[16], aHeader[1024];
	/* 启动：装载 RS 公钥缓存 + IdP JWKS */
	G_RSA_KEY = xjwtKeyParse(FIX_RSA_PUBLIC);
	if ( G_RSA_KEY == NULL || !refresh_idp_jwks() ) goto done;

	/* --- 1+2. 登录 → 中间件 --- */
	stage = "login and middleware";
	token = handle_login("alice", "admin");
	if ( token == NULL ) goto done;
	int length = snprintf(aHeader, sizeof(aHeader), "Bearer %s", token);
	if ( length < 0 || (size_t)length >= sizeof(aHeader) ||
		auth_middleware(aHeader, aUser, sizeof(aUser), aRole, sizeof(aRole)) != AUTH_OK )
		goto done;
	printf("1+2. login+middleware: user=%s role=%s\n", aUser, aRole);

	/* 错误路径演示：过期令牌（同样带 Bearer 前缀） */
	{
		stage = "expired token denial";
		expiredClaims = xrtValueObject();
		if ( expiredClaims == NULL ) goto done;
		xjwtconfig cfg; xjwtConfigInit(&cfg);
		cfg.Alg = XJWT_ALG_HS256; cfg.KeyPem = G_HS_SECRET;
		cfg.Issuer = G_ISSUER; cfg.Audience = G_AUDIENCE;
		cfg.ExpireSeconds = -60;
		expired = xjwtSign(&cfg, expiredClaims);   /* exp = now - 60 */
		if ( expired == NULL ) goto done;
		length = snprintf(aHeader, sizeof(aHeader), "Bearer %s", expired);
		if ( length < 0 || (size_t)length >= sizeof(aHeader) ) goto done;
		authresult r = auth_middleware(aHeader, aUser, sizeof(aUser),
			aRole, sizeof(aRole));
		if ( r != AUTH_EXPIRED ) goto done;
		printf("    expired token -> 401 expired\n");
	}

	/* --- 3+4. RS256 签发 + 缓存验证 --- */
	stage = "RS256 access verification";
	access = issue_access_token("alice");
	if ( access == NULL || !verify_access_token(access) ) goto done;
	printf("3+4. RS256 access token: 验证通过\n");

	/* --- 5. IdP JWKS + 轮换重试 --- */
	stage = "IdP JWKS verification";
	if ( !verify_idp_token(FIX_IDP_TOKEN) ) goto done;
	printf("5.  IdP token via JWKS: 验证通过\n");
	result = 0;

	/* 清理 */
done:
	if ( result != 0 ) fprintf(stderr, "JWT example failed at %s (code=%d)\n", stage, xjwtLastError());
	xrtFree(token);
	xrtFree(access);
	xrtFree(expired);
	xrtValueRelease(expiredClaims);
	xjwtKeyFree(G_RSA_KEY);
	xjwtJwksFree(G_IDP_JWKS);
	G_RSA_KEY = NULL;
	G_IDP_JWKS = NULL;
	return result;
}
