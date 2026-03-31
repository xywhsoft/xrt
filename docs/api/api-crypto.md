# Crypto 加密算法模块

> 当前 TLS 与网络主线使用的加密原语模块

[English](api-crypto.en.md) | [中文](api-crypto.md) | [返回索引](README.md)

---

## 📑 目录

- [功能概述](#功能概述)
- [算法来源与授权](#算法来源与授权)
- [数据类型](#数据类型)
- [常量定义](#常量定义)
- [SHA-256 算法](#sha-256-算法)
- [SHA-384/SHA-512 算法](#sha-384sha-512-算法)
- [ChaCha20-Poly1305 算法](#chacha20-poly1305-算法)
- [AES-GCM 算法](#aes-gcm-算法)
- [随机数生成](#随机数生成)
- [HKDF 密钥派生](#hkdf-密钥派生)
- [X25519 密钥交换](#x25519-密钥交换)
- [RSA 签名验证](#rsa-签名验证)
- [secp256r1 ECDH/ECDSA](#secp256r1-ecdhecdsa)
- [使用示例](#使用示例)
- [注意事项](#注意事项)

---

## 功能概述

**Crypto** 是当前 XRT TLS 与网络主线使用的加密原语层。它将多种独立来源的算法实现与 XRT 内部整合工作收束到一个统一模块中，主要特性：

- 🔐 **哈希算法**：SHA-256, SHA-384, SHA-512
- 🔏 **HMAC**：HMAC-SHA256, HMAC-SHA384, HMAC-SHA512
- 🌀 **流加密**：ChaCha20
- 🔒 **AEAD 加密**：ChaCha20-Poly1305, AES-128-GCM, AES-256-GCM
- 🔑 **密钥派生**：HKDF (HMAC-based Extract-and-Expand)
- 🤝 **密钥交换**：X25519, ECDH (secp256r1)
- ✍️ **数字签名**：RSA 签名验证, ECDSA (secp256r1)
- 🎲 **随机数**：加密安全随机数生成器
- 🌐 **跨平台**：支持 Windows 和 Linux
- 📦 **零外部依赖**：所有算法均为纯 C 实现

**依赖模块**：
- `buffer.h` - 缓冲区管理

---

## 算法来源与授权

本模块不应被理解为单一第三方项目的整体移植。不同算法来自不同来源，XRT 将它们整合成统一的内部加密层。

| 算法 | 来源 | 授权 |
|------|------|------|
| SHA-256 | Brad Conte | Public Domain |
| ChaCha20-Poly1305 | portable8439 + poly1305-donna | CC0-1.0 + Public Domain |
| AES-GCM | Steven M. Gibson / GRC.com | Public Domain |
| X25519 | Mike Hamburg / STROBE | MIT License |
| RSA | axTLS bignum library | BSD License |
| secp256r1 | NIST FIPS 186-4, SEC 2 v2 | Public Domain |

---

## 数据类型

### xsha256_ctx

SHA-256 上下文结构体。

**定义**：
```c
typedef struct {
    uint8 buffer[64];
    uint32 state[8];
    size_t len;
    uint64 bits;
} xsha256_ctx;
```

### xsha512_ctx

SHA-512/SHA-384 上下文结构体（SHA-384 使用相同结构）。

**定义**：
```c
typedef struct {
    uint8 buffer[128];
    uint64 state[8];
    size_t len;
    uint64 bits;
} xsha512_ctx;
```

---

## 常量定义

### SHA 常量

| 常量 | 值 | 说明 |
|------|-----|------|
| SHA-256 输出长度 | 32 字节 | SHA-256 哈希输出大小 |
| SHA-384 输出长度 | 48 字节 | SHA-384 哈希输出大小 |
| SHA-512 输出长度 | 64 字节 | SHA-512 哈希输出大小 |

### 加密常量

| 常量 | 值 | 说明 |
|------|-----|------|
| ChaCha20 密钥长度 | 32 字节 | ChaCha20 密钥大小 |
| ChaCha20 Nonce 长度 | 12 字节 | ChaCha20 Nonce 大小 |
| Poly1305 Tag 长度 | 16 字节 | Poly1305 认证标签大小 |
| AES-128 密钥长度 | 16 字节 | AES-128 密钥大小 |
| AES-256 密钥长度 | 32 字节 | AES-256 密钥大小 |
| GCM Tag 长度 | 16 字节 | GCM 认证标签大小 |
| GCM Nonce 长度 | 12 字节 | GCM Nonce 大小 |

### 密钥交换常量

| 常量 | 值 | 说明 |
|------|-----|------|
| X25519 私钥长度 | 32 字节 | X25519 私钥大小 |
| X25519 公钥长度 | 32 字节 | X25519 公钥大小 |
| X25519 共享密钥长度 | 32 字节 | X25519 共享密钥大小 |
| secp256r1 私钥长度 | 32 字节 | secp256r1 私钥大小 |
| secp256r1 公钥长度 | 65 字节 | secp256r1 未压缩公钥大小 |

---

## SHA-256 算法

SHA-256 是一种加密哈希函数，生成 256 位（32 字节）的哈希值。

### 函数：xrtSHA256Init

**函数原型**：
```c
XXAPI void xrtSHA256Init(xsha256_ctx* pCtx);
```

**功能**：
初始化 SHA-256 上下文。

**参数**：
| 参数 | 类型 | 说明 | 是否可为NULL |
|------|------|------|--------------|
| `pCtx` | `xsha256_ctx*` | SHA-256 上下文 | 否 |

### 函数：xrtSHA256Update

**函数原型**：
```c
XXAPI void xrtSHA256Update(xsha256_ctx* pCtx, const ptr pData, size_t iLen);
```

**功能**：
更新 SHA-256 哈希计算。

**参数**：
| 参数 | 类型 | 说明 | 是否可为NULL |
|------|------|------|--------------|
| `pCtx` | `xsha256_ctx*` | SHA-256 上下文 | 否 |
| `pData` | `const ptr` | 数据指针 | 否 |
| `iLen` | `size_t` | 数据长度 | - |

### 函数：xrtSHA256Final

**函数原型**：
```c
XXAPI void xrtSHA256Final(xsha256_ctx* pCtx, uint8* pOut);
```

**功能**：
完成 SHA-256 哈希计算，输出结果。

**参数**：
| 参数 | 类型 | 说明 | 是否可为NULL |
|------|------|------|--------------|
| `pCtx` | `xsha256_ctx*` | SHA-256 上下文 | 否 |
| `pOut` | `uint8*` | 输出缓冲区（至少 32 字节） | 否 |

### 函数：xrtSHA256

**函数原型**：
```c
XXAPI void xrtSHA256(const ptr pData, size_t iLen, uint8* pOut);
```

**功能**：
一次性计算 SHA-256 哈希（便捷函数）。

**参数**：
| 参数 | 类型 | 说明 | 是否可为NULL |
|------|------|------|--------------|
| `pData` | `const ptr` | 数据指针 | 否 |
| `iLen` | `size_t` | 数据长度 | - |
| `pOut` | `uint8*` | 输出缓冲区（至少 32 字节） | 否 |

**示例**：
```c
uint8 hash[32];
xrtSHA256("Hello, World!", 13, hash);
// hash 现在包含 SHA-256 哈希值
```

### 函数：xrtHMAC_SHA256

**函数原型**：
```c
XXAPI void xrtHMAC_SHA256(const uint8* pKey, size_t iKeyLen,
                       const uint8* pMsg, size_t iMsgLen, uint8* pOut);
```

**功能**：
计算 HMAC-SHA256（基于哈希的消息认证码）。

**参数**：
| 参数 | 类型 | 说明 | 是否可为NULL |
|------|------|------|--------------|
| `pKey` | `const uint8*` | 密钥指针 | 否 |
| `iKeyLen` | `size_t` | 密钥长度 | - |
| `pMsg` | `const uint8*` | 消息指针 | 否 |
| `iMsgLen` | `size_t` | 消息长度 | - |
| `pOut` | `uint8*` | 输出缓冲区（至少 32 字节） | 否 |

**示例**：
```c
uint8 key[] = "secret_key";
uint8 mac[32];
xrtHMAC_SHA256(key, strlen(key), (uint8*)"Hello", 5, mac);
// mac 现在包含 HMAC-SHA256 值
```

---

## SHA-384/SHA-512 算法

SHA-512 是一种加密哈希函数，生成 512 位（64 字节）的哈希值。SHA-384 是 SHA-512 的截断版本，生成 384 位（48 字节）的哈希值。

### 函数：xrtSHA512Init

**函数原型**：
```c
XXAPI void xrtSHA512Init(xsha512_ctx* pCtx);
```

**功能**：
初始化 SHA-512 上下文。

**参数**：
| 参数 | 类型 | 说明 | 是否可为NULL |
|------|------|------|--------------|
| `pCtx` | `xsha512_ctx*` | SHA-512 上下文 | 否 |

### 函数：xrtSHA512Update

**函数原型**：
```c
XXAPI void xrtSHA512Update(xsha512_ctx* pCtx, const ptr pData, size_t iLen);
```

**功能**：
更新 SHA-512 哈希计算。

**参数**：
| 参数 | 类型 | 说明 | 是否可为NULL |
|------|------|------|--------------|
| `pCtx` | `xsha512_ctx*` | SHA-512 上下文 | 否 |
| `pData` | `const ptr` | 数据指针 | 否 |
| `iLen` | `size_t` | 数据长度 | - |

### 函数：xrtSHA512Final

**函数原型**：
```c
XXAPI void xrtSHA512Final(xsha512_ctx* pCtx, uint8* pOut);
```

**功能**：
完成 SHA-512 哈希计算，输出结果。

**参数**：
| 参数 | 类型 | 说明 | 是否可为NULL |
|------|------|------|--------------|
| `pCtx` | `xsha512_ctx*` | SHA-512 上下文 | 否 |
| `pOut` | `uint8*` | 输出缓冲区（至少 64 字节） | 否 |

### 函数：xrtSHA512

**函数原型**：
```c
XXAPI void xrtSHA512(const ptr pData, size_t iLen, uint8* pOut);
```

**功能**：
一次性计算 SHA-512 哈希（便捷函数）。

**参数**：
| 参数 | 类型 | 说明 | 是否可为NULL |
|------|------|------|--------------|
| `pData` | `const ptr` | 数据指针 | 否 |
| `iLen` | `size_t` | 数据长度 | - |
| `pOut` | `uint8*` | 输出缓冲区（至少 64 字节） | 否 |

### 函数：xrtSHA384Init

**函数原型**：
```c
XXAPI void xrtSHA384Init(xsha512_ctx* pCtx);
```

**功能**：
初始化 SHA-384 上下文。

**参数**：
| 参数 | 类型 | 说明 | 是否可为NULL |
|------|------|------|--------------|
| `pCtx` | `xsha512_ctx*` | SHA-384 上下文 | 否 |

### 函数：xrtSHA384Final

**函数原型**：
```c
XXAPI void xrtSHA384Final(xsha512_ctx* pCtx, uint8* pOut);
```

**功能**：
完成 SHA-384 哈希计算，输出结果（48 字节）。

**参数**：
| 参数 | 类型 | 说明 | 是否可为NULL |
|------|------|------|--------------|
| `pCtx` | `xsha512_ctx*` | SHA-384 上下文 | 否 |
| `pOut` | `uint8*` | 输出缓冲区（至少 48 字节） | 否 |

### 函数：xrtSHA384

**函数原型**：
```c
XXAPI void xrtSHA384(const ptr pData, size_t iLen, uint8* pOut);
```

**功能**：
一次性计算 SHA-384 哈希（便捷函数）。

**参数**：
| 参数 | 类型 | 说明 | 是否可为NULL |
|------|------|------|--------------|
| `pData` | `const ptr` | 数据指针 | 否 |
| `iLen` | `size_t` | 数据长度 | - |
| `pOut` | `uint8*` | 输出缓冲区（至少 48 字节） | 否 |

### 函数：xrtHMAC_SHA384

**函数原型**：
```c
XXAPI void xrtHMAC_SHA384(const uint8* pKey, size_t iKeyLen,
                       const uint8* pMsg, size_t iMsgLen, uint8* pOut);
```

**功能**：
计算 HMAC-SHA384（基于哈希的消息认证码）。

**参数**：
| 参数 | 类型 | 说明 | 是否可为NULL |
|------|------|------|--------------|
| `pKey` | `const uint8*` | 密钥指针 | 否 |
| `iKeyLen` | `size_t` | 密钥长度 | - |
| `pMsg` | `const uint8*` | 消息指针 | 否 |
| `iMsgLen` | `size_t` | 消息长度 | - |
| `pOut` | `uint8*` | 输出缓冲区（至少 48 字节） | 否 |

### 函数：xrtHMAC_SHA512

**函数原型**：
```c
XXAPI void xrtHMAC_SHA512(const uint8* pKey, size_t iKeyLen,
                       const uint8* pMsg, size_t iMsgLen, uint8* pOut);
```

**功能**：
计算 HMAC-SHA512（基于哈希的消息认证码）。

**参数**：
| 参数 | 类型 | 说明 | 是否可为NULL |
|------|------|------|--------------|
| `pKey` | `const uint8*` | 密钥指针 | 否 |
| `iKeyLen` | `size_t` | 密钥长度 | - |
| `pMsg` | `const uint8*` | 消息指针 | 否 |
| `iMsgLen` | `size_t` | 消息长度 | - |
| `pOut` | `uint8*` | 输出缓冲区（至少 64 字节） | 否 |

---

## ChaCha20-Poly1305 算法

ChaCha20-Poly1305 是一种结合 ChaCha20 流加密和 Poly1305 消息认证码的 AEAD（认证加密）算法，符合 RFC 8439。

### 函数：xrtChaCha20

**函数原型**：
```c
XXAPI void xrtChaCha20(uint8* pOut, const uint8* pKey, const uint8* pNonce,
                     uint32 iCounter, const uint8* pIn, size_t iLen);
```

**功能**：
使用 ChaCha20 流加密算法加密数据。

**参数**：
| 参数 | 类型 | 说明 | 是否可为NULL |
|------|------|------|--------------|
| `pOut` | `uint8*` | 输出缓冲区 | 否 |
| `pKey` | `const uint8*` | 密钥（32 字节） | 否 |
| `pNonce` | `const uint8*` | Nonce（12 字节） | 否 |
| `iCounter` | `uint32` | 计数器 | - |
| `pIn` | `const uint8*` | 输入数据 | 否 |
| `iLen` | `size_t` | 数据长度 | - |

**说明**：
- 输出缓冲区长度应至少等于 `iLen`
- 相同的密钥、Nonce 和计数器用于解密

**示例**：
```c
uint8 key[32] = { /* 32 字节密钥 */ };
uint8 nonce[12] = { /* 12 字节 Nonce */ };
uint8 plaintext[] = "Hello, World!";
uint8 ciphertext[sizeof(plaintext)];

xrtChaCha20(ciphertext, key, nonce, 0, plaintext, sizeof(plaintext));
// ciphertext 现在包含加密后的数据
```

### 函数：xrtChaCha20Poly1305Encrypt

**函数原型**：
```c
XXAPI bool xrtChaCha20Poly1305Encrypt(uint8* pOut, const uint8* pKey,
                                     const uint8* pNonce, const uint8* pAAD,
                                     size_t iAADLen, const uint8* pIn,
                                     size_t iLen);
```

**功能**：
使用 ChaCha20-Poly1305 AEAD 加密数据。

**参数**：
| 参数 | 类型 | 说明 | 是否可为NULL |
|------|------|------|--------------|
| `pOut` | `uint8*` | 输出缓冲区（密文 + Tag，至少 iLen + 16） | 否 |
| `pKey` | `const uint8*` | 密钥（32 字节） | 否 |
| `pNonce` | `const uint8*` | Nonce（12 字节） | 否 |
| `pAAD` | `const uint8*` | 附加认证数据 | 是 |
| `iAADLen` | `size_t` | AAD 长度 | - |
| `pIn` | `const uint8*` | 输入数据 | 否 |
| `iLen` | `size_t` | 数据长度 | - |

**返回值**：
| 返回值 | 说明 |
|--------|------|
| `true` | 加密成功 |
| `false` | 加密失败 |

**说明**：
- 输出缓冲区包含密文（iLen 字节）+ Tag（16 字节）
- AAD（附加认证数据）会被认证但不加密

**示例**：
```c
uint8 key[32] = { /* 密钥 */ };
uint8 nonce[12] = { /* Nonce */ };
uint8 aad[] = "additional_data";
uint8 plaintext[] = "Secret message";
uint8 output[sizeof(plaintext) + 16];

xrtChaCha20Poly1305Encrypt(output, key, nonce, aad, sizeof(aad),
                          plaintext, sizeof(plaintext));
// output 现在包含密文（前 sizeof(plaintext) 字节）和 Tag（后 16 字节）
```

### 函数：xrtChaCha20Poly1305Decrypt

**函数原型**：
```c
XXAPI bool xrtChaCha20Poly1305Decrypt(uint8* pOut, const uint8* pKey,
                                     const uint8* pNonce, const uint8* pAAD,
                                     size_t iAADLen, const uint8* pIn,
                                     size_t iLen);
```

**功能**：
使用 ChaCha20-Poly1305 解密并验证数据。

**参数**：
| 参数 | 类型 | 说明 | 是否可为NULL |
|------|------|------|--------------|
| `pOut` | `uint8*` | 输出缓冲区（至少 iLen - 16） | 否 |
| `pKey` | `const uint8*` | 密钥（32 字节） | 否 |
| `pNonce` | `const uint8*` | Nonce（12 字节） | 否 |
| `pAAD` | `const uint8*` | 附加认证数据 | 是 |
| `iAADLen` | `size_t` | AAD 长度 | - |
| `pIn` | `const uint8*` | 输入数据（密文 + Tag） | 否 |
| `iLen` | `size_t` | 数据长度（密文 + Tag） | - |

**返回值**：
| 返回值 | 说明 |
|--------|------|
| `true` | 解密成功，Tag 验证通过 |
| `false` | 解密失败或 Tag 验证失败 |

**说明**：
- 输入长度必须至少为 16 字节（Tag 大小）
- 常量时间比较 Tag 防止时序攻击

**示例**：
```c
uint8 output[sizeof(plaintext)];
bool ok = xrtChaCha20Poly1305Decrypt(output, key, nonce, aad, sizeof(aad),
                          encrypted_data, sizeof(encrypted_data));
if (ok) {
    // output 现在包含解密后的明文
} else {
    // Tag 验证失败，数据可能被篡改
}
```

---

## AES-GCM 算法

AES-GCM 是一种结合 AES 加密和 GCM 认证模式的 AEAD 算法。支持 AES-128-GCM 和 AES-256-GCM。

### 函数：xrtAES128GCMEncrypt

**函数原型**：
```c
XXAPI bool xrtAES128GCMEncrypt(uint8* pOut, const uint8* pKey, const uint8* pNonce,
                            size_t iNonceLen, const uint8* pAAD, size_t iAADLen,
                            const uint8* pIn, size_t iLen);
```

**功能**：
使用 AES-128-GCM 加密数据。

**参数**：
| 参数 | 类型 | 说明 | 是否可为NULL |
|------|------|------|--------------|
| `pOut` | `uint8*` | 输出缓冲区（密文 + Tag，至少 iLen + 16） | 否 |
| `pKey` | `const uint8*` | 密钥（16 字节） | 否 |
| `pNonce` | `const uint8*` | Nonce | 否 |
| `iNonceLen` | `size_t` | Nonce 长度 | - |
| `pAAD` | `const uint8*` | 附加认证数据 | 是 |
| `iAADLen` | `size_t` | AAD 长度 | - |
| `pIn` | `const uint8*` | 输入数据 | 否 |
| `iLen` | `size_t` | 数据长度 | - |

**返回值**：
| 返回值 | 说明 |
|--------|------|
| `true` | 加密成功 |
| `false` | 加密失败 |

**示例**：
```c
uint8 key[16] = { /* 16 字节密钥 */ };
uint8 nonce[12] = { /* 12 字节 Nonce */ };
uint8 plaintext[] = "Secret message";
uint8 output[sizeof(plaintext) + 16];

xrtAES128GCMEncrypt(output, key, nonce, 12, NULL, 0,
                    plaintext, sizeof(plaintext));
// output 包含密文和 Tag
```

### 函数：xrtAES128GCMDecrypt

**函数原型**：
```c
XXAPI bool xrtAES128GCMDecrypt(uint8* pOut, const uint8* pKey, const uint8* pNonce,
                            size_t iNonceLen, const uint8* pAAD, size_t iAADLen,
                            const uint8* pIn, size_t iLen);
```

**功能**：
使用 AES-128-GCM 解密并验证数据。

**参数**：
| 参数 | 类型 | 说明 | 是否可为NULL |
|------|------|------|--------------|
| `pOut` | `uint8*` | 输出缓冲区（至少 iLen - 16） | 否 |
| `pKey` | `const uint8*` | 密钥（16 字节） | 否 |
| `pNonce` | `const uint8*` | Nonce | 否 |
| `iNonceLen` | `size_t` | Nonce 长度 | - |
| `pAAD` | `const uint8*` | 附加认证数据 | 是 |
| `iAADLen` | `size_t` | AAD 长度 | - |
| `pIn` | `const uint8*` | 输入数据（密文 + Tag） | 否 |
| `iLen` | `size_t` | 数据长度（密文 + Tag） | - |

**返回值**：
| 返回值 | 说明 |
|--------|------|
| `true` | 解密成功，Tag 验证通过 |
| `false` | 解密失败或 Tag 验证失败 |

### 函数：xrtAES256GCMEncrypt

**函数原型**：
```c
XXAPI bool xrtAES256GCMEncrypt(uint8* pOut, const uint8* pKey, const uint8* pNonce,
                            size_t iNonceLen, const uint8* pAAD, size_t iAADLen,
                            const uint8* pIn, size_t iLen);
```

**功能**：
使用 AES-256-GCM 加密数据。

**参数**：
| 参数 | 类型 | 说明 | 是否可为NULL |
|------|------|------|--------------|
| `pOut` | `uint8*` | 输出缓冲区（密文 + Tag，至少 iLen + 16） | 否 |
| `pKey` | `const uint8*` | 密钥（32 字节） | 否 |
| `pNonce` | `const uint8*` | Nonce | 否 |
| `iNonceLen` | `size_t` | Nonce 长度 | - |
| `pAAD` | `const uint8*` | 附加认证数据 | 是 |
| `iAADLen` | `size_t` | AAD 长度 | - |
| `pIn` | `const uint8*` | 输入数据 | 否 |
| `iLen` | `size_t` | 数据长度 | - |

**返回值**：
| 返回值 | 说明 |
|--------|------|
| `true` | 加密成功 |
| `false` | 加密失败 |

### 函数：xrtAES256GCMDecrypt

**函数原型**：
```c
XXAPI bool xrtAES256GCMDecrypt(uint8* pOut, const uint8* pKey, const uint8* pNonce,
                            size_t iNonceLen, const uint8* pAAD, size_t iAADLen,
                            const uint8* pIn, size_t iLen);
```

**功能**：
使用 AES-256-GCM 解密并验证数据。

**参数**：
| 参数 | 类型 | 说明 | 是否可为NULL |
|------|------|------|--------------|
| `pOut` | `uint8*` | 输出缓冲区（至少 iLen - 16） | 否 |
| `pKey` | `const uint8*` | 密钥（32 字节） | 否 |
| `pNonce` | `const uint8*` | Nonce | 否 |
| `iNonceLen` | `size_t` | Nonce 长度 | - |
| `pAAD` | `const uint8*` | 附加认证数据 | 是 |
| `iAADLen` | `size_t` | AAD 长度 | - |
| `pIn` | `const uint8*` | 输入数据（密文 + Tag） | 否 |
| `iLen` | `size_t` | 数据长度（密文 + Tag） | - |

**返回值**：
| 返回值 | 说明 |
|--------|------|
| `true` | 解密成功，Tag 验证通过 |
| `false` | 解密失败或 Tag 验证失败 |

---

## 随机数生成

### 函数：xrtRandomBytes

**函数原型**：
```c
XXAPI void xrtRandomBytes(uint8* pBuf, size_t iLen);
```

**功能**：
生成加密安全的随机字节。

**参数**：
| 参数 | 类型 | 说明 | 是否可为NULL |
|------|------|------|--------------|
| `pBuf` | `uint8*` | 输出缓冲区 | 否 |
| `iLen` | `size_t` | 随机字节数 | - |

**说明**：
- Windows：使用 `RtlGenRandom` (SystemFunction036)
- Linux：使用 `/dev/urandom`
- 回退机制：如果系统随机数生成器不可用，使用 XRT 内部随机数

**示例**：
```c
uint8 key[32];
uint8 nonce[12];

xrtRandomBytes(key, sizeof(key));
xrtRandomBytes(nonce, sizeof(nonce));
// key 和 nonce 现在包含加密安全的随机数
```

---

## HKDF 密钥派生

HKDF (HMAC-based Extract-and-Expand Key Derivation) 是基于 HMAC 的密钥派生函数，符合 RFC 5869。

### 函数：xrtHKDFExtract

**函数原型**：
```c
XXAPI void xrtHKDFExtract(uint8* pPRK, const uint8* pSalt, size_t iSaltLen,
                        const uint8* pIKM, size_t iIKMLen);
```

**功能**：
HKDF Extract 阶段：从输入密钥材料派生伪随机密钥。

**参数**：
| 参数 | 类型 | 说明 | 是否可为NULL |
|------|------|------|--------------|
| `pPRK` | `uint8*` | 输出伪随机密钥（至少 32 字节） | 否 |
| `pSalt` | `const uint8*` | 盐值 | 是 |
| `iSaltLen` | `size_t` | 盐值长度 | - |
| `pIKM` | `const uint8*` | 输入密钥材料 | 否 |
| `iIKMLen` | `size_t` | 输入密钥材料长度 | - |

**说明**：
- 如果 `pSalt` 为 NULL 或 `iSaltLen` 为 0，使用默认盐值（32 字节零）
- 输出为 32 字节 SHA-256 哈希

### 函数：xrtHKDFExpand

**函数原型**：
```c
XXAPI void xrtHKDFExpand(uint8* pOKM, size_t iOKMLen, const uint8* pPRK,
                       size_t iPRKLen, const uint8* pInfo, size_t iInfoLen);
```

**功能**：
HKDF Expand 阶段：从伪随机密钥派生输出密钥。

**参数**：
| 参数 | 类型 | 说明 | 是否可为NULL |
|------|------|------|--------------|
| `pOKM` | `uint8*` | 输出密钥 | 否 |
| `iOKMLen` | `size_t` | 输出密钥长度 | - |
| `pPRK` | `const uint8*` | 伪随机密钥（32 字节） | 否 |
| `iPRKLen` | `size_t` | 伪随机密钥长度 | - |
| `pInfo` | `const uint8*` | 可选上下文信息 | 是 |
| `iInfoLen` | `size_t` | 上下文信息长度 | - |

**说明**：
- 可以派生任意长度的输出密钥
- 支持可选的上下文信息

### 函数：xrtHKDFExtract_SHA384

**函数原型**：
```c
XXAPI void xrtHKDFExtract_SHA384(uint8* pPRK, const uint8* pSalt, size_t iSaltLen,
                              const uint8* pIKM, size_t iIKMLen);
```

**功能**：
HKDF Extract 阶段（SHA-384 版本）。

**参数**：
| 参数 | 类型 | 说明 | 是否可为NULL |
|------|------|------|--------------|
| `pPRK` | `uint8*` | 输出伪随机密钥（至少 48 字节） | 否 |
| `pSalt` | `const uint8*` | 盐值 | 是 |
| `iSaltLen` | `size_t` | 盐值长度 | - |
| `pIKM` | `const uint8*` | 输入密钥材料 | 否 |
| `iIKMLen` | `size_t` | 输入密钥材料长度 | - |

**说明**：
- 输出为 48 字节 SHA-384 哈希

### 函数：xrtHKDFExpand_SHA384

**函数原型**：
```c
XXAPI void xrtHKDFExpand_SHA384(uint8* pOKM, size_t iOKMLen, const uint8* pPRK,
                               size_t iPRKLen, const uint8* pInfo, size_t iInfoLen);
```

**功能**：
HKDF Expand 阶段（SHA-384 版本）。

**参数**：
| 参数 | 类型 | 说明 | 是否可为NULL |
|------|------|------|--------------|
| `pOKM` | `uint8*` | 输出密钥 | 否 |
| `iOKMLen` | `size_t` | 输出密钥长度 | - |
| `pPRK` | `const uint8*` | 伪随机密钥（48 字节） | 否 |
| `iPRKLen` | `size_t` | 伪随机密钥长度 | - |
| `pInfo` | `const uint8*` | 可选上下文信息 | 是 |
| `iInfoLen` | `size_t` | 上下文信息长度 | - |

**示例**：
```c
uint8 salt[16] = { /* 盐值 */ };
uint8 ikm[] = "input_key_material";
uint8 prk[32];
uint8 key[32];

// HKDF Extract
xrtHKDFExtract(prk, salt, sizeof(salt), (uint8*)ikm, sizeof(ikm));

// HKDF Expand
xrtHKDFExpand(key, sizeof(key), prk, 32, (uint8*)"context", 7);
// key 现在包含派生出的密钥
```

---

## X25519 密钥交换

X25519 是一种椭圆曲线 Diffie-Hellman 密钥交换算法，用于安全地协商共享密钥。

### 函数：xrtX25519Keypair

**函数原型**：
```c
XXAPI void xrtX25519Keypair(uint8* pPrivKey, uint8* pPubKey);
```

**功能**：
生成 X25519 密钥对（私钥和公钥）。

**参数**：
| 参数 | 类型 | 说明 | 是否可为NULL |
|------|------|------|--------------|
| `pPrivKey` | `uint8*` | 输出私钥（32 字节） | 否 |
| `pPubKey` | `uint8*` | 输出公钥（32 字节） | 否 |

**说明**：
- 私钥使用加密安全随机数生成
- 公钥从私钥派生

**示例**：
```c
uint8 privkey[32];
uint8 pubkey[32];

xrtX25519Keypair(privkey, pubkey);
// privkey 和 pubkey 现在包含密钥对
```

### 函数：xrtX25519SharedSecret

**函数原型**：
```c
XXAPI void xrtX25519SharedSecret(uint8* pOut, const uint8* pPrivKey,
                                 const uint8* pPubKey);
```

**功能**：
计算 X25519 共享密钥（ECDH）。

**参数**：
| 参数 | 类型 | 说明 | 是否可为NULL |
|------|------|------|--------------|
| `pOut` | `uint8*` | 输出共享密钥（32 字节） | 否 |
| `pPrivKey` | `const uint8*` | 私钥（32 字节） | 否 |
| `pPubKey` | `const uint8*` | 对方公钥（32 字节） | 否 |

**示例**：
```c
// Alice 生成密钥对
uint8 alice_priv[32], alice_pub[32];
xrtX25519Keypair(alice_priv, alice_pub);

// Bob 生成密钥对
uint8 bob_priv[32], bob_pub[32];
xrtX25519Keypair(bob_priv, bob_pub);

// Alice 计算共享密钥
uint8 alice_shared[32];
xrtX25519SharedSecret(alice_shared, alice_priv, bob_pub);

// Bob 计算共享密钥
uint8 bob_shared[32];
xrtX25519SharedSecret(bob_shared, bob_priv, alice_pub);

// alice_shared 和 bob_shared 现在包含相同的共享密钥
```

---

## RSA 签名验证

RSA 签名验证用于验证 RSA 数字签名的有效性。当前实现支持签名验证（不支持签名生成）。

### 函数：xrtRSAVerify

**函数原型**：
```c
XXAPI bool xrtRSAVerify(const uint8* pHash, size_t iHashLen,
                      const uint8* pSig, size_t iSigLen,
                      const uint8* pPubKey, size_t iPubKeyLen);
```

**功能**：
验证 RSA 签名。

**参数**：
| 参数 | 类型 | 说明 | 是否可为NULL |
|------|------|------|--------------|
| `pHash` | `const uint8*` | 哈希值 | 否 |
| `iHashLen` | `size_t` | 哈希长度 | - |
| `pSig` | `const uint8*` | 签名（DER 编码） | 否 |
| `iSigLen` | `size_t` | 签名长度 | - |
| `pPubKey` | `const uint8*` | 公钥（DER 编码） | 否 |
| `iPubKeyLen` | `size_t` | 公钥长度 | - |

**返回值**：
| 返回值 | 说明 |
|--------|------|
| `true` | 签名验证通过 |
| `false` | 签名验证失败 |

**说明**：
- 使用 PKCS#1 v1.5 填充
- 支持常见哈希算法（SHA-256, SHA-384, SHA-512）
- 常量时间比较防止时序攻击

---

## secp256r1 ECDH/ECDSA

secp256r1（NIST P-256）是一种椭圆曲线算法，用于 ECDH 密钥交换和 ECDSA 数字签名。

### 函数：xrtECDHSecp256r1Keypair

**函数原型**：
```c
XXAPI void xrtECDHSecp256r1Keypair(uint8* pPrivKey, uint8* pPubKey);
```

**功能**：
生成 secp256r1 密钥对（私钥和公钥）。

**参数**：
| 参数 | 类型 | 说明 | 是否可为NULL |
|------|------|------|--------------|
| `pPrivKey` | `uint8*` | 输出私钥（32 字节） | 否 |
| `pPubKey` | `uint8*` | 输出公钥（65 字节，未压缩格式） | 否 |

**说明**：
- 公钥格式：0x04 || X || Y（65 字节未压缩格式）
- 私钥格式：32 字节标量

**示例**：
```c
uint8 privkey[32];
uint8 pubkey[65];

xrtECDHSecp256r1Keypair(privkey, pubkey);
// privkey 和 pubkey 现在包含密钥对
```

### 函数：xrtECDHSecp256r1SharedSecret

**函数原型**：
```c
XXAPI void xrtECDHSecp256r1SharedSecret(uint8* pOut, const uint8* pPrivKey,
                                      const uint8* pPubKey);
```

**功能**：
计算 secp256r1 共享密钥（ECDH）。

**参数**：
| 参数 | 类型 | 说明 | 是否可为NULL |
|------|------|------|--------------|
| `pOut` | `uint8*` | 输出共享密钥（32 字节） | 否 |
| `pPrivKey` | `const uint8*` | 私钥（32 字节） | 否 |
| `pPubKey` | `const uint8*` | 对方公钥（65 字节，未压缩格式） | 否 |

**示例**：
```c
// Alice 生成密钥对
uint8 alice_priv[32], alice_pub[65];
xrtECDHSecp256r1Keypair(alice_priv, alice_pub);

// Bob 生成密钥对
uint8 bob_priv[32], bob_pub[65];
xrtECDHSecp256r1Keypair(bob_priv, bob_pub);

// Alice 计算共享密钥
uint8 alice_shared[32];
xrtECDHSecp256r1SharedSecret(alice_shared, alice_priv, bob_pub);

// Bob 计算共享密钥
uint8 bob_shared[32];
xrtECDHSecp256r1SharedSecret(bob_shared, bob_priv, alice_pub);

// alice_shared 和 bob_shared 现在包含相同的共享密钥
```

### 函数：xrtECDSAVerify

**函数原型**：
```c
XXAPI bool xrtECDSAVerify(const uint8* pHash, size_t iHashLen,
                          const uint8* pSig, size_t iSigLen,
                          const uint8* pPubKey, size_t iPubKeyLen);
```

**功能**：
验证 secp256r1 ECDSA 签名。

**参数**：
| 参数 | 类型 | 说明 | 是否可为NULL |
|------|------|------|--------------|
| `pHash` | `const uint8*` | 哈希值 | 否 |
| `iHashLen` | `size_t` | 哈希长度 | - |
| `pSig` | `const uint8*` | 签名（DER 编码） | 否 |
| `iSigLen` | `size_t` | 签名长度 | - |
| `pPubKey` | `const uint8*` | 公钥（65 字节，未压缩格式） | 否 |
| `iPubKeyLen` | `size_t` | 公钥长度 | - |

**返回值**：
| 返回值 | 说明 |
|--------|------|
| `true` | 签名验证通过 |
| `false` | 签名验证失败 |

**说明**：
- 支持常见的哈希算法（SHA-256, SHA-384）
- 签名格式：DER 编码的 SEQUENCE { INTEGER r, INTEGER s }
- 常量时间比较防止时序攻击

---

## 使用示例

### 示例1：数据哈希

```c
#include "xrt.h"

int main() {
    xrtInit();

    // SHA-256 哈希
    uint8 sha256_hash[32];
    xrtSHA256("Hello, World!", 13, sha256_hash);

    // SHA-512 哈希
    uint8 sha512_hash[64];
    xrtSHA512("Hello, World!", 13, sha512_hash);

    xrtUnit();
    return 0;
}
```

### 示例2：HMAC 认证

```c
int main() {
    xrtInit();

    uint8 key[] = "secret_key";
    uint8 message[] = "authenticated_message";
    uint8 mac[32];

    xrtHMAC_SHA256(key, strlen(key), message, strlen(message), mac);

    // 验证 HMAC
    uint8 verify_mac[32];
    xrtHMAC_SHA256(key, strlen(key), message, strlen(message), verify_mac);

    bool valid = memcmp(mac, verify_mac, 32) == 0;
    printf("HMAC valid: %s\n", valid ? "yes" : "no");

    xrtUnit();
    return 0;
}
```

### 示例3：ChaCha20-Poly1305 加密

```c
int main() {
    xrtInit();

    uint8 key[32];
    uint8 nonce[12];
    uint8 plaintext[] = "Secret message";
    uint8 ciphertext[sizeof(plaintext) + 16];

    // 生成随机密钥和 Nonce
    xrtRandomBytes(key, sizeof(key));
    xrtRandomBytes(nonce, sizeof(nonce));

    // 加密
    xrtChaCha20Poly1305Encrypt(ciphertext, key, nonce, NULL, 0,
                              plaintext, sizeof(plaintext));

    // 解密
    uint8 decrypted[sizeof(plaintext)];
    bool ok = xrtChaCha20Poly1305Decrypt(decrypted, key, nonce, NULL, 0,
                              ciphertext, sizeof(ciphertext));

    if (ok) {
        printf("Decrypted: %s\n", decrypted);
    }

    xrtUnit();
    return 0;
}
```

### 示例4：X25519 密钥交换

```c
int main() {
    xrtInit();

    // Alice 生成密钥对
    uint8 alice_priv[32], alice_pub[32];
    xrtX25519Keypair(alice_priv, alice_pub);

    // Bob 生成密钥对
    uint8 bob_priv[32], bob_pub[32];
    xrtX25519Keypair(bob_priv, bob_pub);

    // 交换公钥（通过网络发送）
    // Alice 发送 alice_pub 给 Bob
    // Bob 发送 bob_pub 给 Alice

    // 计算共享密钥
    uint8 alice_shared[32], bob_shared[32];
    xrtX25519SharedSecret(alice_shared, alice_priv, bob_pub);
    xrtX25519SharedSecret(bob_shared, bob_priv, alice_pub);

    // 验证共享密钥相同
    bool keys_match = memcmp(alice_shared, bob_shared, 32) == 0;
    printf("Keys match: %s\n", keys_match ? "yes" : "no");

    xrtUnit();
    return 0;
}
```

### 示例5：HKDF 密钥派生

```c
int main() {
    xrtInit();

    uint8 salt[16] = { /* 随机盐值 */ };
    uint8 ikm[] = "input_key_material";
    uint8 info[] = "encryption_context";
    uint8 prk[32];
    uint8 key[32];

    // HKDF Extract
    xrtHKDFExtract(prk, salt, sizeof(salt), (uint8*)ikm, sizeof(ikm));

    // HKDF Expand
    xrtHKDFExpand(key, sizeof(key), prk, 32, (uint8*)info, sizeof(info));

    printf("Derived key: ");
    for (int i = 0; i < 32; i++) {
        printf("%02x", key[i]);
    }
    printf("\n");

    xrtUnit();
    return 0;
}
```

### 示例6：AES-GCM 加密

```c
int main() {
    xrtInit();

    uint8 key[16];
    uint8 nonce[12];
    uint8 plaintext[] = "Secret message";
    uint8 aad[] = "additional_data";
    uint8 ciphertext[sizeof(plaintext) + 16];

    // 生成随机密钥和 Nonce
    xrtRandomBytes(key, sizeof(key));
    xrtRandomBytes(nonce, sizeof(nonce));

    // 加密
    xrtAES128GCMEncrypt(ciphertext, key, nonce, 12, aad, sizeof(aad),
                      plaintext, sizeof(plaintext));

    // 解密
    uint8 decrypted[sizeof(plaintext)];
    bool ok = xrtAES128GCMDecrypt(decrypted, key, nonce, 12, aad, sizeof(aad),
                      ciphertext, sizeof(ciphertext));

    if (ok) {
        printf("Decrypted: %s\n", decrypted);
    }

    xrtUnit();
    return 0;
}
```

### 示例7：secp256r1 ECDH

```c
int main() {
    xrtInit();

    // Alice 生成密钥对
    uint8 alice_priv[32], alice_pub[65];
    xrtECDHSecp256r1Keypair(alice_priv, alice_pub);

    // Bob 生成密钥对
    uint8 bob_priv[32], bob_pub[65];
    xrtECDHSecp256r1Keypair(bob_priv, bob_pub);

    // 交换公钥（通过网络发送）

    // 计算共享密钥
    uint8 alice_shared[32], bob_shared[32];
    xrtECDHSecp256r1SharedSecret(alice_shared, alice_priv, bob_pub);
    xrtECDHSecp256r1SharedSecret(bob_shared, bob_priv, alice_pub);

    // 验证共享密钥相同
    bool keys_match = memcmp(alice_shared, bob_shared, 32) == 0;
    printf("Keys match: %s\n", keys_match ? "yes" : "no");

    xrtUnit();
    return 0;
}
```

---

## 注意事项

1. **密钥安全**：
   - 私钥必须妥善保管
   - 不要硬编码密钥到源代码
   - 建议使用安全的密钥管理系统

2. **Nonce 使用**：
   - ChaCha20 和 AES-GCM 的 Nonce 必须唯一
   - 不要重复使用相同的密钥和 Nonce 组合
   - 建议使用计数器或随机数生成 Nonce

3. **随机数质量**：
   - `xrtRandomBytes` 提供加密安全的随机数
   - 仅在特殊情况下使用回退机制
   - 对于密钥生成，建议使用专用 CSPRNG

4. **内存安全**：
   - 加密完成后及时擦除敏感数据
   - 使用 `memset` 清零不再需要的密钥和明文
   - 考虑使用安全内存分配器

5. **性能考虑**：
   - SHA-512 比 SHA-256 慢约 2 倍
   - AES-256-GCM 比 AES-128-GCM 慢约 40%
   - ChaCha20-Poly1305 在无硬件加速时比 AES-GCM 快

6. **算法选择**：
   - SHA-256：通用哈希，性能与安全平衡
   - SHA-512：需要更高安全性时使用
   - ChaCha20-Poly1305：移动设备和无 AES 硬件加速
   - AES-GCM：有 AES 硬件加速时

7. **跨平台兼容**：
   - 所有算法在 Windows 和 Linux 上行为一致
   - 测试时注意大小端序问题

---

<div align="center">

**XRT Crypto Module** | 版本 1.0 | 最后更新: 2025-01

</div>
