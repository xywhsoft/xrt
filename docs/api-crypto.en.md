# Crypto Module

> Cryptographic algorithm module ported from Mongoose built-in TLS primitives

[English](api-crypto.en.md) | [中文](api-crypto.md) | [Back to Index](README.md)

---

## 📑 Table of Contents

- [Feature Overview](#feature-overview)
- [Algorithm Sources and Licensing](#algorithm-sources-and-licensing)
- [Data Types](#data-types)
- [Constants](#constants)
- [SHA-256 Algorithm](#sha-256-algorithm)
- [SHA-384/SHA-512 Algorithm](#sha-384sha-512-algorithm)
- [ChaCha20-Poly1305 Algorithm](#chacha20-poly1305-algorithm)
- [AES-GCM Algorithm](#aes-gcm-algorithm)
- [Random Number Generation](#random-number-generation)
- [HKDF Key Derivation](#hkdf-key-derivation)
- [X25519 Key Exchange](#x25519-key-exchange)
- [RSA Signature Verification](#rsa-signature-verification)
- [secp256r1 ECDH/ECDSA](#secp256r1-ecdh-ecdsa)
- [Usage Examples](#usage-examples)
- [Notes](#notes)

---

## Feature Overview

**Crypto** is a complete cryptographic algorithm module ported from Mongoose built-in TLS primitives, with the following features:

- 🔐 **Hash Algorithms**: SHA-256, SHA-384, SHA-512
- 🔏 **HMAC**: HMAC-SHA256, HMAC-SHA384, HMAC-SHA512
- 🌀 **Stream Cipher**: ChaCha20
- 🔒 **AEAD Encryption**: ChaCha20-Poly1305, AES-128-GCM, AES-256-GCM
- 🔑 **Key Derivation**: HKDF (HMAC-based Extract-and-Expand)
- 🤝 **Key Exchange**: X25519, ECDH (secp256r1)
- ✍️ **Digital Signatures**: RSA signature verification, ECDSA (secp256r1)
- 🎲 **Random Numbers**: Cryptographically secure random number generator
- 🌐 **Cross-Platform**: Supports Windows and Linux
- 📦 **Zero External Dependencies**: All algorithms are pure C implementations

**Dependencies**:
- `buffer.h` - Buffer management

---

## Algorithm Sources and Licensing

| Algorithm | Source | License |
|-----------|--------|----------|
| SHA-256 | Brad Conte | Public Domain |
| ChaCha20-Poly1305 | portable8439 + poly1305-donna | CC0-1.0 + Public Domain |
| AES-GCM | Steven M. Gibson / GRC.com | Public Domain |
| X25519 | Mike Hamburg / STROBE | MIT License |
| RSA | axTLS bignum library | BSD License |
| secp256r1 | NIST FIPS 186-4, SEC 2 v2 | Public Domain |

---

## Data Types

### xsha256_ctx

SHA-256 context structure.

**Definition**:
```c
typedef struct {
    uint8 buffer[64];
    uint32 state[8];
    size_t len;
    uint64 bits;
} xsha256_ctx;
```

### xsha512_ctx

SHA-512/SHA-384 context structure (SHA-384 uses the same structure).

**Definition**:
```c
typedef struct {
    uint8 buffer[128];
    uint64 state[8];
    size_t len;
    uint64 bits;
} xsha512_ctx;
```

---

## Constants

### SHA Constants

| Constant | Value | Description |
|----------|-------|-------------|
| SHA-256 Output Length | 32 bytes | SHA-256 hash output size |
| SHA-384 Output Length | 48 bytes | SHA-384 hash output size |
| SHA-512 Output Length | 64 bytes | SHA-512 hash output size |

### Encryption Constants

| Constant | Value | Description |
|----------|-------|-------------|
| ChaCha20 Key Length | 32 bytes | ChaCha20 key size |
| ChaCha20 Nonce Length | 12 bytes | ChaCha20 nonce size |
| Poly1305 Tag Length | 16 bytes | Poly1305 authentication tag size |
| AES-128 Key Length | 16 bytes | AES-128 key size |
| AES-256 Key Length | 32 bytes | AES-256 key size |
| GCM Tag Length | 16 bytes | GCM authentication tag size |
| GCM Nonce Length | 12 bytes | GCM nonce size |

### Key Exchange Constants

| Constant | Value | Description |
|----------|-------|-------------|
| X25519 Private Key Length | 32 bytes | X25519 private key size |
| X25519 Public Key Length | 32 bytes | X25519 public key size |
| X25519 Shared Secret Length | 32 bytes | X25519 shared secret size |
| secp256r1 Private Key Length | 32 bytes | secp256r1 private key size |
| secp256r1 Public Key Length | 65 bytes | secp256r1 uncompressed public key size |

---

## SHA-256 Algorithm

SHA-256 is a cryptographic hash function that produces a 256-bit (32-byte) hash value.

### Function: xrtSHA256Init

**Function Prototype**:
```c
XXAPI void xrtSHA256Init(xsha256_ctx* pCtx);
```

**Description**:
Initialize SHA-256 context.

**Parameters**:
| Parameter | Type | Description | Can be NULL |
|-----------|------|-------------|------------|
| `pCtx` | `xsha256_ctx*` | SHA-256 context | No |

### Function: xrtSHA256Update

**Function Prototype**:
```c
XXAPI void xrtSHA256Update(xsha256_ctx* pCtx, const ptr pData, size_t iLen);
```

**Description**:
Update SHA-256 hash calculation.

**Parameters**:
| Parameter | Type | Description | Can be NULL |
|-----------|------|-------------|------------|
| `pCtx` | `xsha256_ctx*` | SHA-256 context | No |
| `pData` | `const ptr` | Data pointer | No |
| `iLen` | `size_t` | Data length | - |

### Function: xrtSHA256Final

**Function Prototype**:
```c
XXAPI void xrtSHA256Final(xsha256_ctx* pCtx, uint8* pOut);
```

**Description**:
Complete SHA-256 hash calculation, output the result.

**Parameters**:
| Parameter | Type | Description | Can be NULL |
|-----------|------|-------------|------------|
| `pCtx` | `xsha256_ctx*` | SHA-256 context | No |
| `pOut` | `uint8*` | Output buffer (at least 32 bytes) | No |

### Function: xrtSHA256

**Function Prototype**:
```c
XXAPI void xrtSHA256(const ptr pData, size_t iLen, uint8* pOut);
```

**Description**:
Calculate SHA-256 hash in one go (convenience function).

**Parameters**:
| Parameter | Type | Description | Can be NULL |
|-----------|------|-------------|------------|
| `pData` | `const ptr` | Data pointer | No |
| `iLen` | `size_t` | Data length | - |
| `pOut` | `uint8*` | Output buffer (at least 32 bytes) | No |

**Example**:
```c
uint8 hash[32];
xrtSHA256("Hello, World!", 13, hash);
// hash contains the SHA-256 hash value
```

### Function: xrtHMAC_SHA256

**Function Prototype**:
```c
XXAPI void xrtHMAC_SHA256(const uint8* pKey, size_t iKeyLen,
                       const uint8* pMsg, size_t iMsgLen, uint8* pOut);
```

**Description**:
Calculate HMAC-SHA256 (Hash-based Message Authentication Code).

**Parameters**:
| Parameter | Type | Description | Can be NULL |
|-----------|------|-------------|------------|
| `pKey` | `const uint8*` | Key pointer | No |
| `iKeyLen` | `size_t` | Key length | - |
| `pMsg` | `const uint8*` | Message pointer | No |
| `iMsgLen` | `size_t` | Message length | - |
| `pOut` | `uint8*` | Output buffer (at least 32 bytes) | No |

**Example**:
```c
uint8 key[] = "secret_key";
uint8 mac[32];
xrtHMAC_SHA256(key, strlen(key), (uint8*)"Hello", 5, mac);
// mac contains the HMAC-SHA256 value
```

---

## SHA-384/SHA-512 Algorithm

SHA-512 is a cryptographic hash function that produces a 512-bit (64-byte) hash value. SHA-384 is a truncated version of SHA-512, producing a 384-bit (48-byte) hash value.

### Function: xrtSHA512Init

**Function Prototype**:
```c
XXAPI void xrtSHA512Init(xsha512_ctx* pCtx);
```

**Description**:
Initialize SHA-512 context.

**Parameters**:
| Parameter | Type | Description | Can be NULL |
|-----------|------|-------------|------------|
| `pCtx` | `xsha512_ctx*` | SHA-512 context | No |

### Function: xrtSHA512Update

**Function Prototype**:
```c
XXAPI void xrtSHA512Update(xsha512_ctx* pCtx, const ptr pData, size_t iLen);
```

**Description**:
Update SHA-512 hash calculation.

**Parameters**:
| Parameter | Type | Description | Can be NULL |
|-----------|------|-------------|------------|
| `pCtx` | `xsha512_ctx*` | SHA-512 context | No |
| `pData` | `const ptr` | Data pointer | No |
| `iLen` | `size_t` | Data length | - |

### Function: xrtSHA512Final

**Function Prototype**:
```c
XXAPI void xrtSHA512Final(xsha512_ctx* pCtx, uint8* pOut);
```

**Description**:
Complete SHA-512 hash calculation, output the result.

**Parameters**:
| Parameter | Type | Description | Can be NULL |
|-----------|------|-------------|------------|
| `pCtx` | `xsha512_ctx*` | SHA-512 context | No |
| `pOut` | `uint8*` | Output buffer (at least 64 bytes) | No |

### Function: xrtSHA512

**Function Prototype**:
```c
XXAPI void xrtSHA512(const ptr pData, size_t iLen, uint8* pOut);
```

**Description**:
Calculate SHA-512 hash in one go (convenience function).

**Parameters**:
| Parameter | Type | Description | Can be NULL |
|-----------|------|-------------|------------|
| `pData` | `const ptr` | Data pointer | No |
| `iLen` | `size_t` | Data length | - |
| `pOut` | `uint8*` | Output buffer (at least 64 bytes) | No |

### Function: xrtSHA384Init

**Function Prototype**:
```c
XXAPI void xrtSHA384Init(xsha512_ctx* pCtx);
```

**Description**:
Initialize SHA-384 context.

**Parameters**:
| Parameter | Type | Description | Can be NULL |
|-----------|------|-------------|------------|
| `pCtx` | `xsha512_ctx*` | SHA-384 context | No |

### Function: xrtSHA384Final

**Function Prototype**:
```c
XXAPI void xrtSHA384Final(xsha512_ctx* pCtx, uint8* pOut);
```

**Description**:
Complete SHA-384 hash calculation, output the result (48 bytes).

**Parameters**:
| Parameter | Type | Description | Can be NULL |
|-----------|------|-------------|------------|
| `pCtx` | `xsha512_ctx*` | SHA-384 context | No |
| `pOut` | `uint8*` | Output buffer (at least 48 bytes) | No |

### Function: xrtSHA384

**Function Prototype**:
```c
XXAPI void xrtSHA384(const ptr pData, size_t iLen, uint8* pOut);
```

**Description**:
Calculate SHA-384 hash in one go (convenience function).

**Parameters**:
| Parameter | Type | Description | Can be NULL |
|-----------|------|-------------|------------|
| `pData` | `const ptr` | Data pointer | No |
| `iLen` | `size_t` | Data length | - |
| `pOut` | `uint8*` | Output buffer (at least 48 bytes) | No |

### Function: xrtHMAC_SHA384

**Function Prototype**:
```c
XXAPI void xrtHMAC_SHA384(const uint8* pKey, size_t iKeyLen,
                       const uint8* pMsg, size_t iMsgLen, uint8* pOut);
```

**Description**:
Calculate HMAC-SHA384 (Hash-based Message Authentication Code).

**Parameters**:
| Parameter | Type | Description | Can be NULL |
|-----------|------|-------------|------------|
| `pKey` | `const uint8*` | Key pointer | No |
| `iKeyLen` | `size_t` | Key length | - |
| `pMsg` | `const uint8*` | Message pointer | No |
| `iMsgLen` | `size_t` | Message length | - |
| `pOut` | `uint8*` | Output buffer (at least 48 bytes) | No |

### Function: xrtHMAC_SHA512

**Function Prototype**:
```c
XXAPI void xrtHMAC_SHA512(const uint8* pKey, size_t iKeyLen,
                       const uint8* pMsg, size_t iMsgLen, uint8* pOut);
```

**Description**:
Calculate HMAC-SHA512 (Hash-based Message Authentication Code).

**Parameters**:
| Parameter | Type | Description | Can be NULL |
|-----------|------|-------------|------------|
| `pKey` | `const uint8*` | Key pointer | No |
| `iKeyLen` | `size_t` | Key length | - |
| `pMsg` | `const uint8*` | Message pointer | No |
| `iMsgLen` | `size_t` | Message length | - |
| `pOut` | `uint8*` | Output buffer (at least 64 bytes) | No |

---

## ChaCha20-Poly1305 Algorithm

ChaCha20-Poly1305 is an AEAD (Authenticated Encryption with Associated Data) algorithm combining ChaCha20 stream cipher and Poly1305 message authentication, compliant with RFC 8439.

### Function: xrtChaCha20

**Function Prototype**:
```c
XXAPI void xrtChaCha20(uint8* pOut, const uint8* pKey, const uint8* pNonce,
                     uint32 iCounter, const uint8* pIn, size_t iLen);
```

**Description**:
Encrypt data using ChaCha20 stream cipher.

**Parameters**:
| Parameter | Type | Description | Can be NULL |
|-----------|------|-------------|------------|
| `pOut` | `uint8*` | Output buffer | No |
| `pKey` | `const uint8*` | Key (32 bytes) | No |
| `pNonce` | `const uint8*` | Nonce (12 bytes) | No |
| `iCounter` | `uint32` | Counter | - |
| `pIn` | `const uint8*` | Input data | No |
| `iLen` | `size_t` | Data length | - |

**Notes**:
- Output buffer length should be at least equal to `iLen`
- Same key, nonce, and counter are used for decryption

**Example**:
```c
uint8 key[32] = { /* 32-byte key */ };
uint8 nonce[12] = { /* 12-byte nonce */ };
uint8 plaintext[] = "Hello, World!";
uint8 ciphertext[sizeof(plaintext)];

xrtChaCha20(ciphertext, key, nonce, 0, plaintext, sizeof(plaintext));
// ciphertext contains encrypted data
```

### Function: xrtChaCha20Poly1305Encrypt

**Function Prototype**:
```c
XXAPI bool xrtChaCha20Poly1305Encrypt(uint8* pOut, const uint8* pKey,
                                     const uint8* pNonce, const uint8* pAAD,
                                     size_t iAADLen, const uint8* pIn,
                                     size_t iLen);
```

**Description**:
Encrypt data using ChaCha20-Poly1305 AEAD.

**Parameters**:
| Parameter | Type | Description | Can be NULL |
|-----------|------|-------------|------------|
| `pOut` | `uint8*` | Output buffer (ciphertext + Tag, at least iLen + 16) | No |
| `pKey` | `const uint8*` | Key (32 bytes) | No |
| `pNonce` | `const uint8*` | Nonce (12 bytes) | No |
| `pAAD` | `const uint8*` | Additional Authenticated Data | Yes |
| `iAADLen` | `size_t` | AAD length | - |
| `pIn` | `const uint8*` | Input data | No |
| `iLen` | `size_t` | Data length | - |

**Return Value**:
| Return Value | Description |
|--------------|-------------|
| `true` | Encryption successful |
| `false` | Encryption failed |

**Notes**:
- Output buffer contains ciphertext (iLen bytes) + Tag (16 bytes)
- AAD (Additional Authenticated Data) is authenticated but not encrypted

**Example**:
```c
uint8 key[32] = { /* key */ };
uint8 nonce[12] = { /* nonce */ };
uint8 aad[] = "additional_data";
uint8 plaintext[] = "Secret message";
uint8 output[sizeof(plaintext) + 16];

xrtChaCha20Poly1305Encrypt(output, key, nonce, aad, sizeof(aad),
                          plaintext, sizeof(plaintext));
// output contains ciphertext (first sizeof(plaintext) bytes) and Tag (last 16 bytes)
```

### Function: xrtChaCha20Poly1305Decrypt

**Function Prototype**:
```c
XXAPI bool xrtChaCha20Poly1305Decrypt(uint8* pOut, const uint8* pKey,
                                     const uint8* pNonce, const uint8* pAAD,
                                     size_t iAADLen, const uint8* pIn,
                                     size_t iLen);
```

**Description**:
Decrypt and verify data using ChaCha20-Poly1305.

**Parameters**:
| Parameter | Type | Description | Can be NULL |
|-----------|------|-------------|------------|
| `pOut` | `uint8*` | Output buffer (at least iLen - 16) | No |
| `pKey` | `const uint8*` | Key (32 bytes) | No |
| `pNonce` | `const uint8*` | Nonce (12 bytes) | No |
| `pAAD` | `const uint8*` | Additional Authenticated Data | Yes |
| `iAADLen` | `size_t` | AAD length | - |
| `pIn` | `const uint8*` | Input data (ciphertext + Tag) | No |
| `iLen` | `size_t` | Data length (ciphertext + Tag) | - |

**Return Value**:
| Return Value | Description |
|--------------|-------------|
| `true` | Decryption successful, Tag verification passed |
| `false` | Decryption failed or Tag verification failed |

**Notes**:
- Input length must be at least 16 bytes (Tag size)
- Constant-time comparison is used to prevent timing attacks

**Example**:
```c
uint8 output[sizeof(plaintext)];
bool ok = xrtChaCha20Poly1305Decrypt(output, key, nonce, aad, sizeof(aad),
                          encrypted_data, sizeof(encrypted_data));
if (ok) {
    // output contains decrypted plaintext
} else {
    // Tag verification failed, data may have been tampered
}
```

---

## AES-GCM Algorithm

AES-GCM is an AEAD algorithm combining AES encryption and GCM authentication mode. Supports both AES-128-GCM and AES-256-GCM.

### Function: xrtAES128GCMEncrypt

**Function Prototype**:
```c
XXAPI bool xrtAES128GCMEncrypt(uint8* pOut, const uint8* pKey, const uint8* pNonce,
                            size_t iNonceLen, const uint8* pAAD, size_t iAADLen,
                            const uint8* pIn, size_t iLen);
```

**Description**:
Encrypt data using AES-128-GCM.

**Parameters**:
| Parameter | Type | Description | Can be NULL |
|-----------|------|-------------|------------|
| `pOut` | `uint8*` | Output buffer (ciphertext + Tag, at least iLen + 16) | No |
| `pKey` | `const uint8*` | Key (16 bytes) | No |
| `pNonce` | `const uint8*` | Nonce | No |
| `iNonceLen` | `size_t` | Nonce length | - |
| `pAAD` | `const uint8*` | Additional Authenticated Data | Yes |
| `iAADLen` | `size_t` | AAD length | - |
| `pIn` | `const uint8*` | Input data | No |
| `iLen` | `size_t` | Data length | - |

**Return Value**:
| Return Value | Description |
|--------------|-------------|
| `true` | Encryption successful |
| `false` | Encryption failed |

**Example**:
```c
uint8 key[16] = { /* 16-byte key */ };
uint8 nonce[12] = { /* 12-byte nonce */ };
uint8 plaintext[] = "Secret message";
uint8 output[sizeof(plaintext) + 16];

xrtAES128GCMEncrypt(output, key, nonce, 12, NULL, 0,
                    plaintext, sizeof(plaintext));
// output contains ciphertext and Tag
```

### Function: xrtAES128GCMDecrypt

**Function Prototype**:
```c
XXAPI bool xrtAES128GCMDecrypt(uint8* pOut, const uint8* pKey, const uint8* pNonce,
                            size_t iNonceLen, const uint8* pAAD, size_t iAADLen,
                            const uint8* pIn, size_t iLen);
```

**Description**:
Decrypt and verify data using AES-128-GCM.

**Parameters**:
| Parameter | Type | Description | Can be NULL |
|-----------|------|-------------|------------|
| `pOut` | `uint8*` | Output buffer (at least iLen - 16) | No |
| `pKey` | `const uint8*` | Key (16 bytes) | No |
| `pNonce` | `const uint8*` | Nonce | No |
| `iNonceLen` | `size_t` | Nonce length | - |
| `pAAD` | `const uint8*` | Additional Authenticated Data | Yes |
| `iAADLen` | `size_t` | AAD length | - |
| `pIn` | `const uint8*` | Input data (ciphertext + Tag) | No |
| `iLen` | `size_t` | Data length (ciphertext + Tag) | - |

**Return Value**:
| Return Value | Description |
|--------------|-------------|
| `true` | Decryption successful, Tag verification passed |
| `false` | Decryption failed or Tag verification failed |

### Function: xrtAES256GCMEncrypt

**Function Prototype**:
```c
XXAPI bool xrtAES256GCMEncrypt(uint8* pOut, const uint8* pKey, const uint8* pNonce,
                            size_t iNonceLen, const uint8* pAAD, size_t iAADLen,
                            const uint8* pIn, size_t iLen);
```

**Description**:
Encrypt data using AES-256-GCM.

**Parameters**:
| Parameter | Type | Description | Can be NULL |
|-----------|------|-------------|------------|
| `pOut` | `uint8*` | Output buffer (ciphertext + Tag, at least iLen + 16) | No |
| `pKey` | `const uint8*` | Key (32 bytes) | No |
| `pNonce` | `const uint8*` | Nonce | No |
| `iNonceLen` | `size_t` | Nonce length | - |
| `pAAD` | `const uint8*` | Additional Authenticated Data | Yes |
| `iAADLen` | `size_t` | AAD length | - |
| `pIn` | `const uint8*` | Input data | No |
| `iLen` | `size_t` | Data length | - |

**Return Value**:
| Return Value | Description |
|--------------|-------------|
| `true` | Encryption successful |
| `false` | Encryption failed |

### Function: xrtAES256GCMDecrypt

**Function Prototype**:
```c
XXAPI bool xrtAES256GCMDecrypt(uint8* pOut, const uint8* pKey, const uint8* pNonce,
                            size_t iNonceLen, const uint8* pAAD, size_t iAADLen,
                            const uint8* pIn, size_t iLen);
```

**Description**:
Decrypt and verify data using AES-256-GCM.

**Parameters**:
| Parameter | Type | Description | Can be NULL |
|-----------|------|-------------|------------|
| `pOut` | `uint8*` | Output buffer (at least iLen - 16) | No |
| `pKey` | `const uint8*` | Key (32 bytes) | No |
| `pNonce` | `const uint8*` | Nonce | No |
| `iNonceLen` | `size_t` | Nonce length | - |
| `pAAD` | `const uint8*` | Additional Authenticated Data | Yes |
| `iAADLen` | `size_t` | AAD length | - |
| `pIn` | `const uint8*` | Input data (ciphertext + Tag) | No |
| `iLen` | `size_t` | Data length (ciphertext + Tag) | - |

**Return Value**:
| Return Value | Description |
|--------------|-------------|
| `true` | Decryption successful, Tag verification passed |
| `false` | Decryption failed or Tag verification failed |

---

## Random Number Generation

### Function: xrtRandomBytes

**Function Prototype**:
```c
XXAPI void xrtRandomBytes(uint8* pBuf, size_t iLen);
```

**Description**:
Generate cryptographically secure random bytes.

**Parameters**:
| Parameter | Type | Description | Can be NULL |
|-----------|------|-------------|------------|
| `pBuf` | `uint8*` | Output buffer | No |
| `iLen` | `size_t` | Number of random bytes | - |

**Notes**:
- Windows: Uses `RtlGenRandom` (SystemFunction036)
- Linux: Uses `/dev/urandom`
- Fallback: Uses XRT internal random generator if system source unavailable

**Example**:
```c
uint8 key[32];
uint8 nonce[12];

xrtRandomBytes(key, sizeof(key));
xrtRandomBytes(nonce, sizeof(nonce));
// key and nonce contain cryptographically secure random numbers
```

---

## HKDF Key Derivation

HKDF (HMAC-based Extract-and-Expand Key Derivation) is a key derivation function based on HMAC, compliant with RFC 5869.

### Function: xrtHKDFExtract

**Function Prototype**:
```c
XXAPI void xrtHKDFExtract(uint8* pPRK, const uint8* pSalt, size_t iSaltLen,
                        const uint8* pIKM, size_t iIKMLen);
```

**Description**:
HKDF Extract phase: Derive a pseudorandom key from input key material.

**Parameters**:
| Parameter | Type | Description | Can be NULL |
|-----------|------|-------------|------------|
| `pPRK` | `uint8*` | Output pseudorandom key (at least 32 bytes) | No |
| `pSalt` | `const uint8*` | Salt value | Yes |
| `iSaltLen` | `size_t` | Salt length | - |
| `pIKM` | `const uint8*` | Input key material | No |
| `iIKMLen` | `size_t` | Input key material length | - |

**Notes**:
- If `pSalt` is NULL or `iSaltLen` is 0, default salt value is used (32 bytes of zeros)
- Output is 32-byte SHA-256 hash

### Function: xrtHKDFExpand

**Function Prototype**:
```c
XXAPI void xrtHKDFExpand(uint8* pOKM, size_t iOKMLen, const uint8* pPRK,
                       size_t iPRKLen, const uint8* pInfo, size_t iInfoLen);
```

**Description**:
HKDF Expand phase: Derive output key from pseudorandom key.

**Parameters**:
| Parameter | Type | Description | Can be NULL |
|-----------|------|-------------|------------|
| `pOKM` | `uint8*` | Output key | No |
| `iOKMLen` | `size_t` | Output key length | - |
| `pPRK` | `const uint8*` | Pseudorandom key (32 bytes) | No |
| `iPRKLen` | `size_t` | Pseudorandom key length | - |
| `pInfo` | `const uint8*` | Optional context information | Yes |
| `iInfoLen` | `size_t` | Context information length | - |

**Notes**:
- Can derive output keys of arbitrary length
- Supports optional context information

### Function: xrtHKDFExtract_SHA384

**Function Prototype**:
```c
XXAPI void xrtHKDFExtract_SHA384(uint8* pPRK, const uint8* pSalt, size_t iSaltLen,
                              const uint8* pIKM, size_t iIKMLen);
```

**Description**:
HKDF Extract phase (SHA-384 version).

**Parameters**:
| Parameter | Type | Description | Can be NULL |
|-----------|------|-------------|------------|
| `pPRK` | `uint8*` | Output pseudorandom key (at least 48 bytes) | No |
| `pSalt` | `const uint8*` | Salt value | Yes |
| `iSaltLen` | `size_t` | Salt length | - |
| `pIKM` | `const uint8*` | Input key material | No |
| `iIKMLen` | `size_t` | Input key material length | - |

**Notes**:
- Output is 48-byte SHA-384 hash

### Function: xrtHKDFExpand_SHA384

**Function Prototype**:
```c
XXAPI void xrtHKDFExpand_SHA384(uint8* pOKM, size_t iOKMLen, const uint8* pPRK,
                               size_t iPRKLen, const uint8* pInfo, size_t iInfoLen);
```

**Description**:
HKDF Expand phase (SHA-384 version).

**Parameters**:
| Parameter | Type | Description | Can be NULL |
|-----------|------|-------------|------------|
| `pOKM` | `uint8*` | Output key | No |
| `iOKMLen` | `size_t` | Output key length | - |
| `pPRK` | `const uint8*` | Pseudorandom key (48 bytes) | No |
| `iPRKLen` | `size_t` | Pseudorandom key length | - |
| `pInfo` | `const uint8*` | Optional context information | Yes |
| `iInfoLen` | `size_t` | Context information length | - |

**Example**:
```c
uint8 salt[16] = { /* random salt */ };
uint8 ikm[] = "input_key_material";
uint8 prk[32];
uint8 key[32];

// HKDF Extract
xrtHKDFExtract(prk, salt, sizeof(salt), (uint8*)ikm, sizeof(ikm));

// HKDF Expand
xrtHKDFExpand(key, sizeof(key), prk, 32, (uint8*)"context", 7);
// key contains the derived key
```

---

## X25519 Key Exchange

X25519 is an elliptic curve Diffie-Hellman key exchange algorithm, used to securely negotiate shared secrets.

### Function: xrtX25519Keypair

**Function Prototype**:
```c
XXAPI void xrtX25519Keypair(uint8* pPrivKey, uint8* pPubKey);
```

**Description**:
Generate X25519 key pair (private key and public key).

**Parameters**:
| Parameter | Type | Description | Can be NULL |
|-----------|------|-------------|------------|
| `pPrivKey` | `uint8*` | Output private key (32 bytes) | No |
| `pPubKey` | `uint8*` | Output public key (32 bytes) | No |

**Notes**:
- Private key is generated using cryptographically secure random numbers
- Public key is derived from private key

**Example**:
```c
uint8 privkey[32];
uint8 pubkey[32];

xrtX25519Keypair(privkey, pubkey);
// privkey and pubkey contain the key pair
```

### Function: xrtX25519SharedSecret

**Function Prototype**:
```c
XXAPI void xrtX25519SharedSecret(uint8* pOut, const uint8* pPrivKey,
                                 const uint8* pPubKey);
```

**Description**:
Calculate X25519 shared secret (ECDH).

**Parameters**:
| Parameter | Type | Description | Can be NULL |
|-----------|------|-------------|------------|
| `pOut` | `uint8*` | Output shared secret (32 bytes) | No |
| `pPrivKey` | `const uint8*` | Private key (32 bytes) | No |
| `pPubKey` | `const uint8*` | Peer public key (32 bytes) | No |

**Example**:
```c
// Alice generates key pair
uint8 alice_priv[32], alice_pub[32];
xrtX25519Keypair(alice_priv, alice_pub);

// Bob generates key pair
uint8 bob_priv[32], bob_pub[32];
xrtX25519Keypair(bob_priv, bob_pub);

// Alice calculates shared secret
uint8 alice_shared[32];
xrtX25519SharedSecret(alice_shared, alice_priv, bob_pub);

// Bob calculates shared secret
uint8 bob_shared[32];
xrtX25519SharedSecret(bob_shared, bob_priv, alice_pub);

// alice_shared and bob_shared contain the same shared secret
```

---

## RSA Signature Verification

RSA signature verification is used to verify the validity of RSA digital signatures. Current implementation supports signature verification only (not signature generation).

### Function: xrtRSAVerify

**Function Prototype**:
```c
XXAPI bool xrtRSAVerify(const uint8* pHash, size_t iHashLen,
                      const uint8* pSig, size_t iSigLen,
                      const uint8* pPubKey, size_t iPubKeyLen);
```

**Description**:
Verify RSA signature.

**Parameters**:
| Parameter | Type | Description | Can be NULL |
|-----------|------|-------------|------------|
| `pHash` | `const uint8*` | Hash value | No |
| `iHashLen` | `size_t` | Hash length | - |
| `pSig` | `const uint8*` | Signature (DER encoded) | No |
| `iSigLen` | `size_t` | Signature length | - |
| `pPubKey` | `const uint8*` | Public key (DER encoded) | No |
| `iPubKeyLen` | `size_t` | Public key length | - |

**Return Value**:
| Return Value | Description |
|--------------|-------------|
| `true` | Signature verification passed |
| `false` | Signature verification failed |

**Notes**:
- Uses PKCS#1 v1.5 padding
- Supports common hash algorithms (SHA-256, SHA-384, SHA-512)
- Constant-time comparison to prevent timing attacks

---

## secp256r1 ECDH/ECDSA

secp256r1 (NIST P-256) is an elliptic curve algorithm used for ECDH key exchange and ECDSA digital signatures.

### Function: xrtECDHSecp256r1Keypair

**Function Prototype**:
```c
XXAPI void xrtECDHSecp256r1Keypair(uint8* pPrivKey, uint8* pPubKey);
```

**Description**:
Generate secp256r1 key pair (private key and public key).

**Parameters**:
| Parameter | Type | Description | Can be NULL |
|-----------|------|-------------|------------|
| `pPrivKey` | `uint8*` | Output private key (32 bytes) | No |
| `pPubKey` | `uint8*` | Output public key (65 bytes, uncompressed format) | No |

**Notes**:
- Public key format: 0x04 || X || Y (65 bytes uncompressed)
- Private key format: 32 bytes scalar

**Example**:
```c
uint8 privkey[32];
uint8 pubkey[65];

xrtECDHSecp256r1Keypair(privkey, pubkey);
// privkey and pubkey contain the key pair
```

### Function: xrtECDHSecp256r1SharedSecret

**Function Prototype**:
```c
XXAPI void xrtECDHSecp256r1SharedSecret(uint8* pOut, const uint8* pPrivKey,
                                      const uint8* pPubKey);
```

**Description**:
Calculate secp256r1 shared secret (ECDH).

**Parameters**:
| Parameter | Type | Description | Can be NULL |
|-----------|------|-------------|------------|
| `pOut` | `uint8*` | Output shared secret (32 bytes) | No |
| `pPrivKey` | `const uint8*` | Private key (32 bytes) | No |
| `pPubKey` | `const uint8*` | Peer public key (65 bytes, uncompressed format) | No |

**Example**:
```c
// Alice generates key pair
uint8 alice_priv[32], alice_pub[65];
xrtECDHSecp256r1Keypair(alice_priv, alice_pub);

// Bob generates key pair
uint8 bob_priv[32], bob_pub[65];
xrtECDHSecp256r1Keypair(bob_priv, bob_pub);

// Alice calculates shared secret
uint8 alice_shared[32];
xrtECDHSecp256r1SharedSecret(alice_shared, alice_priv, bob_pub);

// Bob calculates shared secret
uint8 bob_shared[32];
xrtECDHSecp256r1SharedSecret(bob_shared, bob_priv, alice_pub);

// alice_shared and bob_shared contain the same shared secret
```

### Function: xrtECDSAVerify

**Function Prototype**:
```c
XXAPI bool xrtECDSAVerify(const uint8* pHash, size_t iHashLen,
                          const uint8* pSig, size_t iSigLen,
                          const uint8* pPubKey, size_t iPubKeyLen);
```

**Description**:
Verify secp256r1 ECDSA signature.

**Parameters**:
| Parameter | Type | Description | Can be NULL |
|-----------|------|-------------|------------|
| `pHash` | `const uint8*` | Hash value | No |
| `iHashLen` | `size_t` | Hash length | - |
| `pSig` | `const uint8*` | Signature (DER encoded) | No |
| `iSigLen` | `size_t` | Signature length | - |
| `pPubKey` | `const uint8*` | Public key (65 bytes, uncompressed format) | No |
| `iPubKeyLen` | `size_t` | Public key length | - |

**Return Value**:
| Return Value | Description |
|--------------|-------------|
| `true` | Signature verification passed |
| `false` | Signature verification failed |

**Notes**:
- Supports common hash algorithms (SHA-256, SHA-384)
- Signature format: DER encoded SEQUENCE { INTEGER r, INTEGER s }
- Constant-time comparison to prevent timing attacks

---

## Usage Examples

### Example 1: Data Hashing

```c
#define XRT_IMPLEMENTATION
#include "xrt.h"

int main() {
    xrtInit();

    // SHA-256 hash
    uint8 sha256_hash[32];
    xrtSHA256("Hello, World!", 13, sha256_hash);

    // SHA-512 hash
    uint8 sha512_hash[64];
    xrtSHA512("Hello, World!", 13, sha512_hash);

    xrtUnit();
    return 0;
}
```

### Example 2: HMAC Authentication

```c
int main() {
    xrtInit();

    uint8 key[] = "secret_key";
    uint8 message[] = "authenticated_message";
    uint8 mac[32];

    xrtHMAC_SHA256(key, strlen(key), message, strlen(message), mac);

    // Verify HMAC
    uint8 verify_mac[32];
    xrtHMAC_SHA256(key, strlen(key), message, strlen(message), verify_mac);

    bool valid = memcmp(mac, verify_mac, 32) == 0;
    printf("HMAC valid: %s\n", valid ? "yes" : "no");

    xrtUnit();
    return 0;
}
```

### Example 3: ChaCha20-Poly1305 Encryption

```c
int main() {
    xrtInit();

    uint8 key[32];
    uint8 nonce[12];
    uint8 plaintext[] = "Secret message";
    uint8 ciphertext[sizeof(plaintext) + 16];

    // Generate random key and nonce
    xrtRandomBytes(key, sizeof(key));
    xrtRandomBytes(nonce, sizeof(nonce));

    // Encrypt
    xrtChaCha20Poly1305Encrypt(ciphertext, key, nonce, NULL, 0,
                              plaintext, sizeof(plaintext));

    // Decrypt
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

### Example 4: X25519 Key Exchange

```c
int main() {
    xrtInit();

    // Alice generates key pair
    uint8 alice_priv[32], alice_pub[32];
    xrtX25519Keypair(alice_priv, alice_pub);

    // Bob generates key pair
    uint8 bob_priv[32], bob_pub[32];
    xrtX25519Keypair(bob_priv, bob_pub);

    // Exchange public keys (send over network)
    // Alice sends alice_pub to Bob
    // Bob sends bob_pub to Alice

    // Calculate shared secret
    uint8 alice_shared[32], bob_shared[32];
    xrtX25519SharedSecret(alice_shared, alice_priv, bob_pub);
    xrtX25519SharedSecret(bob_shared, bob_priv, alice_pub);

    // Verify shared secrets match
    bool keys_match = memcmp(alice_shared, bob_shared, 32) == 0;
    printf("Keys match: %s\n", keys_match ? "yes" : "no");

    xrtUnit();
    return 0;
}
```

### Example 5: HKDF Key Derivation

```c
int main() {
    xrtInit();

    uint8 salt[16] = { /* random salt */ };
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

### Example 6: AES-GCM Encryption

```c
int main() {
    xrtInit();

    uint8 key[16];
    uint8 nonce[12];
    uint8 plaintext[] = "Secret message";
    uint8 aad[] = "additional_data";
    uint8 ciphertext[sizeof(plaintext) + 16];

    // Generate random key and nonce
    xrtRandomBytes(key, sizeof(key));
    xrtRandomBytes(nonce, sizeof(nonce));

    // Encrypt
    xrtAES128GCMEncrypt(ciphertext, key, nonce, 12, aad, sizeof(aad),
                      plaintext, sizeof(plaintext));

    // Decrypt
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

### Example 7: secp256r1 ECDH

```c
int main() {
    xrtInit();

    // Alice generates key pair
    uint8 alice_priv[32], alice_pub[65];
    xrtECDHSecp256r1Keypair(alice_priv, alice_pub);

    // Bob generates key pair
    uint8 bob_priv[32], bob_pub[65];
    xrtECDHSecp256r1Keypair(bob_priv, bob_pub);

    // Exchange public keys (send over network)

    // Calculate shared secret
    uint8 alice_shared[32], bob_shared[32];
    xrtECDHSecp256r1SharedSecret(alice_shared, alice_priv, bob_pub);
    xrtECDHSecp256r1SharedSecret(bob_shared, bob_priv, alice_pub);

    // Verify shared secrets match
    bool keys_match = memcmp(alice_shared, bob_shared, 32) == 0;
    printf("Keys match: %s\n", keys_match ? "yes" : "no");

    xrtUnit();
    return 0;
}
```

---

## Notes

1. **Key Security**:
   - Keep private keys secure
   - Never hardcode keys in source code
   - Use secure key management systems

2. **Nonce Usage**:
   - Nonce must be unique for ChaCha20 and AES-GCM
   - Never reuse the same key and nonce combination
   - Use counters or random number generators for nonces

3. **Random Number Quality**:
   - `xrtRandomBytes` provides cryptographically secure random numbers
   - Only use fallback mechanism in special circumstances
   - Consider using specialized CSPRNG for key generation

4. **Memory Safety**:
   - Wipe sensitive data after encryption/decryption
   - Use `memset` to zero keys and plaintext that are no longer needed
   - Consider using secure memory allocators

5. **Performance Considerations**:
   - SHA-512 is about 2x slower than SHA-256
   - AES-256-GCM is about 40% slower than AES-128-GCM
   - ChaCha20-Poly1305 is faster than AES-GCM without AES hardware acceleration

6. **Algorithm Selection**:
   - SHA-256: General-purpose hash, good balance of security and performance
   - SHA-512: When higher security is needed
   - ChaCha20-Poly1305: Mobile devices and systems without AES hardware acceleration
   - AES-GCM: When AES hardware acceleration is available

7. **Cross-Platform Compatibility**:
   - All algorithms behave consistently on Windows and Linux
   - Test carefully for endianness issues

---

<div align="center">

**XRT Crypto Module** | Version 1.0 | Last Updated: 2025-01

</div>
