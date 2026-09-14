---
num: 74
slug: crypto-hash
title: 密码学（上）：哈希与 HMAC
volume: 卷八 安全
type: practice
lead: 摘要家族、流式三段式、HMAC 与双 KDF（PBKDF2/HKDF）——一切完整性与密钥派生的地基。
api: crypto, hash, random
---

## 导读

本卷从最古老也最常用的原语开始。哈希函数把任意数据压成定长摘要——文件校验、内容寻址、集合指纹都靠它；HMAC 给哈希加上密钥，把"完整性"升级成"认证的完整性"；KDF（密钥派生函数）从口令或协商秘密造出真正的密钥。本章覆盖 MD5 到 SHA-512/256 的摘要家族、流式三段式与一次性两种调用形态、HMAC 的预计算状态，以及 PBKDF2 与 HKDF 这对分工明确的派生函数。它们是后面一切的砖石：AEAD 的密钥从 KDF 来、TLS 的握手摘要是哈希、证书的签名对象是摘要。学完本章你应该能对任意场景回答"用哪个摘要、要不要密钥、派生还是直接用"。

## 引入

三个真实场景。你要给一个大文件生成校验码，供下载方比对——文件有几个 GiB，不可能先整个读进内存。你的服务对外提供 Webhook 回调，需要验证"这条请求确实来自配置了密钥的合作方"——裸哈希不行，任何人都能对任意内容算哈希；你验证的是"持密钥者才算得出"。你的用户用口令登录，你需要从口令得到一个对称加密密钥——口令熵很低（人类记不住 256 位随机数），直接当密钥用会被字典攻击秒杀。

三个场景对应三类原语：**摘要**（无密钥、公开可算、用于指纹）、**MAC**（带密钥、只有持钥方能算、用于认证）、**KDF**（从低熵口令或高熵协商秘密**派生**出符合要求的密钥材料）。XRT 把三者实现在一套可精细裁剪的特性族里——SHA-256 与 HMAC-SHA-256 是两个独立宏，只做文件校验的工具不必编入 MAC 代码。

## 概念

### 摘要家族全景

XRT 内建七个摘要算法，由 `xrtCryptoHashSize` 统一描述长度元数据：

| 算法 | 摘要长度 | 定位 |
| --- | --- | --- |
| MD5 | 16 字节 | **仅历史协议互操作**（HTTP Digest 等）；已不具备抗碰撞安全性 |
| SHA-1 | 20 字节 | WebSocket 与历史协议；**不得用于新签名设计** |
| SHA-224 | 28 字节 | 与 SHA-256 共享压缩实现 |
| SHA-256 | 32 字节 | 现代默认——完整性、HMAC、签名摘要的主力 |
| SHA-384 | 48 字节 | 与 SHA-512 共享压缩核心 |
| SHA-512 | 64 字节 | 64 位平台长摘要 |
| SHA-512/256 | 32 字节 | 复用 SHA-512 压缩核心的 256 位变体，抗长度扩展 |

选择的第一直觉：**新设计一律 SHA-256 起步**；需要与旧协议对话才碰 MD5/SHA-1，且只用于协议要求的字段，不用于任何安全决策。`xrtCryptoHashSize(XCRYPTO_HASH_SHA256)` 返回 32——注意这是元数据查询，与该算法是否编入程序无关，容量计算不必依赖编译配置。

### 流式三段式与一次性

数据不会总是完整地躺在内存里——几 GiB 的文件、网络上的分块到达，都需要**喂一点算一点**。所有摘要与 HMAC 家族共享同一个状态机：

```diagram state
未初始化 -> 已初始化: Init（写摘要初始状态）
已初始化 -> 已初始化: Update × N（任意次，任意长度）
已初始化 -> 完成: Final（写出摘要，状态消耗）
```

`Init` 建立状态，`Update` 可调用任意次、每次任意长度——内部按块累积，与一次性喂入结果逐位一致；`Final` 写出定长摘要。一次性入口 `xrtSha256(数据, 长度, 输出)` 是"三步并一步"的便利形态，语义等价。状态结构（`xsha256` 等）是调用方栈上的值对象，无堆分配、无共享状态，可任意多实例并发。

### HMAC：给哈希配上钥匙

`xrtHmacSha256(密钥, 密钥长, 数据, 数据长, 输出)` 的签名只比裸哈希多一对密钥参数，语义却是质的跳跃：**只有持密钥方才能算出同样的 MAC**。`HmacSha256Init` 时预计算 inner/outer 两个摘要状态——密钥只处理一次，之后无论多少次 `Update` 都不再碰密钥。这就是"API 签名、Webhook 校验、会话令牌"的标准做法：发送方 `HMAC(密钥, 内容)` 随内容附上，接收方重算并比对——用 `xrtConstTimeEqual`，不是 `memcmp`（第 77 章讲为什么）。

HMAC 家族按底层摘要独立裁剪：SHA-256 版只依赖 SHA-256；SHA-384/512 共享实现。密钥长度任意（超长会被先摘要），推荐直接用 32 字节随机数。

### KDF 双子：PBKDF2 与 HKDF

两个派生函数解决相反的问题，签名却惊人地对称——记住分工就不会混：

- **PBKDF2（口令 → 密钥）**：输入是**低熵口令**，靠**工作因子**（迭代次数）拉高攻击成本——攻击者每猜一个口令也要付出同样的十万次迭代。`xrtPbkdf2Sha256(口令, salt, 迭代数, 输出)`。salt 必须每用户随机、随密文存储——防彩虹表、防相同口令派生出相同密钥。派生过程零堆分配，返回前安全清除全部中间状态。
- **HKDF（高熵 → 密钥）**：输入是**已经高熵的材料**（DH 共享秘密、主密钥），不需要慢速——它解决的是**形状与用途**：把任意长度熵变成任意长度密钥，并用 `info` 参数绑定用途。`xrtHkdfSha256(salt, IKM, info, 输出)`。同一份 IKM，`info` 取 `"session"` 与 `"cookie"` 派生出互不相同的密钥——**同源不同用途隔离**的标准做法，TLS 1.3 的全部密钥调度就是 HKDF 驱动的。

HKDF 公开由浅入深三层：`Extract`（salt+IKM → 固定长 PRK）、`Expand`（PRK+info → 任意长 OKM）、组合函数（栈上保存 PRK 一次完成）——多数场景用组合函数即可，流式握手（如 TLS）才需要拆开。

### 与第 12 章哈希库的分界

XRT 还有一个非密码学哈希库（第 12 章，SipHash 家族的非密码哈希）——它快得多，但**没有安全保证**，只用于哈希表、去重指纹、缓存键。分界线一句话：**会被攻击者控制的输入参与、且结果用于安全决策的，一律用本章的密码学摘要**；纯粹的性能场景用第 12 章。

## 示例

### 第一个完整程序：SHA-256 流式三段式

下面的程序来自 `examples/crypto/sha256/main.c`，对 `"hello "` + `"world"` 两块输入算摘要——"hello world" 的 SHA-256 是最常用的自测向量：

```embed path="examples/crypto/sha256/main.c" title="examples/crypto/sha256/main.c"
```

```term
$ gcc -O1 -DXRT_MODULE_ALL -I single -include xrt.h impl.c examples/crypto/sha256/main.c -lws2_32 -liphlpapi
b94d27b9934d3e08a52e52d7da7dabfac484efe37a5380ee9088f7ace2efcde9
```

**刚才发生了什么。** ① `xsha256` 状态在栈上，`XRT_SHA256_SIZE` 常量声明输出缓冲——长度永远来自宏而不是手写数字。② 两次 `Update` 的结果与一次性 `xrtSha256("hello world", 11, ...)` 逐位一致——这就是"分块到达不必拼完整缓冲"的兑现：第 45 章的文件流、第 67 章的接收回调里，摘要都是这样逐块累积的。③ 输出是原始字节，示例用 `%02x` 循环打印成 64 个十六进制字符——摘要 API 只产字节，文本化是调用方的事（第 28 章的 HEX 编解码正好可用）。

### 第二个完整程序：HMAC——只有持钥者算得出

第二个程序来自 `examples/crypto/hmac_sha256/main.c`，密钥 `"secret"` 对消息 `"message"` 的标准 MAC：

```embed path="examples/crypto/hmac_sha256/main.c" title="examples/crypto/hmac_sha256/main.c"
```

```term
$ gcc -O1 -DXRT_MODULE_ALL -I single -include xrt.h impl.c examples/crypto/hmac_sha256/main.c -lws2_32 -liphlpapi
8b5f48702995c1598c573db1e21866a9b825d4a794d169d7060a03605796360b
```

**刚才发生了什么。** 与第一个程序并排读，差异只有两处：`Init` 多了密钥参数，输出长度还是 32——但语义完全不同。任何人都能对 `"message"` 算出 SHA-256，所以裸哈希不能证明"消息来自谁"；只有知道 `"secret"` 的一方才能算出这个 MAC。真实系统里密钥从配置或协商而来、长度用满 32 字节随机数，校验方用 `xrtConstTimeEqual` 比对。注意 HMAC 也有流式三段式（`HmacSha256Init/Update/Final`），`Init` 预计算密钥状态后，多消息场景每条只需一次 Update——`examples/crypto/hash_tour` 演示了全部变体的流式与一次性自洽。

### 第三个完整程序：PBKDF2——从口令安全地造钥匙

第三个程序来自 `examples/crypto/pbkdf2_sha256/main.c`，十万次迭代从口令派生 32 字节密钥：

```embed path="examples/crypto/pbkdf2_sha256/main.c" title="examples/crypto/pbkdf2_sha256/main.c"
```

```term
$ gcc -O1 -DXRT_MODULE_ALL -I single -include xrt.h impl.c examples/crypto/pbkdf2_sha256/main.c -lws2_32 -liphlpapi
ecfde924b9512da31933191fd1d31754e252fce15eae769808c3cdda9702cbd5
```

**刚才发生了什么。** ① 参数序是（口令、salt、迭代、输出）——迭代数 100000 是工作因子：合法用户每次登录付十万次迭代（毫秒级，可接受），攻击者每试一个候选口令也要付同样的代价，字典攻击成本被拉高十万倍。② salt 是显式的 16 字节常量——示例里固定以便复现，生产中必须**每用户随机生成**并随密文存储（见坑 3）。③ 结尾 `xrtSecureZero(arrKey, ...)` 把派生出的密钥从栈上抹掉——密钥材料用完即清，这是全卷的固定动作。密码与 salt 可以重叠吗？不可以——输出不得与输入重叠，契约在首次写入前校验全部参数。

### 第四个完整程序：HKDF——同源密钥按用途隔离

第四个程序来自 `examples/crypto/hkdf_sha256/main.c`，一行调用把三段材料变成会话密钥：

```embed path="examples/crypto/hkdf_sha256/main.c" title="examples/crypto/hkdf_sha256/main.c"
```

```term
$ gcc -O1 -DXRT_MODULE_ALL -I single -include xrt.h impl.c examples/crypto/hkdf_sha256/main.c -lws2_32 -liphlpapi
184de99cd5c9f1af2dee024de950759b818bce38644013e38c890f4745a9ff8e
```

**刚才发生了什么。** 三个参数各司其职：`salt` 是公开随机值（换 salt 就是全新密钥）、IKM 是原始熵（这里是演示字符串，真实场景是 DH 共享秘密——第 76 章）、`info` 是用途绑定。把 `info` 换成 `"cookie"` 再跑一次，输出完全不同——这就是"一份熵派生多把钥匙、钥匙之间互不影响"的用途隔离。第 77 章的会话示例将把 HKDF 放进完整链条：x25519 协商 → HKDF 派生 → AEAD 加密。

## 契约

- **摘要族**：七算法（MD5/SHA-1/SHA-224/256/384/512/512_256），`xrtCryptoHashSize` 给长度元数据（与编入无关）；流式三段式与一次性语义逐位一致；状态为栈值对象，无堆分配，实例间任意并发。
- **MD5/SHA-1 边界**：仅历史协议互操作（HTTP Digest、WebSocket）；不得用于新签名、证书、内容可信性或口令存储设计。
- **HMAC**：Init 预计算密钥状态，Update 不再触键；SHA-256 独立裁剪、SHA-384/512 共享；密钥长度任意，推荐 32 字节随机。
- **PBKDF2**：低熵口令专用；迭代与输出长度必须大于零；输出不得与口令/salt 重叠；零堆分配，返回前清除 HMAC 状态与中间值；最大输出 `UINT32_MAX × HashLen`（块编号 32 位）。
- **HKDF**：高熵材料专用；三层入口（Extract/Expand/组合）；info 绑定用途实现同源隔离；组合函数在栈上保存 PRK。
- **核心工具**：`xrtConstTimeEqual` 常量时间比较（不因首字节不同早退）；`xrtSecureZero` volatile 清零（防死存储删除）；随机源由独立 `RANDOM_SECURE` 特性提供（第 12 章），crypto_core 不携带随机数。
- **错误**：失败经返回值宣告、详情在线程错误槽；参数/范围/块上限错误都在**首次写入输出之前**返回——失败不污染输出缓冲。

## 避坑

### 坑 1：给新设计选了 MD5 或 SHA-1

症状：审计不过、依赖拒绝集成；运气更差时，攻击者构造出同摘要的两份内容（MD5 碰撞早已是秒级操作）。

原因：MD5 与 SHA-1 的抗碰撞特性已经破坏。XRT 把它们编入是因为 HTTP Digest 与 WebSocket 协议仍然要求——互操作需要，不等于安全可用。

```c bad
uint8 Digest[16];
xrtMd5(Content, iSize, Digest);   /* 新设计的"完整性校验" */
store(Digest);                     /* 攻击者可构造碰撞内容 */
```

```c good
uint8 Digest[XRT_SHA256_SIZE];
xrtSha256(Content, iSize, Digest);  /* 新设计默认 SHA-256 起 */
store(Digest);
/* 只有协议明文要求 MD5 的字段（如 Digest 认证）才用 MD5，
   且不把它的结果用于任何安全决策 */
```

### 坑 2：用裸哈希做"认证"

症状：伪造请求通行无阻——"我算了哈希并比对一致"却挡不住攻击者，因为攻击者也算得出同样的哈希。

原因：哈希无密钥，任何人可算。完整性校验（防传输误码）与认证完整性（防恶意构造）是两件事，中间隔着一个密钥。

```c bad
/* 请求带 ?sig=sha256(body)，服务端重算比对 */
uint8 Digest[XRT_SHA256_SIZE];
xrtSha256(Body, iSize, Digest);
if ( memcmp(Digest, SentSig, sizeof(Digest)) == 0 ) {
	accept();   /* 攻击者改 body 后重算 sig 即可伪造 */
}
```

```c good
/* HMAC：只有持密钥方才能产生合法 MAC */
uint8 Mac[XRT_SHA256_SIZE];
xrtHmacSha256(ApiKey, sizeof(ApiKey), Body, iSize, Mac);
if ( xrtConstTimeEqual(Mac, SentMac, sizeof(Mac)) ) {
	accept();
}
```

### 坑 3：PBKDF2 的 salt 全局共用或硬编码

症状：两个用户口令相同 → 派生密钥相同 → 一人密钥泄露，另一人数据同时暴露；预计算彩虹表对全库有效。

原因：salt 的使命是"让每个派生输入都独一无二"。全局 salt 等于没有 salt——攻击者可以为这个 salt 预计算一张口令字典。

```c bad
static const uint8 Salt[16] = { /* 固定写死在代码里 */ };
xrtPbkdf2Sha256(Password, iLen, Salt, sizeof(Salt),
	100000, Key, sizeof(Key));
```

```c good
uint8 Salt[16];
if ( !xrtSecureRandom(Salt, sizeof(Salt)) ) {  /* 每用户随机 */
	return false;
}
xrtPbkdf2Sha256(Password, iLen, Salt, sizeof(Salt),
	100000, Key, sizeof(Key));
save_with_ciphertext(Salt);   /* salt 不保密，随密文存储 */
```

## 练习

### 基础：文件摘要工具

用第 45 章的文件读取 + 本章流式三段式，实现对任意路径文件的 SHA-256 摘要打印（64 个十六进制字符）。验收标准：对同一文件分 1 KiB 块读与一次性读结果一致；与系统工具（`sha256sum` / `certutil -hashfile ... SHA256`）输出相同。

### 进阶：Webhook 签名校验器

实现 `bool verify_webhook(cstr sBody, const uint8* pKey, size_t iKeySize, const uint8* pSentMac)`：用 HMAC-SHA-256 重算并以 `xrtConstTimeEqual` 比对。再故意把比对换成 `memcmp`，用第 77 章将要讲的时序攻击视角思考：为什么这里应该常量时间？写进你的代码注释。

### 挑战：多用途密钥管理器

实现 `derive_keys(IKM, iSize) → {会话密钥, cookie 密钥, 备份密钥}`：用 HKDF-SHA-256 的同一 IKM、三个不同 `info` 派生三把 32 字节密钥。然后验证隔离性：改动 IKM 一个字节，三把密钥全部变化；固定 IKM 重复派生，结果逐位一致。最后把三把密钥 `xrtSecureZero` 清除。验收标准：全程零堆分配（PBKDF2/HKDF 本身无堆分配，你的封装也不许有）。

## 速查

| 知识点 | 速查 |
| --- | --- |
| 算法选择 | 新设计 SHA-256 起；MD5/SHA-1 仅协议互操作；64 位长摘要用 SHA-512 |
| 长度元数据 | `xrtCryptoHashSize(XCRYPTO_HASH_SHA256)` 等：16/20/28/32/48/64，与编入无关 |
| 流式三段式 | `Init → Update×N → Final`；与一次性逐位一致；栈值状态任意并发 |
| HMAC | Init 预计算密钥状态；持钥方才算得出；比对用 `xrtConstTimeEqual` |
| PBKDF2 | 低熵口令 + 工作因子（迭代）；salt 每用户随机随密文存；返回前清中间态 |
| HKDF | 高熵材料；Extract/Expand/组合三层；info 绑定用途同源隔离；TLS 1.3 同款 |
| 核心工具 | `xrtConstTimeEqual` 常量时间比较；`xrtSecureZero` volatile 清零 |
| 与第 12 章分界 | 安全决策用本章；哈希表/指纹/缓存键用第 12 章非密码哈希 |
| 错误契约 | 参数错误在首次写输出前返回；输出与输入不得重叠（PBKDF2） |
| 裁剪 | SHA/HMAC/PBKDF2/HKDF 全部独立特性宏；只用摘要不携带 MAC |
