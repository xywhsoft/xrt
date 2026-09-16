/*
	xjwt API 手写体验示例：一个 Web 服务的完整身份认证中间件。

	场景覆盖（真实工程里会遇到的全部路径）：
	  1. 登录：校验用户名密码 → 签发 HS256 令牌（iss/aud/sub/角色 claims）
	  2. 中间件：Bearer 解析 → 验签 + iss/aud + 过期 → 取角色做权限判断
	  3. RS256 密钥对：启动时从文件加载，签发访问令牌
	  4. 高频验证：xjwtKeyParse 公钥缓存（避免每次 PEM 解析）
	  5. 第三方 IdP：JWKS 验证 id_token，密钥轮换失败自动刷新重试
	  6. 错误诊断：区分过期 / 签名错误 / 未到期，返回不同 HTTP 状态码

	编译：
	  gcc -std=c11 -I.. -I../../single -o auth_middleware examples/auth_middleware.c \
	      xjwt.c -lws2_32 -lbcrypt -ladvapi32 -liphlpapi
*/
#define XRT_MODULE_ALL
#define XRT_IMPLEMENTATION
#include "../xjwt.c"
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

/* ================================================================== */
/* 1. 登录处理器：签发令牌                                              */
/* ================================================================== */
static char* handle_login(const char* user, const char* role)
{
	/* 组装业务 claims —— 全程 xrt value API */
	xvalue* claims = xrtValueObject();
	xrtValueObjectSetNew(claims, xrtStrView("role"),
		xrtValueString(xrtStrView(role)));
	xrtValueObjectSetNew(claims, xrtStrView("pref"),
		xrtValueString(xrtStrView("dark-mode")));

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
	return xjwtSign(&cfg, claims);
}

/* ================================================================== */
/* 2. 认证中间件：每请求调用                                            */
/* ================================================================== */
typedef enum { AUTH_OK, AUTH_EXPIRED, AUTH_BAD_TOKEN, AUTH_NO_TOKEN } authresult;

static authresult auth_middleware(const char* authHeader,
                                  char* outUser, size_t userCap,
                                  char* outRole, size_t roleCap)
{
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

	/* 取业务 claims：一行一个 */
	xjwtClaimString(claims, "sub", outUser, userCap);
	xjwtClaimString(claims, "role", outRole, roleCap);
	xrtValueRelease(claims);
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
	xrtValueObjectSetNew(claims, xrtStrView("scope"),
		xrtValueString(xrtStrView("profile orders")));
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
static void refresh_idp_jwks(void)
{
	/* 真实场景：xhttp GET IdP 的 /.well-known/jwks.json；演示用夹具 */
	if ( G_IDP_JWKS ) xjwtJwksFree(G_IDP_JWKS);
	G_IDP_JWKS = xjwtJwksParse(FIX_JWKS);
}

static bool verify_idp_token(const char* idToken)
{
	xjwtcheck check;
	xjwtCheckInit(&check);
	check.Issuer = "https://idp.example.com";

	xvalue* claims = xjwtVerifyJwks(idToken, G_IDP_JWKS, &check);
	if ( claims != NULL ) { xrtValueRelease(claims); return true; }

	/* 轮换场景：kid 找不到 → 刷新 JWKS 重试一次 */
	if ( xjwtLastError() == XJWT_ERROR_KEY_NOT_FOUND ) {
		refresh_idp_jwks();
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
	/* 启动：装载 RS 公钥缓存 + IdP JWKS */
	G_RSA_KEY = xjwtKeyParse(FIX_RSA_PUBLIC);
	refresh_idp_jwks();

	/* --- 1+2. 登录 → 中间件 --- */
	char* token = handle_login("alice", "admin");
	if ( token == NULL ) { printf("login: sign failed\n"); return 1; }
	char aUser[64] = {0}, aRole[16] = {0};
	char aHeader[600];
	snprintf(aHeader, sizeof(aHeader), "Bearer %s", token);
	xrtFree(token);
	switch ( auth_middleware(aHeader, aUser, sizeof(aUser), aRole, sizeof(aRole)) ) {
	case AUTH_OK:
		printf("1+2. login+middleware: user=%s role=%s\n", aUser, aRole);
		break;
	case AUTH_EXPIRED: printf("1+2. 401 expired\n"); break;
	case AUTH_BAD_TOKEN: printf("1+2. 401 bad token\n"); break;
	default: printf("1+2. 401 no token\n"); break;
	}

	/* 错误路径演示：过期令牌（同样带 Bearer 前缀） */
	{
		xvalue* c = xrtValueObject();
		xjwtconfig cfg; xjwtConfigInit(&cfg);
		cfg.Alg = XJWT_ALG_HS256; cfg.KeyPem = G_HS_SECRET;
		cfg.Issuer = G_ISSUER; cfg.Audience = G_AUDIENCE;
		cfg.ExpireSeconds = -60;
		char* t = xjwtSign(&cfg, c);   /* exp = now - 60 */
		xrtValueRelease(c);
		snprintf(aHeader, sizeof(aHeader), "Bearer %s", t ? t : "");
		xrtFree(t);
		authresult r = auth_middleware(aHeader, aUser, sizeof(aUser),
			aRole, sizeof(aRole));
		printf("    expired token -> %s\n",
			r == AUTH_EXPIRED ? "401 expired (错误码区分正确)" : "其他错误");
	}

	/* --- 3+4. RS256 签发 + 缓存验证 --- */
	char* access = issue_access_token("alice");
	printf("3+4. RS256 access token: %s\n",
		access && verify_access_token(access) ? "验证通过" : "失败");
	xrtFree(access);

	/* --- 5. IdP JWKS + 轮换重试 --- */
	printf("5.  IdP token via JWKS: %s\n",
		verify_idp_token(FIX_IDP_TOKEN) ? "验证通过" : "失败");

	/* 清理 */
	xjwtKeyFree(G_RSA_KEY);
	xjwtJwksFree(G_IDP_JWKS);
	return 0;
}
