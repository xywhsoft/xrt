---
num: 83
slug: tls-identity
title: TLS 身份：证书、私钥与签名
volume: 卷八 安全
type: practice
lead: 把"一张证书链 + 一把私钥"打包成不可变共享对象——握手期 CertificateVerify 的签名引擎与它的硬件扩展口。
api: tls, tls_identity, crypto
---

## 导读

TLS 有两方要"证明自己"：客户端证明"我信任谁"（验证器，第 85 章），服务端证明"我是谁"（**身份**，本章）。`xtlsidentity` 把一张 DER 证书链和一把私钥打包成**不可变的共享对象**：创建时深复制全部材料并完成交叉验证（私钥真的对应证书里的公钥吗？这套证书能签哪些 TLS 方案？），之后多线程、多服务端配置并发共享，握手时用它产出 CertificateVerify 签名。四种强类型构造器（RSA/P-256/P-384/Ed25519）覆盖主流服务端；`xrtTlsIdentityCreate` 的扩展接口把签名能力外包给 HSM、系统密钥库或远程签名器。客户端证书认证当前未实现（第 82 章契约），所以身份今天就是**服务端**的概念——但对象本身与角色解耦。

## 引入

部署一个 TLS 服务端要准备什么？一张证书链（叶证书 + 中间 CA，PEM 文件）和一把私钥。麻烦从装载开始：私钥的 DER 格式有好几种（PKCS#1、PKCS#8、SEC1 裸标量、Ed25519 种子），证书里的公钥算法要和私钥匹配，RSA-PSS 证书还有参数约束（摘要、MGF1、盐长下限）——任何一处不匹配，握手期签名才会失败，那时你面对的是凌晨三点的告警而不是启动时的清晰报错。

`xtlsidentity` 的答案是**创建期全量验证**：构造器解析私钥、核对证书 SPKI、检查两者在 TLS 各版本的共同签名方案——"没有任何共同 TLS 方案的身份在构造时直接拒绝"。启动时炸比握手时炸好一万倍，这是部署体验与安全体验的同一件事。验证通过的身份证实不可变：证书链深复制在单块紧凑分配里（释放前安全清零），借用视图有效期到最后一个引用释放——多 Worker 共享同一个对象，零锁。

## 概念

### 四种构造器与它们的私钥方言

| 构造器 | 私钥输入形态 | 创建期验证 |
| --- | --- | --- |
| `xrtTlsIdentityRsa` | PKCS#1 或未加密 PKCS#8 DER | 模数、指数、全部 CRT 参数、叶 SPKI、RSA-PSS 限制 |
| `xrtTlsIdentityP256` | 32 字节标量 / SEC1 / 未加密 PKCS#8 | 标量范围、曲线 OID、可选 SEC1 公钥、叶 P-256 SPKI |
| `xrtTlsIdentityP384` | 48 字节标量 / SEC1 / 未加密 PKCS#8 | 同上（P-384） |
| `xrtTlsIdentityEd25519` | 32 字节种子 / 单双层 DER OCTET / RFC 8410 PKCS#8 | 算法参数缺省、派生公钥、叶 Ed25519 SPKI |

三个共性：**证书链第一张必须是叶**；**私钥不被借用**（深复制进身份，调用方缓冲可立即复用）；**RSA 保留完整 CRT 视图、Ed25519 保留一次展开的签名密钥**——握手路径不再重复派生。PEM/文件/系统库不属于身份核心：先用第 78 章的 PEM 解码拿 DER，再交给构造器——层与层之间只传 DER。

### CanSign：三重能力查询

`xrtTlsIdentityCanSign(身份, 版本, 方案)` 回答"这个身份能不能用这个方案签"——同时检查 TLS 版本限制、证书身份类型、标准方案与后端编入。规则要点：TLS 1.3 不接受 RSA-PKCS#1 的 CertificateVerify（现代协议的硬规定）；ECDSA 方案必须与证书曲线一致；`rsaEncryption` 与 `RSASSA-PSS` 证书分别只进 `rsa_pss_rsae_*` 与 `rsa_pss_pss_*` 路径；受限 PSS 密钥还要同时满足证书与私钥两侧的参数约束。策略（第 85 章）的签名白名单与 `CanSign` 取交集才是握手实际方案——本章示例的 `exampleSignature` 演示按身份类型选常用方案。

### Sign：两段式签名与确定性

`xrtTlsIdentitySign(身份, 版本, 方案, 内容, 输出, 容量, &长度)` 接收**完整 TLS 待签内容**（协议上下文头由调用方构造——TLS 1.3 的 64 空格角色前缀在公开原语 `xrtTls13CertificateVerifySignature` 层处理）。两段式与全库惯例一致：空输出查询精确长度、容量不足不调签名器、失败不发布部分签名。三种后端的随机性策略：RSA-PSS 用密码安全随机盐；ECDSA 用 RFC 6979 确定性 low-S 签名（第 76 章讲过为什么确定性是优点）；Ed25519 纯模式本身确定。

### Create：硬件与远程签名器

真实企业环境里私钥常在 HSM 或系统密钥库里，不能导出成 DER。`xrtTlsIdentityCreate` 是扩展接口：证书链仍由 XRT 深复制，签名能力通过 `Supports`/`Sign` 回调外包——两个回调必须允许并发调用，`Supports` 是不设错误的能力谓词，`Sign` 失败保持输出不变。没有硬件需求就用内置强类型构造器——回调口是为可插拔准备的，不是日常路径。

### 生命周期：从装载到共享

```diagram flow
- 装载：PEM/文件 → DER（第 78 章），证书链 + 私钥交给构造器
- 验证：私钥解析 + SPKI 交叉核对 + 共同方案检查（失败即拒绝）
- 共享：不可变对象被服务端配置/多 Worker 并发引用（Retain/Release）
- 销毁：最后一个引用释放 → 全部密钥材料安全清零 → Release 回调恰好一次
```

`Retain`/`Release` 引用计数；最后一个引用释放时安全清零全部密钥材料并调用 `Release` 回调（Create 形态）。身份不可变意味着：多服务端配置共享、多 Worker 并发签名、与策略/上下文任意组合——**装配一次，处处共享**，与第 22 章对象池的"只读共享最便宜"同一哲学，但这里共享的是不可变值而非可复用槽。

`Retain`/`Release` 引用计数；最后一个引用释放时安全清零全部密钥材料并调用 `Release` 回调（Create 形态）。身份不可变意味着：多服务端配置共享、多 Worker 并发签名、与策略/上下文任意组合——**装配一次，处处共享**，与第 22 章对象池的"只读共享最便宜"同一哲学，但这里共享的是不可变值而非可复用槽。

## 示例

### 第一个完整程序：四种密钥形态构造与类型查询

下面的程序来自 `examples/tls/identity/main.c`，无参数时用嵌入材料自检四种身份构造器：

```embed path="examples/tls/identity/main.c" title="examples/tls/identity/main.c"
```

```term
$ gcc -O1 -DXRT_MODULE_ALL -I single -include xrt.h impl.c examples/tls/identity/main.c -lws2_32 -liphlpapi
embedded rsa identity=1 certificates=1
embedded p256 identity=3 certificates=1
embedded p384 identity=4 certificates=1
embedded ed25519 identity=5 certificates=1
```

**刚才发生了什么。** ① 四种形态各走一个构造器：RSA（1）、P-256（3）、P-384（4）、Ed25519（5）——`IdentityType` 的枚举值与 `xtlsidentitytype` 对应，跨平台稳定。② 每次构造即全量验证：私钥解析、SPKI 交叉核对、共同方案检查——嵌入材料全部合法所以构造通过；换成证书私钥不匹配的一对，构造直接失败并给出结构化错误（identity 错误码 + DER/X.509/crypto 原因链）。③ `CertificateCount` 确认链长（这里每形态一张叶）；真实部署传中间 CA 的完整数组。④ 有参数时程序切换到"读 DER 文件 → 构造 → 两段式 Sign"的完整路径——`exampleSignature` 按类型选方案（RSA 走 PSS、Ed25519 直签），空输出查询长度、分配、再签，打印签名字节数（Ed25519 恒 64，RSA 等于模长）。⑤ 每个身份用完 `Release`——不可变共享对象的生命周期管理只有这一对调用。

### 第二个完整程序：签名方案的底层域

第二个程序来自 `examples/crypto/sign_tour/main.c`，演示身份签名所依赖的底层方案族——Ed25519 三域与 ECDSA 的 DER 表示：

```embed path="examples/crypto/sign_tour/main.c" title="examples/crypto/sign_tour/main.c"
```

```term
$ gcc -O1 -DXRT_MODULE_ALL -I single -include xrt.h impl.c examples/crypto/sign_tour/main.c -lws2_32 -liphlpapi
sign: ed25519 pure sign->verify ok
sign: ed25519 context + prehash modes ok
sign: ecdsa-p256 sign->verify(der) roundtrip ok
sign: ecdsa-p384 sign->verify(der) roundtrip ok
sign: der r||s <-> der roundtrip ok
```

**刚才发生了什么。** ① **Ed25519 三域互不兼容**：PURE（直接签消息）、CONTEXT（带上下文字符串——TLS 1.3 的 CertificateVerify 用的正是这种域分隔思想）、PREHASH（签 64 字节 SHA-512 预哈希）。签名跨域不可验证——这是"同一算法不同用途互不混淆"的密码学机制，TLS 的角色前缀（"TLS 1.3, server CertificateVerify"）防的就是把站点的 TLS 签名挪用到别的协议。② ECDSA 按"收摘要而非消息"的接口工作（`Hash` 枚举声明摘要算法），raw `r||s` 定宽与 DER 线格式互转——第 76 章 ECDSA 双表示的往返验证。③ 对照身份层：`IdentitySign` 内部就是这些底层原语加上 TLS 上下文封装——理解底层有助于诊断"CanSign 说行但握手失败"这类层间问题（通常是策略白名单没放开方案）。

## 契约

- **不可变共享**：创建后不可变；引用计数共享；借用视图（证书/公钥）有效到最后引用释放；多线程签名无锁。
- **创建期验证**：私钥解析 + 叶 SPKI 交叉核对 + TLS 共同方案检查；无共同方案直接拒绝；证书链第一张必须是叶。
- **私钥深复制**：构造器不借用私钥输入；单块紧凑分配，释放前安全清零；RSA 留 CRT 视图、Ed25519 留展开密钥。
- **CanSign 语义**：版本 + 类型 + 方案 + 后端四重检查；1.3 拒绝 PKCS#1；ECDSA 方案随证书曲线；PSS 受限密钥双侧参数约束。
- **Sign 契约**：两段式（空输出查询长度）；容量不足不调签名器；失败不发布部分签名；PSS 随机盐 / ECDSA 确定 low-S / Ed25519 纯模式。
- **Create 扩展**：`Supports` 谓词不设错误；`Sign` 可并发、失败不变输出；`Release` 恰好一次；证书链仍深复制。
- **错误模型**：`xrt.tls` 的 identity 错误码；DER/X.509/crypto 失败保留为 Cause——上层可按 TLS 阶段映射，C 调用方可读底层原因。
- **裁剪**：`TLS_IDENTITY` 核心 + `_RSA/_EC/_P256/_P384/_ED25519` 各自独立——只跑 Ed25519 服务端不编入 RSA 代码。

## 避坑

### 坑 1：证书与私钥不匹配，上线当晚才发现

症状：服务启动正常，客户端连接在 CertificateVerify 阶段失败——日志里只有"signature invalid"，看不出是材料配错。

原因：很多装载路径只在握手时才用私钥签名；配错证书（比如续期后换了新证书、私钥还是旧的）要到第一次真实连接才暴露。

```c bad
/* 只读文件、不构造身份：错误推迟到握手 */
load_file("cert.pem", &Cert, &CertSize);
load_file("key.pem", &Key, &KeySize);
/* 存起来，握手时直接用 —— 不匹配无人发现 */
```

```c good
/* 启动即构造身份：不匹配当场失败 */
xtlsidentity* pIdentity = xrtTlsIdentityP256(
	&(xbytesview){ Cert, CertSize }, 1u,
	(xbytesview){ Key, KeySize }
);
if ( pIdentity == NULL ) {
	fprintf(stderr, "identity: %s\n",
		xrtErrorMessage(xrtGetError()));
	return EXIT_FAILURE;   /* 启动期暴露，附带原因链 */
}
```

### 坑 2：自造签名方案组合（CanSign 没过的硬用）

症状：给 Ed25519 身份配了 RSA-PSS 方案、或给 P-256 证书配 P-384 方案——`IdentitySign` 返回失败，握手挂起。

原因：TLS 方案与密钥类型强绑定（RFC 规定）：ECDSA 方案带曲线名、RSA 分 rsae/pss 两族、Ed25519 只有纯方案。这不是"试试看"的配置项。

```c bad
/* Ed25519 身份 + RSA 方案：必然失败 */
xrtTlsIdentitySign(pIdentity, XTLS_VERSION_13,
	XTLS_SIGNATURE_RSA_PSS_RSAE_SHA256, ...);
```

```c good
/* 方案随身份类型走：CanSign 先问一次 */
xtlssignature Scheme = pick_scheme_for(xrtTlsIdentityType(pIdentity));
if ( !xrtTlsIdentityCanSign(pIdentity, XTLS_VERSION_13, Scheme) ) {
	/* 配置错误：启动期报出，而不是握手期 */
}
```

### 坑 3：HSM 扩展里 Sign 回调带了共享可变状态

症状：并发握手时间歇性签名失败或输出错乱——`Create` 文档明说 Supports/Sign 必须可并发，但回调实现里用了非重入的句柄。

原因：身份对象被多 Worker 并发共享，签名回调同时跑在多个握手里。回调是无锁合约——把有状态设备访问做成排队或每线程会话是你的责任。

```c bad
static bool hsmSign(..., void* pOutput, size_t* pSize) {
	hsm_begin(pHsmSession);   /* 共享会话：并发握手互踩 */
	hsm_op(...);
	hsm_end(pHsmSession);
}
```

```c good
/* 每次调用独立会话（或会话池）；失败不改输出与长度 */
static bool hsmSign(const void* pContent, size_t iSize,
		void* pOutput, size_t iCapacity, size_t* pSize) {
	hsmsession* pS = hsm_session_acquire();   /* 池化 */
	bool bOk = hsm_sign(pS, pContent, iSize, pOutput, iCapacity, pSize);
	hsm_session_release(pS);
	return bOk;
}
```

## 练习

### 基础：嵌入材料四形态自检

跑通无参形态的 identity 示例，把四行的类型枚举值与 `xtlsidentitytype` 对上。再用 OpenSSL 生成一对自签 Ed25519 证书与私钥（DER 导出），走有参路径验证签名 64 字节。验收标准：四形态自检全过；自备材料路径输出 `identity=5 certificates=1 signature=64 bytes`。

### 进阶：不匹配材料的启动期拦截

故意配错两对材料（RSA 证书 + EC 私钥；P-256 证书 + P-384 私钥），分别构造身份并打印错误消息的原因链（`xrtErrorMessage` + `xrtErrorCause`）。验收标准：两种错配都在构造期失败而非签名期；错误链能区分"DER 解析失败"与"SPKI 不匹配"两类原因。

### 挑战：HSM 模拟签名器

用 `xrtTlsIdentityCreate` 实现一个"模拟 HSM"：`Supports` 只认 Ed25519；`Sign` 内部用真实现（种子保存在"硬件"结构里，模拟不可导出），并加一个原子计数器统计调用次数。把它接入第 86 章的服务端（或直接单元测试），并发 8 线程同时签名。验收标准：全部签名成功且可验证；计数器等于签名次数（证明并发）；"硬件"种子从未离开模拟结构（代码审查确认）。

## 速查

| 知识点 | 速查 |
| --- | --- |
| 身份对象 | 证书链+私钥打包；不可变、引用计数、多线程无锁共享 |
| 四构造器 | Rsa/P256/P384/Ed25519；私钥方言各异（PKCS#1/#8/SEC1/种子） |
| 创建期验证 | 私钥解析+SPKI 交叉核对+共同方案检查；无方案即拒绝 |
| 私钥处理 | 深复制不借用；单块分配释放前清零；CRT/展开密钥驻留 |
| CanSign | 版本+类型+方案+后端；1.3 拒 PKCS#1；ECDSA 随曲线；PSS 双侧约束 |
| Sign | 两段式查询/签名；失败不发布部分；PSS 随机盐/ECDSA low-S/Ed25519 纯 |
| 方案绑定 | 方案随密钥类型；白名单策略与 CanSign 交集才是握手方案 |
| Create 扩展 | HSM/系统库/远程签名；Supports 谓词+可并发 Sign+恰好一次 Release |
| 输入边界 | PEM/文件/系统库不属身份核心——先拿 DER 再构造 |
| 裁剪 | _RSA/_EC/_P256/_P384/_ED25519 独立；单算法服务端不携带其他 |
