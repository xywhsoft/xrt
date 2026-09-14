---
num: 106
slug: xhttp-auth
title: 认证：Basic、Bearer 与 Digest 会话
volume: 卷十 扩展库：xhttp
type: practice
lead: 三方案的两端实现：Basic 的明文底线、Bearer 的令牌形态、Digest 的 nonce 挑战与重放表——认证是协议不是口号。
api: xhttp-http_auth, xhttp-http_server, xhttp-http_digest
---

## 导读

"加个认证"在 HTTP 里是三个方案家族的选型题。**Basic**：`Authorization: Basic base64(用户:密码)`——明文（TLS 之下的可接受形态），两端各一行。**Bearer**：`Authorization: Bearer <令牌>`——OAuth2/JWT 时代的载体，客户端只负责携带。**Digest**：服务端发 nonce 挑战、客户端以 `H(用户:域:密码)` 应答——**密码不上线**、自带防重放（nonce+nc 计数）的挑战应答协议，服务端要管 nonce 生命周期、重放表与 `Authentication-Info` 回执。xhttp 把三方案的两端都实现为结构化 API：客户端构建（`xrtHttpBasicWrite`/`BearerWrite`/Digest 应答）、服务端读取（`xrtHttpServerRequestBasicAuth/BearerAuth/DigestAuth` 三态）、Digest 的完整验证链（`xrtHttpDigestVerify` 的 verify 四态 + 线程安全重放表）。第 105 章鉴权中间件由此拼装。

## 引入

三个部署形态对号入座。内网工具：Basic + TLS 够用——一行代码两端，密码明文但隧道加密。API 平台：Bearer——令牌签发/刷新是平台的事，服务端只验令牌；客户端的"认证"就是带上它。不可信环境或协议合规（HTTP 语义下的密码保护）：Digest——密码摘要存服务端、挑战应答防听重放，代价是服务端要维护 nonce 与重放状态。**方案选错的代价**：在明文 HTTP 上跑 Basic 等于广播密码；在需要撤回令牌的平台用无状态 Bearer 等于无法撤回；把 Digest 当"更安全的 Basic"用而不管理 nonce 过期，重放窗口照样打开。

认证 API 的统一形态值得注意：全部走 `XHTTP_NEXT_END/ITEM/ERROR` 三态（缺失/有效/错误）——与结构化字段读取（第 92/104 章）同一契约；challenge 构建"追加"（AddChallenge——多方案共存），回执"设置"（SetDigestInfo——唯一字段）——RFC 的多值/唯一语义落在 API 形状上。

## 概念

### Basic：两端一行与明文底线

客户端 `xrtHttpBasicWrite(用户, 密码, 输出, 容量, &长度)` 产 `Basic <base64>` 字段值；服务端 `xrtHttpServerRequestBasicAuth(请求, 解码缓冲, 容量, &长度, &描述符)`——**Basic 与 Digest 的已解码结果借用调用方缓冲**（短缓冲只发布精确所需长度、错误清空描述符但保持调用方长度与正文不变——不提前提交半份明文）。底线纪律：Basic 只在 TLS 下使用；`http` 明文上的 Basic 是配置事故。

### Bearer：令牌的携带与挑战

客户端 `xrtHttpBearerWrite` 产 `Bearer <token>`；服务端 `xrtHttpServerRequestBearerAuth` 取令牌视图（借用请求快照——令牌本体归你的验证层：本地验签、查库、调鉴权服务）。**Bearer 的安全边界在令牌管理**：签发、过期、撤回、最小权限域——协议层只是载体；401 challenge（`WWW-Authenticate: Bearer error="invalid_token"`）的构建入口负责语法，策略归应用。

### Digest：挑战、应答与验证四态

```diagram flow
- 挑战：服务端 401 + WWW-Authenticate: Digest realm/nonce/qop/algorithm/opaque（AddChallenge）
- 应答：客户端 H(A1)=H(用户:realm:密码) 缓存；按 nonce+nc 计算 request-digest 应答
- 验证：服务端查 H(A1)（按用户名/userhash）→ 填充验证描述符（方法/target/challenge）
  → xrtHttpDigestVerify → verify 四态判定
- 回执：VALID 后 DigestRspAuth 生成 rspauth → SetDigestInfo 设 Authentication-Info
- 重放：VALID 才提交 nc；ReplayCheck 线程安全重放表（分布式用 ReplayKey+共享存储）
```

四态的精确语义：`XHTTP_DIGEST_VERIFY_VALID`（签名与证明全对）、`_STALE`（**签名与证明正确但 nonce 过期**——客户端应带新 nonce 重试，不是失败）、`_INVALID`（证明错误——拒绝）、`_ERROR`（输入/结构错误）。**只有 VALID 才提交单调 nc**——提前提交会让错误证明抢占合法客户端的计数（重放表被污染）。nonce 管理：默认入口内建；自定义 nonce 或轮换 key-ring 用 `xrtHttpDigestProofVerify`+`NonceVerify` 两层组合。`Authentication-Info` 回执（rspauth 双向认证 + nextnonce 轮换）在验证成功后用同一证明上下文生成——挑战应答的完整闭环。

### 重放防御的部署形态

单进程：`xrtHttpDigestReplayCheck`——线程安全、硬容量的重放表；多进程/分布式：`xrtHttpDigestReplayKey` 生成规范键，共享存储做**带过期的原子最大值更新**（nc 是单调计数——CAS 语义的 max 更新）。重放窗口=nonce 期限；期限是安全与体验的权衡参数。

### 客户端挑战解析

`xrtHttpChallengeNext` 迭代 401 响应里的多方案挑战（`Digest realm=..., Basic ...` 混合声明）——客户端按能力选方案：有密码缓存选 Digest、有令牌选 Bearer、退 Basic。自动重试（第 100 章）收到 401 后的策略层由此装配。

## 示例

### 第一个完整程序：服务端挑战的逐方案解析

下面的程序来自 `examples/http/auth`——多方案 challenge 的结构化读取：

```embed path="extlibs/xhttp/examples/http/auth/main.c" title="extlibs/xhttp/examples/http/auth/main.c"
```

```term
$ gcc -O1 -DXRT_MODULE_ALL -I extlibs/xhttp/single -include xhttp.h impl.c extlibs/xhttp/examples/http/auth/main.c -lws2_32 -liphlpapi
Digest
Basic
```

**刚才发生了什么。** ① 输入是混合挑战 `Digest realm="api", Basic QWxhZGRpbjpvcGVuIHNlc2VtZQ==`——一个 `WWW-Authenticate` 值里两个方案声明。② `xrtHttpChallengeNext` 偏移迭代逐个产出 `xhttpauth`（Scheme 视图 + 参数）——Digest 与 Basic 各一项；与第 92 章字段族迭代同一形态（逗号域、偏移持有）。③ 客户端策略的起点：按序扫描、按能力匹配——"服务端声明了什么"是结构化事实不是字符串猜测。Digest 的参数（realm/nonce/qop）继续用第 92 章参数层解析。

### 第二个完整程序：Basic 两端的最短路径

第二个程序来自 `examples/http/auth_basic`——客户端构建：

```embed path="extlibs/xhttp/examples/http/auth_basic/main.c" title="extlibs/xhttp/examples/http/auth_basic/main.c"
```

```term
$ gcc -O1 -DXRT_MODULE_ALL -I extlibs/xhttp/single -include xhttp.h impl.c extlibs/xhttp/examples/http/auth_basic/main.c -lws2_32 -liphlpapi
Basic QWxhZGRpbjpvcGVuIHNlc2VtZQ==
```

**刚才发生了什么。** ① `xrtHttpBasicWrite("Aladdin", "open sesame")` 一个调用产完整字段值——Base64（第 28 章）内建、容量原子性同全库。② 输出正是 RFC 7617 的规范示例值——标准向量又一次锚定实现正确性。③ 服务端对侧 `xrtHttpServerRequestBasicAuth` 解码到调用方缓冲——两端合计四个调用就是 Basic 的全部。配套示例族：`auth_bearer`（令牌构建）、`auth_digest`（完整挑战应答）、`auth_digest_client`/`auth_digest_session`（会话与 nonce 复用）、`auth_digest_nonce`/`_replay`（nonce 与重放）、`digest_sha2`（SHA-2 算法族）——Digest 的每一段都有独立样本。

## 契约

- **三态统一**：读取族 `END`（缺失）/`ITEM`（有效）/`ERROR`（重复字段、非法语法、方案不匹配、解码失败）；重复 Authorization 归 Header 协议错误。
- **借用边界**：通用与 Bearer 结果借请求快照；Basic 与 Digest 解码结果借调用方缓冲；短缓冲只发布精确长度、错误不动调用方状态。
- **Basic 底线**：明文（base64 非加密）；只在 TLS 下使用；两端 API 各一个调用。
- **Bearer 边界**：令牌验证/撤回/权限归应用层；challenge 语法构建归认证层。
- **Digest 四态**：VALID/STALE（签名对但 nonce 过期——可重试）/INVALID（证明错）/ERROR；只有 VALID 提交 nc。
- **nonce 管理**：默认内建；自定义用 ProofVerify+NonceVerify 组合；期限=重放窗口。
- **重放表**：单进程 ReplayCheck（线程安全硬容量）；分布式 ReplayKey+共享存储原子 max 更新。
- **challenge/回执**：Add 追加多方案；Set 设唯一 Authentication-Info（rspauth+nextnonce）；不自动改 401/407 状态——策略归应用。
- **裁剪**：`HTTP_AUTH`/`HTTP_DIGEST` 独立；SHA-2 算法族可选。

## 避坑

### 坑 1：明文 HTTP 上跑 Basic

症状：抓包可见 `Authorization: Basic <base64(密码)>`——密码等于广播。

原因：base64 是编码不是加密。Basic 的全部安全性来自传输层——TLS 之下它是"防止肩窥"级别。

```c bad
/* http:// 内网地址 + Basic：内网抓包/代理日志全是明文密码 */
xrtHttpRequestSetBasicAuth(pReq, User, Pass);
send_http(url);   /* 无 TLS */
```

```c good
/* Basic 只与 https 同用 */
if ( !url_is_https(pUrl) ) {
	return error("basic auth requires TLS");
}
xrtHttpRequestSetBasicAuth(pReq, User, Pass);
```

### 坑 2：Digest 验证没区分 STALE 就当失败

症状：nonce 过期的合法客户端收到 401 被踢出登录——体验崩坏；或反过来把 INVALID 也当 STALE 放重试——暴力证明无限循环。

原因：四态语义必须分支处理：STALE 是"请用新 nonce 重来"（客户端已有 H(A1) 可无感重试），INVALID 是"证明就是错的"（该拒绝计数）。

```c bad
switch ( xrtHttpDigestVerify(&V) ) {
default:
	reject_and_logout();   /* STALE 也被当失败 */
}
```

```c good
xhttpdigestverifycheck R = xrtHttpDigestVerify(&V, Key, Ctx, Now, Life, Skew, NULL);
if ( R == XHTTP_DIGEST_VERIFY_VALID ) { accept(); }
else if ( R == XHTTP_DIGEST_VERIFY_STALE ) {
	challenge_with_fresh_nonce();   /* 客户端会带新 nonce 重试 */
} else {
	reject_and_count();             /* INVALID/ERROR：拒绝+计数 */
}
```

### 坑 3：验证前就提交 nc 计数

症状：重放表计数被错误证明抢占——合法客户端的下一个请求被判重放拒绝。

原因：nc 是"这个 nonce 下已用过的请求数"的单调计数；INVALID 的请求不该占号——契约明确"只有 VALID 才提交"。

```c bad
verify_partial(&V);                 /* 只验了签名 */
replay_commit(nonce, nc);           /* 先占号——错了也占 */
if ( full_verify(&V) ) { accept(); }
```

```c good
if ( xrtHttpDigestVerify(&V, Key, Ctx, Now, Life, Skew, NULL) ==
		XHTTP_DIGEST_VERIFY_VALID ) {
	replay_commit(nonce, nc);       /* 证明全对才占号 */
	accept();
}
```

## 练习

### 基础：三方案 challenge 矩阵

构造含 Digest+Basic+Bearer 的三方案 challenge，客户端解析并按配置（有密码/有令牌/都无）选择方案打印。验收标准：三种配置各选对方案；challenge 参数（realm/nonce）结构化可读。

### 进阶：Digest 挑战应答闭环

服务端（第 104 章服务 + 中间件）发 Digest 挑战；客户端解析 nonce、计算应答、提交；服务端 `DigestVerify` 走四态分支。重复一次请求验证 nc 递增。验收标准：首请求 VALID；同 nonce 重放被重放表拒；nonce 过期走 STALE 重试成功。

### 挑战：令牌撤回的 Bearer 网关

Bearer 网关中间件：令牌查本地缓存（第 18 章 Map + TTL），未命中查后端鉴权服务（模拟）；支持撤回列表（撤回即缓存失效）。验收标准：有效令牌放行、无效 401、撤回后立即 401；缓存命中路径零后端调用（统计验证）；令牌从不入日志（第 77 章纪律）。

## 速查

| 知识点 | 速查 |
| --- | --- |
| 三方案 | Basic（明文+TLS 底线）/ Bearer（令牌载体）/ Digest（挑战应答） |
| 三态读取 | END 缺失 / ITEM 有效 / ERROR 重复·非法·不匹配·解码失败 |
| 借用边界 | 通用与 Bearer 借请求；Basic/Digest 解码借调用方缓冲（短缓冲报精确长度） |
| 挑战构建 | AddChallenge 追加多方案；Basic/Bearer/Digest 入口各自完整校验 |
| Digest 四态 | VALID / STALE（nonce 过期可重试）/ INVALID（证明错）/ ERROR |
| nc 纪律 | 只有 VALID 提交单调计数——先提交会让错误证明占号 |
| nonce | 默认内建；自定义 ProofVerify+NonceVerify；期限=重放窗口 |
| 重放表 | 单进程 ReplayCheck；分布式 ReplayKey+共享存储原子 max |
| 回执 | VALID 后 RspAuth 生成、SetDigestInfo 设唯一 Authentication-Info |
| 客户端 | ChallengeNext 迭代多方案按能力选择；H(A1) 本地缓存 |
