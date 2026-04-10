# Crypto 加密原语模块

> 当前 XRT 公开的 `crypto` 模块不是“算法大全”，而是一组面向 TLS、网络和通用安全场景的底层原语接口。

[返回索引](README.md)

---

## 1. 定位

当前公开 `crypto` 主线覆盖这些能力：

- 哈希：`SHA-1 / SHA-256 / SHA-384 / SHA-512`
- HMAC：`SHA-256 / SHA-384 / SHA-512`
- 对称与 AEAD：`ChaCha20`、`ChaCha20-Poly1305`、`AES-128-GCM`、`AES-256-GCM`
- 随机数：`xrtRandomBytes()`
- 密钥派生：`HKDF-SHA256 / HKDF-SHA384`
- 密钥交换：`X25519 / X448 / ECDH secp256r1 / secp384r1`
- 签名：`Ed25519` 生成与验证，`ECDSA / RSA` 验证

它更适合：

- TLS / WebSocket / HTTP 安全相关实现
- 业务里的摘要、认证、会话密钥派生
- 对现有签名结果做校验

不适合直接被理解成：

- 证书链策略层
- 高层密钥管理系统
- 完整认证协议框架


## 2. 使用边界

在当前公开 API 里，最值得先记住的是下面 7 条：

1. Hash、HMAC、HKDF、AEAD、密钥交换、签名验证解决的是不同问题，不能互相代替。
2. `xrtRandomBytes()` 才是 seed / key / nonce / salt 这类随机材料的正式入口，不要用 `rand()` 或时间戳凑。
3. `xrtChaCha20Poly1305*()` 和 `xrtAES*GCM*()` 的输出缓冲区都需要 `明文长度 + 16` 字节空间；解密时传入长度也包含这 16 字节 tag。
4. `xrtX25519SharedSecret()`、`xrtX448SharedSecret()`、`xrtECDHSecp*SharedSecret()` 的输出是共享秘密，不是最终业务密钥；正式场景通常要继续走 HKDF。
5. `xrtEd25519Verify()` 验证的是原始消息；`xrtECDSAVerify()`、`xrtRSAPSSVerify()`、`xrtRSAPKCS1Verify()` 验证的是消息哈希。
6. 当前公开签名接口并不对称：`Ed25519` 有生成和验证，而 `ECDSA / RSA` 主线公开的是验证。
7. `SHA-384` 的增量上下文使用 `xsha512_ctx`，更新阶段走 `xrtSHA512Update()`；当前没有单独 `xrtSHA384Update()`。


## 3. 核心类型

### `xsha1_ctx`

`SHA-1` 上下文。

```c
typedef struct {
	uint32 state[5];
	uint8 buffer[64];
	uint32 len;
	uint64 bits;
} xsha1_ctx;
```


### `xsha256_ctx`

`SHA-256` 上下文。

```c
typedef struct {
	uint32 state[8];
	uint8 buffer[64];
	uint64 bits;
	uint64 len;
} xsha256_ctx;
```


### `xsha512_ctx`

`SHA-512` 上下文，同时也是 `SHA-384` 的上下文类型。

```c
typedef struct {
	uint64 state[8];
	uint8 buffer[128];
	uint32 len;
	uint64 bits;
} xsha512_ctx;
```


## 4. 长度速查

| 能力 | 典型长度 |
|------|----------|
| `SHA-1` 输出 | 20 字节 |
| `SHA-256` 输出 | 32 字节 |
| `SHA-384` 输出 | 48 字节 |
| `SHA-512` 输出 | 64 字节 |
| `HMAC-SHA256` 输出 | 32 字节 |
| `ChaCha20-Poly1305` tag | 16 字节 |
| `AES-GCM` tag | 16 字节 |
| `ChaCha20-Poly1305` nonce | 12 字节 |
| `X25519` 私钥 / 公钥 / 共享秘密 | 32 / 32 / 32 字节 |
| `X448` 私钥 / 公钥 / 共享秘密 | 56 / 56 / 56 字节 |
| `ECDH secp256r1` 私钥 / 公钥 / 共享秘密 | 32 / 65 / 32 字节 |
| `ECDH secp384r1` 私钥 / 公钥 / 共享秘密 | 48 / 97 / 48 字节 |
| `Ed25519` seed / 公钥 / 签名 | 32 / 32 / 64 字节 |


## 5. 公开 API

### 5.1 Hash 与 HMAC

```c
XXAPI void xrtSHA256(const ptr pData, size_t iLen, uint8 *pOut);
XXAPI void xrtSHA256Init(xsha256_ctx *pCtx);
XXAPI void xrtSHA256Update(xsha256_ctx *pCtx, const ptr pData, size_t iLen);
XXAPI void xrtSHA256Final(xsha256_ctx *pCtx, uint8 *pOut);

XXAPI void xrtSHA1(const ptr pData, size_t iLen, uint8 *pOut);
XXAPI void xrtSHA1Init(xsha1_ctx *pCtx);
XXAPI void xrtSHA1Update(xsha1_ctx *pCtx, const ptr pData, size_t iLen);
XXAPI void xrtSHA1Final(xsha1_ctx *pCtx, uint8 *pOut);

XXAPI void xrtSHA384(const ptr pData, size_t iLen, uint8 *pOut);
XXAPI void xrtSHA384Init(xsha512_ctx *pCtx);
XXAPI void xrtSHA384Final(xsha512_ctx *pCtx, uint8 *pOut);

XXAPI void xrtSHA512(const ptr pData, size_t iLen, uint8 *pOut);
XXAPI void xrtSHA512Init(xsha512_ctx *pCtx);
XXAPI void xrtSHA512Update(xsha512_ctx *pCtx, const ptr pData, size_t iLen);
XXAPI void xrtSHA512Final(xsha512_ctx *pCtx, uint8 *pOut);

XXAPI void xrtHMAC_SHA256(const uint8 *pKey, size_t iKeyLen, const uint8 *pMsg, size_t iMsgLen, uint8 *pOut);
XXAPI void xrtHMAC_SHA384(const uint8 *pKey, size_t iKeyLen, const uint8 *pMsg, size_t iMsgLen, uint8 *pOut);
XXAPI void xrtHMAC_SHA512(const uint8 *pKey, size_t iKeyLen, const uint8 *pMsg, size_t iMsgLen, uint8 *pOut);
```

补充说明：

- `xrtSHA1()` 当前主要面向兼容性和 WebSocket 握手相关场景。
- `SHA-384` 的增量流程是 `xrtSHA384Init()` + `xrtSHA512Update()` + `xrtSHA384Final()`。
- HMAC 解决的是共享密钥消息认证，不是公私钥签名。


### 5.2 对称加密与 AEAD

```c
XXAPI void xrtChaCha20(uint8 *pOut, const uint8 *pKey, const uint8 *pNonce, uint32 iCounter, const uint8 *pIn, size_t iLen);

XXAPI bool xrtChaCha20Poly1305Encrypt(uint8 *pOut, const uint8 *pKey, const uint8 *pNonce, const uint8 *pAAD, size_t iAADLen, const uint8 *pIn, size_t iLen);
XXAPI bool xrtChaCha20Poly1305Decrypt(uint8 *pOut, const uint8 *pKey, const uint8 *pNonce, const uint8 *pAAD, size_t iAADLen, const uint8 *pIn, size_t iLen);

XXAPI bool xrtAES128GCMEncrypt(uint8 *pOut, const uint8 *pKey, const uint8 *pNonce, size_t iNonceLen, const uint8 *pAAD, size_t iAADLen, const uint8 *pIn, size_t iLen);
XXAPI bool xrtAES128GCMDecrypt(uint8 *pOut, const uint8 *pKey, const uint8 *pNonce, size_t iNonceLen, const uint8 *pAAD, size_t iAADLen, const uint8 *pIn, size_t iLen);

XXAPI bool xrtAES256GCMEncrypt(uint8 *pOut, const uint8 *pKey, const uint8 *pNonce, size_t iNonceLen, const uint8 *pAAD, size_t iAADLen, const uint8 *pIn, size_t iLen);
XXAPI bool xrtAES256GCMDecrypt(uint8 *pOut, const uint8 *pKey, const uint8 *pNonce, size_t iNonceLen, const uint8 *pAAD, size_t iAADLen, const uint8 *pIn, size_t iLen);
```

补充说明：

- `xrtChaCha20()` 是裸流加密原语；大多数业务场景里更推荐 AEAD。
- `xrtChaCha20Poly1305*()` 的 nonce 长度固定为 12 字节。
- `xrtAES*GCM*()` 的 nonce 长度由 `iNonceLen` 显式传入，常见用法是 12 字节。
- `Encrypt()` 的输出长度是 `iLen + 16`；`Decrypt()` 的输入长度也必须包含这 16 字节 tag。


### 5.3 随机数与 HKDF

```c
XXAPI void xrtHKDFExtract(uint8 *pPRK, const uint8 *pSalt, size_t iSaltLen, const uint8 *pIKM, size_t iIKMLen);
XXAPI void xrtHKDFExpand(uint8 *pOKM, size_t iOKMLen, const uint8 *pPRK, size_t iPRKLen, const uint8 *pInfo, size_t iInfoLen);

XXAPI void xrtHKDFExtract_SHA384(uint8 *pPRK, const uint8 *pSalt, size_t iSaltLen, const uint8 *pIKM, size_t iIKMLen);
XXAPI void xrtHKDFExpand_SHA384(uint8 *pOKM, size_t iOKMLen, const uint8 *pPRK, size_t iPRKLen, const uint8 *pInfo, size_t iInfoLen);

XXAPI void xrtRandomBytes(uint8 *pBuf, size_t iLen);
```

补充说明：

- `Extract()` 输出的是 PRK，不是最终业务密钥。
- `Expand()` 才负责按长度和 `info` 派生出真正要用的 key。
- 当前实现里，当 `pSalt == NULL` 或 `iSaltLen == 0` 时，`xrtHKDFExtract()` 会使用默认零盐。
- `xrtRandomBytes()` 应优先用于 seed、nonce、salt、临时 key 等随机材料。


### 5.4 密钥交换

```c
XXAPI void xrtX25519Keypair(uint8 *pPrivKey, uint8 *pPubKey);
XXAPI void xrtX25519SharedSecret(uint8 *pOut, const uint8 *pPrivKey, const uint8 *pPubKey);

XXAPI void xrtX448Keypair(uint8 *pPrivKey, uint8 *pPubKey);
XXAPI void xrtX448SharedSecret(uint8 *pOut, const uint8 *pPrivKey, const uint8 *pPubKey);

XXAPI void xrtECDHSecp256r1Keypair(uint8 *pPrivKey, uint8 *pPubKey);
XXAPI void xrtECDHSecp256r1SharedSecret(uint8 *pOut, const uint8 *pPrivKey, const uint8 *pPubKey);

XXAPI void xrtECDHSecp384r1Keypair(uint8 *pPrivKey, uint8 *pPubKey);
XXAPI void xrtECDHSecp384r1SharedSecret(uint8 *pOut, const uint8 *pPrivKey, const uint8 *pPubKey);
```

补充说明：

- 这些 API 产出的是共享秘密，不是最终会话 key。
- 推荐做法通常是：`SharedSecret -> HKDF -> AEAD/HMAC key`。
- `secp256r1 / secp384r1` 公钥都是未压缩格式，首字节为 `0x04`。


### 5.5 签名与验证

```c
XXAPI void xrtEd25519Keypair(uint8 *pSeed, uint8 *pPubKey);
XXAPI void xrtEd25519PublicKey(uint8 *pPubKey, const uint8 *pSeed);
XXAPI bool xrtEd25519Sign(uint8 *pSig, const uint8 *pMsg, size_t iMsgLen, const uint8 *pSeed);
XXAPI bool xrtEd25519Verify(const uint8 *pMsg, size_t iMsgLen, const uint8 *pSig, const uint8 *pPubKey);

XXAPI bool xrtECDSAVerify(const uint8 *pHash, size_t iHashLen, const uint8 *pSig, size_t iSigLen, const uint8 *pPubKey, size_t iPubKeyLen);

XXAPI int  xrtRSAModPow(const uint8 *pMod, size_t iModSz, const uint8 *pExp, size_t iExpSz, const uint8 *pMsg, size_t iMsgSz, uint8 *pOut, size_t iOutSz);
XXAPI bool xrtRSAPSSVerify(const uint8 *pHash, size_t iHashLen, const uint8 *pSig, size_t iSigLen, const uint8 *pMod, size_t iModSz, const uint8 *pExp, size_t iExpSz);
XXAPI bool xrtRSAPKCS1Verify(const uint8 *pHash, size_t iHashLen, const uint8 *pSig, size_t iSigLen, const uint8 *pMod, size_t iModSz, const uint8 *pExp, size_t iExpSz);
```

补充说明：

- `Ed25519` 当前公开主线支持生成和验证。
- `xrtEd25519Verify()` 验证的是原始消息。
- `xrtECDSAVerify()`、`xrtRSAPSSVerify()`、`xrtRSAPKCS1Verify()` 验证的是消息哈希。
- `xrtRSAModPow()` 是低层模幂原语，通常只在更底层协议或验证路径里使用。


## 6. 示例

### 6.1 摘要、HMAC 与 HKDF

可直接参考：

- [examples/crypto/hash_functions/main.c](../../examples/crypto/hash_functions/main.c)

这页示例会演示：

- `SHA-256` 一次性摘要
- `SHA-384` 的增量计算
- `HMAC-SHA256`
- `HKDF-SHA256`


### 6.2 共享秘密、派生会话密钥与 AEAD

可直接参考：

- [examples/crypto/x25519_chacha20poly1305/main.c](../../examples/crypto/x25519_chacha20poly1305/main.c)

这页示例会演示：

1. 双方生成 `X25519` 密钥对
2. 双方算出同一份共享秘密
3. 用 `HKDF` 派生 `ChaCha20-Poly1305` key
4. 做一次加密和解密校验
5. 再补一个 `Ed25519` 签名与验证片段


## 7. 推荐阅读

如果你在写教学或业务代码，建议按下面顺序继续：

1. [Crypto 入门](../guide/crypto-intro.md)
2. [xnet-v2 与 TLS session 入门](../guide/xnet-v2-tls-intro.md)
3. [XHTTP](api-xhttp.md)
4. [XWS](api-xws.md)
