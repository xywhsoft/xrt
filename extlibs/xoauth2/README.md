# xoauth2

`xoauth2` 是构建在 xrt 核心之上的 OAuth 2.0 (RFC 6749) 客户端扩展库。
支持授权码流程 + PKCE (RFC 7636) + token 刷新 + 常用 provider 预设 +
OIDC 辅助（nonce / JWKS 拉取 / userinfo）。

2.0 更新扩充了公开的 `xoauth2client` / `xoauth2token` 结构（微信适配标记与
`OpenId`）；从 1.x 升级时必须重新编译所有使用方。

依赖闭包由 `config/modules.json` 声明，公共入口是 `include/xoauth2.h`，实现位于 `src/`，每个 `.c` 独立编译。构建、测试、单头生成与 CI 均使用仓库根目录的统一工具；目录规范见 [扩展库开发说明](../README.md)。

## 与 xjwt 的关系：数据耦合，零依赖

`xoauth2` 与 `xjwt` **互相不知道对方的存在**——没有链接依赖、没有
`requires`、各自可独立编译。OIDC 登录（id_token 验签）在**应用层**
组合，边界数据是纯 C 字符串（id_token / JWKS JSON）与 xrt 核心的
`xvalue*`。`examples/oidc_login.c` 使用离线 mock；真实 HTTPS 示例见
`examples/oidc_live.c`。两者通过私有应用辅助头
`examples/oidc_example_support.h` 组合，不增加库之间的链接依赖：

```c
#include "oidc_example_support.h"

/* oauth 的 issuer、端点和 CA 来自受信配置/发现；已启用 PKCE 和 nonce。 */
xoauth2token* tok = xoauth2CompleteLogin(&oauth, code, state);
char* jwksJson = NULL;
xjwtjwks* keys = NULL;
xvalue* claims = NULL;
oidcExamplePolicy policy; oidcExamplePolicyInit(&policy);
/* 默认 RS256；ES256 仅在应用注册明确采用它时配置。 */
int st = 0;
if ( tok != NULL && tok->IdToken != NULL ) {
    jwksJson = xoauth2HttpGet(&oauth, oauth.Config.JwksUrl, NULL, &st);
    if ( jwksJson != NULL ) keys = xjwtJwksParse(jwksJson);
    if ( keys != NULL ) claims = oidcExampleVerifyIdToken(&oauth, tok, keys, &policy, NULL);
}
bool authenticated = claims != NULL;
/* 使用身份时，userinfo.sub 还须与已验证的 claims.sub 一致。 */
xrtValueRelease(claims); xjwtJwksFree(keys); xrtFree(jwksJson);
xoauth2TokenFree(tok);
/* authenticated == false 时结束本次登录；仍须执行客户端/传输清理。 */
```

通用 JWT 的签名验证不等于完成 OIDC 登录。辅助头要求 `iss/sub/aud/exp/iat`，
支持最长 255 字节的 ASCII subject，检查全部 audience，并只接受注册的
RS256 或 ES256。额外 audience 须由应用显式列入 `TrustedAudiences`。
nonce 在全部 ID token 检查通过后才消费；userinfo 校验失败仍须结束登录。
`email` 是可选声明。该示例使用整数 NumericDate、30 秒时钟宽限和 600 秒
签发年龄限制；整数和年龄限制属于所选接口/应用策略，不能当作规范的通用限制。
它也检查已出现的 `azp` 与 `at_hash`；授权码流程不强制令牌携带这两个声明。
这些检查依据 [OpenID Connect Core 的 ID token 校验规则](https://openid.net/specs/openid-connect-core-1_0.html#IDTokenValidation)。

刷新时保留首次验证得到的不可变 claims，作为辅助函数的 `original` 参数。
刷新响应允许省略 ID token；若返回，重新验签并要求 issuer、subject、audience
接收者集合与首次一致。nonce 可省略，出现时须与首次一致；已知的 `auth_time`
须保持一致，并检查刷新后的 userinfo subject。首次没有 `auth_time` 时，
示例无法证明后来返回的认证时间等于原始认证时间。`max_age`、essential
`auth_time`、acr、加密 ID token 和动态注册不在此示例的支持范围内；需要这些
能力时须扩展应用策略。参见 [刷新 ID token 的规则](https://openid.net/specs/openid-connect-core-1_0.html#RefreshTokenResponse)。

真实示例从仓库根目录构建；`--no-run` 保留程序供显式配置端点后运行：

```sh
python tools/build.py --manifest extlibs/xoauth2/config/modules.json --suite xoauth2_live --no-run --no-single --jobs 4
out/gcc/native/xoauth2_live/oidc_live https://issuer.example client-id https://app.example/callback ca.pem basic RS256
```

保密客户端的 secret 只从运行时环境变量 `XOAUTH2_CLIENT_SECRET` 读取；
认证方式为 `basic` 或 `body`，公开客户端使用 `public`，并始终使用 S256 PKCE。
CA 参数可为 PEM 文件或 `system`，TLS 始终验证证书与身份。issuer 必须与
受信发现文档精确匹配；此示例要求发现文档提供 HTTPS userinfo 和 JWKS 端点。
打开打印的授权 URL，完成提供方登录后，将回调中的 code 和 state 分别输入两行。
程序不提供回调 HTTP 服务；生产网站还须自行实现会话与回调处理。获得刷新令牌
时会执行一次刷新及身份一致性检查。离线示例使用 ES256 测试夹具，这不能推导
真实 Google 或其他提供方的注册算法。

微信网站登录使用独立的 `appid`/GET 适配，响应缺少 `token_type` 时仅在
微信预设下补为 `bearer`；userinfo 需额外提供 `openid`。可离线运行的
流程见 `examples/wechat_login.c`。

密钥轮换重试 = 应用层 6 行（`xjwtLastError()==XJWT_ERROR_KEY_NOT_FOUND`
时重拉 JWKS，参见 xjwt 示例）。

## 构建

从仓库根目录执行（Windows 与 POSIX 使用同一入口）：

```sh
python tools/build.py --compiler gcc --manifest extlibs/xoauth2/config/modules.json --suite xoauth2,xoauth2_tests,xoauth2_oidc --jobs 4
python tools/package.py --compiler gcc --manifest extlibs/xoauth2/config/modules.json --suite xoauth2 --kind static --verify
python tools/amalgamate.py --manifest extlibs/xoauth2/config/modules.json
```

单头实现与声明分别生成到 `single/extlibs/xoauth2.h` 和 `single/extlibs/xoauth2_decl.h`，只包含本库代码。调用方先提供核心 XRT，再按依赖顺序提供扩展；见 [构建说明](../../docs/BUILD.md)。

## 用法（GitHub 登录）

```c
#include "xoauth2.h"

static bool shutdown_oauth(xoauth2client* oauth, xoauth2httpxrt** http);

xoauth2client oauth = {0};  /* 首次 Use* 前必须零初始化 */
xoauth2UseGithub(&oauth, client_id, client_secret, redirect_uri);

/* 传输注入与预设分离：预设管 provider 知识，传输管宿主环境。
 * 栈版（C 宿主）或堆版（opaque 场景/脚本层）二选一。 */
xoauth2httpxrt* http = xoauth2HttpXrtCreate(NULL, NULL, 0);  /* 堆版 */
if ( http == NULL ) {
    /* 查看诊断并结束本次创建；宿主退出前仍须重试下面的 Pending 清理。 */
    xoauth2ClientUnit(&oauth);
    goto shutdown;
}
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
shutdown:
/* 停止新调用并等待在途调用结束后，调用下方的清理函数。 */
if ( !shutdown_oauth(&oauth, &http) ) {
    /* 保存 http，保留库及运行环境；宿主稍后再次调用 shutdown_oauth。 */
}
```

错误码经 `xoauth2LastError()` 一行读取，按来源分级：
`NETWORK`（连接/超时/TLS，或未配置传输）→ `TOKEN_ENDPOINT`（HTTP 错误状态）
→ `TOKEN_DENIED`（provider 返回 error 字段）→ `TOKEN_RESPONSE`（2xx 但体解析失败）。
`AuthStyle` 支持 body 与 HTTP Basic（RFC 6749 §2.3.1）两种客户端认证风格。

分配失败及传输回调给出的诊断保留原始 `xerror`，请同时检查
`xrtGetError()`；来自 Core 或宿主回调的错误域可能使 `xoauth2LastError()`
返回 0。回调未提供错误时才补 `NETWORK`。state 校验通过后，即使请求构造
失败也清除 verifier/challenge；授权码不会自动重发，失败后重新 BeginLogin。

**传输回调**：`xoauth2httpproc` 是唯一的网络出口——宿主可注入任意 HTTP 实现
（反代、代理池、mock 测试），`xoauth2HttpXrt` 是基于 xrt net/tls/http1 的
现成实现：支持 http:// 与 https://（含 IPv6 字面量 `[::1]:8080`）、系统或
指定 CA 验证、响应体上限 1 MiB（防超大声明/无限 chunked 耗尽内存；自定义
回调的响应大小由宿主自行约束）。端点 URL 拒绝 userinfo、含非数字字符的端口
及未编码的控制字符；scheme 按 ASCII 大小写等价处理，无路径的查询 URL
会使用 `/?...` 作为请求目标。片段先校验再丢弃，空片段和合法百分号编码
允许，片段长度不占请求目标的 1023 字节容量。拒绝纯整数、十六进制、
缩写、前导零和混合数值 IPv4 写法；四段十进制 IPv4 及 IPv6 字面量允许。
`0xdead.example` 等含非数值 DNS 标签的名称仍作为 DNS 处理。规则依据
[RFC 3986 的 scheme、片段与旧式地址说明](https://www.rfc-editor.org/rfc/rfc3986)。
IPv6 scope/IPvFuture 不受支持。超时分别
约束连接、整次发送和完整响应，失败时中止连接并丢弃排队发送。
超时放弃 Future 前会请求生产任务取消。异步任务的内存不足保留原始
`MEMORY` 诊断，普通传输失败仍为 `NETWORK`，后续中止和资源释放不覆盖
首个错误。请求不会自动重发；POST 失败时服务端可能已经处理请求，授权码
或刷新令牌的结果未知时，应按提供方的协议恢复，不能据此推断“未发送”。
TLS DNS 端点发送 SNI；IPv4/IPv6 字面量只用于证书身份校验，不作为 SNI 发送。
回调只要设置了非空响应体，无论返回成功或失败，都必须交付 `xrtMalloc` 分配的
内存；库接管它并在内部处理后用 `xrtFree` 释放。若 `xoauth2HttpGet` 成功返回，
所有权转交给其调用方。失败时 `*piStatus` 为 0；内置传输在正文读取失败后也会
清零状态码和响应体指针。
内置传输交付的是 C 字符串，因此拒绝含原始 NUL 的正文；JSON 中的转义
文本 `\u0000` 不属于原始 NUL。令牌响应中的 `access_token`、`refresh_token`、
`id_token`、`openid`、`token_type` 和 `scope` 另在 JSON 解码后拒绝嵌入 NUL，
不向调用方交付被 C 字符串截断的字段。字面量反斜杠文本仍保留；这不是对
整个 JSON 文本的全局字符串过滤。响应头和 trailer 分别最多接受 100 个字段。
没有定长或 chunked 分帧的正文以正常读端结束为界；HTTPS 须收到认证
`close_notify`，直接断流不能作为正文成功结束。定长或 chunked 正文不足时，
即使对端正常关闭，也会拒绝部分结果。

请求和资源清理须由调用方串行执行；使用借用 engine 时，宿主须保证其在整个
使用期一直运行，并在结束后按 engine 退休接口完成异步关闭。
`xoauth2HttpXrtCleanup` 等待自建 engine 的异步关闭退出，
最多等待配置的 `uTimeoutUs`，成功后可重复调用；它不会停止或销毁借用 engine。
清理失败返回 `false` 并保留拥有权，句柄只能用于再次清理或销毁，不能继续请求。
`Unit` 调用同一清理路径；堆版 `Destroy` 仅在清理成功后释放句柄。需要确认
堆句柄可释放时，先判断 `Cleanup` 的结果，再调用 `Destroy`。清理保留调用前
的非空诊断，因此判断清理是否成功应使用返回值，不能只查看上一次错误。

`Init` 仅用于首次或已成功清理的外壳；初始化失败回滚至少有 30 秒预算，
与请求时限分开。失败构造如果仍有私有引擎未退休，`Create` 返回 NULL 并
保留未交付的拥有者，由 `xoauth2HttpXrtCleanupPending` 重试，不启动后台线程，
入列无需额外分配。Pending 的零预算为一次非阻塞轮询，非零为等待预算；
ERROR 提前返回，已有诊断保留。计数包含其他清理调用已取出的对象。
在待清理对象释放前，新的私有引擎构造先非阻塞清理，仍有对象则拒绝；
借用引擎的构造可继续。启动错误作为初始化诊断的 cause 保留，包装分配
失败时保留原始错误。宿主退出/卸载前必须完成以下顺序：停止新调用、等待
在途调用结束、清理已交付句柄，再调用 Pending 至 true；false 时保留库与
运行环境并稍后重试。以下函数可在已停止调用后重复执行：

```c
static bool shutdown_oauth(xoauth2client* oauth, xoauth2httpxrt** http)
{
    xoauth2ClientUnit(oauth);
    if ( *http != NULL ) {
        if ( !xoauth2HttpXrtCleanup(*http) ) return false;
        xoauth2HttpXrtDestroy(*http);
        *http = NULL;
    }
    return xoauth2HttpXrtCleanupPending(5000000u, NULL);
}
```

宿主须对全部已交付句柄执行清理；Pending 只处理本库未交付的失败构造。

## 状态

- PKCE (S256)：完整实现（verifier 用后焚毁）
- State 生成与 CSRF 校验：完整实现（常时比较，校验通过即焚毁）
- 授权 URL 构造（含 provider 特有参数）：完整实现（尊重预设 UsePkce）
- Token 请求构造（authorization_code / refresh_token，三种 AuthStyle）：
  完整实现（全部字段 form 编码）
- Token 响应 JSON 解析：完整实现（token_type 归一小写、时间戳换算）
- Provider 预设（GitHub/Google/WeChat/Microsoft/Custom）：完整实现
  （GitHub 含 api.github.com/user；Microsoft 的端点和 issuer 按 tenant 动态生成，
  所有权归客户端，ClientUnit 释放；WeChat 按其 appid/GET/无 token_type
  响应格式适配，并用 xoauth2GetWechatUserInfo 同时提交 access_token 与 openid）
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
# 从仓库根目录执行，脚本配置平台链接库与严格编译选项。
python tools/test_auth_extensions.py --case oauth2 --case oidc_compose
python tools/test_auth_extensions.py   # JWT/OAuth2 主测试、最小构建与所有离线范例
# Linux 可运行内存与未定义行为检查：
python tools/test_auth_extensions.py --case oauth2 --sanitize
python tools/test_auth_extensions.py --case oauth2_example_fault --case oidc_example_fault --sanitize
# 独立 Python 签发器驱动实际示例和刷新组合（需要 cryptography）：
python -m pip install cryptography==46.0.5
python tools/test_oidc_example_policy.py --output-dir out/oidc-policy-run
python tools/test_oidc_example_policy.py --compiler clang --sanitize --output-dir out/oidc-policy-sanitize-run
```

策略测试覆盖 22 个首次登录及 22 个刷新场景，包含合法边界与错误签名算法、
缺失必需声明、不可信额外 audience、错误 nonce、身份变更，以及允许省略的
email、刷新 ID token 和刷新 nonce。每项使用独立签名与新进程，并检查最终
活动分配、非法释放与重复释放计数。此应用仅使用 Bearer token，刷新响应
即使没有 ID token 也须检查 token_type；不支持的 MAC 等类型会被拒绝。
输出目录必须不存在，以保留原始结果。
`oidc_live_usage` 只验证真实示例的构建和用法出口，不能算作提供方互操作验收。

实际提供方互操作由 Ory Hydra v2.3.0 SQLite 测试实例验证，运行时归档和
可执行文件都固定 SHA256。Linux 从仓库根目录运行：

```sh
python tools/prepare_hydra.py
python tools/test_hydra_interop.py --hydra out/hydra-runtime/hydra --output-dir out/hydra-interoperability/normal
python tools/test_hydra_interop.py --hydra out/hydra-runtime/hydra --compiler clang --sanitize --output-dir out/hydra-interoperability/sanitize
```

驱动先用独立 Python HTTPS 客户端和 cryptography 验证授权码、S256、RS256、
nonce、userinfo 与刷新，再运行实际 C 测试程序和 `oidc_live.c`。
Basic、表单 secret 和公开客户端分别验证；错误 PKCE、错误保密客户端凭据、
错误 state、签名/issuer/audience 和 nonce 重放作为拒绝控制。
本地测试服务只绑定回环端口，客户端使用指定 CA；驱动拦截 login/consent
回调，不连接外部示例域名。服务进程结束且临时私钥移除后才写成功报告，
不记录 secret、授权码、cookie 或令牌。该固定版本用于兼容性回归，不能据此
推断任意提供方或部署配置已经验收。
WSL 可通过 `--windows-python/--windows-root/--windows-output/--windows-mount`
让相同冻结输入的 Windows C 程序连接同一真实服务，必要时指定
`--windows-compiler`；这些路径必须成组提供。输出目录须为全新目录。

主测试当前 Windows 264 项、Linux 257 项；本轮 Linux 主测试与独立
51 场景均通过 ASan/UBSan。OIDC 组合 15 项及完整离线范例的最新记录见
`extlibs/production-readiness.md`。
`api_review.c` 和 `oidc_login.c` 的每条失败路径统一清理已取得的句柄并返回失败；
刷新成功后才替换旧 token。组合范例要求必需的 ID token 声明与登录 nonce，
校验 userinfo 的 `sub` 与已验证的 ID token 一致。故障测试使用实际分配器，
验证响应分配、应用校验、签发和启动失败时无泄漏、重复释放或虚假的成功退出；
应用校验分配失败保留原始 MEMORY 诊断，不提前消费 nonce。
OIDC EC/JWKS 夹具已随测试提供；需要更新夹具时，在
`extlibs/xoauth2/tests` 目录运行 `python gen_oidc_keys.py`。

从仓库根目录执行 `python tools/test_oauth2_tls_interop.py`（Linux 可加 `--sanitize`），可使用
OpenSSL 临时签发的证书与独立 Python TLS 服务端验证 HTTPS：DNS SNI、
IPv4/IPv6 字面量不发送 SNI、非受信 CA、错误 DNS 身份与缺少 IP 身份被拒绝，
以及服务端在 ClientHello 后断开或保持静默时客户端按期失败并清空响应状态。
另覆盖 100 个合法响应字段和 trailer、101 个字段的拒绝，以及 2048 字节正文
携带约 8 MiB chunk 扩展时的内存上限。接收层在借用数据复制后回收已消费前缀，
保留尚未完成的分块行或 trailer，不按整个响应的线格式累积缓冲。
8 MiB POST 的部分上传超时和断流回归保留独立 TLS 观察引用，在销毁 HTTP
对象前检查异常关闭、未发送完整正文和未重试。可用 `--case upload-timeout`
或 `--case chunked-memory` 单独运行；默认运行全部场景。
正文边界还覆盖恰好 1 MiB、超大定长声明、累计超大的 chunked 正文和原始
NUL；EOF 回归分别验证 TLS 认证关闭、TLS 直接断流、TCP FIN，以及正常
关闭不能补全定长或 chunked 正文。每项场景销毁后均检查活动分配归零，
且原始故障诊断未被覆盖。`--case lifecycle` 另用真实 engine Pin 验证清理
超时后的重试、Unit 的诊断与拥有权、Destroy 保留忙碌堆句柄；注入退休错误
验证失败后重试，并确认借用 engine 始终运行。这些清理后的句柄均拒绝再次
发送请求。另覆盖无效 CA 配合 1 微秒时限的初始化失败，以及从首个分配到
成功构造之间逐个位置的 OOM 失败；每个位置都检查资源归零。
`--case url-invalid` 用实际 TCP 监听器检查 85 个非法 URL 均在拨号前拒绝，
`--case url-wire` 核对查询/片段、末尾 DNS 根点、混合大小写 scheme 的实际
请求目标、唯一 Host 字段和 TLS SNI。`--case url-vectors` 覆盖端口与容量
边界、IP/DNS 校验名、合法长片段及 255 个非 NUL 片段字节的 ASCII 语法。
`--case ownership` 另验证未交付失败构造的 ERROR/BUSY、栈外壳保留、
回滚预算及 cause、包装分配失败、已交付句柄不入列、零分配入列/轮询、
已取出对象计数和四个并发实际失败工厂/清理线程，以及 ERROR/超时后的
未处理尾链保留。每组最终检查活动分配归零，无非法或重复释放。
默认共 51 个场景（构造 OOM 扫描及非法 URL 样本各计一组，IPv6
不可用时少一项）。
默认的证书与编译产物只存于临时目录；这项测试已接入 Linux 和 Windows CI。
使用 `--coverage-dir <目录>` 时以 GCC `-O0` 和原子计数构建，并保留该目录中的
二进制、gcno/gcda；证书仍在临时目录。这一模式须与 sanitizer 分开运行。

统一覆盖率从仓库根目录运行：

```sh
python tools/test_gcov_coverage.py          # 真实 GCC 小程序与失效统计的拒绝测试
python tools/test_auth_coverage_inputs.py  # 主测试和密钥夹具变动的报告拒绝回归
python tools/measure_auth_coverage.py --library oauth2
python tools/measure_auth_coverage.py       # JWT 主测试及 OAuth2 合并统计
```

OAuth2 默认统计主测试、独立 HTTP/TLS/生命周期及未交付拥有权三个探针的
并集，按自有 `.c` 文件的实际行和分支计数计算，不把 Core 或测试夹具计入
分母。实现使用 [gcov JSON 的行和分支信息](https://gcc.gnu.org/onlinedocs/gcc/Invoking-Gcov.html)，
要求同一平台上的编译器版本、函数位置和控制流结构一致；源码或依赖在运行
期间发生变化、缺少执行计数、负数计数或结构不一致时直接失败。Windows
与 Linux 产物保存在不同目录，报告绑定实现、Core 单头、主测试、探针、
网络夹具及统计工具的 SHA256；测试输入变动也会失败并移除旧报告。
报告另含每个探针新增的触达量及
未覆盖位置：`out/auth_extensions/coverage/<平台>/oauth2/coverage.json`。
默认最低行覆盖率 91%、分支结果 75%；双平台 CI 执行合并门禁并上传报告。
`--main-only` 只用于诊断主测试基线，不包括独立探针；生产验收不能以该模式
代替默认合并统计。当前结果与仍需补测的分支见 `extlibs/production-readiness.md`。

测试覆盖：PKCE 挑战可复算、state/URL 编码边界、五预设参数、授权 URL
（PKCE 开关按预设）、请求构造（code/id/secret 全编码、Basic 头基准值
`Basic YWJjOmRlZg==`、BASIC 风格 body 无凭据）、响应解析（六字段、
token_type 归一、时间戳换算、error/畸形/缺字段拒绝）、CSRF（错误 state
不焚毁会话、正确 state 焚毁、重放拒绝）、错误码路径（xoauth2LastError）、
**网络层（Phase 2）**：mock 回调全路径（成功/BASIC 头透传/4xx DENIED/
502/空 body/2xx 畸形/传输失败/未配置、Refresh 成功与拒绝）+ 回环真实
服务器端到端（HttpXrt 经 http:// 127.0.0.1 完整交换：请求行/Host/
Content-Type/body 断言、400 DENIED、Refresh、不可达端口与非法 scheme
→ NETWORK；Linux/POSIX 也实际运行回环 HTTP，而非跳过），传输失败时状态码与
部分响应体的所有权回归、稳态零泄漏实测（Microsoft 预设/登录构造/解析循环）、
fuzz 2000 例。
