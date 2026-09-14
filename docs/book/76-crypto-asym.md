---
num: 76
slug: crypto-asym
title: 密码学（下）：RSA 与 ECDSA 与 Ed25519 与 X25519
volume: 卷八 安全
type: practice
lead: 密钥交换与签名两条主线：x25519/ECDH 让陌生人共享秘密，Ed25519/ECDSA/RSA 让声明可验证。
api: crypto, random
---

## 导读

前两章的原语都是**对称**的——通信双方用同一把密钥。但第一句话就卡住了：对称密钥怎么交给对方？在不可信信道上明文发送等于公开；预先见面交换又要求已经存在安全信道。非对称密码拆掉这个死结：密钥成对出现，公开的那把随意分发，私密的那把从不出手。本章覆盖两条彼此独立的主线——**密钥交换**（x25519/x448 与 P-256/P-384 的 ECDH：双方各持私钥、交换公钥、算出相同秘密）与**数字签名**（Ed25519/ECDSA/RSA-PSS：私钥签、公钥验，声明不可抵赖）。它们是第 78 章证书的原料（证书就是"对公钥的签名"）与第 82 章 TLS 握手的引擎。学完本章你应当能对任意协议需求回答"选哪条曲线、哪种签名、为什么"。

## 引入

两个场景。场景一：你的客户端要与从未谋面的服务器建立加密通道——双方在完全公开的信道上各说一句"这是我的公钥"，然后各自用自己的私钥与对方的公钥计算，得到**完全相同**的 32 字节秘密；任何窃听者拿到全部公开消息也算不出来。这就是 DH（Diffie-Hellman）交换，x25519 是它 2026 年的现代形态。场景二：软件发布方要证明"这个更新包确实是我出的"——用只有自己持有的私钥对包摘要签名，全世界任何人用公开分发的公钥验证。签名与 MAC（第 74 章）的区别在信任模型：MAC 验证方必须持有密钥（对称），签名验证方只需要公钥（可公开）——这让它能支撑"一对多"的声明：一个发布方，一百万个验证者。

两条主线在 TLS 1.3 握手里同框出镜：x25519 交换出共享秘密，服务端用证书私钥（RSA-PSS 或 ECDSA）对握手摘要签名证明身份。本章的四个示例分别对应这些角色。

## 概念

### 密钥交换：ECDH 的对称性

迪菲-赫尔曼的魔法可以画成一张对称图：

```diagram flow
- 准备：A 生成私钥 sA 与公钥 pubA=sA·G；B 同理得 sB、pubB
- 交换：公开信道互发公钥（窃听者全看得见）
- A 计算：sharedA = sA · pubB = sA·sB·G
- B 计算：sharedB = sB · pubA = sB·sA·G
- 结果：sharedA == sharedB——只有持有私钥的双方能算到这里
```

乘法结合律保证了两个乘积相等，而窃听者只见过两个公钥——从 pubA 反推 sA 是椭圆曲线离散对数问题，这就是全部安全性。XRT 覆盖三族：

- **x25519**（32 字节私钥/公钥/共享）：RFC 7748，TLS 1.3 默认组，无随机源依赖；`xrtX25519Public`（私钥→公钥）、`xrtX25519`（裸标量乘）、`xrtX25519Shared`（**安全共享秘密入口**——额外处理全零与低阶点等恶意公钥输入）、`xrtX25519KeyPair`（依赖安全随机源的便利组合）。
- **x448**（56 字节形态）：同构的更大安全裕度版本，API 与 x25519 完全对称（`xrtX448Public/Shared/KeyPair`）。
- **P-256/P-384**：NIST 曲线（65/97 字节非压缩点），除了 ECDH 还公开点算术（`Valid` 点校验、`Add` 点加、`Multiply` 标量乘）——某些协议（如 EC 签名实现、零知识证明）需要直接操作点。

**共用纪律**：算出的共享秘密**绝不直接当对称密钥**——它形状不定、可能可区分，必须经第 74 章的 HKDF 派生成"密钥材料"（第 77 章的会话示例是标准链条）。x25519 的私钥就是 32 字节随机数（`xrtX25519KeyPair` 内部完成钳位），P 曲线的标量是大端整数——固定材料仅用于测试复现，生产私钥一律来自安全随机源。

### 签名主线一：Ed25519（现代默认）

Ed25519 是"新一代"签名的代表：64 字节签名、32 字节公钥/种子、**确定性签名**（同一消息同一密钥永远产出同一签名——不依赖随机数，从根上消灭了"随机数劣化导致私钥泄露"这一整类事故，索尼 2010 年的 ECDSA 事故就是前车之鉴）。API 分两层：逐次入口 `xrtEd25519Sign(种子, 消息, 签名)` 与带状态 `xrtEd25519KeyInit/SignKey/KeyClear`——后者预展开签名中间量，同钥多签（证书签发、批量授权）明显更快，用完 `KeyClear` 抹除敏感中间态。验签 `xrtEd25519Verify(公钥, 消息, 签名)` 是纯公开操作，无密钥可清。RFC 8032 测试向量命中即互操作证明——XRT 签的签名 OpenSSL 能验，反之亦然。

### 签名主线二：ECDSA（P-256/P-384）

ECDSA 是 TLS 证书与现有 PKI 生态的主力。XRT 的实现分四层裁剪：`ECDSA_VERIFY`/`ECDSA_SIGN`（曲线无关核心）→ `ECDSA_P256/P384`（raw 验签：64 字节 `r||s` 定长拼接）→ `ECDSA_DER`（raw 与规范 DER 的转换层）→ 曲线专属 DER 便利层。签名采用**确定性 RFC 6979**——与 Ed25519 同样不依赖外部随机数。raw 与 DER 两种表示的取舍：DER 是证书与现存协议的线格式（变长、TLV 编码），raw 是内部计算与紧凑协议的形态（定长）——XRT 把转换独立成层，两种世界都能进。

### 签名主线三：RSA（存量霸主）

RSA 的安全基于大整数分解难题，密钥大（2048 位起步）、运算慢，但存量证书生态最广。XRT 的 RSA 分层最能体现"算法 vs 协议"的边界：

- `xrtRsaPublic`/`xrtRsaPrivate`：**原始模幂**——数学底座，明文绝不能直接这样"加密"（无填充即无安全，示例用它仅证明底座自洽）。
- **RSA-PSS**（`xrtRsaPssSign/PssVerify`）：现代标准，TLS 1.3 唯一的 RSA 签名模式。签名对象是**消息摘要**（两个哈希参数：内容摘要与 MGF1 掩码生成）。盐可选：显式盐 `PssSignSalt`（测试向量与确定性协议用）、随机盐便利入口（默认）、验证方可用 `XRT_RSA_PSS_SALT_ANY` 不约束盐长。
- **RSA-PKCS1** v1.5（`xrtRsaPkcs1Verify` 等）：历史协议兼容层，规范 EMSA 编码。

所有私钥运算的结果都以公开指数**复核**——内部任何中间步骤出错都会被发现而不是静默产出错误签名，这是防御性实现的样板。

### 选型速断

新系统签名**优先 Ed25519**（快、小、确定性、无侧信道惯性），需要融入现存 PKI 用 ECDSA P-256，对接老系统才 RSA-PSS；密钥交换一律 x25519（裕度要求高用 x448），政府/合规指定 NIST 曲线才 P-256/P-384。这张优先级表与 TLS 1.3 的组协商顺序一致——第 82 章会看到它落地。

## 示例

### 第一个完整程序：ECDH 对称性与曲线算术自检

下面的程序来自 `examples/crypto/ecdh_tour/main.c`，一次验证 x25519/x448 双方共享秘密一致与 P 曲线算术自洽：

```embed path="examples/crypto/ecdh_tour/main.c" title="examples/crypto/ecdh_tour/main.c"
```

```term
$ gcc -O1 -DXRT_MODULE_ALL -I single -include xrt.h impl.c examples/crypto/ecdh_tour/main.c -lws2_32 -liphlpapi
ecdh: x25519 public + shared-secret symmetric ok
ecdh: x448 public + shared-secret symmetric ok
ecdh: p256 public/valid + add == multiply-by-2 ok
ecdh: p384 public/valid + add == multiply-by-2 ok
```

**刚才发生了什么。** ① 双方私钥用固定序列（1..56 与倒序）——可复现；生产中 `xrtX25519KeyPair` 一类入口从安全随机源采样。② 对称性验证是 ECDH 的核心断言：`xrtX25519(sA, pubB)` 与 `xrtX25519(sB, pubA)` 逐字节相等——数学上它们都是 sA·sB·G。③ P 曲线段做两件自检：`Valid` 接受公钥导出的合法点、**拒绝全 0xFF 的伪点**（点校验是 ECDH 安全的一部分——未校验的点可被构造成小子群攻击）；`Add(P,P) == Multiply(2,P)` 验证点算术一致，不需要预置基点常量。④ 注意 NIST 点是大端编码、长度随曲线（65/97 字节），而 x25519 固定 32 字节——两种族的字节形态差异在协议设计时要心里有数。

### 第二个完整程序：Ed25519 带状态签名

第二个程序来自 `examples/crypto/ed25519_sign/main.c`，展开密钥状态上签一条消息：

```embed path="examples/crypto/ed25519_sign/main.c" title="examples/crypto/ed25519_sign/main.c"
```

```term
$ gcc -O1 -DXRT_MODULE_ALL -I single -include xrt.h impl.c examples/crypto/ed25519_sign/main.c -lws2_32 -liphlpapi
signed: yes
```

**刚才发生了什么。** ① `xrtEd25519KeyInit` 从 32 字节种子预展开签名中间量——种子全零仅为复现；同钥多签场景（CA 批量签证书、令牌服务）这个状态复用是性能关键。② 签名是确定性的：重复运行输出恒定，没有随机数参与——这也意味着签名结果可作测试向量。③ `KeyClear` 立即抹除展开状态（里面含可推导私钥的材料），然后才打印结果——**清零在成功路径上也要做**，不只是错误路径。配套的 `examples/crypto/ed25519_verify` 用 RFC 8032 §7.1 的空消息向量验证公开钥签名——标准向量命中就是互操作性证明。

### 第三个完整程序：RSA 三层——底座、显式盐、随机盐

第三个程序来自 `examples/crypto/rsa_pss/main.c`，从原始模幂到 PSS 随机盐一次走完：

```embed path="examples/crypto/rsa_pss/main.c" title="examples/crypto/rsa_pss/main.c"
```

```term
$ gcc -O1 -DXRT_MODULE_ALL -I single -include xrt.h impl.c examples/crypto/rsa_pss/main.c -lws2_32 -liphlpapi
RSA-PSS: valid
```

**刚才发生了什么。** ① 第一段故意演示**原始模幂**并往返自证数学正确——头注释与代码注释都强调"普通消息不得这样加密"：没有填充的 RSA 是可被数学攻击直接求解的教科书例子，这一段存在的意义是让你看见底座与安全协议之间的距离。② PSS 签名两次：显式盐 `PssSignSalt`（同输入产出可复现签名——协议测试的形态）与随机盐 `PssSign`（每次不同——生产默认）；验证用同一个 `PssVerify`，第二次传 `XRT_RSA_PSS_SALT_ANY` 表示不约束盐长——**验证方通常不该假设签名方的盐策略**。③ 签名对象是 `Fixture.Hash`（摘要）而非原始消息——RSA 签名永远签摘要，两个 `XCRYPTO_HASH_SHA256` 参数分别是内容摘要与 MGF1 的掩码哈希。④ 清理段把密钥结构、明文缓冲、签名全部 `xrtSecureZero`——RSA 私钥结构里含 CRT 参数与素数，敏感面比对称密钥大得多。

## 契约

- **交换族**：x25519/x448/P-256/P-384 全部支持私钥→公钥导出与双方共享秘密；`Shared`/安全入口对恶意公钥（低阶点、全零输出）有处理，裸标量乘入口（`xrtX25519`）不做输入校验——协议层直接用 `Shared` 形态；点算术（Valid/Add/Multiply）仅 P 曲线公开。
- **随机源边界**：裸交换与签名核心**不依赖随机源**（x25519 私钥即随机数、Ed25519/ECDSA 确定性）；只有 `KeyPair` 便利层与 PSS 随机盐依赖 `RANDOM_SECURE`——裁剪时随机源只随这些入口进入。
- **签名表示**：Ed25519 定长 64 字节；ECDSA raw 为 r||s 定长、DER 为证书线格式，转换层独立；RSA-PSS/PKCS1 签名长度等于模长（2048 位即 256 字节）。
- **RSA 分层**：原始模幂仅演示层次，无填充无安全；PSS 是现代标准（TLS 1.3 唯一 RSA 模式）；显式盐可复现、随机盐默认、验证可 `XRT_RSA_PSS_SALT_ANY`；私钥运算结果以公开指数复核。
- **确定性签名**：Ed25519 与 ECDSA（RFC 6979）签名不依赖外部随机数——重复签名同消息结果恒定，可作测试向量。
- **敏感材料清理**：`xed25519key` 用后 `KeyClear`；RSA 私钥结构、种子、共享秘密用 `xrtSecureZero`；验签是公开操作无密钥可清。
- **错误**：失败经返回值宣告；参数长度/点编码非法直接拒绝；点校验（Valid）拒绝非曲线点与伪编码。

## 避坑

### 坑 1：RSA 原始模幂直接加密消息

症状：安全评审一眼否决；认真攻击下，无填充 RSA 的明文可被低指数/选择密文等经典手法直接恢复——教科书 RSA 不是加密方案。

原因：`xrtRsaPublic`/`xrtRsaPrivate` 是数学底座（模幂），没有任何填充与随机化。安全等级为零：相同明文永远产出相同密文、明文与密文可互相代数操作。

```c bad
uint8 Cipher[128];
xrtRsaPublic(&Key.Public, SecretMessage, 128, Cipher);
send(Cipher);   /* 无填充 RSA：可被数学攻击直接解出明文 */
```

```c good
/* 现代做法只有两条路：
   1) 加密：不用 RSA 加密数据——用 ECDH 交换 + AEAD（见第 75/77 章）
   2) 签名：xrtRsaPssSign（TLS 1.3 唯一 RSA 签名模式） */
uint8 Signature[128];
xrtRsaPssSign(&Key, XCRYPTO_HASH_SHA256, XCRYPTO_HASH_SHA256,
	Digest, Signature);
```

### 坑 2：ECDH 共享秘密直接当对称密钥

症状：安全评审不过；某些场景下共享秘密的低位有统计偏差，直接喂给 AES 的密钥表削弱安全性；协议也无法派生多把用途密钥。

原因：DH 输出是"数学产物"而不是"合格密钥材料"——形状、分布、用途绑定都不达标。HKDF 的设计目的就是这层转换。

```c bad
uint8 Shared[32];
xrtX25519Shared(sA, pubB, Shared);
xaesgcm State;
xrtAesGcmInit(&State, Shared, 32, 16);  /* 原始秘密直接当密钥 */
```

```c good
uint8 Shared[32], SessionKey[32];
xrtX25519Shared(sA, pubB, Shared);
xrtHkdfSha256(Salt, iSalt, Shared, sizeof(Shared),
	Transcript, iTranscript, SessionKey, sizeof(SessionKey));
xrtSecureZero(Shared, sizeof(Shared));  /* 中间秘密即弃 */
xrtAesGcmInit(&State, SessionKey, 32, 16);
```

### 坑 3：验签结果用 memcmp 惯性处理或忽略返回值

症状：验签函数明明返回了 `false`，代码却继续走成功分支；或者试图把签名"比对"成期望值——签名含随机盐（PSS）或虽确定（Ed25519）但代码逻辑把"验证"写成了"比较"。

原因：验证是**谓词函数**（输入公钥/消息/签名，输出 bool），不是"算出一个期望值再比对"。PSS 的随机盐意味着同一消息同一密钥的两次签名本就不同——拿"重算签名再比对"的思路写验证，遇到随机盐签名必错。

```c bad
uint8 Expect[128];
xrtRsaPssSign(&Key, XCRYPTO_HASH_SHA256, XCRYPTO_HASH_SHA256,
	Digest, Expect);                       /* 重算签名 */
if ( memcmp(Expect, Received, 128) == 0 ) { /* 随机盐下永不相等 */
	accept();
}
```

```c good
if ( xrtRsaPssVerify(&Key.Public, XCRYPTO_HASH_SHA256,
		XCRYPTO_HASH_SHA256, XRT_RSA_PSS_SALT_ANY, Digest,
		Received, 128) ) {
	accept();   /* 验证是谓词：一步得到答案 */
}
```

## 练习

### 基础：x25519 三步握手复现

固定 A/B 双方私钥（如 32 字节的 0x01 与 0x02），完整走 `Public → 互发 → Shared`，打印双方共享秘密的十六进制并确认一致。再交换私钥角色重跑，确认对称性不依赖"谁是发起方"。验收标准：两次运行输出逐位一致（确定性）。

### 进阶：确定性签名测试向量生成器

用 Ed25519 带状态入口对三条不同消息签名，输出"消息十六进制 + 签名十六进制"的向量表；再用逐次入口 `xrtEd25519Sign` 重签，验证与带状态结果逐位一致。提示：种子用固定值保证可复现；这就是给同事的互操作测试向量——对方用任意 RFC 8032 实现（如 OpenSSL）应能全部验过。

### 挑战：迷你密钥协商协议

在回环 TCP 上实现两方握手：各自 `xrtX25519KeyPair` 生成临时密钥，交换公钥（明文 JSON 即可），双方 `Shared` + HKDF（info 含"迷你协议 v1"与双方公钥）派生会话密钥，随后用 ChaCha20-Poly1305 互发一条加密消息验证。攻击实验：中间人替换公钥——说明为什么裸协商不抗中间人（答案在第 79 章证书），把"需要证书"的结论写进你的 README。验收标准：全程密钥材料（私钥/共享秘密/会话密钥）用后即 `xrtSecureZero`；第 6 章统计零泄漏。

## 速查

| 知识点 | 速查 |
| --- | --- |
| 两条主线 | 交换（x25519/x448/P-256/P-384 ECDH）与签名（Ed25519/ECDSA/RSA）互相独立、按需裁剪 |
| ECDH 对称性 | sA·pubB == sB·pubA == sA·sB·G；窃听者只见公钥算不出秘密 |
| 交换入口 | 裸标量乘（无校验）vs `Shared` 安全入口（处理恶意公钥）vs `KeyPair` 便利（含随机源） |
| Ed25519 | 32 种子/32 公钥/64 签名；确定性；`KeyInit/SignKey/KeyClear` 带状态加速多签 |
| ECDSA | P-256/P-384；raw 定长 r‖s 与 DER 证书线格式，转换层独立；RFC 6979 确定性 |
| RSA 分层 | 原始模幂（仅底座）→ PSS（TLS 1.3 唯一 RSA 模式）→ PKCS1（兼容）；结果公开指数复核 |
| PSS 盐 | 显式盐可复现、随机盐默认、验证 `XRT_RSA_PSS_SALT_ANY` 不约束 |
| 选型 | 新系统 Ed25519+x25519；融 PKI 用 ECDSA P-256；接存量才 RSA-PSS |
| 铁律 | 共享秘密必经 HKDF；RSA 不直接加密数据；验证是谓词不是比对 |
| 清理 | 展开密钥 `KeyClear`；种子/私钥/共享秘密 `xrtSecureZero`；验签无密钥可清 |
