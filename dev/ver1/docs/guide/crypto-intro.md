# Crypto 入门：摘要、密钥派生、AEAD 和签名验证不是同一件事

> 目标：把 `xrtSHA256()`、`xrtHMAC_SHA256()`、`xrtHKDFExtract()/Expand()`、`xrtChaCha20Poly1305Encrypt()/Decrypt()`、`xrtX25519Keypair()/SharedSecret()`、`xrtEd25519Sign()/Verify()` 讲成第 3 阶段里“加密原语和边界”这条正式主线。读完这页后，你应该明确知道：哈希、消息认证、密钥派生、加密、密钥交换、签名验证各自解决什么问题，什么时候应该把它们串起来，什么时候又绝对不能混用。

[返回教学文档](README.md)

---

## 1. 为什么 `crypto` 必须单独讲，而不是在网络/TLS 里顺手带过

很多人第一次接触加密接口时，会把它们都理解成：

- “反正都是安全函数”
- “选一个顺手的就行”

这在真实项目里会很快出问题。

因为下面这些问题根本不是同一种问题：

- 我只想要一份稳定摘要
- 我想确认消息是不是来自共享密钥对端
- 我拿到了共享秘密，想派生成会话密钥
- 我想把数据加密并校验篡改
- 我想验证一个签名

如果你把这些能力混在一起理解，就会得到一些很危险的写法：

- 以为 `SHA-256` 能证明“消息来自谁”
- 以为 `X25519` 输出就是最终业务密钥
- 以为 `ChaCha20` 和 `ChaCha20-Poly1305` 只是“是否多一个 tag”
- 以为 `Ed25519Verify()` 和 `xrtECDSAVerify()` 可以直接互换

所以 `crypto` 这页真正要解决的是：

1. 每种原语解决什么问题
2. 它们之间应该怎样串联
3. 当前 XRT 公开 API 的真实边界是什么


## 2. 先把 9 个边界分清楚

### 2.1 Hash、HMAC、HKDF、AEAD、密钥交换、签名验证不是一个层级

推荐你先把它们拆成下面这张表：

| 能力 | 解决的问题 |
|------|------------|
| Hash | 给一段数据做稳定摘要 |
| HMAC | 用共享密钥证明“这段消息来自持钥方” |
| HKDF | 把输入熵或共享秘密整理成可用密钥材料 |
| AEAD | 同时完成加密和完整性校验 |
| 密钥交换 | 让双方算出同一份共享秘密 |
| 签名验证 | 验证消息是否来自某个公钥对应私钥 |

所以：

- `SHA-256`
	- 不是消息认证
- `HKDF`
	- 不是加密
- `X25519`
	- 不是最终会话密钥
- `Ed25519Verify`
	- 也不是“算哈希”


### 2.2 Hash 适合做摘要、指纹、内容 ID，不证明消息来自谁

当前公开哈希主线包括：

- `xrtSHA1()`
- `xrtSHA256()`
- `xrtSHA384()`
- `xrtSHA512()`

其中最常用的新设计入口通常是：

- `SHA-256`
- `SHA-384`
- `SHA-512`

而 `SHA-1` 当前更接近：

- WebSocket 握手
- 兼容性场景

不要把普通哈希误当成认证手段。  
任何人都能对同一段公开消息重新算出相同摘要。


### 2.3 HMAC 解决“共享密钥双方如何校验消息”

如果双方已经共享一份密钥，而你想做：

- webhook 校验
- 内部 agent 通道消息认证
- 配置分发签名

那优先考虑的是：

- `xrtHMAC_SHA256()`
- `xrtHMAC_SHA384()`
- `xrtHMAC_SHA512()`

HMAC 的重点不是“把摘要再算一遍”，而是：

- 没有共享密钥的人，不能伪造同样的结果

也正因为如此，HMAC 解决的是：

- 对称认证

不是：

- 公私钥签名


### 2.4 HKDF 负责“把原始秘密整理成真正要用的密钥”

当前公开 HKDF API 有两套：

- `xrtHKDFExtract()` / `xrtHKDFExpand()`
	- SHA-256 版
- `xrtHKDFExtract_SHA384()` / `xrtHKDFExpand_SHA384()`
	- SHA-384 版

要把它理解成：

- 输入可能是随机种子、共享秘密、主密钥
- 输出才是你真正拿去做 AEAD / HMAC / 子密钥分发的材料

这一步非常关键，因为：

- `X25519` 共享秘密不等于最终业务密钥
- 不同用途最好分不同 `info`
- 同一份根秘密不该直接生搬硬套到所有模块

当前实现里还有一个值得明确记住的边界：

- `xrtHKDFExtract()` 的 `pSalt == NULL` 或 `iSaltLen == 0`
	- 会走默认零盐

这能用，但正式协议设计里仍然更推荐：

- 你自己明确管理 salt


### 2.5 AEAD 解决的是“加密 + 篡改校验”，不是只有“能解密”

当前公开 AEAD 主线包括：

- `xrtChaCha20Poly1305Encrypt()/Decrypt()`
- `xrtAES128GCMEncrypt()/Decrypt()`
- `xrtAES256GCMEncrypt()/Decrypt()`

这里最容易写错的有 3 条：

1. 输出缓冲区要比明文多 `16` 字节  
	因为尾部会附带认证 tag。
2. 解密时传入长度包含这 `16` 字节 tag  
	不是只传明文长度。
3. 同一把 key 下，nonce 不能乱复用  
	尤其是 ChaCha20-Poly1305 和 AES-GCM 这种 AEAD。

如果你的目标只是协议内部 keystream 处理，才考虑：

- `xrtChaCha20()`

大多数业务场景里，不应该优先选择“裸流加密”，而应该优先选择：

- AEAD


### 2.6 X25519 / X448 / ECDH 产出的是共享秘密，不是最终会话密钥

当前公开密钥交换能力包括：

- `xrtX25519Keypair()` / `xrtX25519SharedSecret()`
- `xrtX448Keypair()` / `xrtX448SharedSecret()`
- `xrtECDHSecp256r1Keypair()` / `xrtECDHSecp256r1SharedSecret()`
- `xrtECDHSecp384r1Keypair()` / `xrtECDHSecp384r1SharedSecret()`

最稳的心智模型是：

1. 双方先各自生成密钥对
2. 算出同一份共享秘密
3. 再把共享秘密送进 HKDF
4. 最后派生成 AEAD key、nonce base 或 HMAC key

不要直接把共享秘密当：

- 最终对称密钥
- 数据库存储口令
- 长期业务主键


### 2.7 `Ed25519Verify()` 验证的是原始消息；`ECDSA/RSA` 验证的是哈希

这是当前最值得先记住的一条接口边界。

`Ed25519` 主线：

- `xrtEd25519Keypair()`
- `xrtEd25519PublicKey()`
- `xrtEd25519Sign()`
- `xrtEd25519Verify()`

它的验证入口是：

- 原始消息
- 签名
- 公钥

而 `ECDSA / RSA` 当前公开主线是：

- `xrtECDSAVerify()`
- `xrtRSAPSSVerify()`
- `xrtRSAPKCS1Verify()`
- `xrtRSAModPow()`

它们面向的是：

- 已经算好的哈希
- 已有签名
- 已有公钥 / 模数指数

也就是说，下面两类不能直接混着写：

- `Ed25519Sign(message) -> Ed25519Verify(message, sig, pubkey)`
- `SHA256(message) -> ECDSAVerify(hash, sig, pubkey)`


### 2.8 当前公开签名主线并不对称

当前 XRT 公开面里：

- `Ed25519`
	- 有生成和验证
- `ECDSA / RSA`
	- 当前公开主线偏验证

这非常适合：

- TLS 证书链验证
- 对第三方签名结果做校验

但如果你要自己设计一整套：

- RSA 签名生成
- ECDSA 签名生成

当前公开 API 并没有把这两条都补成对称用户接口。


### 2.9 `SHA-384` 的增量上下文是 `xsha512_ctx`，更新阶段走 `xrtSHA512Update()`

这是当前头文件里一个很真实、也很容易漏讲的边界。

公开 API 里：

- 有 `xrtSHA384Init()`
- 有 `xrtSHA384Final()`
- 没有单独 `xrtSHA384Update()`

因此增量 SHA-384 的正确写法是：

```c
xsha512_ctx tCtx;
uint8 aucOut[48];

xrtSHA384Init(&tCtx);
xrtSHA512Update(&tCtx, sPart1, iLen1);
xrtSHA512Update(&tCtx, sPart2, iLen2);
xrtSHA384Final(&tCtx, aucOut);
```

这不是文档技巧，而是当前真实公开合同。


## 3. 最小 DEMO：先把 Hash、HMAC、HKDF 串成一条线

第一步不要一上来就写会话加密。  
先把下面这条基础链路跑通：

1. 对消息算摘要
2. 对同一消息算 HMAC
3. 用一份输入材料派生出业务密钥

下面这个最小例子对应：

- [examples/crypto/hash_functions/main.c](../../examples/crypto/hash_functions/main.c)

```c
#define XRT_IMPLEMENTATION
#include "../../singlehead/xrt.h"
#include <stdio.h>
#include <string.h>

int main(void)
{
	const char* sMsg = "xrt crypto demo";
	const uint8 aucSalt[] = { 0x10, 0x22, 0x34, 0x48, 0x5a, 0x6c, 0x7e, 0x80 };
	const uint8 aucInfo[] = "doc/hash-hmac-hkdf";
	uint8 aucSha256[32];
	uint8 aucSha384[48];
	uint8 aucHmac[32];
	uint8 aucPrk[32];
	uint8 aucOkm[48];
	xsha512_ctx tSha384;

	xrtInit();

	xrtSHA256(sMsg, strlen(sMsg), aucSha256);

	xrtSHA384Init(&tSha384);
	xrtSHA512Update(&tSha384, sMsg, 4);
	xrtSHA512Update(&tSha384, sMsg + 4, strlen(sMsg) - 4);
	xrtSHA384Final(&tSha384, aucSha384);

	xrtHMAC_SHA256((const uint8*)"demo-key", 8, (const uint8*)sMsg, strlen(sMsg), aucHmac);
	xrtHKDFExtract(aucPrk, aucSalt, sizeof(aucSalt), (const uint8*)sMsg, strlen(sMsg));
	xrtHKDFExpand(aucOkm, sizeof(aucOkm), aucPrk, sizeof(aucPrk), aucInfo, strlen((const char*)aucInfo));

	xrtUnit();
	return 0;
}
```

这段代码最重要的不是打印哪串十六进制，而是先建立 3 个观念：

- 摘要是摘要
- HMAC 是带密钥认证
- HKDF 是派生，不是加密


## 4. 升级 DEMO：`X25519 -> HKDF -> ChaCha20-Poly1305`

当你已经知道怎么拿到摘要和派生密钥，第二步才适合进入真正的“会话”主线：

1. 双方生成 X25519 密钥对
2. 双方算出同一份共享秘密
3. 用 HKDF 派生出 AEAD key
4. 用 ChaCha20-Poly1305 做加密和校验

下面这个完整例子对应：

- [examples/crypto/x25519_chacha20poly1305/main.c](../../examples/crypto/x25519_chacha20poly1305/main.c)

```c
uint8 aucPrivA[32], aucPubA[32];
uint8 aucPrivB[32], aucPubB[32];
uint8 aucSecretA[32], aucSecretB[32];
uint8 aucPrk[32];
uint8 aucKey[32];
uint8 aucNonce[12];
uint8 aucCipher[256];
uint8 aucPlain[256];
const uint8 aucAAD[] = "demo:aead:v1";
const uint8 aucInfo[] = "xrt/session/chacha20poly1305";
const char* sMsg = "{\"kind\":\"ping\",\"seq\":7}";
size_t iMsgLen = strlen(sMsg);

xrtX25519Keypair(aucPrivA, aucPubA);
xrtX25519Keypair(aucPrivB, aucPubB);

xrtX25519SharedSecret(aucSecretA, aucPrivA, aucPubB);
xrtX25519SharedSecret(aucSecretB, aucPrivB, aucPubA);

xrtHKDFExtract(aucPrk, NULL, 0, aucSecretA, sizeof(aucSecretA));
xrtHKDFExpand(aucKey, sizeof(aucKey), aucPrk, sizeof(aucPrk), aucInfo, strlen((const char*)aucInfo));

xrtRandomBytes(aucNonce, sizeof(aucNonce));
xrtChaCha20Poly1305Encrypt(aucCipher, aucKey, aucNonce, aucAAD, strlen((const char*)aucAAD), (const uint8*)sMsg, iMsgLen);
xrtChaCha20Poly1305Decrypt(aucPlain, aucKey, aucNonce, aucAAD, strlen((const char*)aucAAD), aucCipher, iMsgLen + 16);
```

这里最该记住的是：

- `aucSecretA` 和 `aucSecretB`
	- 应该相同
- `aucKey`
	- 才是拿去做 AEAD 的真正密钥
- 解密长度
	- 是 `明文长度 + 16`


## 5. 再补一条签名线：`Ed25519` 和 `ECDSA/RSA` 的读取方式不同

如果你的需求是：

- 我自己生成签名
- 我自己验证签名

当前最顺手的公开主线是：

- `Ed25519`

```c
uint8 aucSeed[32];
uint8 aucPub[32];
uint8 aucSig[64];
const char* sMsg = "xrt signed message";

xrtEd25519Keypair(aucSeed, aucPub);
xrtEd25519Sign(aucSig, (const uint8*)sMsg, strlen(sMsg), aucSeed);

if ( xrtEd25519Verify((const uint8*)sMsg, strlen(sMsg), aucSig, aucPub) ) {
	printf("verify ok\n");
}
```

而如果你手里拿到的是：

- 证书公钥
- 对端给出的签名
- 已经算好的消息哈希

那你看的通常是：

- `xrtECDSAVerify()`
- `xrtRSAPSSVerify()`
- `xrtRSAPKCS1Verify()`

它们并不是“再帮你算一次完整签名流程”的高层接口。


## 6. 一张选型表：这次到底该用什么

| 需求 | 优先入口 |
|------|----------|
| 想做内容指纹 / 文件摘要 | `xrtSHA256()` / `xrtSHA384()` / `xrtSHA512()` |
| 想做共享密钥消息认证 | `xrtHMAC_SHA256()` |
| 想把共享秘密派生出会话 key | `xrtHKDFExtract()/Expand()` |
| 想做现代 AEAD 会话加密 | `xrtChaCha20Poly1305Encrypt()/Decrypt()` |
| 想用 AES-GCM 接现有体系 | `xrtAES128GCM*()` / `xrtAES256GCM*()` |
| 想做轻量现代密钥交换 | `xrtX25519Keypair()/SharedSecret()` |
| 想验证自己生成的消息签名 | `xrtEd25519Sign()/Verify()` |
| 想校验证书或第三方签名结果 | `xrtECDSAVerify()` / `xrtRSAPSSVerify()` / `xrtRSAPKCS1Verify()` |


## 7. 常见错误

### 7.1 把普通哈希当认证

错误想法：

- “我把 payload 算个 SHA-256 发过去，对方也能验证，所以安全”

问题在于：

- 谁都能重算

如果要认证，至少考虑：

- HMAC
- 或签名


### 7.2 把共享秘密直接当业务密钥长期复用

更稳的做法是：

1. 先拿共享秘密
2. 再走 HKDF
3. 用不同 `info` 派生不同用途的 key


### 7.3 忘了 AEAD 输出多 `16` 字节 tag

当前 `ChaCha20-Poly1305` 和 `AES-GCM` 都遵循这个边界。  
如果你只给明文长度那点缓冲区，就会直接写错。


### 7.4 重复 nonce

同一把 key 下重复 nonce，会直接破坏安全边界。  
这件事比“到底选 ChaCha20-Poly1305 还是 AES-GCM”更重要。


### 7.5 以为 `Ed25519Verify()` 和 `ECDSAVerify()` 的输入一样

不是。

- `Ed25519Verify()`
	- 吃原始消息
- `xrtECDSAVerify()` / `xrtRSA*Verify()`
	- 吃消息哈希


### 7.6 在新设计里继续优先上 `SHA-1`

当前 `SHA-1` 还在公开面里，主要是为了：

- 兼容已有协议
- WebSocket 握手等场景

新设计里更推荐：

- `SHA-256`
- `SHA-384`
- `SHA-512`


### 7.7 忽略随机源边界

当前密钥、seed、nonce、salt 这类随机材料，优先都应该来自：

- `xrtRandomBytes()`

不要自己拿：

- 时间戳
- `rand()`
- 自增计数

去凑。


## 8. 下一步阅读

如果你接下来要走网络和 TLS 主线，建议继续读：

- [Crypto API 合同页](../api/api-crypto.md)
- [xnet-v2 与 TLS session 入门](xnet-v2-tls-intro.md)
- [用 XHTTP 走完 URL、代理与 TLS 客户端调用链](../case/xhttp-client-proxy-tls.md)

如果你要继续按正式课程顺序往下走，下一篇建议进入：

- [多任务总论：线程、队列、协程与 Future 怎么选](multitask-overview.md)
