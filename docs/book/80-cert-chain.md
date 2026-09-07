---
num: 80
slug: cert-chain
title: 信任链与吊销：从单证书到 PKI
volume: 卷八 安全
type: practice
lead: 认证路径、信任锚与 CRL——回答验证的第三问"这个签发者值得信任吗"，拼上 PKI 的最后一块。
api: x509, crypto
---

## 导读

第 79 章结束在"验签通过 ≠ 可信"——本章补上那缺了的第三问。三个部件按序登场：**认证路径**（叶证书 → 中间 CA → 信任锚的签名接力，`xrtX509PathValidate` 逐环严查、`xrtX509PathBuild` 从无序候选自动找链）；**信任库**（拥有式锚集合——显式加入、PEM 文件、系统根证书三种装载形态，`xrtX509StoreCreate/Add/AddPem/AddSystem` 家族）；**CRL 吊销检查**（`xrtX509CrlValidate` 验一次 + `xrtX509CrlCheck` 轻查 N 次的两层设计）。三者拼起来就是完整的 PKI 客户端验证——第 81 章 TLS 客户端的证书校验正是这套部件的直接组合。本章同时划清本层不管的事：网络获取、缓存、软失败策略属于上层——策略 API 保留明确结果，让你选择自己的获取与失败规则。

## 引入

回到第 79 章的攻击者：他自签一张证书，CN/SAN 全写目标域名，用自己的密钥签名——签名验证通过、身份匹配通过。他缺的只是一样东西：**你的信任锚里没有他的公钥**。信任锚（anchor）是你预先认可并本地保存的根证书——操作系统与浏览器预置一百多个 CA 根，你的公司内网可能还加了自己的根。验证的本质是：**能不能从眼前这张证书出发，沿着"谁给谁签的名"一路走到某个本地信任锚**。

真实世界的链几乎都有中间层：`网站证书 ← 中间CA ← 根证书`。TLS 握手时对端会把叶与中间证书一起发来（顺序还不保证）；根不发——根在你本地。于是完整的验证算法是：解析全部证书 → 从叶出发自动建链到某个锚 → 逐环验签、逐环检查约束（这个中间 CA 真的有权签证书吗？路径深度超限了吗？名字约束允许吗？）→ 全过才算可信。这还不够：一张各方面都合法的证书也可能**已被签发者吊销**（私钥泄露了、域名转手了）——最后一问要拿签发者的 CRL（证书吊销列表）查序列号。本章的三个示例分别对应"建链验链、装载锚、查吊销"三步。

## 概念

### 认证路径：签名接力与它的规则

一条认证路径是"目标在前、中间 CA 依次靠近锚"的证书数组。**信任锚是验证输入、不属于路径**——`xrtX509Anchor(&根证书, &锚)` 从受信任证书提取 Subject、公钥与可选 NameConstraints；路径中再次出现同一锚证书会被拒绝（防"把对端发来的根塞进路径冒充锚"）。

`xrtX509PathValidate(路径, 数量, &锚, &配置)` 在配置给定的精确时间逐环检查，拒绝清单包括：重复证书、发行者链断裂、签名失败、未处理的 critical 扩展；中间 CA 必须有非空 Subject 与 **critical** `BasicConstraints(cA=TRUE)`；有 KeyUsage 就必须允许 `keyCertSign`，出现 `pathLenConstraint` 时必须显式带该 KeyUsage；NameConstraints 逐层求交应用到目标及其下方全部非 self-issued 中间证书；中间 CA 的 ExtendedKeyUsage 若存在也必须允许请求用途。**验证时间由调用方显式提供**（`config.Time`）——测试可复现、缓存可判定、重放可检查，这是与"隐式取当前时间"的设计分野。`X509_PATH_REQUIRE_KEY_USAGE/PURPOSE` 可强制目标证书显式携带用途声明。

### PathBuild：从一堆证书里自动找链

`xrtX509PathValidate` 要求你已经排好路径；`xrtX509PathBuild(目标, &候选源, &配置, 输出路径, 容量, &结果)` 接受**无序候选**自动搜索：每一层先尝试直接终结到锚，再按候选顺序做 Issuer/Subject 与 AKI/SKI 筛选（`xrtX509IssuerMatch`），确定性深度优先回溯；**找到的完整路径始终交给同一个 PathValidate 严验**——建链器不设宽松验证后门。输出路径永远不含锚证书；容量不够返回 `XERR_RANGE`、无路径返回 `XERR_NOT_FOUND`、验证失败保留在 `X509_ERROR_PATH_BUILD` 原因链。容量 ≤16 时搜索零堆分配。这是 TLS 客户端拿到对端链后的标准处理姿势。

### 信任库：锚的家

`xx509store` 是拥有式信任锚集合，三种装载形态按需组合：

- **逐张添加**：`xrtX509StoreAdd(库, DER, 长度)` 复制并严格解析——库里只保存一份 DER，视图全部借用它；完全相同的重复证书返回 `X509_DONE`（幂等）。
- **PEM 文本**：`xrtX509StoreAddPem` 遍历文本中全部块、只导入标签为 `CERTIFICATE` 的块——**事务性**：任何一块失败，本次已加入的全部回滚。
- **文件**：`xrtX509StoreAddFile`（独立裁剪）自动识别单张 DER 或多块 PEM 文件。
- **系统根**：`xrtX509StoreSystem()` 一键生成**独立快照**——Windows 枚举 ROOT 逻辑库、macOS 读钥匙串、Unix 读 `SSL_CERT_FILE/DIR` 或主流 bundle。快照独立于系统（后续系统变更不影响已建库）、不修改平台设置、不静默接受部分结果（枚举失败整体回滚）。HTTPS 客户端的默认信任基础就是它。

`xrtX509StoreSource(库, 外部中间证书数组, 数量, &候选源)` 把"库内锚 + 对端发来的中间证书"组合成 PathBuild 的输入——库是锚的来源，握手材料是候选，各归各位。一条要紧的失效规则：**继续写入可能移动内部结构**，使既有指针与已构造的 `xx509pathsource` 失效——装载完再建源、建完源不再写。库本身不替你判断"该不该信"：显式加入就是授予锚地位。

### CRL：验一次，查 N 次

吊销检查分两层，对应两种使用频率。**重验证层** `xrtX509CrlValidate(CRL, 签发者证书, 配置, &有效视图)`：验 CRL 签名、Issuer 匹配、时间窗、签发者 `cRLSign` 用途、AKI、CRL Number、unknown critical 扩展——产出 `xx509crlvalid` 借用视图（CRL 与签发者必须保持存活）。**轻查询层** `xrtX509CrlCheck(&有效视图, 证书, &结论)`：只查序列号是否在撤销列表里——验证一次、反复查询正是它的设计场景（一张 CA 的 CRL 验一次，查一整批证书）。一次性路径用组合入口 `xrtX509CrlStatus`。

查询三态要精确理解：`X509_ERROR` 输入/协议错误；`X509_DONE` 这份 CRL **不适用**于该证书（或 delta CRL 无新记录）——不是"未吊销"；`X509_VALUE` 才发布结论（`X509_REVOCATION_GOOD/REVOKED` 等）。还有个容易踩的精细点：**原因分段 CRL** 的 `GOOD` 只证明 `CoveredReasons` 覆盖的原因未撤销——多份分段 CRL 应依次 `xrtX509RevocationUpdate` 聚合，全集覆盖或任一分片确认撤销时才有最终结论。complete/delta 组合用 `xrtX509CrlSetInit/Check`。**本层不管的**：网络获取 CRL、缓存、软失败（拿不到 CRL 放不放行）——这些是上层策略决定，策略 API 保留明确结果就是为了把选择权留给你。

### 拼装：完整客户端验证的形状

```diagram flow
- 装载：StoreSystem（或显式 Add）建锚库，握手材料解析为候选
- 建链：StoreSource 组合源 → PathBuild 找到到锚的路径
- 验链：PathValidate 已由 Build 内部执行（时间/约束/签名逐环严查）
- 身份：MatchHost 匹配主机名（第 79 章）
- 吊销：CrlValidate + CrlCheck 查序列号（或上层策略决定软失败）
```

第 81 章会看到 TLS 客户端把这些步骤封装进上下文配置——但每一步的行为边界，就是本章定义的这些。

## 示例

### 第一个完整程序：从无序候选自动建链

下面的程序来自 `examples/x509/path_build/main.c`，三张证书（叶/中间/根）解析后从"一堆候选 + 一个锚"自动构造并验证路径：

```embed path="examples/x509/path_build/main.c" title="examples/x509/path_build/main.c"
```

```term
$ gcc -O1 -DXRT_MODULE_ALL -I single -I . -include xrt.h impl.c examples/x509/path_build/main.c -lws2_32 -liphlpapi
certificate path contains 2 certificate(s)
```

**刚才发生了什么。** ① 三张证书各自 `xrtX509Parse` 成视图；**只有根被 `xrtX509Anchor` 转成锚**——锚来自本地判断（这里手工指定），这是"信任"的注入点。② 候选源 `xx509pathsource` 把根与中间证书都列为 Issuers、锚单列——注意根既在候选里又在锚里：Build 每层先试锚终结、再按候选顺序筛选，两种角色不混淆。③ `PathBuild` 输出 `2`——路径含叶与中间 CA，**锚不计入**；这条输出路径已通过全部逐环检查（签名、时间用 `Config.Time = Leaf.NotBefore` 显式给定、BasicConstraints、KeyUsage）。④ 输出缓冲 `Path` 由调用方提供、容量 3 同时限定最大深度——不可信材料建链时把容量钉小是防御姿势（候选池大时有深度/宽度约束）。对照分工：`examples/x509/path` 示例演示调用方**自己排好链**再 `PathValidate` 的直接形态——两入口共享同一个严格验证器。

### 第二个完整程序：系统信任库一键装载

第二个程序来自 `examples/x509/store_system/main.c`，一行生成当前平台的独立锚快照：

```embed path="examples/x509/store_system/main.c" title="examples/x509/store_system/main.c"
```

```term
$ gcc -O1 -DXRT_MODULE_ALL -I single -include xrt.h impl.c examples/x509/store_system/main.c -lws2_32 -liphlpapi
system anchors=33
```

**刚才发生了什么。** ① `xrtX509StoreSystem()` 返回独立快照——锚数随平台根证书数量变化（本机 33，你的机器多半不同）；快照不使用全局缓存，系统后续增删根不影响已建库。② 计数后 `StoreFree` 释放——快照是拥有式对象，与第 70 章接口快照同一种"一次分配、整体归还"的形态。③ 真实用法是 `StoreSource` 把它变成建链源：HTTPS 客户端的默认信任基础就是这两个函数的组合。要收紧信任面（只信特定 CA）就改用 `StoreCreate` + `Add`/`AddFile` 显式装锚——库不替你判断该信谁，加入即信任。

### 第三个完整程序：吊销查询的轻量路径

第三个程序来自 `examples/x509/crl_policy/main.c`，手工构造已验证视图、聚焦 `CrlCheck` 的查询语义：

```embed path="examples/x509/crl_policy/main.c" title="examples/x509/crl_policy/main.c"
```

```term
$ gcc -O1 -DXRT_MODULE_ALL -I single -include xrt.h impl.c examples/x509/crl_policy/main.c -lws2_32 -liphlpapi
revocation state: 1
```

**刚才发生了什么。** ① `xx509crlvalid` 在真实程序里**只该由 `xrtX509CrlValidate` 产生**——本例为聚焦查询接口手工拼装（CRL 视图挂撤销条目、签发者挂 Subject）。② 撤销条目里躺着序列号 `0x2A`，待查证书的 Serial 恰是 `0x2A` 且 Issuer 匹配——`CrlCheck` 返回 `X509_VALUE` 并发布 `State=1`（`X509_REVOCATION_REVOKED`）。③ 序列号全程是**任意精度字节视图**（第 78 章的 Serial 原样语义在 CRL 侧对齐——精确字节匹配，无截断转换）。配套的 `examples/x509/crl` 示例演示 `CrlParse` + 条目游标遍历：空列表时 `Read` 立即 `DONE`——"结构完整走到尾"与"出错中断"是两种结局，代码里要区分。

## 契约

- **锚语义**：锚是验证输入不属于路径；`xrtX509Anchor` 提取 Subject/公钥/约束；路径中出现锚证书本身即拒绝；对端发来的证书永远只是候选，不是锚。
- **验证时间**：`config.Time` 必须显式提供；验证在给定时间检查每张证书——可复现、可缓存、可重放检查。
- **路径规则**：中间 CA 需非空 Subject + critical BasicConstraints(cA=TRUE)；KeyUsage/pathLen/NameConstraints/EKU 逐环检查；长度只计非 self-issued 中间 CA；目标自带的 NameConstraints 被拒绝。
- **建链器边界**：不读文件、系统库、网络、AIA，不无限扩张候选；完整路径必经 PathValidate；容量 ≤16 零堆分配；`XERR_NOT_FOUND` 无路径、`XERR_RANGE` 容量不足、失败原因链在 `X509_ERROR_PATH_BUILD`。
- **信任库事务性**：AddPem/AddFile/AddSystem 任一块失败整体回滚；重复证书幂等（DONE）；每项一份 DER、视图借用库内存储。
- **失效规则**：写入可能移动结构——装载完成后再取指针/建源；建成后只读可多线程并发，写入与释放不得与读取并发。
- **CRL 分层**：Validate（重）产 `xx509crlvalid` 借用视图（CRL 与签发者必须存活）；Check（轻）查多张；一次性用 CrlStatus；配置默认严格（要求 nextUpdate/CRL Number/AKI/KeyUsage）。
- **吊销三态**：ERROR 输入错误 / DONE 不适用（非"未吊销"）/ VALUE 才有结论；分段 CRL 的 GOOD 限于 CoveredReasons，多份聚合用 `xrtX509RevocationUpdate`。
- **策略归属**：网络获取、缓存、软失败/硬失败、OCSP 组合属上层；本层保留明确结果。
- **裁剪**：store_file、store_system、crl_policy 均独立特性宏；路径层按 RSA/ECDSA/Ed25519 独立后端。

## 避坑

### 坑 1：把对端发来的"根证书"当信任锚

症状：中间人自建"根+中间+叶"整套链发给客户端，客户端把链顶当锚验证通过——信任完全由对端自定。

原因：信任锚的地位只能来自**本地装载**（系统库、配置文件、显式 Add）。对端发来的每张证书都是待验证的候选材料——包括那张长得像根的自签证书。PathBuild 的设计正是把两者分开：锚单独传入、永不进路径。

```c bad
/* 把握手材料的最末张直接当锚 */
xrtX509Anchor(&PeerChain[n - 1], &Anchor);
xrtX509PathValidate(Path, n, &Anchor, &Config);
/* 对端控制链内容 = 对端控制信任 */
```

```c good
/* 锚只来自本地库；对端链全部作为候选交给 Build */
xx509pathsource Source;
xrtX509StoreSource(pLocalStore, Issuers, n, &Source);
if ( !xrtX509PathBuild(&Leaf, &Source, &Config,
		Path, 4u, &Result) ) {
	reject();   /* 走不到本地锚：不可信 */
}
```

### 坑 2：把 CrlCheck 的 DONE 当 GOOD

症状：证书明明可能已吊销，代码却因 `CrlCheck` 返回非 ERROR 而放行——DONE（这份 CRL 不适用该证书）与 GOOD（查过、未吊销）被混为一谈。

原因：三态里 DONE 是"没有结论"——CRL 的签发者与证书签发者不匹配、delta CRL 无记录等情况都返回它。把"没查到"当"没问题"，等于吊销检查形同虚设。

```c bad
if ( xrtX509CrlCheck(&Valid, &Cert, &Status) != X509_ERROR ) {
	accept();   /* DONE 也进来了：不适用被当成未吊销 */
}
```

```c good
switch ( xrtX509CrlCheck(&Valid, &Cert, &Status) ) {
case X509_VALUE:
	if ( Status.State == X509_REVOCATION_REVOKED ) {
		reject("revoked");
	} else {
		accept();   /* 查过且未吊销——唯一放行 */
	}
	break;
default:
	/* DONE=不适用 / ERROR=出错：按策略处理，
	   严格策略下拒绝，软失败策略下记录后放行 */
	handle_no_verdict();
	break;
}
```

### 坑 3：信任库写入后继续用旧指针/旧 source

症状：偶发悬空视图或建链结果错乱——先 `StoreSource` 建了源，中途又 `StoreAdd` 加了张证书。

原因：库内部为数组式存储，写入可能移动证书与锚结构——既有视图指针和已构造的 `xx509pathsource` 全部失效。这是"拥有式集合 + 借用视图"组合的标准陷阱（第 18 章讲过同一现象）。

```c bad
xrtX509StoreSource(pStore, Issuers, n, &Source);
xrtX509StoreAdd(pStore, NewDer, iSize);  /* 可能移动内部结构 */
xrtX509PathBuild(&Leaf, &Source, ...);   /* Source 悬空 */
```

```c good
/* 装载阶段：全部 Add/AddPem/AddFile/AddSystem 完成 */
load_all_anchors(pStore);
/* 使用阶段：取指针、建源、建链——此后不再写 */
xrtX509StoreSource(pStore, Issuers, n, &Source);
xrtX509PathBuild(&Leaf, &Source, &Config, Path, 4u, &Result);
```

## 练习

### 基础：三证书链的两种走法

用 OpenSSL 自建"根→中间→叶"链（各一张），分别走 `PathValidate`（自己排路径）与 `PathBuild`（无序候选）两条路验证。验收标准：两种走法结论一致；故意打乱候选顺序，Build 仍找到同一条路径；把锚换成本地没装的另一张根，两条路都失败。

### 进阶：收紧信任面的 HTTPS 验证器

`StoreCreate` + `AddFile` 只装你自建的那张根，对上面的叶证书走"建链 + MatchHost + ValidAt"完整验证。然后对比 `StoreSystem` 快照下同一张叶证书的验证结果。验收标准：收紧库下验证通过（你的根在库中）；换一个未装根的链立即失败；能说出两种库各自适合的部署场景。

### 挑战：批量吊销审计器

给定一组证书与对应签发者的 CRL 文件：`CrlValidate` 一次，循环 `CrlCheck` 输出每张证书的结论（REVOKED/GOOD/不适用三分类）；对"不适用"的证书报告原因（签发者不匹配等）。再把查询循环包上计时，对比"每张都 CrlStatus 重验证"与"验一次查 N 次"的耗时差。验收标准：三分类与 `openssl crl` 工具核对一致；两种模式的耗时差能解释（验证开销摊销）。

## 速查

| 知识点 | 速查 |
| --- | --- |
| 三问拼图 | 签名（79）+ 身份（79）+ 链到本地锚（本章）+ 吊销（本章）= 完整 PKI 验证 |
| 路径模型 | 叶→中间→…，锚是验证输入不属于路径；锚中出现路径即拒绝 |
| PathValidate | 调用方排好链：时间显式、逐环签名/BasicConstraints/KeyUsage/NameConstraints |
| PathBuild | 无序候选自动建链：先试锚、AKI/SKI 筛选、回溯；完整路径必经严格验证 |
| 结果语义 | `XERR_NOT_FOUND` 无路径 / `XERR_RANGE` 容量不足 / 原因链 `X509_ERROR_PATH_BUILD` |
| 信任库 | Add/AddPem/AddFile/AddSystem 事务性装载；每项一份 DER；显式加入=授予信任 |
| 系统快照 | `StoreSystem` 独立快照：平台枚举失败整体回滚；不修改系统设置 |
| 失效规则 | 装载完再建源；写入使指针/source 失效；建成后只读可并发 |
| CRL 两层 | Validate 验一次（签名/时间/用途）→ crlvalid；Check 轻查 N 张序列号 |
| 吊销三态 | ERROR 坏 / DONE 不适用（非 GOOD）/ VALUE 有结论；分段 CRL 用聚合器 |
| 策略归属 | 获取/缓存/软失败属上层；本层保留明确结果供你决策 |
