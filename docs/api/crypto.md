# Crypto 基础 API

## 分层与裁剪

密码学底座从最小共同原语开始，避免把 TLS 所需的全部算法重新合并为旧版单体：

- `XRT_FEATURE_CRYPTO_CORE`：摘要长度元数据、安全清零和常量时间比较，只依赖 core。
- `XRT_FEATURE_CRYPTO_INT31`：P-256、P-384 与 RSA 共用的内部常数时间多精度整数层，只依赖 `CRYPTO_CORE`；它不公开表示相关 API。
- `XRT_FEATURE_CRYPTO_NIST`：P-256 与 P-384 共用的内部 SEC 1 点校验和常数时间标量乘法层，依赖 `CRYPTO_INT31`；公开曲线模块在其上提供稳定 API。
- `XRT_FEATURE_CRYPTO_P256`：P-256 公共点校验、点乘、点加、公钥派生和 ECDH，共用 `CRYPTO_NIST`，不自动引入随机源。
- `XRT_FEATURE_CRYPTO_P384`：P-384 公共点校验、点乘、点加、公钥派生和 ECDH，共用 `CRYPTO_NIST`，不自动引入随机源。
- `XRT_FEATURE_CRYPTO_NIST_KEYPAIR`：NIST 曲线共用的安全随机标量采样与密钥对原子发布层，依赖 `CRYPTO_NIST` 和独立 `RANDOM_SECURE`。
- `XRT_FEATURE_CRYPTO_P256_KEYPAIR` / `XRT_FEATURE_CRYPTO_P384_KEYPAIR`：分别公开两条曲线的随机密钥对便利入口。
- `XRT_FEATURE_CRYPTO_ECDSA_CORE`：曲线无关的 ECDSA 错误、摘要和标量公共契约。
- `XRT_FEATURE_CRYPTO_ECDSA_DER`：曲线无关的 raw 与规范 DER 签名转换层。
- `XRT_FEATURE_CRYPTO_ECDSA_MATH`：ECDSA 共用的 NIST 标量与双标量点运算层。
- `XRT_FEATURE_CRYPTO_ECDSA_VERIFY` / `XRT_FEATURE_CRYPTO_ECDSA_SIGN`：曲线无关的验签与确定性签名核心。
- `XRT_FEATURE_CRYPTO_ECDSA_VERIFY_DER` / `XRT_FEATURE_CRYPTO_ECDSA_SIGN_DER`：在唯一 DER 表示层上组合的验签与签名适配层。
- `XRT_FEATURE_CRYPTO_ECDSA_P256` / `XRT_FEATURE_CRYPTO_ECDSA_P384`：两条曲线的 raw 验签入口。
- `XRT_FEATURE_CRYPTO_ECDSA_P256_SIGN` / `XRT_FEATURE_CRYPTO_ECDSA_P384_SIGN`：两条曲线的 raw 签名入口。
- `XRT_FEATURE_CRYPTO_ECDSA_P256_SIGN_DER` / `XRT_FEATURE_CRYPTO_ECDSA_P384_SIGN_DER`：两条曲线的 DER 签名便利入口。
- `XRT_FEATURE_CRYPTO_MD5`：仅供 HTTP Digest 等历史协议互操作的独立 MD5 单元。
- `XRT_FEATURE_CRYPTO_SHA1`：WebSocket 与历史协议所需 SHA-1。
- `XRT_FEATURE_CRYPTO_SHA224`：与 SHA-256 共享压缩实现的独立 SHA-224 接口。
- `XRT_FEATURE_CRYPTO_SHA256`：独立 SHA-256。
- `XRT_FEATURE_CRYPTO_SHA512`：共享压缩核心的 SHA-384 与 SHA-512。
- `XRT_FEATURE_CRYPTO_SHA512_256`：复用 SHA-512 压缩核心但使用独立初始向量的 SHA-512/256。
- `XRT_FEATURE_CRYPTO_HMAC_SHA256`：流式与一次性 HMAC-SHA256。
- `XRT_FEATURE_CRYPTO_HMAC_SHA512`：流式与一次性 HMAC-SHA384/512。
- `XRT_FEATURE_CRYPTO_PBKDF2_SHA256`：基于预计算 HMAC 状态的 PBKDF2-HMAC-SHA256。
- `XRT_FEATURE_CRYPTO_PBKDF2_SHA512`：共享派生循环的 PBKDF2-HMAC-SHA384/512。
- `XRT_FEATURE_CRYPTO_HKDF_SHA256`：HKDF-SHA256 Extract/Expand/组合入口。
- `XRT_FEATURE_CRYPTO_HKDF_SHA512`：HKDF-SHA384/512 Extract/Expand/组合入口。
- `XRT_FEATURE_CRYPTO_CHACHA20`：IETF 96 位 nonce 的裸 ChaCha20 变换。
- `XRT_FEATURE_CRYPTO_POLY1305`：流式与一次性 Poly1305。
- `XRT_FEATURE_CRYPTO_CHACHA20_POLY1305`：依赖前两项的 RFC 8439 AEAD。
- `XRT_FEATURE_CRYPTO_AES`：常量时间软件 AES-128/192/256 块密码。
- `XRT_FEATURE_CRYPTO_AES_GCM`：依赖 AES 的 NIST SP 800-38D AEAD 与 GMAC。
- `XRT_FEATURE_CRYPTO_X25519`：无随机源依赖的 RFC 7748 标量乘法、公钥派生与安全共享秘密入口。
- `XRT_FEATURE_CRYPTO_X25519_KEYPAIR`：依赖 X25519 与系统安全随机源的密钥对便利入口。
- `XRT_FEATURE_CRYPTO_X448`：独立的 RFC 7748 X448 标量乘法、公钥派生与安全共享秘密入口。
- `XRT_FEATURE_CRYPTO_X448_KEYPAIR`：依赖 X448 与系统安全随机源的密钥对便利入口。
- `XRT_FEATURE_CRYPTO_RSA`：RSA 公钥模幂与公钥视图，依赖 `CRYPTO_INT31`。
- `XRT_FEATURE_CRYPTO_RSA_PRIVATE`：完整私有指数与双素数 CRT 私钥运算，依赖 RSA；所有结果都以公开指数复核。
- `XRT_FEATURE_CRYPTO_RSA_PSS`：RSA-PSS 严格验签与共享 MGF1/PSS 编码底座。
- `XRT_FEATURE_CRYPTO_RSA_PSS_SIGN`：显式盐和系统安全随机盐 PSS 签名，依赖 PSS、RSA 私钥与安全随机源。
- `XRT_FEATURE_CRYPTO_RSA_PKCS1`：规范 EMSA-PKCS1-v1_5 验签与共享 DigestInfo 底座。
- `XRT_FEATURE_CRYPTO_RSA_PKCS1_SIGN`：PKCS#1 v1.5 签名，依赖 PKCS#1 与 RSA 私钥，不引入随机源。

后续摘要、HMAC/HKDF、AEAD、密钥交换、签名、X.509 和 TLS Session 分别建立在这些最小原语之上。只使用 WebSocket SHA-1 或令牌随机数的程序不必携带完整 TLS 实现。

## `xrtCryptoHashSize`

```c
size_t xrtCryptoHashSize(xcryptohash Hash);
```

返回 `XCRYPTO_HASH_MD5`、`XCRYPTO_HASH_SHA1`、`XCRYPTO_HASH_SHA224`、`XCRYPTO_HASH_SHA256`、`XCRYPTO_HASH_SHA384`、`XCRYPTO_HASH_SHA512` 或 `XCRYPTO_HASH_SHA512_256` 的固定摘要长度。未知标识返回零且不修改错误槽。摘要长度与具体实现是否编入无关，因此协议元数据、容量计算和裁剪探测不必各自复制一份算法长度表；这项查询本身不表示对应摘要后端可执行。

公开枚举标签是 `xcrypto_hash`，常用类型名是 `xcryptohash`。两者表示同一组稳定算法标识。

## `xrtSecureZero`

```c
void xrtSecureZero(ptr pData, size_t iSize);
```

通过 volatile 写入清零密钥、口令和中间状态，避免普通 `memset` 被编译器按死存储删除。`iSize == 0` 时允许 `pData == NULL`；非空区间传入空指针会设置 `XERR_ARGUMENT`。

这项保证只覆盖调用给出的内存区间。调用方仍须避免编译器、日志、交换文件、崩溃转储或上层副本留下其他敏感数据。

## `xrtConstTimeEqual`

```c
bool xrtConstTimeEqual(const void* pLeft, const void* pRight, size_t iSize);
```

比较过程不会根据首个不同字节提前退出，适合摘要、认证标签和派生结果。运行时间仍与公开的 `iSize` 成正比；它不隐藏长度，也不等同于完整的侧信道防护。空区间返回 `true` 并允许空指针；非空区间的空指针设置 `XERR_ARGUMENT`。

密码模块使用独立 `XRT_FEATURE_RANDOM_SECURE` 提供的 `xrtSecureRandom`，契约、平台实现和示例见 `docs/api/random.md`。随机源不再属于 `crypto_core`，因此文件、网络和运行时基础设施可以安全复用而不携带密码算法。

摘要元数据、常量时间比较与敏感区间清理的组合示例位于 `examples/crypto/core/main.c`。

## MD5 协议互操作

`XRT_FEATURE_CRYPTO_MD5` 是只依赖 `CRYPTO_CORE` 的独立裁剪单元，用于 HTTP Digest 等仍要求 MD5 的历史协议。MD5 已不具备抗碰撞安全性，不应被用于新签名、证书、内容可信性或口令存储设计；现代 HTTP Digest 默认路径也不会自动携带本模块。

```c
#define XRT_MD5_SIZE 16u
#define XRT_MD5_BLOCK_SIZE 64u

void xrtMd5Init(xmd5* pState);
bool xrtMd5Update(xmd5* pState, const void* pData, size_t iSize);
bool xrtMd5Final(const xmd5* pState, void* pDigest);
bool xrtMd5(const void* pData, size_t iSize, void* pDigest);
```

接口不分配内存。`Update` 仅缓存不足 64 字节的尾部；`Final` 在状态副本上完成填充，可重复调用并允许随后继续追加。参数、损坏状态和 64 位 bit-length 上限错误在修改状态前返回。完整示例位于 `examples/crypto/md5/main.c`。

## SHA-1

`XRT_FEATURE_CRYPTO_SHA1` 是独立裁剪单元，只依赖 `CRYPTO_CORE`。它仅用于 WebSocket 握手和必须兼容 SHA-1 的历史协议，不应用于新签名、口令或抗碰撞安全设计。

```c
#define XRT_SHA1_SIZE 20u
#define XRT_SHA1_BLOCK_SIZE 64u

void xrtSha1Init(xsha1* pState);
bool xrtSha1Update(xsha1* pState, const void* pData, size_t iSize);
bool xrtSha1Final(const xsha1* pState, void* pDigest);
bool xrtSha1(const void* pData, size_t iSize, void* pDigest);
```

流状态由调用方持有，不分配内存。`Update` 直接压缩完整输入块，只复制不足 64 字节的尾部。`Final` 在状态快照上完成 padding，不修改或结束原状态，因此可以重复取得摘要，也可以继续追加数据。一次性函数覆盖常见的一行调用路径。

## SHA-224 与 SHA-256

`XRT_FEATURE_CRYPTO_SHA256` 只依赖 `CRYPTO_CORE`。`XRT_FEATURE_CRYPTO_SHA224` 依赖 SHA-256 的共享块压缩实现，但具有独立初始向量、状态标记和公开接口；启用 SHA-256 不会反向携带 SHA-224。两者都不会自动携带 SHA-1、SHA-384、SHA-512、HMAC 或 TLS。

```c
#define XRT_SHA224_SIZE 28u
#define XRT_SHA224_BLOCK_SIZE 64u

void xrtSha224Init(xsha224* pState);
bool xrtSha224Update(xsha224* pState, const void* pData, size_t iSize);
bool xrtSha224Final(const xsha224* pState, void* pDigest);
bool xrtSha224(const void* pData, size_t iSize, void* pDigest);

#define XRT_SHA256_SIZE 32u
#define XRT_SHA256_BLOCK_SIZE 64u

void xrtSha256Init(xsha256* pState);
bool xrtSha256Update(xsha256* pState, const void* pData, size_t iSize);
bool xrtSha256Final(const xsha256* pState, void* pDigest);
bool xrtSha256(const void* pData, size_t iSize, void* pDigest);
```

SHA-1、SHA-224 与 SHA-256 都检查初始化 guard、尾部长度和 64 位 bit-length 上限。SHA-224 与 SHA-256 的状态布局相同，但 API 会拒绝跨算法混用。参数、状态或长度失败发生在本次状态修改之前；非空数据不允许空指针，空区间允许空指针。公开状态字段只用于栈上或内嵌存储，不允许调用方直接修改。

完整示例位于 `examples/crypto/sha1/main.c`、`examples/crypto/sha224/main.c` 与 `examples/crypto/sha256/main.c`。

## SHA-384、SHA-512 与 SHA-512/256

SHA-384 与 SHA-512 使用相同的 64 位压缩函数和 128 字节块，因此由一个 `XRT_FEATURE_CRYPTO_SHA512` 裁剪单元提供；它不携带 HMAC、HKDF、签名或 TLS。SHA-512/256 由独立的 `XRT_FEATURE_CRYPTO_SHA512_256` 开启，只增加 FIPS 180-4 初始向量、状态标记和 32 字节输出入口，不复制 80 轮压缩实现。它不是把普通 SHA-512 摘要截断到 256 位。

```c
#define XRT_SHA384_SIZE 48u
#define XRT_SHA512_SIZE 64u
#define XRT_SHA512_256_SIZE 32u
#define XRT_SHA384_BLOCK_SIZE 128u
#define XRT_SHA512_BLOCK_SIZE 128u
#define XRT_SHA512_256_BLOCK_SIZE 128u

void xrtSha384Init(xsha384* pState);
bool xrtSha384Update(xsha384* pState, const void* pData, size_t iSize);
bool xrtSha384Final(const xsha384* pState, void* pDigest);
bool xrtSha384(const void* pData, size_t iSize, void* pDigest);

void xrtSha512Init(xsha512* pState);
bool xrtSha512Update(xsha512* pState, const void* pData, size_t iSize);
bool xrtSha512Final(const xsha512* pState, void* pDigest);
bool xrtSha512(const void* pData, size_t iSize, void* pDigest);

void xrtSha512_256Init(xsha512_256* pState);
bool xrtSha512_256Update(
	xsha512_256* pState, const void* pData, size_t iSize
);
bool xrtSha512_256Final(
	const xsha512_256* pState, void* pDigest
);
bool xrtSha512_256(
	const void* pData, size_t iSize, void* pDigest
);
```

三种状态都使用独立 guard，跨算法混用在修改状态前返回 `XERR_STATE`。`Final` 只处理状态快照，可以重复调用或在随后继续 `Update`；一次性入口和流式入口都不分配内存，并在返回前清理临时状态。`XCRYPTO_HASH_SHA512_256` 的固定摘要长度为 `XRT_SHA512_256_SIZE`。

SHA-512/256 的固定向量、全部 257 个输入分割点、状态串用和失败原子性由独立模块化与单头测试覆盖。示例位于 `examples/crypto/sha512_256/main.c`。

状态使用 `SizeHigh:SizeLow` 维护 128 位字节计数，Final 按 FIPS 180-4 写入完整 128 位 bit-length，不再把高 64 位固定为零。`xsha384` 与 `xsha512` 布局相同以共享压缩实现，但 Guard 会拒绝算法串用。完整块直接从调用方输入压缩，只有不足 128 字节的尾部进入状态缓冲。

完整示例位于 `examples/crypto/sha512/main.c`。

## HMAC

HMAC 按底层摘要家族独立裁剪。SHA-256 版本只依赖 `CRYPTO_SHA256`；SHA-384/512 共享 128 字节密钥块处理，只依赖 `CRYPTO_SHA512`。初始化时预计算 inner/outer 摘要状态，后续多次 `Update` 不重复处理密钥。

```c
bool xrtHmacSha256Init(xhmacsha256* pState, const void* pKey, size_t iKeySize);
bool xrtHmacSha256Update(xhmacsha256* pState, const void* pData, size_t iSize);
bool xrtHmacSha256Final(const xhmacsha256* pState, void* pMac);
bool xrtHmacSha256(
	const void* pKey, size_t iKeySize,
	const void* pData, size_t iSize,
	void* pMac
);

bool xrtHmacSha384Init(xhmacsha384* pState, const void* pKey, size_t iKeySize);
bool xrtHmacSha384Update(xhmacsha384* pState, const void* pData, size_t iSize);
bool xrtHmacSha384Final(const xhmacsha384* pState, void* pMac);
bool xrtHmacSha384(
	const void* pKey, size_t iKeySize,
	const void* pData, size_t iSize,
	void* pMac
);

bool xrtHmacSha512Init(xhmacsha512* pState, const void* pKey, size_t iKeySize);
bool xrtHmacSha512Update(xhmacsha512* pState, const void* pData, size_t iSize);
bool xrtHmacSha512Final(const xhmacsha512* pState, void* pMac);
bool xrtHmacSha512(
	const void* pKey, size_t iKeySize,
	const void* pData, size_t iSize,
	void* pMac
);
```

`xhmacsha512` 是公开状态结构，`xhmacsha384` 是同一布局的算法专用类型；guard 会拒绝两种算法串用。`Final` 从 inner/outer 快照计算认证码，不修改原状态，可以重复调用，也可在 Final 后继续 Update。

空密钥和空消息是合法输入，长度为零时相应指针可以为 `NULL`。非零长度空指针、算法状态串用、损坏状态和空输出都会返回 `false` 并设置结构化错误。超过摘要块长的密钥先按 RFC 2104 压缩；密钥块、ipad/opad、中间摘要和栈状态在使用后安全清零。

完整示例位于 `examples/crypto/hmac_sha256/main.c` 和 `examples/crypto/hmac_sha512/main.c`。

## PBKDF2

PBKDF2 是面向密码的同步密钥派生原语。SHA-256 单独裁剪；SHA-384 与 SHA-512 共享一个裁剪单元和派生循环。三种入口参数顺序与语义完全一致：

```c
bool xrtPbkdf2Sha256(
	const void* pPassword, size_t iPasswordSize,
	const void* pSalt, size_t iSaltSize,
	uint32 iIterations,
	void* pOutput, size_t iOutputSize
);
bool xrtPbkdf2Sha384(/* 同上 */);
bool xrtPbkdf2Sha512(/* 同上 */);
```

密码和 salt 可以为空，此时长度必须为零；迭代次数和输出长度必须大于零。最大输出为 `UINT32_MAX * HashLen`，因为 PBKDF2 块编号是 32 位大端整数。输出不得与密码或 salt 重叠，所有参数、范围和块上限错误都在首次写入输出之前返回。派生过程不分配堆内存，并在返回前安全清除 HMAC 状态、中间 `U` 值和块累加值。

实现只预处理一次密码密钥，也只向基础 HMAC 状态追加一次 salt；每轮通过复制预计算状态避免重新构造 ipad/opad。工作因子是应用策略，xrt 不内置随时间变化的默认值。持久化密码时，应用必须为每条记录生成不可预测且独立的 salt，并连同算法、迭代次数和派生结果一起保存。

PBKDF2 不是内存困难型密码哈希。新系统若面对离线口令猜测威胁，应优先评估 Argon2id 或 scrypt；xrt 的 PBKDF2 用于互操作、已有格式和明确选择 PBKDF2 的部署。该原语不负责生成 salt、不定义文本封装，也不隐式擦除调用方提供的密码或输出。

完整示例位于 `examples/crypto/pbkdf2_sha256/main.c` 和 `examples/crypto/pbkdf2_sha512/main.c`。

## HKDF

HKDF 公开由浅入深的三层入口：`Extract` 从 salt 和 IKM 生成固定摘要长度 PRK；`Expand` 从 PRK 和 info 生成任意合规长度 OKM；组合函数在栈上保存 PRK，一次完成两个阶段。SHA-256、SHA-384 和 SHA-512 分别使用 `xrtHkdfSha256...`、`xrtHkdfSha384...`、`xrtHkdfSha512...`。

```c
bool xrtHkdfSha256Extract(
	const void* pSalt, size_t iSaltSize,
	const void* pIkm, size_t iIkmSize,
	void* pPrk
);
bool xrtHkdfSha256Expand(
	const void* pPrk, size_t iPrkSize,
	const void* pInfo, size_t iInfoSize,
	void* pOkm, size_t iOkmSize
);
bool xrtHkdfSha256(
	const void* pSalt, size_t iSaltSize,
	const void* pIkm, size_t iIkmSize,
	const void* pInfo, size_t iInfoSize,
	void* pOkm, size_t iOkmSize
);
```

SHA-384 与 SHA-512 的对应入口完整列于下表，参数顺序与 SHA-256 完全相同，PRK 长度分别为 48 和 64 字节：

| 摘要 | Extract | Expand | 组合入口 |
| --- | --- | --- | --- |
| SHA-384 | `xrtHkdfSha384Extract` | `xrtHkdfSha384Expand` | `xrtHkdfSha384` |
| SHA-512 | `xrtHkdfSha512Extract` | `xrtHkdfSha512Expand` | `xrtHkdfSha512` |

空 salt 按 RFC 5869 解释为一段 `HashLen` 的零字节；空 IKM、info 和 OKM 合法，长度为零时对应指针可以为 `NULL`。Expand 最大输出是 `255 * HashLen`，再多一个字节会设置 `XERR_RANGE` 且不写输出。

Expand 在循环前只处理一次 PRK，并复制预计算 HMAC 状态生成每个块。OKM 可以与 PRK 重叠；OKM 不得与 info 重叠，因为前序输出会改变后续块仍需读取的 info，此时函数返回 `XERR_ARGUMENT`。组合入口允许 OKM 与 salt 或 IKM 重叠，因为 Extract 在输出开始前已完成。

TLS 1.3 的 HKDF-Expand-Label 属于 TLS 编码层，将在 TLS 模块中基于这里的 Expand 构建，不污染通用 HKDF API。

完整示例位于 `examples/crypto/hkdf_sha256/main.c` 和 `examples/crypto/hkdf_sha512/main.c`。

## ChaCha20

`XRT_FEATURE_CRYPTO_CHACHA20` 是独立的低层流密码裁剪单元，只依赖 `CRYPTO_CORE`。固定使用 32 字节密钥和 RFC 8439 的 12 字节 IETF nonce：

```c
#define XRT_CHACHA20_KEY_SIZE 32u
#define XRT_CHACHA20_NONCE_SIZE 12u
#define XRT_CHACHA20_BLOCK_SIZE 64u

bool xrtChaCha20(
	const void* pKey,
	const void* pNonce,
	uint32 iCounter,
	const void* pInput,
	void* pOutput,
	size_t iSize
);
```

输入与输出可以同起点原位变换，也可以完全分离；其他部分重叠以及输出覆盖 key/nonce 会设置 `XERR_ARGUMENT`。空变换允许输入和输出为 `NULL`，但固定 key/nonce 仍必须存在。

函数在写输出前计算需要的 64 字节块数。若从 `iCounter` 开始会越过 `UINT32_MAX`，返回 `XERR_RANGE` 且输出保持不变。旧版实现会让计数器静默回绕，可能复用密钥流；这一行为已经替换。

裸 ChaCha20 只提供保密性，不验证篡改。普通消息、网络记录和文件块应优先使用下面的 ChaCha20-Poly1305；只有实现已有协议或构建更高层原语时才直接调用本入口。

完整示例位于 `examples/crypto/chacha20/main.c`。

## Poly1305

`XRT_FEATURE_CRYPTO_POLY1305` 同样独立裁剪，公开流式底层与一行便利函数：

```c
#define XRT_POLY1305_KEY_SIZE 32u
#define XRT_POLY1305_TAG_SIZE 16u
#define XRT_POLY1305_BLOCK_SIZE 16u

bool xrtPoly1305Init(xpoly1305* pState, const void* pKey);
bool xrtPoly1305Update(xpoly1305* pState, const void* pData, size_t iSize);
bool xrtPoly1305Final(const xpoly1305* pState, void* pTag);
bool xrtPoly1305(
	const void* pKey,
	const void* pData,
	size_t iSize,
	void* pTag
);
```

状态不分配内存，完整 16 字节块直接从调用方输入处理。`Final` 在快照上约减并加 pad，因此可以重复取得标签，也可以继续 `Update`。损坏 guard、非法尾部、状态与输入/标签重叠都会在修改状态或输出前失败。

Poly1305 的 32 字节 key 是一次性密钥；同一 key 用于两条不同消息会破坏安全性。通用业务不应自行管理这个约束，ChaCha20-Poly1305 会按 nonce 从 ChaCha20 counter 0 自动生成一次性 key。

流式和一次性入口的固定向量示例位于 `examples/crypto/poly1305/main.c`。

## ChaCha20-Poly1305

`XRT_FEATURE_CRYPTO_CHACHA20_POLY1305` 依赖 `CRYPTO_CHACHA20` 与 `CRYPTO_POLY1305`，实现 [RFC 8439](https://www.rfc-editor.org/rfc/rfc8439.html) 的 32 字节 key、12 字节 nonce、16 字节 tag 形式。协议层可以选择分离标签，常见封包可以直接使用连续便利层：

```c
#define XRT_CHACHA20_POLY1305_KEY_SIZE 32u
#define XRT_CHACHA20_POLY1305_NONCE_SIZE 12u
#define XRT_CHACHA20_POLY1305_TAG_SIZE 16u

bool xrtChaCha20Poly1305Encrypt(
	const void* pKey, const void* pNonce,
	const void* pAad, size_t iAadSize,
	const void* pPlain, size_t iPlainSize,
	void* pCipher, void* pTag
);
bool xrtChaCha20Poly1305Decrypt(
	const void* pKey, const void* pNonce,
	const void* pAad, size_t iAadSize,
	const void* pCipher, size_t iCipherSize,
	const void* pTag, void* pPlain
);

bool xrtChaCha20Poly1305Seal(
	const void* pKey, const void* pNonce,
	const void* pAad, size_t iAadSize,
	const void* pPlain, size_t iPlainSize,
	void* pOutput, size_t iOutputSize
);
bool xrtChaCha20Poly1305Open(
	const void* pKey, const void* pNonce,
	const void* pAad, size_t iAadSize,
	const void* pInput, size_t iInputSize,
	void* pPlain, size_t iPlainSize
);
```

`Encrypt/Decrypt` 适合 TLS record 等已经分别管理密文和 tag 的协议。`Seal` 输出 `cipher || tag`，容量至少是 `iPlainSize + XRT_CHACHA20_POLY1305_OVERHEAD`；`Open` 接收同一布局，明文容量至少是 `iInputSize - 16`。容量不足设置 `XERR_RANGE`，长度不可能相加时同样在写入前失败。

两种路径都允许明文与密文同起点原位操作。输出与 key、nonce、AAD、tag 的重叠，以及输入输出不同起点的部分重叠均设置 `XERR_ARGUMENT`。解密总是先计算并以常量时间比较 tag，再写明文；认证失败设置 `xrt.crypto` / `XCRYPTO_ERROR_AUTHENTICATION` / `XERR_PROTOCOL`，输出保持逐字节不变。

同一 key 下的 nonce 必须唯一。随机 nonce 只有 96 位，单靠无状态随机抽取不能提供无限期不重复保证；长连接和协议实现应使用连接随机前缀加单调序号，或由协议本身定义 nonce 派生。RFC 形式从 counter 1 加密，单条消息上限是 `XRT_CHACHA20_POLY1305_MAX_SIZE`，即 `(2^32 - 1) * 64` 字节，超过时返回 `XERR_RANGE`。

旧版 ChaCha20 核心轮与 Poly1305 26 位 limb 算法作为成熟资产保留；公开 API、计数器边界、参数检查、密钥清理、结构化认证错误和测试体系均已替换。旧版只支持把 tag 追加在密文后的单一路径，新版同时公开 detached 与 packed 层，不保留兼容版本。

完整示例位于 `examples/crypto/chacha20_poly1305/main.c`。

## AES

`XRT_FEATURE_CRYPTO_AES` 只依赖 `CRYPTO_CORE`，提供调用方持有、无动态分配的 AES-128、AES-192 与 AES-256 正向轮密钥状态：

```c
#define XRT_AES_BLOCK_SIZE 16u
#define XRT_AES128_KEY_SIZE 16u
#define XRT_AES192_KEY_SIZE 24u
#define XRT_AES256_KEY_SIZE 32u
#define XRT_AES_MAX_ROUND_KEY_SIZE 240u

bool xrtAesInit(xaes* pState, const void* pKey, size_t iKeySize);
void xrtAesClear(xaes* pState);
bool xrtAesEncrypt(const xaes* pState, const void* pInput, void* pOutput);
bool xrtAesDecrypt(const xaes* pState, const void* pInput, void* pOutput);
```

实现按进程缓存一次运行时指令能力，并在密钥初始化时把后端写入状态：x86 选择
AES-NI，AES-GCM 同时选择 PCLMUL；AArch64 选择 ARMv8 AES，AES-GCM 同时选择
PMULL。ARM AES 批量路径一次推进四个块，PMULL GHASH 在同一目标函数内完成整段
输入，并使用三次 128 位 Karatsuba 乘法组成一个域乘法，避免逐块特性分派和字节序
往返。不支持这些指令的环境使用 Boyar-Peralta S-box 电路和四块 64 位位切片 AES，
以及常量时间算术 GHASH。所有路径都不使用密钥相关查表或密钥相关可变分支；后端选择
不增加公开函数或函数指针适配层。状态可以只读并发使用；初始化和清除不能与同一状态
上的加解密并发。`Encrypt/Decrypt` 只处理一个完整块，允许输入输出完全相同，拒绝
部分重叠以及对轮密钥状态的覆盖。参数、状态或重叠检查失败时不修改输出。

低级 AES 块接口用于构建标准模式或兼容既有协议，不提供认证。业务消息、网络记录和文件块应优先使用 AES-GCM 或 ChaCha20-Poly1305。密钥使用结束后调用 `xrtAesClear`，它会使用不可优化删除的写入清除完整状态。

AES-128 块加密、原位解密和状态清理示例位于 `examples/crypto/aes/main.c`。

## AES-GCM 与 GMAC

`XRT_FEATURE_CRYPTO_AES_GCM` 依赖 `CRYPTO_AES`。`xaesgcm` 在初始化时固定绑定一个 AES-128/192/256 密钥和一个标签长度，避免每条消息重新扩展密钥，也符合每个密钥固定标签长度的协议契约：

```c
#define XRT_AES_GCM_TAG_MIN_SIZE 4u
#define XRT_AES_GCM_TAG_MAX_SIZE 16u
#define XRT_AES_GCM_TAG_DEFAULT_SIZE 16u
#define XRT_AES_GCM_NONCE_DEFAULT_SIZE 12u

bool xrtAesGcmInit(
	xaesgcm* pState,
	const void* pKey,
	size_t iKeySize,
	size_t iTagSize
);
void xrtAesGcmClear(xaesgcm* pState);
size_t xrtAesGcmTagSize(const xaesgcm* pState);

bool xrtAesGcmEncrypt(
	const xaesgcm* pState,
	const void* pNonce, size_t iNonceSize,
	const void* pAad, size_t iAadSize,
	const void* pPlain, size_t iPlainSize,
	void* pCipher, void* pTag
);
bool xrtAesGcmDecrypt(
	const xaesgcm* pState,
	const void* pNonce, size_t iNonceSize,
	const void* pAad, size_t iAadSize,
	const void* pCipher, size_t iCipherSize,
	const void* pTag, void* pPlain
);

bool xrtAesGcmSeal(
	const xaesgcm* pState,
	const void* pNonce, size_t iNonceSize,
	const void* pAad, size_t iAadSize,
	const void* pPlain, size_t iPlainSize,
	void* pOutput, size_t iOutputSize
);
bool xrtAesGcmOpen(
	const xaesgcm* pState,
	const void* pNonce, size_t iNonceSize,
	const void* pAad, size_t iAadSize,
	const void* pInput, size_t iInputSize,
	void* pPlain, size_t iPlainSize
);
bool xrtAesGmac(
	const xaesgcm* pState,
	const void* pNonce, size_t iNonceSize,
	const void* pData, size_t iSize,
	void* pTag
);
bool xrtAesGmacVerify(
	const xaesgcm* pState,
	const void* pNonce, size_t iNonceSize,
	const void* pData, size_t iSize,
	const void* pTag
);
```

标签长度只接受 NIST SP 800-38D 定义的 `4、8、12、13、14、15、16` 字节。32 位和 64 位短标签有额外的单包长度、失败次数与密钥生命周期限制，通用库无法替协议维护这些状态；使用 4 或 8 字节标签的协议必须执行 SP 800-38D Appendix C 的限制。常规新协议应固定使用 16 字节标签。

96 位 IV 走不需要 GHASH 的标准快路径，也是推荐值；为了协议扩展性，接口接受任意非空、bit length 可由 64 位表示的字节 IV。无论长度如何，同一密钥下 IV 都必须唯一。库不隐式生成 IV，因为无状态随机生成无法替调用方保证整个密钥生命周期内不重复。

`Encrypt/Decrypt` 使用分离的密文和标签，适合 TLS record 等协议；`Seal/Open` 使用 `cipher || tag`，并显式接收输出容量。两条路径都允许明文和密文完全原位，拒绝部分重叠、输出覆盖状态/nonce/AAD，以及标签与任一输入输出重叠。单条消息最大明文为 `XRT_AES_GCM_MAX_SIZE`，即 `2^36 - 32` 字节；AAD 与 IV 的 bit length 不得超过 64 位。

解密先对 AAD 与密文计算 GHASH，再以常量时间验证标签，成功后才执行 GCTR 并写明文。认证失败设置 `XERR_PROTOCOL`、domain `xrt.crypto`、code `XCRYPTO_ERROR_AUTHENTICATION`，输出逐字节保持不变。`GMAC` 和 `GmacVerify` 复用同一条空明文 GCM 路径，不维护第二份认证实现。

旧版 AES-128/256、通用 IV、空消息和 packed 输出能力均已保留；旧版全局 T 表懒初始化、缓存相关查表、缺少参数边界和验签前写明文的实现被替换。完整示例位于 `examples/crypto/aes_gcm/main.c`。

## X25519

X25519 按算法与熵源拆成两个裁剪单元。`XRT_FEATURE_CRYPTO_X25519` 只依赖 `CRYPTO_CORE`，适合 TLS、协议实现和确定性测试；`XRT_FEATURE_CRYPTO_X25519_KEYPAIR` 再依赖 `RANDOM_SECURE`，覆盖常见的一行密钥对生成路径：

```c
#define XRT_X25519_PRIVATE_SIZE 32u
#define XRT_X25519_PUBLIC_SIZE 32u
#define XRT_X25519_SHARED_SIZE 32u

bool xrtX25519(
	const void* pScalar,
	const void* pPoint,
	void* pOutput
);
bool xrtX25519Public(const void* pPrivate, void* pPublic);
bool xrtX25519Shared(
	const void* pPrivate,
	const void* pPeerPublic,
	void* pShared
);
bool xrtX25519KeyPair(void* pPrivate, void* pPublic);
```

`xrtX25519` 是 [RFC 7748](https://www.rfc-editor.org/rfc/rfc7748.html) 的底层原语：内部钳制 scalar，并忽略输入 u-coordinate 最后一个字节的最高位。三个 32 字节区间可以任意重叠，函数会先快照输入再发布结果。它允许并返回全零结果，因为底层协议实现可能需要自行决定验证策略。

`xrtX25519Public` 用标准基点 9 导出公钥，允许完全原位覆盖私钥。`xrtX25519Shared` 用于实际密钥协商，以常量时间检查并拒绝低阶对端公钥产生的全零共享秘密；失败时返回 `false`，设置 `XERR_PROTOCOL` / `xrt.crypto` / `XCRYPTO_ERROR_KEY_AGREEMENT`，并保持调用方输出逐字节不变。协议层应使用 `Shared`，除非它确实需要接管低阶点策略。

`xrtX25519KeyPair` 从操作系统密码安全随机源生成私钥，显式保存规范化的钳制位并导出对应公钥。两个输出各为 32 字节且不得重叠；随机源或公钥派生失败时不会发布半个密钥对。算法入口不分配内存、不使用共享可变状态，可以并发调用；输入和输出缓冲的生命周期由调用方管理。

X25519 输出只是密钥材料，不应直接作为 AES、ChaCha20 或业务密钥。协议必须把共享秘密连同双方身份、握手 transcript 或协议定义的上下文交给 HKDF 等 KDF；X25519 本身不认证对端，也不阻止中间人攻击。私钥和不再使用的共享秘密应调用 `xrtSecureZero` 清除。

旧版 Mike Hamburg / STROBE 算术内核作为成熟资产保留，并补上 RFC 独立向量、Alice/Bob、1000 次迭代、输入最高位屏蔽、低阶点拒绝、任意缓冲重叠和失败原子性测试。旧版随机失败不可见、共享秘密不验证、API 参数顺序不统一和依赖实现定义有符号右移的部分已经替换。完整示例位于 `examples/crypto/x25519/main.c`。

## X448

X448 与 X25519 使用相同的四层 API 结构，但采用 56 字节标量、u-coordinate 和共享秘密。算法层与随机密钥对层分别由 `XRT_FEATURE_CRYPTO_X448` 和 `XRT_FEATURE_CRYPTO_X448_KEYPAIR` 裁剪：

```c
#define XRT_X448_PRIVATE_SIZE 56u
#define XRT_X448_PUBLIC_SIZE 56u
#define XRT_X448_SHARED_SIZE 56u

bool xrtX448(
	const void* pScalar,
	const void* pPoint,
	void* pOutput
);
bool xrtX448Public(const void* pPrivate, void* pPublic);
bool xrtX448Shared(
	const void* pPrivate,
	const void* pPeerPublic,
	void* pShared
);
bool xrtX448KeyPair(void* pPrivate, void* pPublic);
```

`xrtX448` 按 [RFC 7748](https://www.rfc-editor.org/rfc/rfc7748.html) 钳制 scalar 的最低两位并设置最高位。X448 不屏蔽 u-coordinate 的最高位；所有 56 字节输入都按规范先对 `2^448 - 2^224 - 1` 归约，因此接受 RFC 要求的非规范编码。raw 入口允许并返回全零结果，三个固定长度缓冲可任意重叠。

`xrtX448Public` 以标准基点 5 派生公钥，并允许原位覆盖私钥。`xrtX448Shared` 是协议默认入口：它以常量时间汇总结果并拒绝低阶对端公钥产生的全零共享秘密，返回 `XERR_PROTOCOL` / `xrt.crypto` / `XCRYPTO_ERROR_KEY_AGREEMENT`，失败时保持输出不变。`xrtX448KeyPair` 使用系统安全随机源，显式保存钳制后的私钥，只在私钥与公钥都生成成功后发布两个互不重叠的输出。

算法实现不分配内存、不使用共享可变状态，可并发调用。域运算保留旧版 14 x 32 位乘法和 Montgomery ladder，但替换了按秘密数据决定次数的规范化循环及容易产生无符号下溢传播错误的减法；归约、交换和标量路径均使用固定轮次。测试覆盖 RFC 两组独立向量、Alice/Bob、1000 次迭代、非规范输入、低阶点、任意重叠、失败原子性、随机密钥对、裁剪和单头文件。

X448 提供约 224 位经典安全强度，代价明显高于 X25519。协议协商应按互操作和安全策略选择，而不是把 X448 输出直接当作业务密钥；共享秘密仍需绑定双方身份、握手 transcript 或协议上下文后交给 HKDF。完整示例位于 `examples/crypto/x448/main.c`。

## P-256 与 P-384

两条 NIST 素数曲线采用同一组从浅到深的公开入口。`CRYPTO_P256` 和 `CRYPTO_P384` 只携带确定性点运算与 ECDH；随机密钥对分别由 `CRYPTO_P256_KEYPAIR` 和 `CRYPTO_P384_KEYPAIR` 引入。公共点固定使用 SEC 1 未压缩格式 `0x04 || X || Y`，不接受压缩、混合或无穷远点编码。

```c
#define XRT_P256_PRIVATE_SIZE 32u
#define XRT_P256_PUBLIC_SIZE 65u
#define XRT_P256_SHARED_SIZE 32u

bool xrtP256Valid(const void* pPublic);
bool xrtP256Multiply(const void* pScalar, const void* pPoint, void* pOutput);
bool xrtP256Add(const void* pLeft, const void* pRight, void* pOutput);
bool xrtP256Public(const void* pPrivate, void* pPublic);
bool xrtP256Shared(
	const void* pPrivate,
	const void* pPeerPublic,
	void* pShared
);
bool xrtP256KeyPair(void* pPrivate, void* pPublic);

#define XRT_P384_PRIVATE_SIZE 48u
#define XRT_P384_PUBLIC_SIZE 97u
#define XRT_P384_SHARED_SIZE 48u

bool xrtP384Valid(const void* pPublic);
bool xrtP384Multiply(const void* pScalar, const void* pPoint, void* pOutput);
bool xrtP384Add(const void* pLeft, const void* pRight, void* pOutput);
bool xrtP384Public(const void* pPrivate, void* pPublic);
bool xrtP384Shared(
	const void* pPrivate,
	const void* pPeerPublic,
	void* pShared
);
bool xrtP384KeyPair(void* pPrivate, void* pPublic);
```

P-384 与 P-256 保持完全对称的契约。私钥和点乘标量都必须严格处于 `[1, n)`；公共点会检查前缀、坐标小于域模数以及曲线方程，不能把任意 65/97 字节内容带入 ECDH。

`Valid` 是无副作用的布尔检查：空指针设置 `XERR_ARGUMENT`，格式错误或不在曲线上的数据只返回 `false`。`Multiply` 是底层逃生口，可计算有效标量与任意有效公共点的乘积；`Add` 提供公共点加法，并在和为无穷远点时失败。两者均先快照输入再发布结果，输入和输出可完全或部分重叠。无效标量、无效点或无穷远结果设置 `XERR_PROTOCOL` / `xrt.crypto` / `XCRYPTO_ERROR_KEY`，失败时输出不变。

`Public` 是常用的 `private * G` 路径；`Shared` 在完成私钥和对端公钥验证后返回 ECDH 结果点的定长横坐标。协商失败设置 `XCRYPTO_ERROR_KEY_AGREEMENT`，不会用全零或部分结果覆盖输出。ECDH 输出只是密钥材料，协议仍须把双方身份、算法选择和握手 transcript 绑定进 HKDF 等 KDF；函数本身不提供对端身份认证。

`KeyPair` 从操作系统密码安全随机源按曲线群阶拒绝采样，只在私钥和公钥都完成后原子发布。两个输出不得重叠；随机源失败、参数错误或派生失败都不会发布半个密钥对。确定性曲线运算不分配堆内存、不维护共享可变状态，可并发调用；私钥、临时 Jacobian 点和中间标量在返回前安全清零。

旧版 P-256/P-384 的 API 场景、固定尺寸、未压缩点格式、双向 ECDH 和已有 `1*G` / `2*G` 向量作为历史资产保留。实现层已替换私钥位分支、未验证对端点、随机失败不可见和 P-384 每次运算构造堆大整数上下文的路径。新测试增加严格标量边界、非法点、结构化错误、结果原子性、任意缓冲重叠、单头文件、TinyCC/x86 以及对 OpenSSL 的随机差分。完整示例位于 `examples/crypto/p256/main.c` 和 `examples/crypto/p384/main.c`。

## ECDSA DER 表示层

`XRT_FEATURE_CRYPTO_ECDSA_DER` 只依赖密码核心和共享 ECDSA 错误层，不绑定具体曲线、摘要或签名实现。协议解析器可以独立使用它，在固定宽度 `raw r || s` 与 ASN.1 DER `SEQUENCE(INTEGER r, INTEGER s)` 之间转换：

```c
bool xrtEcdsaDerEncode(
	const void* pRaw,
	size_t iScalarSize,
	void* pDer,
	size_t iCapacity,
	size_t* pSize
);
bool xrtEcdsaDerDecode(
	const void* pDer,
	size_t iDerSize,
	void* pRaw,
	size_t iScalarSize
);
```

编码器接受 `1..66` 字节的定宽标量。`pDer == NULL` 且容量为零时只查询精确长度；容量不足返回 `XERR_RANGE`，通过 `pSize` 返回所需长度，目标缓冲保持不变。编解码都先在局部缓冲完成，因此签名输入与字节输出可以任意重叠；`pSize` 是独立的标量输出，与输入或输出字节区间重叠时返回 `XERR_ARGUMENT`。

解码器只接受规范 DER：长度必须使用最短形式，两个 INTEGER 必须为非负数，不允许冗余前导零、空整数、超宽整数、缺失字段或尾随数据。格式错误返回 `XERR_PROTOCOL` / `xrt.crypto` / `XCRYPTO_ERROR_SIGNATURE`，并保持 raw 输出逐字节不变。表示层允许 `r` 或 `s` 为零；具体曲线的签名验证层负责执行 `[1, n)` 范围检查，这样 DER 工具不会重复或猜测曲线规则。

## ECDSA 验签

验签 API 不猜测曲线、摘要算法或签名表示。调用方显式选择曲线并传入已经计算好的非空摘要；P-256 使用 64 字节 `r || s`，P-384 使用 96 字节 `r || s`：

```c
#define XRT_ECDSA_P256_SIGNATURE_SIZE 64u
#define XRT_ECDSA_P384_SIGNATURE_SIZE 96u

bool xrtEcdsaP256Verify(
	const void* pHash,
	size_t iHashSize,
	const void* pSignature,
	const void* pPublic
);
bool xrtEcdsaP384Verify(
	const void* pHash,
	size_t iHashSize,
	const void* pSignature,
	const void* pPublic
);
```

摘要按 ECDSA `bits2int` 规则解释：长于群阶位数时只取左侧位，较短时按大端整数左补零。因此 SHA-224、SHA-256、SHA-384 和 SHA-512 可以与 P-256/P-384 按协议要求组合，X.509 不会被错误限制为曲线同宽摘要。零长度摘要返回 `XERR_ARGUMENT`。

公开点分别使用 65/97 字节 SEC 1 未压缩格式，并在计算前完成前缀、坐标范围和曲线方程验证。`r` 与 `s` 必须严格处于 `[1, n)`；验证接受数学上有效的 high-S 签名，以保持与通用 ECDSA 标准和现有证书生态互操作。后续签名 API 统一产生 low-S，协议若要求唯一签名表示，可以在解析策略层额外拒绝 high-S。

`XRT_FEATURE_CRYPTO_ECDSA_P256_DER` 和 `XRT_FEATURE_CRYPTO_ECDSA_P384_DER` 提供 `xrtEcdsaP256VerifyDer` 与 `xrtEcdsaP384VerifyDer`。它们严格复用 DER 表示层，再进入同一 raw 验签核心；最大 DER 容量分别是 `72` 和 `104` 字节。格式错误保留 `ecdsa-der-decode` 操作名，数学验证失败使用 `ecdsa-p256-verify` 或 `ecdsa-p384-verify`，调用方可以准确区分输入编码损坏和签名不匹配。

对应容量常量是 `XRT_ECDSA_P256_DER_MAX_SIZE` 与 `XRT_ECDSA_P384_DER_MAX_SIZE`；它们是最坏情况容量，不是每个签名的实际编码长度。

所有验证失败返回 `XERR_PROTOCOL` / `xrt.crypto` / `XCRYPTO_ERROR_SIGNATURE`，空参数返回 `XERR_ARGUMENT`。实现使用固定轨迹群阶求逆、共享 NIST 双标量点运算和常量时间最终比较，不分配堆内存，也不维护共享可变状态。旧版按公钥长度猜测 P-256/P-384、内嵌宽松 DER 解析和 P-384 每次创建堆大整数上下文的接口已经替换；可变摘要能力经过修订后保留为显式长度契约。新测试使用独立密码实现生成的有效签名，并覆盖短摘要、长摘要截断、零摘要、群阶边界、非法点、损坏签名、严格 DER、错误契约、裁剪和单头文件。

## ECDSA 签名

签名入口显式接收摘要算法，并直接输出定宽 raw 签名：

```c
bool xrtEcdsaP256Sign(
	xcryptohash Hash,
	const void* pHash,
	const void* pPrivate,
	void* pSignature
);
bool xrtEcdsaP384Sign(
	xcryptohash Hash,
	const void* pHash,
	const void* pPrivate,
	void* pSignature
);
```

`Hash` 同时声明 `pHash` 的精确长度和 RFC 6979 使用的 HMAC。当前签名层支持 SHA-256、SHA-384 与 SHA-512；P-256/SHA-384、P-256/SHA-512 和 P-384/SHA-256 都按标准 `bits2octets` 与多块 `T` 生成规则执行，不把摘要宽度错误地等同于曲线群阶宽度。相同私钥、算法和摘要总是产生相同签名，不依赖全局随机源，也不会出现旧版忽略随机数失败或在固定次数随机拒绝后无原因失败的问题。

常见裁剪保持轻量：`crypto_ecdsa_p256_sign` 默认只带 HMAC-SHA256，`crypto_ecdsa_p384_sign` 默认带共享的 HMAC-SHA384/512；调用未编入的摘要算法返回 `XERR_UNSUPPORTED` 且不修改输出。需要 TLS 1.2 完整交叉组合时同时启用 HMAC-SHA256 和 HMAC-SHA384/512，TLS 身份模块已经声明这项依赖。确定性 nonce 不会降低私钥唯一性要求；调用方仍必须保护私钥，并避免把未经标准摘要处理的可控字节误当作摘要。

输出统一规范化为 low-S。验签端仍接受 high-S 以保证互操作，而签名端不产生可延展的第二种等价表示。私钥必须严格处于 `[1, n)`；无效私钥返回 `XCRYPTO_ERROR_KEY`，内部点派生或签名生成失败返回 `XCRYPTO_ERROR_SIGNATURE`。raw 输出只在全部计算完成后发布，并允许摘要、私钥和输出缓冲任意重叠；全部私钥、nonce、HMAC 状态、中间标量与临时点在返回前安全清零。

`xrtEcdsaP256SignDer` 和 `xrtEcdsaP384SignDer` 在同一条 raw 签名路径后调用严格 DER 表示层：

```c
bool xrtEcdsaP256SignDer(
	xcryptohash Hash,
	const void* pHash,
	const void* pPrivate,
	void* pDer,
	size_t iCapacity,
	size_t* pSize
);
```

P-384 提供完全对称的入口。`pDer == NULL` 且容量为零可查询这一次确定性签名的精确编码长度；容量不足返回 `XERR_RANGE`、通过 `pSize` 返回所需长度且不修改目标。`pSize` 与 DER 输出重叠时返回 `XERR_ARGUMENT`。示例位于 `examples/crypto/ecdsa_p256/main.c` 与 `examples/crypto/ecdsa_p384/main.c`。

固定向量与 RFC 6979 确定性签名在 low-S 规范化后逐字节一致。专项门禁覆盖 P-256/SHA-384、P-256/SHA-512、P-384/SHA-256 和 P-384/SHA-512，其中 P-384/SHA-256 明确验证需要拼接两个 HMAC 输出块的路径；同时检查 `s <= n/2`、XRT 自验和单头文件。旧版已有曲线场景和签名用途作为资产保留，随机 nonce、宽松 DER、重复 P-256/P-384 大整数实现、堆上下文和模糊的单一入口均被替换。

## RSA 公钥、私钥与签名

RSA 按原始公钥运算、PSS 验签和 PKCS#1 v1.5 验签拆成三个独立裁剪单元。公钥使用不拥有内存的只读视图，模数和指数均为无前导零的大端正整数：

```c
#define XRT_RSA_MODULUS_MIN_SIZE 128u
#define XRT_RSA_MODULUS_MAX_SIZE 1024u

typedef struct xrsa_public_key {
	const void* Modulus;
	size_t ModulusSize;
	const void* Exponent;
	size_t ExponentSize;
} xrsapublickey;

bool xrtRsaPublic(
	const xrsapublickey* pKey,
	const void* pInput,
	size_t iInputSize,
	void* pOutput
);
```

`xrtRsaPublic` 接受 128 到 1024 字节的奇数模数和大于一的奇数指数，覆盖 1024 到 8192 位 RSA。输入和输出长度固定等于模数长度；输入必须严格小于模数。函数先快照为 `int31` 表示，使用固定指数位数轨迹的 Montgomery 模幂，在结果完整生成后才发布输出，因此输入、密钥字节和输出可任意重叠，任何失败都保持输出不变。算法不分配堆内存，不维护共享可变状态。

私钥视图保留 PKCS#1 中真正参与运算的公共部分、完整私有指数和 CRT 五参数：

```c
typedef struct xrsa_private_key {
	xrsapublickey Public;
	const void* PrivateExponent;
	size_t PrivateExponentSize;
	const void* Prime1;
	size_t Prime1Size;
	const void* Prime2;
	size_t Prime2Size;
	const void* Exponent1;
	size_t Exponent1Size;
	const void* Exponent2;
	size_t Exponent2Size;
	const void* Coefficient;
	size_t CoefficientSize;
} xrsaprivatekey;

bool xrtRsaPrivate(
	const xrsaprivatekey* pKey,
	const void* pInput,
	size_t iInputSize,
	void* pOutput
);
```

`xrsaprivatekey` 不拥有任何字节。CRT 五参数必须全部存在或全部省略；完整 CRT 时使用 `m1 = m^dP mod p`、`m2 = m^dQ mod q` 与 `qInv` 重组，完全省略 CRT 时使用 `PrivateExponent` 的固定轨迹模幂，部分 CRT 一律拒绝。CRT 只接受标准平衡双素数密钥，逐次确认 `p * q == n`，完成后再执行 `result^e mod n == input` 的常量时间复核，防止参数错配和计算故障发布错误签名。两条路径都不分配堆内存、都允许输入/输出/密钥字节重叠；原始私钥运算不包含填充，只应作为 OAEP、PSS、PKCS#1 等协议构建器的底层入口。

PSS 接口显式区分消息摘要与 MGF1 摘要，并由调用方决定是否严格约束盐长度：

```c
bool xrtRsaPssVerify(
	const xrsapublickey* pKey,
	xcryptohash iHash,
	xcryptohash iMaskHash,
	size_t iSaltSize,
	const void* pHash,
	const void* pSignature,
	size_t iSignatureSize
);
```

`iSaltSize` 使用具体值时严格匹配 PSS 编码；使用 `XRT_RSA_PSS_SALT_ANY` 时，从规范的 `PS || 0x01 || salt` 中取得实际盐长。验证支持 SHA-1、SHA-256、SHA-384 和 SHA-512 的 MGF1，允许 X.509 参数显式选择与消息摘要不同的 MGF1 摘要；具体协议仍应执行自己的算法策略，例如 TLS 1.3 只允许 SHA-256/SHA-384 并要求盐长等于摘要长度。实现严格检查 `emBits = modBits - 1`、高位掩码、尾字节 `0xBC`、零填充、分隔符和最终摘要。

```c
bool xrtRsaPssSignSalt(
	const xrsaprivatekey* pKey,
	xcryptohash iHash,
	xcryptohash iMaskHash,
	const void* pSalt,
	size_t iSaltSize,
	const void* pHash,
	void* pSignature
);

bool xrtRsaPssSign(
	const xrsaprivatekey* pKey,
	xcryptohash iHash,
	xcryptohash iMaskHash,
	const void* pHash,
	void* pSignature
);
```

`xrtRsaPssSignSalt` 是协议层入口，允许零长度盐和当前模数容量内的任意盐长度；PSS 的 `M'` 采用流式摘要，因此长盐不会扩大固定栈缓冲。`xrtRsaPssSign` 是 TLS 等常见路径的便利入口，使用与消息摘要等长的操作系统密码安全随机盐。两者都可独立选择 MGF1 摘要，输出固定等于模数宽度，失败不修改签名缓冲。

```c
bool xrtRsaPkcs1Verify(
	const xrsapublickey* pKey,
	xcryptohash iHash,
	const void* pHash,
	const void* pSignature,
	size_t iSignatureSize
);

bool xrtRsaPkcs1Sign(
	const xrsaprivatekey* pKey,
	xcryptohash iHash,
	const void* pHash,
	void* pSignature
);
```

PKCS#1 v1.5 签名和验签共享唯一一份 SHA-1/SHA-256/SHA-384/SHA-512 规范 DER `DigestInfo` 映射。验签要求至少八字节且全部为 `0xFF` 的填充，不接受缺失 `NULL` 参数、缩短填充、尾随数据或仅比较摘要尾部的宽松编码；签名在完整编码形成后调用带复核的私钥路径。密钥问题返回 `XERR_VALUE / xrt.crypto / XCRYPTO_ERROR_KEY`；签名代表、编码和摘要不匹配返回 `XERR_PROTOCOL / xrt.crypto / XCRYPTO_ERROR_SIGNATURE`。

旧版 `lib/crypto.h` 的 RSA 模幂、PSS/PKCS#1 编码和 `nettls.h` 的服务器签名场景均作为资产保留；旧实现只保存完整私有指数、每次签名建立多个堆缓冲、忽略 PKCS#1 已携带的 CRT 参数，也没有计算故障复核。新实现复用已压实的 BearSSL `int31` 常数时间原语，补充固定栈 CRT 重组，并让 PSS、PKCS#1 的签名与验签各自共享唯一编码底座。固定向量来自独立密码实现；随机差分覆盖 1024/2048/3072/4096/8192 位密钥、指数 3/65537、CRT 与完整指数逐字节一致、Python PSS 验签、PKCS#1 签名字节一致和 600 字节 PSS 长盐。GCC、TinyCC x86、单头文件和独立裁剪门禁均为发布前证据；当前多轮实测中，CRT 路径通常约为完整指数路径的 2.5 到 4.3 倍。

原始公私钥运算、显式盐和随机盐 PSS 示例位于 `examples/crypto/rsa_pss/main.c`；PKCS#1 v1.5 签名与验签示例位于 `examples/crypto/rsa_pkcs1/main.c`。两个示例共用的固定 1024 位密钥只用于展示 API，不代表部署安全策略；生产系统应从受信任密钥存储加载符合当前策略的密钥。

## Ed25519

Ed25519 拆成五个边界明确的裁剪单元：

- `XRT_FEATURE_CRYPTO_CURVE25519`：X25519 与 Ed25519 共用的内部模 `2^255 - 19` 有限域，不公开表示相关 API。
- `XRT_FEATURE_CRYPTO_ED25519`：固定栈标量、扩展坐标点、种子展开和公钥派生，依赖 `CURVE25519` 与 SHA-512。
- `XRT_FEATURE_CRYPTO_ED25519_SIGN`：纯 Ed25519、Ed25519ctx 与 Ed25519ph 签名。
- `XRT_FEATURE_CRYPTO_ED25519_VERIFY`：三种模式的严格验签，可以不携带签名入口。
- `XRT_FEATURE_CRYPTO_ED25519_KEYPAIR`：依赖系统安全随机源的种子与公钥生成，可以不携带签名或验签入口。

```c
#define XRT_ED25519_SEED_SIZE 32u
#define XRT_ED25519_PUBLIC_SIZE 32u
#define XRT_ED25519_SIGNATURE_SIZE 64u
#define XRT_ED25519_PREHASH_SIZE 64u
#define XRT_ED25519_CONTEXT_MAX_SIZE 255u

typedef enum xed25519_mode {
	XED25519_PURE = 0,
	XED25519_CONTEXT,
	XED25519_PREHASH
} xed25519mode;

bool xrtEd25519KeyInit(xed25519key* pKey, const void* pSeed);
void xrtEd25519KeyClear(xed25519key* pKey);
bool xrtEd25519Public(const void* pSeed, void* pPublic);
bool xrtEd25519KeyPair(void* pSeed, void* pPublic);
```

`xed25519key` 保存展开后的私有标量、确定性 nonce 前缀和公钥。服务器、签名服务等重复使用同一密钥的路径应初始化一次并调用 `xrtEd25519KeyClear`；一次性调用可以直接使用种子便利函数。`xrtEd25519Public` 允许输出覆盖种子，`xrtEd25519KeyPair` 的两个输出不得重叠。密钥派生与密钥对生成在完整结果形成前不发布输出。

公开结构标签是 `xed25519_key`，常用类型名是 `xed25519key`；字段只为无分配存储公开，调用方不得修改展开状态。

```c
bool xrtEd25519Sign(
	const void* pSeed,
	const void* pMessage,
	size_t iMessageSize,
	void* pSignature
);

bool xrtEd25519SignKey(
	const xed25519key* pKey,
	const void* pMessage,
	size_t iMessageSize,
	void* pSignature
);

bool xrtEd25519SignMode(
	const xed25519key* pKey,
	xed25519mode iMode,
	const void* pContext,
	size_t iContextSize,
	const void* pMessage,
	size_t iMessageSize,
	void* pSignature
);

bool xrtEd25519Verify(
	const void* pPublic,
	const void* pMessage,
	size_t iMessageSize,
	const void* pSignature
);

bool xrtEd25519VerifyMode(
	const void* pPublic,
	xed25519mode iMode,
	const void* pContext,
	size_t iContextSize,
	const void* pMessage,
	size_t iMessageSize,
	const void* pSignature
);
```

`XED25519_PURE` 不携带域前缀，也不接受非空上下文；`XED25519_CONTEXT` 按 RFC 8032 加入最长 255 字节上下文；`XED25519_PREHASH` 要求 `pMessage` 恰为调用方以 SHA-512 得到的 64 字节预哈希，因此流式大消息不需要被库重新缓存。三种模式不可混用。空消息允许使用空指针和零长度。

签名使用固定窗口、常数时间选点的私有基点乘法；512 位标量约简和乘加使用固定栈、固定轨迹整数运算，不再建立旧版临时大整数上下文。签名先在局部缓冲完整形成，因此输出可覆盖种子、展开密钥、消息或上下文，任何参数、长度或摘要失败都保持签名输出不变。

验签拒绝 `S >= L`、非规范 `R`/公钥编码、`x = 0` 却设置符号位、单位元公钥以及不属于阶 `L` 主子群的公钥，然后执行无 cofactor 放宽的 `S * B == R + H(R || A || M) * A`。这使 X.509、TLS 和一般签名调用得到同一套严格语义，不会因协议入口不同而接受不同的可塑签名。

旧版 `lib/crypto.h:4956-5469` 的 25519 字段表示、扩展坐标公式、RFC 签名流程，以及 `nettls.h` 中 Ed25519 证书和握手签名调用点均作为资产保留。新实现把与 X25519 重复的字段算术抽成唯一内部模块，替换了每次约简/乘加都分配堆大整数和私有标量点乘按位分支的实现，并补充单次平方根指数链、可复用展开密钥、RFC 8032 context/prehash 模式与严格子群检查。

测试包括 RFC 8032 的纯、`ctx`、`ph` 固定向量，规范编码、低阶公钥、`S == L`、模式与上下文边界、输出重叠和失败原子性。独立随机差分使用 Python `cryptography`，覆盖 100 组随机种子以及 0 到 1024 字节消息，公钥和确定性签名逐字节一致，并双向验证篡改拒绝。示例分别位于 `examples/crypto/ed25519`、`examples/crypto/ed25519_sign` 和 `examples/crypto/ed25519_verify`。
