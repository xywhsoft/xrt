---
num: 80
slug: x509-verify
title: X.509 签名验证与服务身份
volume: 卷八 安全
type: practice
lead: 签名在数学上成立吗？主体身份匹配吗？——验证的前两问；第三问"该信任吗"留给信任链。
api: x509, crypto
---

## 导读

第 79 章的解析器告诉你证书**说了什么**；本章的两个验证器回答前两个"是真是假"的问题：**签名验证**——这张证书的签名用它声称的算法与签发者公钥去验，数学上成立吗（`xrtX509CertificateVerify`，底层是协议分派的 `xrtX509SignatureVerify`）；**服务身份**——证书声称的身份与你要连的主机名匹配吗（RFC 9525 的 `xrtX509MatchHost/MatchDns`）。第三问"这个签发者值得信任吗"需要链构建与信任库——那是第 81 章的主题，本章在概念上划清边界。两个验证器都是零分配借用式路径；后端按算法族裁剪（RSA/ECDSA/Ed25519 各自独立），只验证 ECDSA 证书的固件不必编入 RSA 代码。

## 引入

考虑一次 HTTPS 连接里的验证时序：收到证书 → 解析（第 79 章）→ 验证签名（本章）→ 匹配主机名（本章）→ 沿 Issuer 找上级证书、逐级验到信任锚（第 81 章）。如果只做前两步会发生什么？攻击者可以**自签一张证书**：CN 和 SAN 都写 `bank.example.com`，用自己生成的密钥对给自己签名——签名验证 100% 通过（数学上完全成立！），身份匹配也通过（SAN 确实写着目标域名）。缺的那一环是"信任"——签名成立只证明"某个持有对应私钥的人签了它"，**至于是不是你认可的 CA 签的**，签名本身无法回答。

反过来，只做信任链不验签行不行？更不行——链的每一环就是"用上级公钥验证下级签名"。所以这三问是串联的闸门，本章实现前两问、并把第三问的接口边界讲清楚。理解了"每问各挡什么攻击"，你就永远不会写出"验签通过就 accept"这种经典漏洞代码。

## 概念

### 签名验证的分层：协议分派 + 密码后端

验证入口分两层。**便利层** `xrtX509CertificateVerify(证书, 签发者证书)`：解析证书的签名方案与签发者 SPKI，调用底层完成验证——最常见的"用父证书验子证书"一步到位。变体 `xrtX509CertificateVerifyKey(证书, 公钥)`：接受已提取或信任配置直接给的公钥——验证独立信任锚（-pin 形态的公钥钉扎）时不必伪造证书对象。**底层** `xrtX509SignatureVerify(方案, 内容, 签名, 公钥)`：接受公开协议模型——证书、CRL、OCSP 或你自己的签名对象通用。

后端按算法族独立裁剪：`XRT_FEATURE_X509_VERIFY_RSA`（PKCS#1 v1.5 与 PSS，SHA-1/224/256/384/512 全摘要）、`XRT_FEATURE_X509_VERIFY_ECDSA`（严格 DER r,s 解码；按 SPKI 曲线选 P-256/P-384；摘要按 bits2int 进曲线底座——不是死板的"P-256 配 SHA-256"）、`XRT_FEATURE_X509_VERIFY_ED25519`。**没有编入的后端返回 `XERR_UNSUPPORTED`**——明确"我不支持这个算法"，绝不伪装成"签名不匹配"；已解析但无后端的 P-521/Ed448 同样如此。这个区分对调试与策略都关键：不匹配是安全问题，不支持是配置问题。

### 验证什么、不验证什么：Verify 的边界

`CertificateVerify` 只验证 TBSCertificate 的**密码签名**。它明确不做的事：不比较 issuer/subject Name（链的匹配是第 81 章的职责）、不检查时间、不检查 BasicConstraints/KeyUsage、不检查未知 critical 扩展、不查吊销、不判断信任锚。这条边界让根证书健康检查（自签证书自验）、CA 材料审计（离线验每张证书的签名完整性）可以复用同一个入口——它们只需要数学，不需要策略。

### 服务身份：RFC 9525 的匹配规则

身份匹配回答"这张证书是不是给 `api.example.test` 的"：

- `xrtX509MatchHost(证书, 主机名, &呈现身份)`：证书入口——只读 **SubjectAltName** 的 DNS-ID 与 IP-ID，**完全不读 CN**。返回三态：`X509_VALUE`（匹配，可选产出实际命中的 SAN）、`X509_DONE`（引用身份合法但没有匹配——**TLS 层必须当握手失败**）、`X509_ERROR`（引用身份或 SAN 结构非法）。
- `xrtX509MatchDns(模式, 主机名)`：纯字符串形态的 DNS 匹配——通配符规则内建：**只匹配最左一个完整标签**（`*.example.test` 命中 `api.example.test`、不命中 `a.b.example.test`）、只在左标签出现、大小写不敏感 ASCII、结尾根点规范化（`example.test.` 等价 `example.test`）。无效 presented DNS-ID 被忽略而不是报错。
- IP 身份：IPv4/IPv6 文本经 `xrtNetAddrParse` 严格解析后与 iPAddress SAN 做 4/16 字节**精确比较**（支持 URL 常见的方括号 IPv6；Scope ID 拒绝）。注意"由 DNS 解析得到的连接地址"仍应匹配原 DNS-ID，不是解析后的 IP。

手工实现这些规则是历史漏洞重灾区——`*.example.test` 匹配了 `a.b.example.test`、通配符出现在中间、大小写敏感比较导致漏配——内建入口一次消灭整类问题。

### 验证路径的工程形态

一次完整的"前两问"验证（TLS 客户端收到证书后的最小动作）：

```diagram flow
- 解析：Parse 产出视图（结构合法性由解析器保证）
- 签名：CertificateVerify(证书, 签发者证书) —— 数学成立？
- 身份：MatchHost(证书, 期望主机名) == X509_VALUE —— 名字对上？
- 时间：ValidAt(证书, now) —— 没过期？
- （第 81 章）链：沿 Issuer 找到信任锚 + 吊销检查
```

注意"时间"混在其中——它既不是签名也不是身份，但作为独立检查项总在同一时刻执行；第 81 章的路径验证把这三项与链构建、策略一起编排。

## 示例

### 第一个完整程序：自签名证书的签名自验

下面的程序来自 `examples/x509/verify/main.c`，解析一张真实自签名 RSA 证书并验证其签名：

```embed path="examples/x509/verify/main.c" title="examples/x509/verify/main.c"
```

```term
$ gcc -O1 -DXRT_MODULE_ALL -I single -I . -include xrt.h impl.c examples/x509/verify/main.c -lws2_32 -liphlpapi
certificate signature is valid
```

**刚才发生了什么。** ① 输入是测试夹具里的真实证书字节（`tests/fixtures/x509_legacy_cert.h`）——比 inspect 的玩具证书大得多，走的是完整解析校验。② 两次传同一张证书：`CertificateVerify(&Cert, &Cert)`——待验证者与"签发者"都是它自己（自签名的定义就是"自己给自己签，公钥自带"）。验证通过输出的是**数学事实**：这个签名确实由这个公钥对应的私钥生成。③ 失败分支打印 `xrtErrorMessage(xrtGetError())`——验证失败的原因链（`xrt.x509` 的签名错误、`xrt.crypto` 的底层原因）直接给人看；程序化判断用第 4 章的类别查询。④ 注意这里**没有**任何"信任"判断——自签证书验签通过是正常现象（每个根证书都自签），它能否被信任是第 81 章信任库的事。

### 第二个完整程序：通配符身份匹配

第二个程序来自 `examples/x509/identity/main.c`，验证 `*.example.test` 是否匹配 `api.example.test`：

```embed path="examples/x509/identity/main.c" title="examples/x509/identity/main.c"
```

```term
$ gcc -O1 -DXRT_MODULE_ALL -I single -include xrt.h impl.c examples/x509/identity/main.c -lws2_32 -liphlpapi
matched=yes
```

**刚才发生了什么。** ① `xrtX509MatchDns` 是纯函数形态——证书模式与引用主机名都是字符串视图，不需要证书对象；证书场景由 `MatchHost` 内部对每条 SAN DNS-ID 调用同样的规则。② 命中演示的是最核心的通配符边界：`*` 吞掉且只吞掉**一个最左完整标签**——改用 `a.b.example.test` 会得到 `X509_DONE`（合法但未匹配），改用 `api.example.org` 同样 DONE。③ 大小写不敏感在这里自动成立（`API.EXAMPLE.TEST` 也命中）——ASCII 标签级比较，不做 Unicode 折叠（U-label 该由 IDNA 层先转 A-label）。把这三组变体都跑一遍，是你理解 RFC 9525 边界最快的路径。

## 契约

- **分层裁剪**：基础 `x509_verify` 不拉入密码算法；`XRT_FEATURE_X509_VERIFY_RSA/ECDSA/ED25519` 按证书族启用；未启用算法返回 `XERR_UNSUPPORTED`（不伪装签名不匹配）。
- **便利入口**：`CertificateVerify(证书, 签发者证书)` 与 `CertificateVerifyKey(证书, 公钥)` 只验 TBSCertificate 密码签名——不查名称匹配、时间、约束、吊销、信任。
- **底层入口**：`SignatureVerify(方案, 内容, 签名, 公钥)` 接受公开协议模型——证书/CRL/OCSP/自定义对象通用；方案来自第 79 章的算法翻译层。
- **RSA 规则**：PKCS#1 v1.5 与 PSS 全摘要；id-RSASSA-PSS 公钥只验 PSS 且参数一致、盐长不小于公钥声明下限；无参 PSS 公钥不加限制。
- **ECDSA 规则**：严格 DER r,s 解码；曲线来自 SPKI；摘要按 bits2int 进曲线底座。
- **身份规则**：RFC 9525——只读 SAN 的 DNS-ID/IP-ID、不读 CN；通配符仅最左单标签；ASCII 大小写不敏感；IP 精确比较、Scope ID 拒绝。
- **三态语义**：`MatchHost` 的 `X509_VALUE` 匹配 / `X509_DONE` 合法未匹配（TLS 必须当失败）/ `X509_ERROR` 结构非法。
- **失败形态**：验证失败返回 `false` 并设 `xrt.x509` 错误，密码原因经 `xrtErrorCause` 保留 `xrt.crypto`；有效路径栈摘要+借用视图零堆分配。

## 避坑

### 坑 1：验签通过就信任

症状：代码里 `CertificateVerify` 成功即 accept——攻击者用自签证书（CN/SAN 全填目标域名）直接通过：签名数学上完全成立。

原因：验证签名的答案是"私钥持有者签了它"，不是"你认可的 CA 签了它"。信任是链与信任库的判断（第 81 章）。

```c bad
if ( xrtX509CertificateVerify(&Cert, &Issuer) ) {
	accept_connection();   /* 数学成立 ≠ 可信：自签也成立 */
}
```

```c good
/* 签名 + 身份 + （第 81 章）链到信任锚，三问全过才 accept */
if ( xrtX509CertificateVerify(&Cert, &Issuer) &&
		xrtX509MatchHost(&Cert, XRT_STR_LITERAL(sHost),
			&Presented) == X509_VALUE &&
		xrtX509ValidAt(&Cert, xrtNow()) &&
		trusted_to_anchor(&Cert /* 第 81 章 */) ) {
	accept_connection();
}
```

### 坑 2：把 X509_DONE 当成功（或当崩溃）

症状：主机名没匹配上（DONE），调用方只判 `!= X509_ERROR` 就继续——错误的服务放行；或者对 ERROR 做了断言崩溃——非法 SAN 输入变成可用性故障。

原因：三态里 `DONE` 是"合法但没有匹配"——**对 TLS 而言就是身份验证失败**，必须拒绝；`ERROR` 是"输入结构非法"——同样拒绝，但属于可报告的协议错误而不是崩溃。

```c bad
if ( xrtX509MatchHost(&Cert, XRT_STR_LITERAL(sHost),
		&Presented) != X509_ERROR ) {
	accept();   /* DONE 也进来了：身份未匹配被放行 */
}
```

```c good
switch ( xrtX509MatchHost(&Cert, XRT_STR_LITERAL(sHost),
		&Presented) ) {
case X509_VALUE:
	accept();           /* 唯一放行路径 */
	break;
case X509_DONE:
	reject("identity mismatch");   /* 合法但不是这台主机 */
	break;
default:
	reject("malformed identity");  /* 结构非法：报告后拒绝 */
	break;
}
```

### 坑 3：身份匹配回退到 CN

症状：对没有 SAN 的老证书（v1/v2 或早期 v3）连接仍然成功——代码在 SAN 未匹配时回退去比 CN；攻击者用一张 CN 写目标域名、无 SAN 的证书通过验证。

原因：RFC 9525 明确废止 CN 回退。无 SAN 的证书在现代 TLS 里就该身份失败——"向后兼容"在这里等于打开伪造通道。

```c bad
if ( xrtX509MatchHost(&Cert, ...) != X509_VALUE ) {
	/* 回退 CN：伪造证书的 CN 可以是任何字符串 */
	if ( cn_equals_host(&Cert, sHost) ) { accept(); }
}
```

```c good
/* 没有 SAN：身份验证失败，结束。
   例外仅限你自己完全控制证书的内部系统——那就用
   CertificateVerifyKey 做公钥钉扎，比 CN 更诚实 */
if ( xrtX509MatchHost(&Cert, XRT_STR_LITERAL(sHost),
		&Presented) != X509_VALUE ) {
	reject("no SAN match; CN fallback is forbidden");
}
```

## 练习

### 基础：三态匹配矩阵

对 `xrtX509MatchDns` 构造六组输入：精确命中、大小写变体命中、通配符单标签命中、通配符多标签未命中、后缀不同未匹配、结尾带根点的命中。逐组打印三态结果。验收标准：六组结果与 RFC 9525 规则手推一致；理解为什么"通配符多标签"是 DONE 而不是 ERROR。

### 进阶：公钥钉扎验证器

实现 `verify_pinned(证书 DER, 主机名, 钉扎公钥 SPKI)`：解析 → `CertificateVerifyKey` 用钉扎公钥验签 → `MatchHost` 匹配主机名 → `ValidAt` 检查时间，三问通过返回 true。提示：钉扎场景不需要证书链——公钥本身就是信任锚。验收标准：用自生成的密钥对与自签证书走通正反两路（换钉扎密钥必须失败）。

### 挑战：验证后端探测报告

对一批证书（RSA-PKCS1、RSA-PSS、ECDSA P-256、ECDSA P-384、Ed25519 各一张，OpenSSL 生成）写探测工具：解析算法与公钥类型、尝试 `CertificateVerify`（自签自验）、把 `XERR_UNSUPPORTED` 与签名不匹配区分打印。再分别在"全后端"与"只编 ECDSA"两种裁剪下运行，对照输出。验收标准：裁剪前后，已支持算法的结论不变、未支持算法从"valid"变为清晰的 UNSUPPORTED 报告——证明裁剪不产生静默的假阴性。

## 速查

| 知识点 | 速查 |
| --- | --- |
| 三问模型 | 签名（本章）→ 身份（本章）→ 信任链（第 81 章）；串联闸门缺一不可 |
| 便利入口 | `CertificateVerify(证书, 签发者)` / `VerifyKey(证书, 公钥)`——只验数学签名 |
| 底层入口 | `SignatureVerify(方案, 内容, 签名, 公钥)`——证书/CRL/OCSP/自定义通用 |
| 后端裁剪 | XRT_FEATURE_X509_VERIFY_RSA / _ECDSA / _ED25519 独立；未启用 → UNSUPPORTED |
| RSA 规则 | v1.5+PSS 全摘要；PSS 公钥参数一致、盐长下限；无参公钥不限制 |
| ECDSA 规则 | 严格 DER r,s；曲线随 SPKI；摘要按 bits2int 进曲线 |
| 身份入口 | `MatchHost`（证书+主机名）三态 / `MatchDns`（纯字符串）同规则 |
| 通配符 | 仅最左单完整标签；ASCII 大小写不敏感；结尾根点规范化 |
| 三态铁律 | VALUE 才放行；DONE=身份失败必须拒绝；ERROR=结构非法报告后拒绝 |
| CN 边界 | 永不回退 CN；无 SAN 即身份失败；内部系统改用公钥钉扎 |
