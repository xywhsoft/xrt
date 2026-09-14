---
num: 78
slug: der-pem
title: DER 与 PEM：证书的字节形态
volume: 卷八 安全
type: practice
lead: TLV 游标解析与文本封装——读懂 X.509/TLS 世界里一切二进制结构的第一层词汇表。
api: asn1, pem, codec
---

## 导读

从本章开始的四章处理"信任的数据结构"：证书怎么编码（本章）、怎么解析成视图（第 79 章）、签名怎么验证（第 80 章）、怎么串成信任链（第 81 章）。而这一切的字节基础是 **DER**——ASN.1 家族的规范编码格式：X.509 证书、TLS 消息里的多数结构化字段、密钥文件，全是 DER。DER 的形态是统一的 **TLV 三元组**（类型、长度、值），掌握"游标逐层读"这一个模型，就拿到了解析整个证书生态的钥匙。**PEM** 则是 DER 的文本外衣——`-----BEGIN CERTIFICATE-----` 那种块状 Base64，让二进制能在邮件、配置文件、环境变量里安全旅行。两个模块都是零分配借用式的：不复制一个字节，只产出视图。

## 引入

你从同事那里收到一个 `.pem` 文件，说是服务证书。打开一看是几段 `-----BEGIN CERTIFICATE-----` 开头的 Base64 文本——这怎么变成程序能用的东西？反过来，你的 TLS 服务要把 ECDSA 私钥存进配置中心，那边只收文本。这两个方向就是 PEM 的编解码。

再往下钻：PEM 块里 Base64 解出来的字节是什么？是一张 DER 编码的证书。你想读出"颁发者是谁、有效期到哪天、公钥是什么算法"——就得解析 DER。DER 的规则写在 ASN.1 标准里，但工程上你只需要一个模型：**每个元素都是"类型-长度-值"三段**，类型决定值的结构（整数、字符串、OID，或"包着更多元素的构造类型"）。递归展开，任何证书都能读。XRT 的 `asn1` 模块把这个模型做成了零分配游标——第 79 章的证书解析器、第 82 章的 TLS 消息解析，全部建在它上面。

## 概念

### TLV：一切 DER 文档的原子形态

一个 DER 元素永远三段：

```diagram flow
- 类型（Tag）：类别（UNIVERSAL/上下文…）+ 构造位 + 标签号，1 到多个字节
- 长度（Length）：内容字节数，规范要求最短编码——0x05 就是 5，不许 0x00 0x05
- 值（Value）：内容字节；构造类型（SEQUENCE/SET）的值又是嵌套的 TLV 序列
```

例如 SEQUENCE 包两个 INTEGER：`30 06 02 01 07 02 01 2A`——`30` 是"UNIVERSAL 构造 SEQUENCE"，`06` 是内容长 6 字节，后面两个 `02 01 xx` 是各占 1 字节内容的 INTEGER。**递归性**是 DER 的全部：一个证书就是一个大 SEQUENCE 里嵌着 Issuer、Subject、公钥、扩展……每层都是同样的 TLV。

### DER 的"严格"：为什么不能宽松解析

ASN.1 有个更宽的兄弟 BER（基本编码规则）：允许同一数据多种编码——长长度前导零、非最短整数、无限长度。宽格式对传输不友好（同一证书可以有不同的字节序列），于是 DER（**规范**编码规则）规定**每种值只有唯一编码**。XRT 游标的每次读取都按 DER 严格模式把关：拒绝 BER 无限长度与截断、非最短长度、非最短或溢出的高标签号、错误 primitive/constructed 形式的常用类型、非规范的 BOOLEAN/INTEGER/BIT STRING/NULL/OID。游标模式适合"协议结构已知"的高性能路径；对**不可信的独立 DER 文档**（下载的证书、密钥文件），入口处先用 `xrtDerValidate` 一次性整体校验——杜绝"读到一半才发现越界"。

### 游标模型：零分配、零拷贝、三态返回

四个值对象构成解析的全部状态：`xasn1tag`（类别+构造位+32 位标签号）、`xdervalue`（类型+字节范围的**视图**，Raw 含完整 TLV、Value 只含内容，都借用输入）、`xdercursor`（输入+边界+下一项偏移，可按值复制后独立遍历）、`xderresult`（三态）。核心入口六个：

- `xrtDerInit`：绑定输入建根游标——只记指针长度，不解析任何字节。
- `xrtDerRead`：顺序取下一个元素，三态返回——`XDER_VALUE`（取到）、`XDER_DONE`（正常读完，**不是错误**）、`XDER_ERROR`（结构非法，设线程错误）。
- `xrtDerExpect`：断言"下一个必须是某类型"并取值——协议里标签固定的路径用它（比"读了再判类型"少一次分支）。
- `xrtDerEnter`：进入 SEQUENCE/SET/显式标签的内部得到子游标——不复制内容，逐层深入。
- `xrtDerDone`：断言游标恰好消费完——多一个字节都算结构非法（防尾随垃圾）。
- `xrtDerIs`：窥视不推进——CHOICE、OPTIONAL 字段的分派判断。

两条铁律：**失败调用是空操作**（`Read`/`Expect` 只在成功时推进游标并发布输出）；**全程零分配零拷贝**（值转换由类型辅助层按需触发）。

### 类型辅助层：视图到数值的最后一步

`xrtDerUInt64/Int64`（INTEGER 视图→整数）、`xrtDerUnsigned`、`xrtDerBoolean`、`xrtDerOctets`、`xrtDerBitString`、`xrtDerOid`（对象标识符）与 `xrtDerOidEqual`（OID 比较）——游标给的是"字节范围"，要不要转成数值、转成什么，由调用方决定。这个设计让"只要结构不要数值"的路径（如证书指纹遍历）零转换开销。

### PEM：文本外衣的块协议

PEM 把任意字节包成带标签的文本块：

```text
-----BEGIN CERTIFICATE-----
MIIB...(64 字符一行的 Base64)
-----END CERTIFICATE-----
```

`xrtPemFind(文本, 长度, 标签, 块)` 在文本中定位指定标签的**下一个**块——块前后允许说明文本（证书链文件里常见注释），LF/CRLF/CR 三种换行都认，开始与结束标签必须**精确匹配**，嵌套边界、缺结束边界、非法标签一律拒绝。返回的 `xpemblock` 只含借用视图（Label/Body/Raw 都指向原文）。`xrtPemDecodeNew(块, &长度)` 把 Body 的 Base64 解码为**拥有式**字节缓冲（`xrtFree` 释放）——这一步才分配；`xrtPemEncodeNew(标签, 字节, 长度)` 反向生成规范 PEM 文本（64 字符换行）。一个文件多个块（私钥+证书链）：循环 `Find` 同一文本即可——游标语义与 DER 一致。

## 示例

### 第一个完整程序：游标读一个 DER SEQUENCE

下面的程序来自 `examples/asn1/der/main.c`，从 8 字节的最小 DER 文档读出两个整数——证书解析的微缩模型：

```embed path="examples/asn1/der/main.c" title="examples/asn1/der/main.c"
```

```term
$ gcc -O1 -DXRT_MODULE_ALL -I single impl.c examples/asn1/der/main.c -lws2_32 -liphlpapi
7 + 42 = 49
```

**刚才发生了什么。** ① `xrtDerValidate` 先整体校验——长度前缀、嵌套边界一遍过；对不可信输入这是入口动作，结构坏了后面根本不会开始。② `Expect` 断言根元素是 `(UNIVERSAL, SEQUENCE, 构造式)`——三个参数把 TLV 的类型段完整指定；然后 `Enter` 进入内部得到子游标，原文档一个字节都没复制。③ 两次 `Read` 各取一个 `xdervalue`（类型+范围视图），`xrtDerUInt64` 按需转数值——7 与 42。④ `Done` 断言恰好消费完：输入末尾多一个 0x00 都会让它失败——"防尾随垃圾"是协议解析的安全底线（后面的证书解析、TLS 消息解析全都以 Done 收尾）。任何一环失败走同一个错误分支——串联式解析链是本章的推荐写法。

### 第二个完整程序：PEM 编码、查找、解码往返

第二个程序来自 `examples/asn1/pem/main.c`，把 5 字节载荷包成 PEM 文本再无损还原：

```embed path="examples/asn1/pem/main.c" title="examples/asn1/pem/main.c"
```

```term
$ gcc -O1 -DXRT_MODULE_ALL -I single impl.c examples/asn1/pem/main.c -lws2_32 -liphlpapi
-----BEGIN XRT DATA-----
AQIDBAU=
-----END XRT DATA-----
```

**刚才发生了什么。** ① `xrtPemEncodeNew("XRT DATA", ...)` 自定义标签生成完整 PEM 文本（含结尾换行）——证书场景把标签换成 `CERTIFICATE` 或 `PRIVATE KEY` 就是标准文件。② `xrtPemFind` 在生成的文本里定位该标签的块——注意它找的是**借用视图**，不分配；真实场景是在磁盘读出的整段文本里循环 Find，逐块处理证书链。③ `xrtPemDecodeNew` 解码 Base64 得到拥有式字节，三重校验（非空、长度、逐字节 memcmp）证明往返无损。④ 两个拥有式资源（文本与字节）逐一 `xrtFree`——Encode/Decode 是仅有的两个分配点，Find 与 DER 游标全程零分配。

## 契约

- **游标语义**：失败调用对游标与输出都是空操作；`Read/Expect` 只在成功时推进；`XDER_DONE` 是正常结束不设错误。
- **严格 DER**：每次读取拒绝无限长度、非最短编码、溢出标签号、错误构造形式、非规范基本类型；不可信独立文档入口先 `Validate`。
- **借用生命周期**：`xdervalue`/`xpemblock` 全部借用输入；输入内存在所有视图用完前必须有效（第 15 章缓冲视图的同一条规则）。
- **数值转换分离**：视图与数值转换是两步（`Read` 给视图、`UInt64` 等按需转）——不要数值的路径零转换成本。
- **PEM 严格边界**：开始/结束标签精确匹配；嵌套、缺失边界、非法标签拒绝；块前后允许说明文本；三种换行等价。
- **所有权**：`PemEncodeNew`/`PemDecodeNew` 返回拥有式（`xrtFree`）；`PemFind`/游标族零分配。
- **线程**：全部值对象无共享可变状态，各自持有的游标/视图任意线程并发。
- **裁剪**：`XRT_MODULE_ASN1_DER`（DER 游标）与 `XRT_MODULE_PEM`（依赖 Base64 编解码）独立裁剪；PEM 不拖入 DER，反之亦然。

## 避坑

### 坑 1：跳过 Validate 直接解析不可信 DER

症状：解析一张外部证书，读到中段才报"长度越界"——前面几层视图已经发布，错误处理要回滚一堆状态；更糟的是把 BER 宽松格式当 DER 收下，两台机器对同一文件解析结果不同。

原因：游标是惰性的——`Init` 不解析任何字节，`Read` 到哪层校验到哪层。对结构未知的输入，"边读边发现非法"意味着一半状态已发布。

```c bad
xrtDerInit(&Root, pUntrusted, iSize);   /* 直接开始 */
xrtDerRead(&Root, &Value);               /* 读到中段才失败 */
```

```c good
if ( !xrtDerValidate(pUntrusted, iSize) ) {
	return false;   /* 入口整体校验：坏结构在这里止步 */
}
xrtDerInit(&Root, pUntrusted, iSize);
```

### 坑 2：读完不断言 Done，吞下尾随垃圾

症状：协议解析"成功"，但对方在合法结构后面附了一段任意字节——下一次解析位置错乱，或被注入未审计数据。

原因：`Read` 返回 `XDER_DONE` 只说明"当前游标边界读完了"；如果外层容器长度与实际内容不一致，或输入尾部有额外数据，不断言就会漏检。

```c bad
while ( xrtDerRead(&Items, &Value) == XDER_VALUE ) {
	handle(&Value);   /* 循环完就结束：尾部垃圾无人过问 */
}
```

```c good
while ( xrtDerRead(&Items, &Value) == XDER_VALUE ) {
	handle(&Value);
}
if ( !xrtDerDone(&Items) ) {
	return false;   /* 恰好消费完——多一个字节都算非法 */
}
```

### 坑 3：PEM 标签想当然，大小写或空格不匹配

症状：`PemFind` 找不到明明存在的块——文件里的标签是 `TRUSTED CERTIFICATE` 或前后多了空白，而查找用了 `CERTIFICATE`。

原因：PEM 边界匹配是**精确**的——`-----BEGIN CERTIFICATE-----` 与 `-----BEGIN TRUSTED CERTIFICATE-----` 是两种不同的块（历史上还有更多标签变体共存）。宽松的"包含即命中"会把私钥块误当证书块。

```c bad
/* 凭印象写标签，不核对文件实际边界 */
xrtPemFind(Text, iSize, "CERTIFICATE", &Block);
/* 文件实际是 -----BEGIN TRUSTED CERTIFICATE-----：找不到，或找错块 */
```

```c good
/* 先按文件真实标签查找；多标签文件逐个尝试或遍历所有块 */
if ( !xrtPemFind(Text, iSize, "CERTIFICATE", &Block) &&
	!xrtPemFind(Text, iSize, "TRUSTED CERTIFICATE", &Block) ) {
	return false;   /* 明确失败优于静默找错 */
}
```

## 练习

### 基础：手工解码一个 DER 文档

十六进制 `30 09 02 01 0A 02 04 00 A1 B2 C3`——先手推结构（SEQUENCE 包两个 INTEGER），再用游标程序读出并打印两个值（10 与 0xA1B2C3）。再故意把中间的 `04` 改成 `05`（长度破坏），验证 `Validate` 拒绝。验收标准：手推结果与程序输出一致；破坏实验返回失败。

### 进阶：证书链文件拆包器

读一个含多张证书的 PEM 文件（可用 OpenSSL 自生成），循环 `PemFind` + `PemDecodeNew` 把每块解码为 DER 字节，逐块打印"第 N 块：X 字节，DER 校验通过"。验收标准：三张证书的链文件输出三行；`Validate` 对每块都通过；解码缓冲逐一释放无泄漏（第 6 章统计）。

### 挑战：递归结构遍历器

实现 `dump(xdercursor* Cursor, int iDepth)`：递归打印任意 DER 文档的结构树——构造类型按缩进展开、基本类型打印类型与长度（INTEGER 顺带打印数值）。用它打印第 79 章 inspect 示例的内置证书，对照 RFC 5280 的 Certificate 结构图逐层标注（tbsCertificate/subjectPublicKeyInfo/extensions）。验收标准：能完整展开到叶子层；对 BER 宽松输入（手工构造非最短长度）报错退出而不是崩溃。

## 速查

| 知识点 | 速查 |
| --- | --- |
| TLV 三元组 | 类型（类别+构造位+标签号）+ 长度（最短编码）+ 值（构造类型嵌套 TLV） |
| DER vs BER | DER 每值唯一编码；游标按严格模式拒绝无限长度/非最短/溢出标签 |
| 游标六入口 | Init（绑定）/ Read（三态）/ Expect（断言类型）/ Enter（进构造体）/ Done（恰好读完）/ Is（窥视） |
| 三态返回 | `XDER_VALUE` 取到 / `XDER_DONE` 正常读完（非错误）/ `XDER_ERROR` 结构非法 |
| 失败空操作 | Read/Expect 失败不推进游标、不发布输出 |
| 值转换分离 | 视图→数值两步：UInt64/Int64/Boolean/Oid 按需触发 |
| 入口校验 | 不可信独立 DER 先 `Validate` 整体把关；游标模式适合已知结构 |
| PEM 结构 | BEGIN/END 精确标签 + 64 字符换行 Base64；块前后允许说明文本 |
| PEM 三入口 | Find（借用视图零分配）/ DecodeNew（拥有式字节）/ EncodeNew（拥有式文本） |
| 换行兼容 | LF / CRLF / CR 三种等价；嵌套与缺失边界拒绝 |
| 裁剪 | ASN1 与 PEM（依赖 Base64）独立；互不拖入 |
