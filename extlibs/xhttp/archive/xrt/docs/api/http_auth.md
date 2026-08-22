# HTTP 认证

`http_auth` 提供不绑定网络、客户端或服务端对象的 HTTP 认证字段语法层。它覆盖 RFC 9110 的 `auth-scheme`、`token68`、`auth-param` 与 challenge 列表，并为 RFC 7617 Basic 和 RFC 6750 Bearer 提供独立可裁剪模块。

## 通用认证值

```c
xhttpauth auth;
size_t offset = 0;

while ( xrtHttpChallengeNext(value, &offset, &auth) == XHTTP_NEXT_ITEM ) {
	/* auth.Scheme、auth.Data 借用 value */
}
```

- `xrtHttpChallengeNext` 用参数名后的等号区分“同一 challenge 的下一个参数”和“下一个认证方案”。引号内逗号不会切断参数。
- `xrtHttpFieldChallengeNext` 使用 `xhttpauthcursor` 跨多条重复认证字段和每条字段内的 challenge 列表迭代；游标只保存两个索引，不分配内存。
- `xrtHttpAuthParse` 只接受单份 `Authorization` 或 `Proxy-Authorization` 凭据，拒绝在外层拼接 challenge 列表。
- `xrtHttpAuthParamNext` 读取逗号分隔且必须带值的 `auth-param`。
- `xrtHttpAuthWrite` 支持长度查询、失败原子短缓冲和输入输出非重叠检查；`xrtHttpAuthBuild` 返回由 `xrtFree` 释放的零结尾文本。

所有借用文本都必须覆盖完整且地址不回绕的内存范围。通用层的 `size_t`、`xhttpparam`、`xhttpauth`、`xhttpauthcursor` 和 `xhttpfield` 数组均支持合法的未对齐存储；实现通过本地快照读取，并只在成功返回条目或结束时发布游标。语法错误不会推进调用方游标，合法的结果存储会被清零，避免上层误用半份 challenge。

`xrtHttpFieldChallengeNext` 每次调用都会验证完整字段描述符数组及其借用视图，并拒绝游标或结果与任何输入范围重叠。这个入口适合直接消费裸 C 字段数组；拥有型 Header 容器和客户端、服务端对象应继续使用各自的认证 Helper，避免反复手工组织数组。

`xrtHttpAuthToken68Valid` 是不修改线程错误的纯谓词。`xrtHttpAuthWrite` 的长度输出可未对齐；空输出和零容量执行精确长度查询，容量不足只发布所需长度且不修改输出。`xrtHttpAuthBuild` 分配失败时不修改可选长度输出。

## Basic

```c
char value[128];
size_t size;

xrtHttpBasicWrite(
	XRT_STR_LITERAL("Aladdin"),
	XRT_STR_LITERAL("open sesame"),
	value,
	sizeof(value),
	&size
);
```

`xrtHttpBasicWrite` 按原始 `user:password` 字节编码。用户名不得包含冒号，用户名和密码都不得包含控制字符。实现使用同址 Base64 扩张，常见凭据在 256 字节栈缓冲中完成，超出后才精确分配；所有临时明文与 Base64 副本在返回前都会清零。

`xrtHttpBasicRead` 严格验证完整字段值、规范 Base64、冒号分隔和控制字符，查询模式返回解码后的 `user:password` 字节数；实际模式把用户名和密码视图绑定到调用方输出缓冲。该函数先在同一套栈或堆临时区中完成全部语义校验，再发布输出，因此格式错误、OOM 和短缓冲都不会部分覆盖结果。

Basic 的长度和结果描述符支持合法的未对齐存储。`xrtHttpBasicRead` 在这些描述符有效后会先发布空结果，只有实际解码成功才发布借用调用方输出的视图；查询成功时视图保持为空。所有输入、输出和描述符都会拒绝地址回绕与不允许的别名。

`xrtHttpBasicChallengeRead` 与 `xrtHttpBasicChallengeWrite` 对称处理 RFC 7617 challenge。读取要求恰好一个 `realm`，接受参数重排、token 或 quoted-string 值并忽略未知扩展参数；重复 `realm`、重复 `charset` 或不区分大小写后仍不是 `UTF-8` 的 charset 会被拒绝。查询模式完成全部验证并发布 `Utf8` 事实，实际模式把解码后的 realm 写入调用方缓冲。写出入口始终使用安全转义的 quoted realm，并可追加 `charset="UTF-8"`。

## Bearer

`xrtHttpBearerWrite` 与 `xrtHttpBearerRead` 使用 RFC 6750 的 `b64token` 字符集合。Bearer 解析只返回借用视图，不分配内存。Token 中的空格、逗号或非末尾等号会被拒绝；谓词不修改线程错误，写入能区分无效内存与无效 token。

Bearer 长度和结果描述符支持合法的未对齐存储。解析会先清空有效结果，随后再验证完整认证值；格式失败不会泄露部分 token，结果描述符也不得覆盖输入字段值。

### Bearer challenge

`http_auth_bearer_challenge` 是独立裁剪层，并额外依赖严格的 RFC 3986 URL 解析器。`xrtHttpBearerChallengeRead` 与 `xrtHttpBearerChallengeWrite` 覆盖 `realm`、`scope`、`error`、`error_description` 和 `error_uri` 五个 RFC 6750 标准参数：

```c
xhttpbearerchallenge challenge = {
	XHTTP_BEARER_HAS_REALM |
	XHTTP_BEARER_HAS_ERROR |
	XHTTP_BEARER_HAS_ERROR_DESCRIPTION,
	XRT_STR_INIT("api"),
	{ NULL, 0 },
	XRT_STR_INIT("invalid_token"),
	XRT_STR_INIT("The access token expired"),
	{ NULL, 0 }
};

xrtHttpBearerChallengeWrite(
	&challenge, value, sizeof(value), &size
);
```

存在位用于区分缺失参数与显式空 `realm`。写出顺序固定，全部值统一使用 `quoted-string`；`scope` 必须是单空格分隔的非空 scope-token，错误文本使用 NQSCHAR 集合，`error_uri` 必须是无 fragment 的绝对 URI。应用通常把 `error_description` 和 `error_uri` 与 `error` 一同发送，但协议层不会把这条使用建议升级成规范没有规定的强制依赖。

读取入口接受参数重排、合法 `quoted-pair` 和未知扩展参数，但拒绝重复标准参数。查询模式会完成全部语义校验并发布存在位，只是不发布依赖输出缓冲的值视图；实际模式把五个标准值解码到调用方缓冲。未知参数需要通过下层 `xrtHttpAuthParse` 与 `xrtHttpAuthParamNext` 读取，包含扩展参数的自定义写出则直接使用 `xrtHttpAuthWrite`，协议层不会封死扩展路径。

常规 URI 和全部写出路径不分配内存。只有超长且使用非规范但合法 `quoted-pair` 的 `error_uri` 在完整 URI 校验时可能回退到堆；该路径具有独立 OOM 原子性测试。

认证凭据会直接暴露访问能力。应用应只在满足自身机密性要求的传输上发送凭据，并避免把完整认证字段写入日志或错误消息。

## Digest

Digest 按 [RFC 7616](https://www.rfc-editor.org/rfc/rfc7616.html) 分成可独立裁剪的元数据、challenge、凭据、`Authentication-Info` 和摘要计算层。纯协议解析不会引入 SHA-2 或兼容 MD5 实现；`xrtHttpDigestAlgorithmSupported` 只报告当前裁剪闭包实际具备的计算后端。因此代理、日志分析器和协议网关可以识别并转发未知扩展算法，而不必链接密码学模块。

`xrtHttpDigestAlgorithmParse`、`xrtHttpDigestAlgorithmName`、`xrtHttpDigestSize` 和 `xrtHttpDigestAlgorithmSession` 覆盖 MD5、SHA-256、SHA-512/256 及其 `-sess` 变体。名称和 qop 按 ASCII 大小写不敏感读取，规范写出固定使用注册名称；未知算法返回 `XHTTP_DIGEST_ALGORITHM_UNKNOWN`，不会被误报成语法错误。

### Digest challenge

```c
xhttpdigestchallenge challenge = {
	XHTTP_DIGEST_CHALLENGE_UTF8 |
	XHTTP_DIGEST_CHALLENGE_HAS_USERHASH |
	XHTTP_DIGEST_CHALLENGE_USERHASH |
	XHTTP_DIGEST_CHALLENGE_QOP_AUTH |
	XHTTP_DIGEST_CHALLENGE_ALGORITHM_EXPLICIT,
	XHTTP_DIGEST_ALGORITHM_SHA256,
	XRT_STR_INIT("api@example.org"),
	{ NULL, 0 },
	XRT_STR_INIT("server-nonce"),
	{ NULL, 0 },
	{ NULL, 0 }
};

xrtHttpDigestChallengeWrite(
	&challenge, value, sizeof(value), &size
);
```

`xrtHttpDigestChallengeRead` 要求 `realm`、`nonce` 和 RFC 7616 规定所有实现必须使用的 `qop`，忽略未知扩展参数，并拒绝重复标准参数。`realm`、`domain`、`nonce`、`opaque` 和 `qop` 只接受 quoted-string 线路形式；`stale`、`algorithm`、`charset` 和 `userhash` 只接受 token。`charset` 唯一合法值是 `UTF-8`，未知 qop 被忽略，已知的 `auth` 与 `auth-int` 通过能力位保留。

算法参数缺失时按规范解析为 MD5，但这只表示线路默认值，不会自动引入或推荐 MD5 计算后端。未知且语法合法的算法保存在 `AlgorithmName` 中，供上层忽略当前 challenge、选择其他 challenge 或交给扩展实现。服务器应按最强到最弱的偏好顺序发送不同算法的多个 challenge；客户端应选择首个受本地策略支持的算法。

读取查询模式执行全部语法和语义验证，返回四个解码文本所需的精确总字节数，并发布标志及算法；依赖输出缓冲的 `Realm`、`Domain`、`Nonce`、`Opaque` 视图保持为空。已知 `AlgorithmName` 使用静态规范名称，未知名称借用输入字段值。实际读取把四个 quoted-string 解码到调用方缓冲；空值合法，存在位负责区分缺失的可选参数。

写出按 `realm`、`domain`、`nonce`、`opaque`、`stale`、`algorithm`、`qop`、`charset`、`userhash` 的固定顺序生成规范字段。至少选择 `auth` 或 `auth-int`，未知算法使用显式且合法的 `AlgorithmName` token。需要保留未知 qop 或额外扩展参数时，应退回 `xrtHttpAuthWrite` 与通用参数层自行构建，结构化 Helper 不封死扩展路径。

challenge 读写不分配内存，支持未对齐固定描述符、精确长度查询和失败原子短缓冲。输入、输出、长度和结构体范围必须互不覆盖；写入前会验证所有借用视图，避免别名发布破坏后续读取。

### Digest 凭据

`xrtHttpDigestAuthRead` 与 `xrtHttpDigestAuthWrite` 处理完整 `Digest Authorization` 值，覆盖 `username`、RFC 8187 `username*`、`realm`、`uri`、`algorithm`、`nonce`、`nc`、`cnonce`、`qop`、`response`、`opaque` 和 `userhash`。解析器要求 RFC 7616 的完整必填组，拒绝重复标准参数、错误线路形式、零 `nc`、错误摘要长度和不一致的 `username*`/`userhash` 组合；未知扩展算法保留在 `AlgorithmName`，供调用方自行扩展。认证侧名称显式包含 `Auth`，不会与 RFC 9530 Digest Fields 的 `xrtHttpDigestRead/Write` 冲突。

```c
xhttpdigestauth auth;
char decoded[512];
size_t size;

if ( xrtHttpDigestAuthRead(
	value,
	decoded,
	sizeof(decoded),
	&size,
	&auth
) ) {
	/* auth 的标准文本视图借用 decoded。 */
}
```

写出器固定参数顺序并使用规范小写摘要、八位十六进制 `nc` 和正确的 quoted/token 形式。结构化写出不承诺保留未知参数；需要透明转发扩展参数时，应使用通用认证参数层或直接保留原始字段。

### Digest 计算

`http_auth_digest_sha2` 提供 SHA-256、SHA-256-sess、SHA-512/256 和 SHA-512-256-sess；可选 `http_auth_digest_md5` 只为同一 API 增加 MD5 与 MD5-sess 兼容后端，不产生第二套函数。新应用应优先选择 SHA-256 或 SHA-512/256。

```c
char secret[XRT_HTTP_DIGEST_MAX_TEXT_SIZE];
char response[XRT_HTTP_DIGEST_MAX_TEXT_SIZE];
size_t size;

xrtHttpDigestSecret(
	XHTTP_DIGEST_ALGORITHM_SHA256,
	XRT_STR_LITERAL("Mufasa"),
	XRT_STR_LITERAL("api@example.org"),
	XRT_STR_LITERAL("Circle Of Life"),
	secret,
	sizeof(secret),
	&size
);

xhttpdigestproof proof = {
	XHTTP_DIGEST_ALGORITHM_SHA256,
	XHTTP_DIGEST_QOP_AUTH,
	1,
	{ secret, size },
	XRT_STR_INIT("server-nonce"),
	XRT_STR_INIT("client-nonce"),
	XRT_STR_INIT("/private"),
	{ NULL, 0 }
};

xrtHttpDigestRequest(
	&proof,
	XRT_STR_LITERAL("GET"),
	response,
	sizeof(response),
	&size
);
```

`xrtHttpDigestSecret` 生成可持久化的基础 `H(username:realm:password)`；即使选择 `-sess` 算法，它也不混入本次 nonce。服务器可以只保存这个值，不保存明文密码。`xrtHttpDigestUserHash` 生成线路 `userhash`，`xrtHttpDigestRequest` 生成请求证明，`xrtHttpDigestRspAuth` 生成服务器回执，`xrtHttpDigestEqual` 不修改错误槽地常量时间比较大小写不敏感的十六进制摘要。

证明结构只接受 `auth` 与 `auth-int`，并要求非零 `nc`、非空 nonce、cnonce 和 URI。`auth-int` 的 `EntityHash` 是已经计算好的规范十六进制实体摘要；小正文可由 `xrtHttpDigestHash` 一次性计算，大正文可使用对应密码学模块的增量状态流式计算后传入。这样 Digest 层不缓存正文，也不把认证计算绑定到 HTTP Body 对象。

所有计算入口支持精确长度查询、失败原子短缓冲、未对齐描述符和范围/别名检查。临时摘要、A1、A2 和 nonce-count 文本在返回前清零；查询模式完成参数与长度验证，但不会为仅获取固定摘要长度执行散列。

### Authentication-Info

`xrtHttpDigestInfoRead` 与 `xrtHttpDigestInfoWrite` 处理不带字段名的 `Authentication-Info` 参数列表。算法来自原请求上下文，不在线路中重复。非空 `nextnonce` 可以单独出现；`qop`、`rspauth`、`cnonce` 和 `nc` 必须作为完整证明组同时出现，空 `nextnonce`、部分组、重复参数、错误摘要长度和零 `nc` 都会被拒绝。

读取和写出均不分配内存，并具有与 challenge、凭据相同的查询、短缓冲、未对齐和别名契约。客户端应使用原请求的算法与证明上下文重新计算 `rspauth`，再用 `xrtHttpDigestEqual` 比较；收到有效 `nextnonce` 后如何更新 nonce 状态仍由客户端策略决定。

### Digest 客户端协议层

`http_auth_digest_client` 把客户端容易分散实现的协商、凭据构建和响应证明校验收敛为无状态、零分配协议层。它不持有 HTTP 请求对象，不保存密码，也不自动发送或重试请求，因此可供同步客户端、异步客户端、代理认证和自定义传输共同复用。

`xrtHttpDigestChallengeChoose` 按本地 `xhttpdigestpolicy` 判断一份已解析 challenge。空策略使用现代默认值：允许 SHA-256、SHA-256-sess、SHA-512/256 与 SHA-512/256-sess，允许 `auth` 和 `auth-int`，两者同时提供时优先互操作性更好的 `auth`，并在服务器声明 `userhash=true` 时使用用户名摘要。MD5 必须由应用同时编入 MD5 后端并显式加入算法集合。调用方按线路顺序迭代 challenge，采用第一个返回 `XHTTP_DIGEST_CHOOSE_ACCEPTED` 的结果，即可遵循服务器的算法偏好；`REJECTED` 是普通协商结果，不污染线程错误槽。

```c
xhttpdigestchoice choice;
xhttpdigestclientauth input = { 0 };
xhttpdigestauth auth;
char work[XRT_HTTP_DIGEST_MAX_TEXT_SIZE * 2];
size_t work_size;

if ( xrtHttpDigestChallengeChoose(
	&challenge, NULL, &choice
) == XHTTP_DIGEST_CHOOSE_ACCEPTED ) {
	input.Challenge = &challenge;
	input.Choice = &choice;
	input.Username = XRT_STR_LITERAL("user");
	input.Secret = (xstrview){ secret, secret_size }; /* H(A1) */
	input.Method = XRT_STR_LITERAL("GET");
	input.RequestTarget = XRT_STR_LITERAL("/private");
	input.Cnonce = XRT_STR_LITERAL("client-generated-nonce");
	input.NonceCount = 1;
	xrtHttpDigestClientAuth(
		&input, work, sizeof(work), &work_size, &auth
	);
}
```

`xrtHttpDigestClientAuth` 只接收预先计算的 `Secret`，不会迫使密码进入长期对象。输出缓冲仅保存新生成的 `userhash` 和 request-digest，`realm`、nonce、opaque、request-target 与 cnonce 继续借用输入；查询模式给出精确总长度。普通用户名中的冒号会被拒绝，`username*` 必须显式选择且只接受声明 UTF-8 的 challenge。`auth-int` 要求调用方传入传输编码之前的实体摘要，因此流式正文仍可由上层选择合适的增量散列方式。

`xrtHttpDigestInfoVerify` 使用某一次请求实际保存的 `xhttpdigestproof` 校验 `rspauth`、qop、cnonce 和 nc。并发或乱序响应必须分别保留各自的 Proof，不能从一个全局“最后请求”状态推断。只有返回 `XHTTP_DIGEST_INFO_VALID` 后，上层才应采用 `nextnonce`；`auth-int` 还必须传入响应实体摘要。缺少 `rspauth` 返回普通 `INVALID`，由应用决定是否允许没有双向证明的服务器响应。

### Digest 客户端会话

`http_auth_digest_session` 是建立在无状态客户端协议层之上的可选状态层。它适合连接池、异步客户端和并发请求，不替代底层 `xrtHttpDigestClientAuth`。会话只保存 challenge、协商结果、规范 Secret、用户名、cnonce 和下一 nonce-count；全部输入在创建或更新时按实际长度复制，不使用固定 8K 缓冲。

```c
xhttpdigestsessionconfig config = {
	0,
	&challenge,
	&choice,
	XRT_STR_INIT("user"),
	{ NULL, 0 },
	secret,
	XRT_STR_INIT("client-nonce")
};
xhttpdigestsession* session = xrtHttpDigestSessionCreate(&config);
xhttpdigestexchange* exchange = xrtHttpDigestSessionAuthorize(
	session,
	XRT_STR_LITERAL("GET"),
	XRT_STR_LITERAL("/private"),
	(xstrview){ NULL, 0 }
);
const xhttpdigestauth* auth = xrtHttpDigestExchangeAuth(exchange);
```

`xrtHttpDigestSessionAuthorize` 在短临界区内为每个请求保留唯一 `nc`，然后返回不可变 Exchange。Exchange 同时公开可直接写入 Authorization 的 `xhttpdigestauth` 和校验响应使用的 `xhttpdigestproof`，并持有原状态快照与会话引用；因此请求可以跨线程完成，响应也可以乱序到达。授权前的分配失败不消耗计数。`UINT32_MAX` 可正常使用一次，之后该 nonce 状态拒绝继续授权，必须通过新 challenge 或 `nextnonce` 更新。

`xrtHttpDigestSessionAccept` 先校验 `rspauth`、qop、cnonce 和 nc。没有 `nextnonce` 时返回 `VALID`；当前状态对应的有效 `nextnonce` 被原子采用并把计数重置为 1，返回 `UPDATED`；已经被另一响应或显式更新取代的旧 Exchange 返回 `SUPERSEDED`，不会让会话回滚。`NextCnonce` 为空表示沿用原 cnonce。无效证明返回 `INVALID`，参数、分配或底层错误才返回 `ERROR`。Update 与 Accept 都先完整构造新状态再提交，OOM 后旧状态保持不变并可重试。

会话的普通操作线程安全；每个并发调用必须持有有效会话引用。`xrtHttpDigestSessionRelease` 不能与使用同一份最后引用的调用并发。Exchange 是不可变引用对象，`Auth` 和 `Proof` 指针只在 Exchange 生命周期内有效。长期 Secret 和 Exchange 内的派生结果在最后释放时使用 `xrtSecureZero` 清除。

启用 `http_client_prepare_auth_digest_session` 后，`xrtHttp1RequestPrepareDigest` 和代理对称入口会使用准备器最终选定的 HTTP/1 request-target 创建 Exchange，并把认证字段只写入冻结计划。这样不需要先猜测 URL 将采用哪种 target form，也不会修改可复用的请求构建器。请求准备在凭据创建后的分配失败可能留下合法的 `nc` 跳号，但绝不会复用计数或发布半成品。

响应侧的 `http_client_response_auth_digest_session` 继续复用公开回执解析器。`xrtHttpResponseDigestSessionAccept` 以 `xhttpnext` 区分字段缺失、存在和读取失败，并仅在字段存在时发布 `xhttpdigestsessioncheck`；代理入口保持对称。它按实际解码长度申请临时缓冲，验证后立即清零释放。缺失回执不会被强制视为协议错误，应用仍可按自己的双向认证策略选择接受或拒绝。

### 无状态 nonce

`http_auth_digest_nonce` 提供不持有全局状态的服务端 nonce 底座。线路值固定为 76 字节无填充 Base64URL，内部包含版本、Unix 秒、16 字节 salt 和完整 HMAC-SHA256。HMAC 绑定调用方提供的 `Context`；通常至少使用 realm，也可以使用虚拟主机、部署标识或其他稳定作用域。Key 必须不少于 32 字节。

```c
uint8 key[32] = { /* 从受保护配置读取 */ };
char nonce[XRT_HTTP_DIGEST_NONCE_TEXT_SIZE];
size_t size;

xrtHttpDigestNonceCreate(
	(xbytesview){ key, sizeof(key) },
	XRT_BYTES_LITERAL("api@example.test"),
	xrtTimeUnix(xrtNow()),
	nonce,
	sizeof(nonce),
	&size
);
```

底层 `xrtHttpDigestNonceWrite` 接受调用方给定的 16 字节 salt，适合确定性测试、外部随机源或由应用统一管理熵的环境；可独立裁剪的 `xrtHttpDigestNonceCreate` 才依赖操作系统安全随机源。两者都支持固定长度查询、失败原子短缓冲和别名检查，实际生成不分配内存。

`xrtHttpDigestNonceVerify` 返回四态结果：`VALID` 表示签名和时间窗口有效，`STALE` 表示签名有效但超过 lifetime，`INVALID` 表示线路、版本、签名、上下文或未来时间不合法，`ERROR` 才表示调用参数或底层库失败。未来容差和 lifetime 都以秒为单位；边界值仍有效。只有 `VALID` 与 `STALE` 会发布签名覆盖的签发时间。

无状态 nonce 只能证明值由服务器签发并限制有效期，不能独自阻止有效期内的请求重放。密钥轮换可以先用当前 key 验证，失败后再尝试仍处于轮换窗口的旧 key。

### Digest 证明验证

`http_auth_digest_verify` 把服务端最容易写错的绑定规则收敛为零分配验证器。`xhttpdigestverification` 同时引用已经解析的凭据、服务器实际发布的 challenge、已查得的 `Secret`、请求方法、原始 request-target 和可选实体摘要。`xrtHttpDigestProofVerify` 会验证算法、realm、nonce、opaque、qop、`userhash`、`username*`、请求 target 与 request-digest，但有意不决定 nonce 的签名和生命周期，适合 key-ring、外部 nonce 或自定义认证策略。

```c
xhttpdigestverification verification = {
	0,
	&auth,
	&challenge,
	secret,
	request_method,
	request_target,
	entity_hash
};

xhttpdigestverifycheck check = xrtHttpDigestVerify(
	&verification,
	nonce_key,
	nonce_context,
	now_seconds,
	300,
	5,
	&issued_seconds
);
```

组合入口 `xrtHttpDigestVerify` 还验证内置 HMAC nonce，返回 `VALID`、`STALE`、`INVALID` 或 `ERROR`。只有 `ERROR` 表示 API 或底层失败；普通认证失败不污染线程错误。`STALE` 只会在 nonce 签名有效且 request-digest 也正确时返回，错误密码不能借过期 nonce 探测 stale 策略。`IssuedSeconds` 只在 `VALID` 或 `STALE` 时写入。

challenge 声明 `opaque` 时凭据必须原样返回；客户端 qop 必须属于 challenge 的能力集合；`uri` 必须逐字节等于请求行 target。challenge 未声明 UTF-8 时拒绝 `username*`，未声明 `userhash=true` 时拒绝散列用户名。服务器可以接受 RFC 7616 允许的明文用户名回退，也可以设置 `XHTTP_DIGEST_VERIFY_REQUIRE_USERHASH` 强制使用散列用户名。

验证器不查用户、不持有密码、不缓存正文，也不提交重放状态。应用先按 `Username` 和 `XHTTP_DIGEST_AUTH_USERHASH` 查得持久化 Secret；`auth-int` 正文较大时自行流式计算 `EntityHash`。证明返回 `VALID` 后，再把 `Username + Nonce + Cnonce + nc` 交给内置重放表或外部原子存储。需要密钥轮换时，可以先调用纯证明入口一次，再依次使用 `xrtHttpDigestNonceVerify` 尝试当前与旧 key，避免复制协议绑定逻辑。

### Digest 重放防护

`http_auth_digest_replay` 是独立裁剪的 nonce-count 策略层。`xrtHttpDigestReplayKey` 对 `username`、`nonce` 和 `cnonce` 分别加入 64 位长度边界，再使用带版本域的 SHA-256 得到固定 32 字节键。相似拼接不能产生字段歧义。该键既可交给内置表，也可作为外部数据库或共享缓存执行原子“新 nc 大于旧 nc”操作的规范输入；它用于稳定索引，不是密码或凭据加密。

```c
xhttpdigestreplayconfig config;
xhttpdigestreplay* replay;
xhttpdigestreplaycheck check;

xrtHttpDigestReplayConfigInit(&config);
config.LifetimeSeconds = 300;
replay = xrtHttpDigestReplayCreate(&config);

check = xrtHttpDigestReplayCheck(
	replay,
	auth.Username,
	auth.Nonce,
	auth.Cnonce,
	auth.NonceCount,
	issued_seconds,
	now_seconds
);
```

内置表按 `Shards * EntriesPerShard` 设置固定硬容量，每个分片由独立 Mutex、创建时分配的开放寻址槽和过期小根堆组成。记录只保存不可逆键、最大 `nc` 和固定过期时间；创建后的键派生、首次插入、已有键推进、查询和过期清理都不分配，过期清理为每条 `O(log n)`，并发请求不会让同一 `nc` 出现多个胜者。默认配置为 16 个分片、每片 1024 条和 300 秒有效期；应用必须让 `LifetimeSeconds` 与 nonce 验证策略一致。

`xrtHttpDigestReplayCheckKey` 和便利入口 `xrtHttpDigestReplayCheck` 返回 `ACCEPTED`、`REPLAY`、`EXPIRED`、`FULL` 或 `ERROR`。只有 `ERROR` 设置库错误。容量满时实现不会驱逐尚未过期记录，因为那会重新打开重放窗口；服务端应拒绝或暂时失败该请求，并通过 `xrtHttpDigestReplayStats` 观察容量。`xrtHttpDigestReplayPurge` 可主动清理，`Clear` 保留已分配容量与累计统计。对象操作线程安全，销毁不能与其他操作并发。

重放提交必须放在 `xrtHttpDigestVerify` 返回 `VALID` 之后。若先推进 `nc`，错误 `response` 可以抢占合法客户端的计数。完整顺序应为：解析凭据并查得 Secret，验证 challenge、nonce、请求绑定和摘要，最后调用重放检查。多进程或分布式服务不应各自使用孤立内置表，而应以公开重放键在共享存储中执行带有效期的原子最大值更新。

## 客户端与服务端

客户端请求层提供 `xrtHttpRequestSetAuth`、`xrtHttpRequestSetBasicAuth`、`xrtHttpRequestSetBearerAuth`、`xrtHttpRequestSetDigestAuth` 及对应代理入口。设置操作会折叠同名字段；临时认证值在 Header 容器拥有副本后立即清零。跨 origin 重定向仍默认移除源站与代理认证字段。

客户端响应的通用层使用 `xrtHttpResponseChallengeNext` 和 `xrtHttpResponseProxyChallengeNext`，不会错误地把多条 `WWW-Authenticate` 合并成一条不可定位文本。Basic、Bearer 和 Digest 结构化层分别提供同名 `ChallengeNext` 入口及代理对称入口；它们跨字段过滤 scheme，并复用本页的协议解析器，不维护第二套参数语法。`xrtHttpResponseDigestInfo` 与代理入口读取唯一回执字段，重复字段按协议错误处理。

结构化响应入口采用可重试的两遍缓冲契约。`pOutput == NULL` 且容量为零时完成验证、返回所需解码字节数，但不推进游标；实际缓冲写入成功或确认到达末尾后才提交游标。短缓冲和畸形匹配项都保留当前 challenge，结果保持为空，错误原因被包装到稳定的客户端响应认证错误域。调用方仍可退回通用迭代器读取未知方案和扩展参数，不受结构化 Helper 限制。

服务端请求使用通用、Basic、Bearer 和 Digest 四组认证入口。缺失返回 `XHTTP_NEXT_END`，重复凭据或无效值返回 `XHTTP_NEXT_ERROR`。Reply 可追加任意通用 challenge，或使用 Basic、Bearer、Digest 的结构化入口；Digest `Authentication-Info` 使用设置语义保证唯一字段。所有代理入口保持对称，Reply 拥有写入后的字段，也不替应用修改 401/407 状态。

## 裁剪与验证

- 通用认证、Basic、Bearer token、Bearer challenge、Digest 元数据、challenge、凭据、回执、SHA-2 计算、MD5 兼容后端、客户端协议层、客户端会话、HTTP/1 会话准备适配器、响应会话适配器、确定性 nonce、安全随机 nonce、证明验证、重放防护及各对象适配器都有独立 `XRT_MODULE_*` 裁剪根。Basic 单独依赖 Base64，Bearer token 不引入 Base64 或 URL，只有 Bearer challenge 的 `error_uri` 语义层依赖 URL。Digest 语法层不引入密码学；SHA-2 根不引入 MD5；客户端协议层不引入网络、请求对象或随机源；客户端会话只额外依赖 Mutex；HTTP/1 适配器只组合会话与请求准备；响应会话适配器只组合会话与原始回执读取；确定性 nonce 不引入系统随机源；证明验证不引入网络或容器；重放防护只增加 SHA-256 与 Mutex，不引入通用 Map。
- 模块化与单头文件测试覆盖正常值、官方向量、双向证明、重复字段、畸形输入、未对齐描述符、地址回绕、游标失败原子性、短缓冲和敏感临时副本清零；对象层闭环测试把 challenge、客户端凭据、服务端组合验证、重放提交和 `rspauth` 回执串成完整往返。客户端协议层另有协商策略、userhash、`username*`、`auth-int`、响应证明和零分配测试；客户端会话另有乱序 nextnonce、跨会话误用、精确小对象分配、线程竞争和 OOM 事务性测试；HTTP/1 会话准备另有最终 target 绑定、源站/代理字段替换、请求不变性和逐分配点 OOM 测试；响应会话适配器另有缺失、无效、乱序、源站/代理、未对齐、别名和双分配点 OOM 测试；证明验证另有 challenge/request 绑定、`auth-int`、stale 不可伪造和零分配测试；重放表另有硬容量、过期、线程竞争和 OOM 原子性测试。
- 通用解析、字段迭代和直接写出在故障分配器下验证零分配；分配型 Build 具有独立 OOM 原子性测试。
- `tests/http/test_http_auth_fuzz.c` 使用固定种子覆盖通用认证、Basic、Bearer、Digest challenge、Digest 凭据和 `Authentication-Info`；`tools/test_protocol_fuzz.py auth` 使用同一入口构建 Clang/libFuzzer、ASan 与 UBSan 门禁。
