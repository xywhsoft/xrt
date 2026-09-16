# xoauth2

`xoauth2` 是构建在 xrt 核心之上的 OAuth 2.0 (RFC 6749) 客户端扩展库。
支持授权码流程 + PKCE (RFC 7636) + token 刷新 + 常用 provider 预设 +
OIDC 辅助（nonce / JWKS 拉取 / userinfo）。

实现是 `xoauth2.c` 单一编译单元：把它加入构建（包含目录指向本目录），
或单 TU 场景直接 `#include "xoauth2.c"`。依赖的 xrt 模块闭包见
`xoauth2-xrt.h`（含 net/tls/http1 全套，供便捷传输使用）。

## 与 xjwt 的关系：数据耦合，零依赖

`xoauth2` 与 `xjwt` **互相不知道对方的存在**——没有链接依赖、没有
`requires`、各自可独立编译。OIDC 登录（id_token 验签）在**应用层**
组合，边界数据是纯 C 字符串（id_token / JWKS JSON）与 xrt 核心的
`xvalue*`。可运行的组合示例见 `examples/oidc_login.c`，组合正确性由
`tests/test_oidc_compose.c` 回归锁定：

```c
xoauth2token* tok = xoauth2CompleteLogin(&oauth, code, state);

int st = 0;
char* jwksJson = xoauth2HttpGet(&oauth, oauth.Config.JwksUrl, NULL, &st);
xjwtjwks* keys = xjwtJwksParse(jwksJson);          /* 字符串进 */
xjwtcheck ck;  xjwtCheckInit(&ck);
ck.Issuer = oauth.Config.Issuer;                    /* 预设知识，但只是字符串 */
ck.Audience = oauth.Config.ClientId;
xvalue* claims = xjwtVerifyJwks(tok->IdToken, keys, &ck);

char nonce[128];
xjwtClaimString(claims, "nonce", nonce, sizeof nonce);
xoauth2NonceConsume(&oauth, nonce);                 /* 常时比对 + 一次性焚毁 */
```

密钥轮换重试 = 应用层 6 行（`xjwtLastError()==XJWT_ERROR_KEY_NOT_FOUND`
时重拉 JWKS，参见 xjwt 示例）。

## 用法（GitHub 登录）

```c
#include "xoauth2.h"

xoauth2client oauth;
xoauth2UseGithub(&oauth, client_id, client_secret, redirect_uri);

/* 传输注入与预设分离：预设管 provider 知识，传输管宿主环境。
 * 栈版（C 宿主）或堆版（opaque 场景/脚本层）二选一。 */
xoauth2httpxrt* http = xoauth2HttpXrtCreate(NULL, NULL, 0);  /* 堆版 */
oauth.Config.Http = xoauth2HttpXrt;
oauth.Config.HttpContext = http;

/* 登录入口：重定向用户到返回的 URL（自动生成 state + PKCE） */
char* url = xoauth2BeginLogin(&oauth);

/* 回调：用 code + state 换 token。
 * state 校验通过即焚毁会话（一次性，防重放）——失败后须重新 BeginLogin。 */
xoauth2token* tok = xoauth2CompleteLogin(&oauth, code, state);
if ( tok ) {
    /* AccessToken / RefreshToken / ExpiresIn / ExpiresAt / Scope ...
     * ExpiresAt = ObtainedAt + ExpiresIn，TokenExpiring 按它判断 */
    xoauth2TokenFree(tok);
}

/* 会话结束：清零敏感 state/verifier 并释放预设持有的端点 URL；
 * 之后复用客户端须重新 Use* 预设 */
xoauth2ClientUnit(&oauth);
xoauth2HttpXrtDestroy(http);
```

错误码经 `xoauth2LastError()` 一行读取，按来源分级：
`NETWORK`（连接/超时/TLS，或未配置传输）→ `TOKEN_ENDPOINT`（HTTP 错误状态）
→ `TOKEN_DENIED`（provider 返回 error 字段）→ `TOKEN_RESPONSE`（2xx 但体解析失败）。
`AuthStyle` 支持 body 与 HTTP Basic（RFC 6749 §2.3.1）两种客户端认证风格。

**传输回调**：`xoauth2httpproc` 是唯一的网络出口——宿主可注入任意 HTTP 实现
（反代、代理池、mock 测试），`xoauth2HttpXrt` 是基于 xrt net/tls/http1 的
现成实现：支持 http:// 与 https://（含 IPv6 字面量 `[::1]:8080`）、系统或
指定 CA 验证、响应体上限 1MB（防超大声明/无限 chunked 耗尽内存；自定义
回调的响应大小由宿主自行约束）。

## 状态

- PKCE (S256)：完整实现（verifier 用后焚毁）
- State 生成与 CSRF 校验：完整实现（常时比较，校验通过即焚毁）
- 授权 URL 构造（含 provider 特有参数）：完整实现（尊重预设 UsePkce）
- Token 请求构造（authorization_code / refresh_token，三种 AuthStyle）：
  完整实现（全部字段 form 编码）
- Token 响应 JSON 解析：完整实现（token_type 归一小写、时间戳换算）
- Provider 预设（GitHub/Google/WeChat/Microsoft/Custom）：完整实现
  （GitHub 含 api.github.com/user；Microsoft issuer 按 tenant 动态=第三块 owned URL，
 * ClientUnit 释放）
  （Microsoft 按 tenant 动态端点，所有权归客户端，ClientUnit 释放）
- **Token 交换与刷新：完整实现** —— 传输回调注入（mock 可测）+
  xoauth2HttpXrt 便捷实现（xrt net/tls/http1，回环服务器端到端验证）
- **OIDC 辅助（数据耦合层）：完整实现** —— nonce 全链路（生成/URL
  参数/常时比对一次性焚毁）、`xoauth2HttpGet`（借传输拉 JWKS 等）、
  `xoauth2GetUserInfo`（Bearer + 对象守卫）；Google/Microsoft 预设
  含 Issuer/JwksUrl/UseNonce，Microsoft issuer 按 tenant 动态生成
  （客户端持有所有权）。id_token 验签由应用层组合 xjwt 完成
  （见上节），xoauth2 本身不依赖 xjwt。

## 测试

```sh
cd tests
python gen_oidc_keys.py   # OIDC 组合测试夹具（EC 密钥 + JWKS，可提交后离线）
gcc -std=c11 -I.. -I../../single -o test_oauth2 test_oauth2.c \
    -lws2_32 -lbcrypt -ladvapi32 -liphlpapi   # Windows；Linux 去掉 -l 参数
./test_oauth2             # 160 项全绿（离线层 + 网络层 + OIDC 辅助 + 审计回归）
gcc -std=c11 -I. -I.. -I../../single -o test_oidc_compose test_oidc_compose.c \
    -lws2_32 -lbcrypt -ladvapi32 -liphlpapi
./test_oidc_compose       # 15 项全绿（xoauth2 + xjwt 数据耦合全链路）
```

测试覆盖：PKCE 挑战可复算、state/URL 编码边界、五预设参数、授权 URL
（PKCE 开关按预设）、请求构造（code/id/secret 全编码、Basic 头基准值
`Basic YWJjOmRlZg==`、BASIC 风格 body 无凭据）、响应解析（六字段、
token_type 归一、时间戳换算、error/畸形/缺字段拒绝）、CSRF（错误 state
不焚毁会话、正确 state 焚毁、重放拒绝）、错误码路径（xoauth2LastError）、
**网络层（Phase 2）**：mock 回调全路径（成功/BASIC 头透传/4xx DENIED/
502/空 body/2xx 畸形/传输失败/未配置、Refresh 成功与拒绝）+ 回环真实
服务器端到端（HttpXrt 经 http:// 127.0.0.1 完整交换：请求行/Host/
Content-Type/body 断言、400 DENIED、Refresh、不可达端口与非法 scheme
→ NETWORK），稳态零泄漏实测（Microsoft 预设/登录构造/解析循环）、
fuzz 2000 例。
