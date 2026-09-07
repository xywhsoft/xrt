---
num: 74
slug: crypto-aead
title: 密码学（中）：AEAD 加密
volume: 卷八 安全
type: practice
lead: AES-GCM 与 ChaCha20-Poly1305——机密性、完整性、认证三合一，以及 nonce 纪律这条生命线。
api: crypto, random
---

## 导读

上一章解决"内容没被改"与"密钥从哪来"，本章解决"内容看不见"——并且把三件安全属性一次打包。AEAD（认证加密附带数据）是现代对称加密的唯一推荐形态：加密的同时计算认证标签，解密先验签——标签不符整体拒绝，绝不吐出"可能被篡改的明文"。XRT 提供两种同级实现：AES-GCM（有 AES-NI 硬件的平台首选）与 ChaCha20-Poly1305（无硬件加速平台的常量时间首选）。本章讲清两族 API 的状态机与无状态两套形态、AAD 的用途绑定、以及 AEAD 的生命线——**nonce 管理**：同一密钥下 nonce 重复一次，机密性与完整性同时崩塌。第 82 章的 TLS 记录层就是本章原语的逐条应用。

## 引入

想象你在设计一个配置同步服务：客户端把加密后的配置放到不可信的对象存储，服务端取回解密。第一版你想得很朴素——用 AES 把明文加密，存密文，取回解密。上线前安全评审问了一个问题：**攻击者篡改密文的一个字节，你的解密会怎样？** 答案是：AES 解密"成功"返回一段乱码——你的程序拿到一段自己没写过、却验证通过的数据。这就是经典的 bit-flipping 攻击：**加密不提供完整性**，CBC/CTR/ECB 等裸模式都一样。

于是你给密文附一个 HMAC——先加密后 MAC？顺序错了会引入长度扩展类漏洞；先 MAC 后加密？理论安全但实现极易出错。密码学界把这十年踩过的坑总结成一个结论：**不要自己组合加密与认证，直接用 AEAD**。加密产出"密文 + 认证标签"，解密先验标签、后出明文，一套原语三个保证（机密性、完整性、真实性），顺序与边界都由实现负责——这正是本章两个家族的全部意义。

## 概念

### AEAD 三件套：一次调用，三个保证

理解 AEAD 的最快方式是看它的一条记录格式：

```diagram flow
- 输入：明文 + AAD（附加认证数据，只认证不加密）+ nonce + 密钥
- 加密（Seal/Encrypt）：明文 → 密文（等长），并产出 16 字节认证标签
- 传输：密文 + 标签（+ 公开的 AAD 与 nonce）一起到达对方
- 解密（Open/Decrypt）：先验证标签——不符则整体失败，绝不输出明文
```

三个要点。① **标签是整条记录的指纹**：密文、AAD、nonce、长度全部参与计算——动任何一个比特，验证都失败。② **AAD 只认证不加密**：版本头、记录序号、上下文标识这些"需要看见但不需保密"的字段用 AAD 绑定进认证范围——攻击者改动版本号同样导致验证失败，这就是"用途绑定"。③ **失败是原子性的**：`Open` 返回 `false` 时输出缓冲保持未定义状态，正确程序把它整体丢弃，而不是"用一下试试"。

### AES-GCM：状态机形态

`XRT_FEATURE_CRYPTO_AES_GCM` 提供带状态的 `xaesgcm`：

- `xrtAesGcmInit(状态, 密钥, 密钥长, 标签长)`：**初始化时固定绑定**一个 AES-128/192/256 密钥与标签长度（4..16 字节，默认 16）——密钥扩展只做一次，之后每条消息不再重复；同一密钥固定标签长度也符合协议契约。
- `xrtAesGcmEncrypt / Decrypt`：分离标签形态——密文与标签写往两个缓冲，协议层自己决定两者的排布。
- `xrtAesGcmSeal / Open`：连续形态——标签直接追加在密文尾部，缓冲长度 = 明文 + 标签长，网络封包最顺手。
- `xrtAesGcmClear`：清除状态里的密钥日程，用完即清。
- nonce 推荐 12 字节（`XRT_AES_GCM_NONCE_DEFAULT_SIZE`），实现自动走 AES-NI 硬件路径。

### ChaCha20-Poly1305：无状态便捷层

`XRT_FEATURE_CRYPTO_CHACHA20_POLY1305` 实现 RFC 8439：32 字节密钥、12 字节 nonce、16 字节标签，固定三件套（无标签长度选择——协议就是 16）。API 同样有分离（`Encrypt/Decrypt`）与连续（`Seal/Open`）两套，但**无状态**：每次调用直接传密钥，不在外部维护密钥日程。取舍：省了状态管理，代价是每条消息重新处理密钥——高频同钥场景（如 TLS 记录层）内部使用带状态的变体，偶发加解密（配置文件、令牌）用便捷层最直接。ChaCha20 是软件实现的流密码，**常量时间、无查表**——在没有 AES 硬件的 ARM 设备与物联网目标上，它比软件 AES 既快又抗侧信道。`XRT_CHACHA20_POLY1305_OVERHEAD` 给出密文相对明文的固定开销（标签 16 字节），缓冲计算写宏不写数字。

### nonce 纪律：AEAD 的生命线

**同一密钥下，nonce 绝不能重复**——这不是风格建议，是数学边界：GCM 的 nonce 重复一次，认证密钥即可被恢复，该密钥保护的所有记录（包括过去的）全部暴露。两种安全的管理法：

- **计数器**：每条记录 nonce = 起始随机值 + 递增序号，发送方保证单调不重——性能最好，要求单点顺序（同一密钥只在一条发送路径使用）。
- **随机**：每条记录 96 位随机 nonce——生日界下同一密钥加密约 2^32 条记录后重复概率显著，适合短生命周期密钥（一次会话）。

第 76 章的会话示例用的是"AAD 绑定记录序号 + 计数器"的组合。密钥本身轮换（会话结束即弃）是最后一道保险——临时密钥（ephemeral）配计数器 nonce 是 TLS 1.3 的标准姿势。

### 与流密码裸 ChaCha20 的关系

`XRT_FEATURE_CRYPTO_CHACHA20` 还提供裸 ChaCha20 变换（无认证）——它只作为 Poly1305 组合的底座与特殊协议的互操作件存在。与 MD5 的纪律同构：**应用层加密永远走 AEAD，裸加密只出现在协议明确要求处**。AES 同理：`CRYPTO_AES` 的块密码层被 GCM 依赖，应用代码不直接碰。

## 示例

### 第一个完整程序：AES-GCM 封装与打开

下面的程序来自 `examples/crypto/aes_gcm/main.c`，固定密钥上原位加密再解密一条消息，AAD 绑定版本头：

```embed path="examples/crypto/aes_gcm/main.c" title="examples/crypto/aes_gcm/main.c"
```

```term
$ gcc -O1 -DXRT_MODULE_ALL -I single -include xrt.h impl.c examples/crypto/aes_gcm/main.c -lws2_32 -liphlpapi
hello from aes-gcm
```

**刚才发生了什么。** ① 密钥与 nonce 缓冲用 `XRT_AES256_KEY_SIZE`/`XRT_AES_GCM_NONCE_DEFAULT_SIZE` 声明——长度永远来自宏；示例全零密钥只为可复现，生产密钥来自 KDF 或随机源。② `Init` 绑定密钥与 16 字节标签后，`Seal` 原位加密：输出缓冲就是输入缓冲（允许重叠），写入 19 字节密文 + 追加 16 字节标签，共 35 字节——缓冲容量 64 足够。③ `Open` 读入"密文+标签"35 字节，验证标签、原位解密；标签不符会返回 `false` 且缓冲内容不可信。④ AAD `"message-v1"` 两边一致地传入——它不进密文，但进标签：改动版本号会让打开失败，这就是"看得见但改不了"。⑤ 末尾 `Clear` 清除密钥日程——状态结构里存着扩展密钥，与明文密钥同等待遇。

### 第二个完整程序：ChaCha20-Poly1305 便捷层

第二个程序来自 `examples/crypto/chacha20_poly1305/main.c`，无状态一对入口完成可认证的原位加解密：

```embed path="examples/crypto/chacha20_poly1305/main.c" title="examples/crypto/chacha20_poly1305/main.c"
```

```term
$ gcc -O1 -DXRT_MODULE_ALL -I single -include xrt.h impl.c examples/crypto/chacha20_poly1305/main.c -lws2_32 -liphlpapi
authenticated message
```

**刚才发生了什么。** ① 没有状态结构——密钥即用即走：`Seal` 直接收 Key/Nonce/AAD/明文，输出密文+16 字节标签（`iSealedSize = 明文长 + XRT_CHACHA20_POLY1305_OVERHEAD`）。② 密钥用 `0x40+i` 模式填充、nonce 逐字节递增——固定材料复现输出，头注释明确提醒"生产中同一密钥下的 nonce 必须唯一"。③ AAD `"header"` 演示协议头绑定；两调用共用同一缓冲原位操作。④ 末尾 `xrtSecureZero(Key, ...)` 清密钥——便捷层没有状态可 Clear，密钥数组本身就是敏感材料。

### 番外：篡改即失败（自己动手验证一次）

两个示例都是"加密→解密"往返成功路径。理解 AEAD 的关键一刻是**亲眼看到失败**：把 `aes_gcm` 示例在 `Seal` 之后加一行 `Buffer[0] ^= 1;`（翻转密文首比特），再调 `Open`——返回 `false`，程序走错误分支。把篡改换成标签的最后一字节、或把 AAD 换成 `"message-v2"`，同样失败。**三个输入动任何一个比特，验证都拒绝**——这就是"认证加密"四个字的分量，也是本卷"失败必须单一"纪律的第一个兑现点。

## 契约

- **AES-GCM 状态**：`Init` 固定密钥（128/192/256）与标签长度（4..16，默认 16）；密钥日程一次扩展、消息间复用；`Clear` 清状态；`TagSize` 查询绑定值。
- **两套调用形态**：`Encrypt/Decrypt` 密文与标签分离（协议自定排布）；`Seal/Open` 标签紧随密文（封包最顺）；两者共享同一密钥日程与安全性。
- **ChaCha20-Poly1305**：RFC 8439 固定参数（32 密钥/12 nonce/16 标签）；无状态便捷层每消息处理密钥；TLS 内部用带状态变体；常量时间软件实现。
- **nonce**：同钥唯一是数学边界——计数器（单调、单点顺序）或 96 位随机（短生命周期密钥）；nonce 可明文传输，保密的是密钥不是 nonce。
- **AAD**：只认证不加密；版本头/序号/上下文用 AAD 绑定；收发双方必须逐字节一致。
- **失败语义**：`Open/Decrypt` 标签不符整体失败，输出缓冲未定义——正确程序丢弃整条记录，不存在"部分解密"。
- **重叠**：`Seal/Open` 支持原位（输入输出同一缓冲）；分离形态的输出与标签缓冲不可与输入重叠（按契约检查）。
- **错误**：参数/容量错误在首次写入输出前返回；密钥长度非法（非 128/192/256）直接拒绝。
- **裁剪**：AES、AES-GCM、ChaCha20、Poly1305、组合层全部独立特性宏——物联网目标只带 ChaCha20-Poly1305 时不携带任何 AES 代码。

## 避坑

### 坑 1：nonce 重复使用

症状：安全审计直接判不合格；更糟的情况没人告诉你——GCM nonce 重复可恢复认证密钥，攻击者由此获得**伪造任意记录**的能力，且事后无法从密文察觉。

原因：GCM 的安全证明建立在"同钥 nonce 不重复"之上。随机 12 字节 nonce 用固定长期密钥加密海量记录，重复概率随记录数平方增长——2^32 条后显著。

```c bad
static const uint8 Nonce[12] = { 0 };  /* 固定 nonce */
for ( each record ) {
	xrtAesGcmSeal(&State, Nonce, sizeof(Nonce),
		NULL, 0, Plain, iSize, Out, sizeof(Out));
	/* 第二次调用起：认证密钥逐步泄露 */
}
```

```c good
uint64 iCounter = 0;
uint8 Nonce[XRT_AES_GCM_NONCE_DEFAULT_SIZE];
for ( each record ) {
	memset(Nonce, 0, sizeof(Nonce));
	/* 计数器 nonce：同一密钥单点顺序递增，永不重复 */
	memcpy(Nonce + 4, &iCounter, sizeof(iCounter));
	iCounter++;
	xrtAesGcmSeal(&State, Nonce, sizeof(Nonce),
		NULL, 0, Plain, iSize, Out, sizeof(Out));
}
```

### 坑 2：Open 失败后继续使用输出缓冲

症状：日志里出现乱码"明文"，下游解析器收到从未写过的数据；或者更隐蔽——错误分支忘了 return，带着被污染的缓冲继续跑。

原因：`Open` 标签不符时**整体失败、输出未定义**——可能部分写入、可能保持旧值。把"失败时缓冲里可能有东西"当成"失败时缓冲是原文"是致命误解。

```c bad
uint8 Plain[64];
if ( !xrtAesGcmOpen(&State, Nonce, 12, pAad, iAad,
		Cipher, iCipherSize, Plain, sizeof(Plain)) ) {
	/* 忘了 return：带着未定义的 Plain 继续走 */
}
parse(Plain);   /* 处理了攻击者影响下的数据 */
```

```c good
if ( !xrtAesGcmOpen(&State, Nonce, 12, pAad, iAad,
		Cipher, iCipherSize, Plain, sizeof(Plain)) ) {
	return false;   /* 整条记录作废：这是唯一的正确动作 */
}
parse(Plain);
```

### 坑 3：自己组合"AES 加密 + HMAC 认证"

症状：组合看起来能跑，评审被拒；或真的被攻击——先加密后 MAC 的 Encrypt-then-MAC 虽理论上正确，但长度边界、nonce 关联、比较时机任何一处手工失误都直接破坏安全性。

原因：密码学工程的十年教训是组合极易出错——顺序、覆盖范围、常量时间比较、密钥分离（同一密钥既做加密又做 MAC 是灾难），每一项都有知名失效案例。AEAD 把这些决策全部内化。

```c bad
/* 裸块加密/流加密得到密文，再手工补一个 HMAC */
encrypt_only(Key1, Nonce, Plain, iSize, Cipher);
xrtHmacSha256(Key2, ..., Cipher, iSize, Mac);
/* 顺序、覆盖、密钥分离……每一步都是待审的攻击面 */
```

```c good
xrtAesGcmSeal(&State, Nonce, 12, pAad, iAad,
	Plain, iSize, Out, sizeof(Out));
/* 或 ChaCha20-Poly1305：一个调用，机密性+完整性+认证 */
```

## 练习

### 基础：往返与篡改三连

跑通 `aes_gcm` 示例后做三个实验：翻转密文一字节、翻转标签一字节、改 AAD 一个字符——三种情况 `Open` 都必须失败。把三次失败路径改成打印 `rejected` 并返回非零。验收标准：三种篡改各自的失败输出一致（你看不出是哪种篡改——这是特性不是缺陷）。

### 进阶：带版本迁移的配置加密器

实现 `seal_config(密钥, 版本号, JSON, 输出)` 与 `open_config`：AAD 绑定 `"config-v1"` 与版本号（版本拼进 AAD 字符串），`Seal/Open` 用 ChaCha20-Poly1305 便捷层。故意用 v1 密钥打开 v2 记录——必须失败。提示：AAD 由版本字符串构造，两边构造代码共享同一函数，避免手抄不一致。

### 挑战：记录流加密通道

在一条 TCP 连接（第 66 章同步面即可）上实现简单加密协议：握手用固定测试密钥，之后每条记录 = 8 字节序号（明文）+ Seal 的密文标签；nonce 用序号填充（计数器法），序号同时进 AAD 防重排。接收方校验序号严格递增，乱序即断开。验收标准：用第 71 章的分帧器拆记录；攻击实验——重放旧记录、乱序、篡改——三种都断开；2^16 条记录无内存增长（第 6 章统计）。

## 速查

| 知识点 | 速查 |
| --- | --- |
| AEAD 三保证 | 机密性 + 完整性 + 认证，一次调用打包；失败原子性（不吐可疑明文） |
| AES-GCM 状态机 | `Init` 绑密钥与标签长（4..16 默认 16）→ 密钥日程一次扩展；`Clear` 清状态 |
| 两套形态 | `Encrypt/Decrypt` 标签分离；`Seal/Open` 标签紧随密文（+16 字节） |
| ChaCha20-Poly1305 | RFC 8439 固定 32/12/16；无状态便捷层；常量时间、无 AES 硬件时首选 |
| 选型 | 有 AES-NI → AES-GCM；ARM/物联网 → ChaCha20-Poly1305；两者同级可互换 |
| nonce 纪律 | 同钥唯一是数学边界；计数器（单点单调）或 96 位随机（短命密钥） |
| AAD | 只认证不加密；版本/序号/上下文绑定；收发逐字节一致 |
| 失败语义 | `Open` 失败输出未定义；整条丢弃是唯一正确动作 |
| 原位操作 | `Seal/Open` 支持输入输出同缓冲；分离形态标签缓冲独立 |
| 裁剪 | AES/AES-GCM/ChaCha20/Poly1305/组合层全独立宏；裸变换仅协议互操作 |
