# TLS

`tls` 是 XRT 内建 TLS 体系的协议基础层。当前这一层公开稳定的版本、状态、Alert 和记录编解码契约；它不直接持有 socket，也不建立第二套异步模型。

## 类型与常量

### `xtlsresult`

TLS 协议操作把正常控制结果与结构化错误分开表达。

```c
typedef enum xtlsresult {
	XTLS_ERROR = -1,
	XTLS_OK = 0,
	XTLS_AGAIN,
	XTLS_CLOSED
} xtlsresult;
```

| 值 | 语义 |
|---|---|
| `XTLS_ERROR` | 失败 |
| `XTLS_OK` | 成功 |
| `XTLS_AGAIN` | 暂不可推进 |

### `xtlsversion`

XRT 只协商 TLS 1.2 和 TLS 1.3。

```c
typedef enum xtlsversion {
	XTLS_VERSION_12 = 0x0303,
	XTLS_VERSION_13 = 0x0304
} xtlsversion;
```

| 值 | 语义 |
|---|---|
| `XTLS_VERSION_12` | TLS 1.2 |

### `xtlscipher`

XRT 支持的密码套件全部使用 AEAD，TLS 1.2 只保留前向保密套件。

```c
typedef enum xtlscipher {
	XTLS_AES_128_GCM_SHA256 = 0x1301,
	XTLS_AES_256_GCM_SHA384 = 0x1302,
	XTLS_CHACHA20_POLY1305_SHA256 = 0x1303,
	XTLS_ECDHE_ECDSA_AES_128_GCM_SHA256 = 0xC02B,
	XTLS_ECDHE_RSA_AES_128_GCM_SHA256 = 0xC02F,
	XTLS_ECDHE_ECDSA_AES_256_GCM_SHA384 = 0xC02C,
	XTLS_ECDHE_RSA_AES_256_GCM_SHA384 = 0xC030,
	XTLS_ECDHE_RSA_CHACHA20_POLY1305_SHA256 = 0xCCA8,
	XTLS_ECDHE_ECDSA_CHACHA20_POLY1305_SHA256 = 0xCCA9
} xtlscipher;
```

| 值 | 语义 |
|---|---|
| `XTLS_AES_128_GCM_SHA256` | TLS_AES_128_GCM_SHA256 |
| `XTLS_AES_256_GCM_SHA384` | TLS_AES_256_GCM_SHA384 |
| `XTLS_CHACHA20_POLY1305_SHA256` | TLS_CHACHA20_POLY1305_SHA256 |
| `XTLS_ECDHE_ECDSA_AES_128_GCM_SHA256` | ECDHE-ECDSA-AES128-GCM-SHA256 |
| `XTLS_ECDHE_RSA_AES_128_GCM_SHA256` | ECDHE-RSA-AES128-GCM-SHA256 |
| `XTLS_ECDHE_ECDSA_AES_256_GCM_SHA384` | ECDHE-ECDSA-AES256-GCM-SHA384 |
| `XTLS_ECDHE_RSA_AES_256_GCM_SHA384` | ECDHE-RSA-AES256-GCM-SHA384 |
| `XTLS_ECDHE_RSA_CHACHA20_POLY1305_SHA256` | ECDHERSACHACHA20POLY1305SHA256 |

### `xtlshash`

TLS 密码套件使用的摘要算法与密码后端解耦。

```c
typedef enum xtlshash {
	XTLS_HASH_SHA256 = 1,
	XTLS_HASH_SHA384
} xtlshash;
```

| 值 | 语义 |
|---|---|
| `XTLS_HASH_SHA256` | XTLSHASHSHA256 |

### `xtlsaead`

TLS 记录保护当前只保留两类现代 AEAD。

```c
typedef enum xtlsaead {
	XTLS_AEAD_AES_GCM = 1,
	XTLS_AEAD_CHACHA20_POLY1305
} xtlsaead;
```

| 值 | 语义 |
|---|---|
| `XTLS_AEAD_AES_GCM` | XTLSAEADAESGCM |

### `xtlscipherauth`

TLS 1.2 把认证类型编码进套件，TLS 1.3 则与套件独立。

```c
typedef enum xtlscipherauth {
	XTLS_CIPHER_AUTH_INDEPENDENT = 0,
	XTLS_CIPHER_AUTH_RSA,
	XTLS_CIPHER_AUTH_ECDSA
} xtlscipherauth;
```

| 值 | 语义 |
|---|---|
| `XTLS_CIPHER_AUTH_INDEPENDENT` | INDEPENDENT |
| `XTLS_CIPHER_AUTH_RSA` | RSA |

### `xtlsrole`

TLS 角色决定握手状态机的方向。

```c
typedef enum xtlsrole {
	XTLS_CLIENT = 1,
	XTLS_SERVER = 2
} xtlsrole;
```

| 值 | 语义 |
|---|---|
| `XTLS_CLIENT` | XTLS客户端角色 |

### `xtlsstate`

会话状态只描述公开生命周期，不暴露内部握手步骤。

```c
typedef enum xtlsstate {
	XTLS_STATE_NEW = 0,
	XTLS_STATE_HANDSHAKE,
	XTLS_STATE_READY,
	XTLS_STATE_CLOSING,
	XTLS_STATE_CLOSED,
	XTLS_STATE_FAILED
} xtlsstate;
```

| 值 | 语义 |
|---|---|
| `XTLS_STATE_NEW` | 状态非法 |
| `XTLS_STATE_HANDSHAKE` | 握手阶段 |
| `XTLS_STATE_READY` | 就绪 |
| `XTLS_STATE_CLOSING` | 关闭中 |
| `XTLS_STATE_CLOSED` | 已关闭 |

### `xtlsrecordtype`

TLS 记录内容类型使用协议规定的稳定数值。

```c
typedef enum xtlsrecordtype {
	XTLS_RECORD_CHANGE_CIPHER_SPEC = 20,
	XTLS_RECORD_ALERT = 21,
	XTLS_RECORD_HANDSHAKE = 22,
	XTLS_RECORD_APPLICATION_DATA = 23
} xtlsrecordtype;
```

| 值 | 语义 |
|---|---|
| `XTLS_RECORD_CHANGE_CIPHER_SPEC` | CHANGECIPHERSPEC |
| `XTLS_RECORD_ALERT` | ALERT |
| `XTLS_RECORD_HANDSHAKE` | 握手阶段 |

### `xtlshandshaketype`

TLS 1.2 与 TLS 1.3 握手消息类型保留标准线路数值。

```c
typedef enum xtlshandshaketype {
	XTLS_HANDSHAKE_HELLO_REQUEST = 0,
	XTLS_HANDSHAKE_CLIENT_HELLO = 1,
	XTLS_HANDSHAKE_SERVER_HELLO = 2,
	XTLS_HANDSHAKE_NEW_SESSION_TICKET = 4,
	XTLS_HANDSHAKE_END_OF_EARLY_DATA = 5,
	XTLS_HANDSHAKE_ENCRYPTED_EXTENSIONS = 8,
	XTLS_HANDSHAKE_CERTIFICATE = 11,
	XTLS_HANDSHAKE_SERVER_KEY_EXCHANGE = 12,
	XTLS_HANDSHAKE_CERTIFICATE_REQUEST = 13,
	XTLS_HANDSHAKE_SERVER_HELLO_DONE = 14,
	XTLS_HANDSHAKE_CERTIFICATE_VERIFY = 15,
	XTLS_HANDSHAKE_CLIENT_KEY_EXCHANGE = 16,
	XTLS_HANDSHAKE_FINISHED = 20,
	XTLS_HANDSHAKE_CERTIFICATE_STATUS = 22,
	XTLS_HANDSHAKE_SUPPLEMENTAL_DATA = 23,
	XTLS_HANDSHAKE_KEY_UPDATE = 24,
	XTLS_HANDSHAKE_COMPRESSED_CERTIFICATE = 25,
	XTLS_HANDSHAKE_MESSAGE_HASH = 254
} xtlshandshaketype;
```

| 值 | 语义 |
|---|---|
| `XTLS_HANDSHAKE_HELLO_REQUEST` | 握手阶段 |
| `XTLS_HANDSHAKE_CLIENT_HELLO` | 客户端角色HELLO |
| `XTLS_HANDSHAKE_SERVER_HELLO` | 服务端角色HELLO |
| `XTLS_HANDSHAKE_NEW_SESSION_TICKET` | 握手阶段 |
| `XTLS_HANDSHAKE_END_OF_EARLY_DATA` | ENDOFEARLY数据损坏 |
| `XTLS_HANDSHAKE_ENCRYPTED_EXTENSIONS` | 握手阶段 |
| `XTLS_HANDSHAKE_CERTIFICATE` | 握手阶段 |
| `XTLS_HANDSHAKE_SERVER_KEY_EXCHANGE` | 服务端角色KEYEXCHANGE |
| `XTLS_HANDSHAKE_CERTIFICATE_REQUEST` | 握手阶段 |
| `XTLS_HANDSHAKE_SERVER_HELLO_DONE` | 服务端角色HELLO完成 |
| `XTLS_HANDSHAKE_CERTIFICATE_VERIFY` | 握手阶段 |
| `XTLS_HANDSHAKE_CLIENT_KEY_EXCHANGE` | 客户端角色KEYEXCHANGE |
| `XTLS_HANDSHAKE_FINISHED` | 已完成 |
| `XTLS_HANDSHAKE_CERTIFICATE_STATUS` | 握手阶段 |
| `XTLS_HANDSHAKE_SUPPLEMENTAL_DATA` | SUPPLEMENTAL数据损坏 |
| `XTLS_HANDSHAKE_KEY_UPDATE` | 握手阶段 |
| `XTLS_HANDSHAKE_COMPRESSED_CERTIFICATE` | 握手阶段 |

### `xtlsextensiontype`

常用 TLS 扩展类型使用 IANA 分配的线路数值。

```c
typedef enum xtlsextensiontype {
	XTLS_EXTENSION_SERVER_NAME = 0,
	XTLS_EXTENSION_MAX_FRAGMENT_LENGTH = 1,
	XTLS_EXTENSION_STATUS_REQUEST = 5,
	XTLS_EXTENSION_SUPPORTED_GROUPS = 10,
	XTLS_EXTENSION_EC_POINT_FORMATS = 11,
	XTLS_EXTENSION_SIGNATURE_ALGORITHMS = 13,
	XTLS_EXTENSION_USE_SRTP = 14,
	XTLS_EXTENSION_HEARTBEAT = 15,
	XTLS_EXTENSION_ALPN = 16,
	XTLS_EXTENSION_SIGNED_CERTIFICATE_TIMESTAMP = 18,
	XTLS_EXTENSION_CLIENT_CERTIFICATE_TYPE = 19,
	XTLS_EXTENSION_SERVER_CERTIFICATE_TYPE = 20,
	XTLS_EXTENSION_PADDING = 21,
	XTLS_EXTENSION_ENCRYPT_THEN_MAC = 22,
	XTLS_EXTENSION_EXTENDED_MASTER_SECRET = 23,
	XTLS_EXTENSION_COMPRESS_CERTIFICATE = 27,
	XTLS_EXTENSION_RECORD_SIZE_LIMIT = 28,
	XTLS_EXTENSION_SESSION_TICKET = 35,
	XTLS_EXTENSION_PRE_SHARED_KEY = 41,
	XTLS_EXTENSION_EARLY_DATA = 42,
	XTLS_EXTENSION_SUPPORTED_VERSIONS = 43,
	XTLS_EXTENSION_COOKIE = 44,
	XTLS_EXTENSION_PSK_KEY_EXCHANGE_MODES = 45,
	XTLS_EXTENSION_CERTIFICATE_AUTHORITIES = 47,
	XTLS_EXTENSION_OID_FILTERS = 48,
	XTLS_EXTENSION_POST_HANDSHAKE_AUTH = 49,
	XTLS_EXTENSION_SIGNATURE_ALGORITHMS_CERT = 50,
	XTLS_EXTENSION_KEY_SHARE = 51,
	XTLS_EXTENSION_RENEGOTIATION_INFO = 65281
} xtlsextensiontype;
```

| 值 | 语义 |
|---|---|
| `XTLS_EXTENSION_SERVER_NAME` | 服务端角色名称 |
| `XTLS_EXTENSION_MAX_FRAGMENT_LENGTH` | 上限FRAGMENTLENGTH |
| `XTLS_EXTENSION_STATUS_REQUEST` | STATUSREQUEST |
| `XTLS_EXTENSION_SUPPORTED_GROUPS` | SUPPORTEDGROUPS |
| `XTLS_EXTENSION_EC_POINT_FORMATS` | ECPOINTFORMATS |
| `XTLS_EXTENSION_SIGNATURE_ALGORITHMS` | SIGNATUREALGORITHMS |
| `XTLS_EXTENSION_USE_SRTP` | USESRTP |
| `XTLS_EXTENSION_HEARTBEAT` | HEARTBEAT |
| `XTLS_EXTENSION_ALPN` | ALPN |
| `XTLS_EXTENSION_SIGNED_CERTIFICATE_TIMESTAMP` | SIGNEDCERTIFICATETIMESTAMP |
| `XTLS_EXTENSION_CLIENT_CERTIFICATE_TYPE` | 客户端角色CERTIFICATE类型 |
| `XTLS_EXTENSION_SERVER_CERTIFICATE_TYPE` | 服务端角色CERTIFICATE类型 |
| `XTLS_EXTENSION_PADDING` | PADDING |
| `XTLS_EXTENSION_ENCRYPT_THEN_MAC` | ENCRYPTTHENMAC |
| `XTLS_EXTENSION_EXTENDED_MASTER_SECRET` | EXTENDEDMASTERSECRET |
| `XTLS_EXTENSION_COMPRESS_CERTIFICATE` | COMPRESSCERTIFICATE |
| `XTLS_EXTENSION_RECORD_SIZE_LIMIT` | RECORD尺寸超限 |
| `XTLS_EXTENSION_SESSION_TICKET` | SESSIONTICKET |
| `XTLS_EXTENSION_PRE_SHARED_KEY` | PRESHAREDKEY |
| `XTLS_EXTENSION_EARLY_DATA` | EARLY数据损坏 |
| `XTLS_EXTENSION_SUPPORTED_VERSIONS` | SUPPORTEDVERSIONS |
| `XTLS_EXTENSION_COOKIE` | COOKIE |
| `XTLS_EXTENSION_PSK_KEY_EXCHANGE_MODES` | PSKKEYEXCHANGEMODES |
| `XTLS_EXTENSION_CERTIFICATE_AUTHORITIES` | CERTIFICATEAUTHORITIES |
| `XTLS_EXTENSION_OID_FILTERS` | OIDFILTERS |
| `XTLS_EXTENSION_POST_HANDSHAKE_AUTH` | POST握手阶段AUTH |
| `XTLS_EXTENSION_SIGNATURE_ALGORITHMS_CERT` | SIGNATUREALGORITHMSCERT |
| `XTLS_EXTENSION_KEY_SHARE` | KEYSHARE |

### `xtlsnamedgroup`

常用命名组保留 IANA 线路值，未知组仍可由 uint16 视图访问。

```c
typedef enum xtlsnamedgroup {
	XTLS_GROUP_SECP256R1 = 23,
	XTLS_GROUP_SECP384R1 = 24,
	XTLS_GROUP_SECP521R1 = 25,
	XTLS_GROUP_X25519 = 29,
	XTLS_GROUP_X448 = 30,
	XTLS_GROUP_FFDHE2048 = 256,
	XTLS_GROUP_FFDHE3072 = 257,
	XTLS_GROUP_FFDHE4096 = 258,
	XTLS_GROUP_FFDHE6144 = 259,
	XTLS_GROUP_FFDHE8192 = 260
} xtlsnamedgroup;
```

| 值 | 语义 |
|---|---|
| `XTLS_GROUP_SECP256R1` | SECP256R1 |
| `XTLS_GROUP_SECP384R1` | SECP384R1 |
| `XTLS_GROUP_SECP521R1` | SECP521R1 |
| `XTLS_GROUP_X25519` | X25519 |
| `XTLS_GROUP_X448` | X448 |
| `XTLS_GROUP_FFDHE2048` | FFDHE2048 |
| `XTLS_GROUP_FFDHE3072` | FFDHE3072 |
| `XTLS_GROUP_FFDHE4096` | FFDHE4096 |
| `XTLS_GROUP_FFDHE6144` | FFDHE6144 |

### `xtlssignature`

TLS 1.2 与 TLS 1.3 常用签名方案保留 IANA 线路值。

```c
typedef enum xtlssignature {
	XTLS_SIGNATURE_RSA_PKCS1_SHA256 = 0x0401,
	XTLS_SIGNATURE_ECDSA_SECP256R1_SHA256 = 0x0403,
	XTLS_SIGNATURE_RSA_PKCS1_SHA384 = 0x0501,
	XTLS_SIGNATURE_ECDSA_SECP384R1_SHA384 = 0x0503,
	XTLS_SIGNATURE_RSA_PKCS1_SHA512 = 0x0601,
	XTLS_SIGNATURE_ECDSA_SECP521R1_SHA512 = 0x0603,
	XTLS_SIGNATURE_RSA_PSS_RSAE_SHA256 = 0x0804,
	XTLS_SIGNATURE_RSA_PSS_RSAE_SHA384 = 0x0805,
	XTLS_SIGNATURE_RSA_PSS_RSAE_SHA512 = 0x0806,
	XTLS_SIGNATURE_ED25519 = 0x0807,
	XTLS_SIGNATURE_ED448 = 0x0808,
	XTLS_SIGNATURE_RSA_PSS_PSS_SHA256 = 0x0809,
	XTLS_SIGNATURE_RSA_PSS_PSS_SHA384 = 0x080A,
	XTLS_SIGNATURE_RSA_PSS_PSS_SHA512 = 0x080B
} xtlssignature;
```

| 值 | 语义 |
|---|---|
| `XTLS_SIGNATURE_RSA_PKCS1_SHA256` | PKCS#1 v1.5 + SHA-256（TLS 1.2 遗留） |
| `XTLS_SIGNATURE_ECDSA_SECP256R1_SHA256` | ECDSA P-256 + SHA-256 |
| `XTLS_SIGNATURE_RSA_PKCS1_SHA384` | PKCS#1 v1.5 + SHA-384（TLS 1.2 遗留） |
| `XTLS_SIGNATURE_ECDSA_SECP384R1_SHA384` | ECDSA P-384 + SHA-384 |
| `XTLS_SIGNATURE_RSA_PKCS1_SHA512` | PKCS#1 v1.5 + SHA-512（TLS 1.2 遗留） |
| `XTLS_SIGNATURE_ECDSA_SECP521R1_SHA512` | ECDSA P-521 + SHA-512 |
| `XTLS_SIGNATURE_RSA_PSS_RSAE_SHA256` | RSA-PSS（RSAE 密钥）+ SHA-256 |
| `XTLS_SIGNATURE_RSA_PSS_RSAE_SHA384` | RSA-PSS（RSAE）+ SHA-384 |
| `XTLS_SIGNATURE_RSA_PSS_RSAE_SHA512` | RSA-PSS（RSAE）+ SHA-512 |
| `XTLS_SIGNATURE_ED25519` | Ed25519 |
| `XTLS_SIGNATURE_ED448` | Ed448 |
| `XTLS_SIGNATURE_RSA_PSS_PSS_SHA256` | RSA-PSS（PSS 密钥）+ SHA-256 |
| `XTLS_SIGNATURE_RSA_PSS_PSS_SHA384` | RSA-PSS（PSS）+ SHA-384 |

### `xtlsitemresult`

TLS 游标把正常结束、读取到值和协议错误分开表达。

```c
typedef enum xtlsitemresult {
	XTLS_ITEM_ERROR = -1,
	XTLS_ITEM_DONE = 0,
	XTLS_ITEM_VALUE = 1
} xtlsitemresult;
```

| 值 | 语义 |
|---|---|
| `XTLS_ITEM_ERROR` | 失败 |
| `XTLS_ITEM_DONE` | 完成 |

### `xtlskeyupdate`

KeyUpdate 请求值保留 TLS 1.3 线路数值。

```c
typedef enum xtlskeyupdate {
	XTLS_KEY_UPDATE_NOT_REQUESTED = 0,
	XTLS_KEY_UPDATE_REQUESTED = 1
} xtlskeyupdate;
```

| 值 | 语义 |
|---|---|
| `XTLS_KEY_UPDATE_NOT_REQUESTED` | XTLSKEYUPDATENOTREQUESTED |

### `xtlscertificatetype`

TLS 1.2 CertificateRequest 证书类型保留标准线路数值。

```c
typedef enum xtlscertificatetype {
	XTLS_CERTIFICATE_RSA_SIGN = 1,
	XTLS_CERTIFICATE_DSS_SIGN = 2,
	XTLS_CERTIFICATE_ECDSA_SIGN = 64
} xtlscertificatetype;
```

| 值 | 语义 |
|---|---|
| `XTLS_CERTIFICATE_RSA_SIGN` | RSASIGN |
| `XTLS_CERTIFICATE_DSS_SIGN` | DSSSIGN |

### `xtlscertificatestatustype`

CertificateStatus 当前标准化的正文类型是 OCSP。

```c
typedef enum xtlscertificatestatustype {
	XTLS_CERTIFICATE_STATUS_OCSP = 1
} xtlscertificatestatustype;
```

| 值 | 语义 |
|---|---|

### `xtlscertificatecompression`

TLS 1.3 压缩证书算法保留 RFC 8879 线路数值。

```c
typedef enum xtlscertificatecompression {
	XTLS_CERTIFICATE_COMPRESSION_ZLIB = 1,
	XTLS_CERTIFICATE_COMPRESSION_BROTLI = 2,
	XTLS_CERTIFICATE_COMPRESSION_ZSTD = 3
} xtlscertificatecompression;
```

| 值 | 语义 |
|---|---|
| `XTLS_CERTIFICATE_COMPRESSION_ZLIB` | zlib 包装 |
| `XTLS_CERTIFICATE_COMPRESSION_BROTLI` | BROTLI |

### `xtlsalertlevel`

Alert 级别保留 TLS 线上的稳定数值。

```c
typedef enum xtlsalertlevel {
	XTLS_ALERT_WARNING = 1,
	XTLS_ALERT_FATAL = 2
} xtlsalertlevel;
```

| 值 | 语义 |
|---|---|
| `XTLS_ALERT_WARNING` | Warning |

### `xtlsalert`

TLS 1.2 和 TLS 1.3 仍可在线路上出现的 Alert 描述。

```c
typedef enum xtlsalert {
	XTLS_ALERT_CLOSE_NOTIFY = 0,
	XTLS_ALERT_UNEXPECTED_MESSAGE = 10,
	XTLS_ALERT_BAD_RECORD_MAC = 20,
	XTLS_ALERT_RECORD_OVERFLOW = 22,
	XTLS_ALERT_HANDSHAKE_FAILURE = 40,
	XTLS_ALERT_BAD_CERTIFICATE = 42,
	XTLS_ALERT_UNSUPPORTED_CERTIFICATE = 43,
	XTLS_ALERT_CERTIFICATE_REVOKED = 44,
	XTLS_ALERT_CERTIFICATE_EXPIRED = 45,
	XTLS_ALERT_CERTIFICATE_UNKNOWN = 46,
	XTLS_ALERT_ILLEGAL_PARAMETER = 47,
	XTLS_ALERT_UNKNOWN_CA = 48,
	XTLS_ALERT_ACCESS_DENIED = 49,
	XTLS_ALERT_DECODE_ERROR = 50,
	XTLS_ALERT_DECRYPT_ERROR = 51,
	XTLS_ALERT_PROTOCOL_VERSION = 70,
	XTLS_ALERT_INSUFFICIENT_SECURITY = 71,
	XTLS_ALERT_INTERNAL_ERROR = 80,
	XTLS_ALERT_INAPPROPRIATE_FALLBACK = 86,
	XTLS_ALERT_USER_CANCELED = 90,
	XTLS_ALERT_MISSING_EXTENSION = 109,
	XTLS_ALERT_UNSUPPORTED_EXTENSION = 110,
	XTLS_ALERT_UNRECOGNIZED_NAME = 112,
	XTLS_ALERT_BAD_CERTIFICATE_STATUS_RESPONSE = 113,
	XTLS_ALERT_UNKNOWN_PSK_IDENTITY = 115,
	XTLS_ALERT_CERTIFICATE_REQUIRED = 116,
	XTLS_ALERT_NO_APPLICATION_PROTOCOL = 120
} xtlsalert;
```

| 值 | 语义 |
|---|---|
| `XTLS_ALERT_CLOSE_NOTIFY` | 关闭Notify |
| `XTLS_ALERT_UNEXPECTED_MESSAGE` | UNEXPECTED消息 |
| `XTLS_ALERT_BAD_RECORD_MAC` | 错误记录Mac |
| `XTLS_ALERT_RECORD_OVERFLOW` | RECORD溢出 |
| `XTLS_ALERT_HANDSHAKE_FAILURE` | 握手阶段FAILURE |
| `XTLS_ALERT_BAD_CERTIFICATE` | 错误证书 |
| `XTLS_ALERT_UNSUPPORTED_CERTIFICATE` | 不支持CERTIFICATE |
| `XTLS_ALERT_CERTIFICATE_REVOKED` | 证书Revoked |
| `XTLS_ALERT_CERTIFICATE_EXPIRED` | 证书已过期 |
| `XTLS_ALERT_CERTIFICATE_UNKNOWN` | CERTIFICATE未知 |
| `XTLS_ALERT_ILLEGAL_PARAMETER` | 非法Parameter |
| `XTLS_ALERT_UNKNOWN_CA` | 未知CA |
| `XTLS_ALERT_ACCESS_DENIED` | 访问被拒绝 |
| `XTLS_ALERT_DECODE_ERROR` | DECODE失败 |
| `XTLS_ALERT_DECRYPT_ERROR` | DECRYPT失败 |
| `XTLS_ALERT_PROTOCOL_VERSION` | 协议非法VERSION |
| `XTLS_ALERT_INSUFFICIENT_SECURITY` | InsufficientSecurity |
| `XTLS_ALERT_INTERNAL_ERROR` | 内部错误失败 |
| `XTLS_ALERT_INAPPROPRIATE_FALLBACK` | InappropriateFallback |
| `XTLS_ALERT_USER_CANCELED` | UserCanceled |
| `XTLS_ALERT_MISSING_EXTENSION` | 缺失Extension |
| `XTLS_ALERT_UNSUPPORTED_EXTENSION` | 不支持EXTENSION |
| `XTLS_ALERT_UNRECOGNIZED_NAME` | UNRECOGNIZED名称 |
| `XTLS_ALERT_BAD_CERTIFICATE_STATUS_RESPONSE` | 错误证书StatusResponse |
| `XTLS_ALERT_UNKNOWN_PSK_IDENTITY` | 未知PSKIDENTITY |
| `XTLS_ALERT_CERTIFICATE_REQUIRED` | 证书必需项 |

### `xtlserror`

TLS 错误码稳定描述失败发生的协议阶段。

```c
typedef enum xtlserror {
	XTLS_ERROR_ARGUMENT = 1,
	XTLS_ERROR_VERSION,
	XTLS_ERROR_RECORD_TYPE,
	XTLS_ERROR_RECORD_VERSION,
	XTLS_ERROR_RECORD_SIZE,
	XTLS_ERROR_RECORD_BUFFER,
	XTLS_ERROR_ALERT,
	XTLS_ERROR_STATE,
	XTLS_ERROR_LIMIT,
	XTLS_ERROR_NEGOTIATION,
	XTLS_ERROR_KEY_EXCHANGE,
	XTLS_ERROR_CIPHER,
	XTLS_ERROR_HANDSHAKE,
	XTLS_ERROR_EXTENSION,
	XTLS_ERROR_TRANSCRIPT,
	XTLS_ERROR_KEY_DERIVATION,
	XTLS_ERROR_CERTIFICATE,
	XTLS_ERROR_IDENTITY,
	XTLS_ERROR_VERIFY,
	XTLS_ERROR_RESUME,
	XTLS_ERROR_CLOSED,
	XTLS_ERROR_TRUNCATED,
	XTLS_ERROR_INTERNAL
} xtlserror;
```

| 值 | 语义 |
|---|---|
| `XTLS_ERROR_ARGUMENT` | 参数非法 |
| `XTLS_ERROR_VERSION` | 失败 |
| `XTLS_ERROR_RECORD_TYPE` | RECORD类型 |
| `XTLS_ERROR_RECORD_VERSION` | 失败 |
| `XTLS_ERROR_RECORD_SIZE` | RECORD尺寸 |
| `XTLS_ERROR_RECORD_BUFFER` | 失败 |
| `XTLS_ERROR_ALERT` | 失败 |
| `XTLS_ERROR_STATE` | 状态非法 |
| `XTLS_ERROR_LIMIT` | 超限 |
| `XTLS_ERROR_NEGOTIATION` | 失败 |
| `XTLS_ERROR_KEY_EXCHANGE` | 失败 |
| `XTLS_ERROR_CIPHER` | 失败 |
| `XTLS_ERROR_HANDSHAKE` | 握手阶段 |
| `XTLS_ERROR_EXTENSION` | 失败 |
| `XTLS_ERROR_TRANSCRIPT` | 失败 |
| `XTLS_ERROR_KEY_DERIVATION` | 失败 |
| `XTLS_ERROR_CERTIFICATE` | 失败 |
| `XTLS_ERROR_IDENTITY` | 失败 |
| `XTLS_ERROR_VERIFY` | 失败 |
| `XTLS_ERROR_RESUME` | 失败 |
| `XTLS_ERROR_CLOSED` | 已关闭 |
| `XTLS_ERROR_TRUNCATED` | 已截断 |

### `xtlsrecord`

记录视图借用输入内存；EncodedSize 是头与负载的总长度。

```c
typedef struct xtlsrecord {
	xtlsrecordtype Type;
	uint16 LegacyVersion;
	xbytesview Payload;
	size_t EncodedSize;
} xtlsrecord;
```

| 字段 | 类型 | 语义 |
|---|---|---|
| `Type` | `xtlsrecordtype` | 类型 |
| `LegacyVersion` | `uint16` | LegacyVersion |
| `Payload` | `xbytesview` | 载荷 |
| `EncodedSize` | `size_t` | EncodedSize |

### `xtlscipherinfo`

密码套件元数据是进程期只读对象，尺寸字段都以字节计。

```c
typedef struct xtlscipherinfo {
	xtlscipher Cipher;
	xtlsversion Version;
	xtlshash Hash;
	xtlsaead Aead;
	xtlscipherauth Authentication;
	uint8 HashSize;
	uint8 KeySize;
	uint8 IvSize;
	uint8 ExplicitNonceSize;
	uint8 TagSize;
} xtlscipherinfo;
```

| 字段 | 类型 | 语义 |
|---|---|---|
| `Cipher` | `xtlscipher` | Cipher |
| `Version` | `xtlsversion` | 结构版本 |
| `Hash` | `xtlshash` | Hash |
| `Aead` | `xtlsaead` | Aead |
| `Authentication` | `xtlscipherauth` | Authentication |
| `HashSize` | `uint8` | HashSize |
| `KeySize` | `uint8` | KeySize |
| `IvSize` | `uint8` | IvSize |
| `ExplicitNonceSize` | `uint8` | ExplicitNonceSize |
| `TagSize` | `uint8` | TagSize |

### `xtlsgroupkind`

命名组类型区分 Montgomery XDH 与未压缩短 Weierstrass ECDH 公钥。

```c
typedef enum xtlsgroupkind {
	XTLS_GROUP_KIND_XDH = 1,
	XTLS_GROUP_KIND_ECDH
} xtlsgroupkind;
```

| 值 | 语义 |
|---|---|
| `XTLS_GROUP_KIND_XDH` | XTLSGROUPKINDXDH |

### `xtlsgroupinfo`

命名组元数据是进程期只读对象，所有尺寸字段均以字节计。

```c
typedef struct xtlsgroupinfo {
	uint16 Group;
	xtlsgroupkind Kind;
	uint16 PrivateSize;
	uint16 PublicSize;
	uint16 SharedSize;
} xtlsgroupinfo;
```

| 字段 | 类型 | 语义 |
|---|---|---|
| `Group` | `uint16` | Group |
| `Kind` | `xtlsgroupkind` | 错误种类 |
| `PrivateSize` | `uint16` | PrivateSize |
| `PublicSize` | `uint16` | PublicSize |
| `SharedSize` | `uint16` | SharedSize |

### `xtlshandshake`

握手消息视图借用输入，EncodedSize 包含 4 字节头和正文。

```c
typedef struct xtlshandshake {
	xtlshandshaketype Type;
	xbytesview Body;
	size_t EncodedSize;
} xtlshandshake;
```

| 字段 | 类型 | 语义 |
|---|---|---|
| `Type` | `xtlshandshaketype` | 类型 |
| `Body` | `xbytesview` | 主体 |
| `EncodedSize` | `size_t` | EncodedSize |

### `xtlsextension`

扩展视图借用输入，EncodedSize 包含类型、长度和扩展负载。

```c
typedef struct xtlsextension {
	xtlsextensiontype Type;
	xbytesview Data;
	size_t EncodedSize;
} xtlsextension;
```

| 字段 | 类型 | 语义 |
|---|---|---|
| `Type` | `xtlsextensiontype` | 类型 |
| `Data` | `xbytesview` | 数据 |
| `EncodedSize` | `size_t` | EncodedSize |

### `xtlsextensioncursor`

扩展游标借用完整扩展向量，并用小型精确桶检测重复类型。

```c
typedef struct xtlsextensioncursor {
	xbytesview Data;
	size_t Offset;
	uint64 Seen[4];
} xtlsextensioncursor;
```

| 字段 | 类型 | 语义 |
|---|---|---|
| `Data` | `xbytesview` | 数据 |
| `Offset` | `size_t` | 偏移量 |

### `xtlsids`

16 位标识列表借用去除线路长度前缀后的偶数字节序列。

```c
typedef struct xtlsids {
	xbytesview Data;
} xtlsids;
```

| 字段 | 类型 | 语义 |
|---|---|---|
| `Data` | `xbytesview` | 数据 |

### `xtlsservername`

SNI 名称保留名称类型和借用的原始名称字节。

```c
typedef struct xtlsservername {
	uint8 Type;
	xbytesview Name;
} xtlsservername;
```

| 字段 | 类型 | 语义 |
|---|---|---|
| `Type` | `uint8` | 类型 |
| `Name` | `xbytesview` | 名称 |

### `xtlsservernamecursor`

SNI 名称游标在 256 种名称类型上精确检测重复。

```c
typedef struct xtlsservernamecursor {
	xbytesview Data;
	size_t Offset;
	uint64 Seen[4];
} xtlsservernamecursor;
```

| 字段 | 类型 | 语义 |
|---|---|---|
| `Data` | `xbytesview` | 数据 |
| `Offset` | `size_t` | 偏移量 |

### `xtlsprotocolcursor`

ALPN 游标借用去除 16 位列表长度后的 ProtocolNameList。

```c
typedef struct xtlsprotocolcursor {
	xbytesview Data;
	size_t Offset;
} xtlsprotocolcursor;
```

| 字段 | 类型 | 语义 |
|---|---|---|
| `Data` | `xbytesview` | 数据 |
| `Offset` | `size_t` | 偏移量 |

### `xtlskeyshare`

TLS 1.3 密钥共享保留命名组和借用的线路公钥。

```c
typedef struct xtlskeyshare {
	uint16 Group;
	xbytesview Key;
} xtlskeyshare;
```

| 字段 | 类型 | 语义 |
|---|---|---|
| `Group` | `uint16` | Group |
| `Key` | `xbytesview` | 键 |

### `xtlskeysharecursor`

ClientHello 密钥共享游标借用列表并检测重复命名组。

```c
typedef struct xtlskeysharecursor {
	xbytesview Data;
	size_t Offset;
	uint64 Seen[4];
} xtlskeysharecursor;
```

| 字段 | 类型 | 语义 |
|---|---|---|
| `Data` | `xbytesview` | 数据 |
| `Offset` | `size_t` | 偏移量 |

### `xtlsclienthello`

ClientHello 视图只发布已经严格验证且恰好消费完整正文的字段。

```c
typedef struct xtlsclienthello {
	uint16 LegacyVersion;
	xbytesview Random;
	xbytesview SessionId;
	xtlsids CipherSuites;
	xbytesview CompressionMethods;
	xbytesview Extensions;
} xtlsclienthello;
```

| 字段 | 类型 | 语义 |
|---|---|---|
| `LegacyVersion` | `uint16` | LegacyVersion |
| `Random` | `xbytesview` | Random |
| `SessionId` | `xbytesview` | SessionId |
| `CipherSuites` | `xtlsids` | CipherSuites |
| `CompressionMethods` | `xbytesview` | CompressionMethods |
| `Extensions` | `xbytesview` | Extensions |

### `xtlsserverhello`

ServerHello 视图保留 HelloRetryRequest 标记和全部借用字段。

```c
typedef struct xtlsserverhello {
	uint16 LegacyVersion;
	xbytesview Random;
	xbytesview SessionId;
	uint16 CipherSuite;
	uint8 CompressionMethod;
	xbytesview Extensions;
	bool Retry;
} xtlsserverhello;
```

| 字段 | 类型 | 语义 |
|---|---|---|
| `LegacyVersion` | `uint16` | LegacyVersion |
| `Random` | `xbytesview` | Random |
| `SessionId` | `xbytesview` | SessionId |
| `CipherSuite` | `uint16` | CipherSuite |
| `CompressionMethod` | `uint8` | CompressionMethod |
| `Extensions` | `xbytesview` | Extensions |
| `Retry` | `bool` | Retry |

### `xtlspskmode`

TLS 1.3 PSK 密钥交换模式保留标准线路值。

```c
typedef enum xtlspskmode {
	XTLS_PSK_KE = 0,
	XTLS_PSK_DHE_KE = 1
} xtlspskmode;
```

| 值 | 语义 |
|---|---|
| `XTLS_PSK_KE` | XTLSPSKKE |

### `xtlspsk`

一项客户端 PSK 同时借用 identity、混淆年龄和对应 binder。

```c
typedef struct xtlspsk {
	xbytesview Identity;
	uint32 ObfuscatedAge;
	xbytesview Binder;
} xtlspsk;
```

| 字段 | 类型 | 语义 |
|---|---|---|
| `Identity` | `xbytesview` | Identity |
| `ObfuscatedAge` | `uint32` | ObfuscatedAge |
| `Binder` | `xbytesview` | Binder |

### `xtlspskcursor`

PSK 游标同步遍历已经验证为等长的 identities 与 binders 列表。

```c
typedef struct xtlspskcursor {
	xbytesview Identities;
	xbytesview Binders;
	size_t IdentityOffset;
	size_t BinderOffset;
} xtlspskcursor;
```

| 字段 | 类型 | 语义 |
|---|---|---|
| `Identities` | `xbytesview` | Identities |
| `Binders` | `xbytesview` | Binders |
| `IdentityOffset` | `size_t` | IdentityOffset |
| `BinderOffset` | `size_t` | BinderOffset |

### `xtlsidentitytype`

身份类型只描述握手签名密钥，不把协商层绑到 X.509 实现。

```c
typedef enum xtlsidentitytype {
	XTLS_IDENTITY_NONE = 0,
	XTLS_IDENTITY_RSA,
	XTLS_IDENTITY_RSA_PSS,
	XTLS_IDENTITY_ECDSA_P256,
	XTLS_IDENTITY_ECDSA_P384,
	XTLS_IDENTITY_ECDSA_P521,
	XTLS_IDENTITY_ED25519,
	XTLS_IDENTITY_ED448
} xtlsidentitytype;
```

| 值 | 语义 |
|---|---|
| `XTLS_IDENTITY_NONE` | 无 |
| `XTLS_IDENTITY_RSA` | RSA |
| `XTLS_IDENTITY_RSA_PSS` | RSAPSS |
| `XTLS_IDENTITY_ECDSA_P256` | ECDSAP256 |
| `XTLS_IDENTITY_ECDSA_P384` | ECDSAP384 |
| `XTLS_IDENTITY_ECDSA_P521` | ECDSAP521 |
| `XTLS_IDENTITY_ED25519` | ED25519 |

### `xtlssignatureinfo`

签名方案元数据描述线路方案要求的密钥身份、摘要长度和协议版本范围。

```c
typedef struct xtlssignatureinfo {
	xtlssignature Signature;
	xtlsidentitytype Identity;
	uint8 HashSize;
	xtlsversion Minimum;
	xtlsversion Maximum;
} xtlssignatureinfo;
```

| 字段 | 类型 | 语义 |
|---|---|---|
| `Signature` | `xtlssignature` | Signature |
| `Identity` | `xtlsidentitytype` | Identity |
| `HashSize` | `uint8` | HashSize |
| `Minimum` | `xtlsversion` | Minimum |
| `Maximum` | `xtlsversion` | Maximum |

### `xtlskeysharepolicy`

密钥共享策略可选择本地组优先级或避免 HelloRetryRequest。

```c
typedef enum xtlskeysharepolicy {
	XTLS_KEY_SHARE_PREFER_GROUP = 0,
	XTLS_KEY_SHARE_PREFER_READY
} xtlskeysharepolicy;
```

| 值 | 语义 |
|---|---|
| `XTLS_KEY_SHARE_PREFER_GROUP` | XTLSKEYSHAREPREFERGROUP |

### `xtlskeyshareselection`

密钥共享选择成功时发布借用公钥；Retry 时 Key 为空。

```c
typedef struct xtlskeyshareselection {
	xtlskeyshare Share;
	bool Retry;
} xtlskeyshareselection;
```

| 字段 | 类型 | 语义 |
|---|---|---|
| `Share` | `xtlskeyshare` | Share |
| `Retry` | `bool` | Retry |

### `xtlspolicy`

TLS 策略借用有序偏好数组；默认初始化后的数组具有进程期生命周期。

```c
typedef struct xtlspolicy {
	const xtlsversion* Versions;
	size_t VersionCount;
	const xtlscipher* Ciphers;
	size_t CipherCount;
	const uint16* Groups;
	size_t GroupCount;
	const xtlssignature* Signatures;
	size_t SignatureCount;
	xtlskeysharepolicy KeySharePolicy;
} xtlspolicy;
```

| 字段 | 类型 | 语义 |
|---|---|---|
| `Versions` | `const xtlsversion*` | Versions |
| `VersionCount` | `size_t` | VersionCount |
| `Ciphers` | `const xtlscipher*` | Ciphers |
| `CipherCount` | `size_t` | CipherCount |
| `Groups` | `const uint16*` | Groups |
| `GroupCount` | `size_t` | GroupCount |
| `Signatures` | `const xtlssignature*` | Signatures |
| `SignatureCount` | `size_t` | SignatureCount |
| `KeySharePolicy` | `xtlskeysharepolicy` | KeySharePolicy |

### `xtlslimits`

TLS 限制只描述硬上限与公平性预算，不会触发预分配。

```c
typedef struct xtlslimits {
	size_t FeedLimit;
	size_t SendLimit;
	size_t PlainLimit;
	size_t HandshakeLimit;
	uint32 RecordBudget;
	uint32 HandshakeBudget;
} xtlslimits;
```

| 字段 | 类型 | 语义 |
|---|---|---|
| `FeedLimit` | `size_t` | FeedLimit |
| `SendLimit` | `size_t` | SendLimit |
| `PlainLimit` | `size_t` | PlainLimit |
| `HandshakeLimit` | `size_t` | HandshakeLimit |
| `RecordBudget` | `uint32` | RecordBudget |
| `HandshakeBudget` | `uint32` | HandshakeBudget |

### `xtlscontextconfig`

创建配置借用可选策略；创建成功后上下文持有独立快照。

```c
typedef struct xtlscontextconfig {
	const xtlspolicy* Policy;
	xtlslimits Limits;
} xtlscontextconfig;
```

| 字段 | 类型 | 语义 |
|---|---|---|
| `Policy` | `const xtlspolicy*` | 策略 |
| `Limits` | `xtlslimits` | Limits |

### `xtlswriter`

TLS writer 直接使用调用方缓冲，并只在完整追加成功后推进 Size。

```c
typedef struct xtlswriter {
	bytes Data;
	size_t Capacity;
	size_t Size;
} xtlswriter;
```

| 字段 | 类型 | 语义 |
|---|---|---|
| `Data` | `bytes` | 数据 |
| `Capacity` | `size_t` | 容量 |
| `Size` | `size_t` | 字节数 |

### `xtlshandshakereaderconfig`

Reader 配置分开控制单消息硬上限和跨消息保留容量。

```c
typedef struct xtlshandshakereaderconfig {
	size_t Limit;
	size_t Retain;
} xtlshandshakereaderconfig;
```

| 字段 | 类型 | 语义 |
|---|---|---|
| `Limit` | `size_t` | 上限 |
| `Retain` | `size_t` | Retain |

### `xtlshandshakereader`

Reader 只在消息跨输入分片时渐进分配连续重组缓冲。

```c
typedef struct xtlshandshakereader {
	bytes Data;
	size_t Size;
	size_t Capacity;
	size_t Required;
	size_t Limit;
	size_t Retain;
	uint8 Header[XTLS_HANDSHAKE_HEADER_SIZE];
	uint8 HeaderSize;
	bool Ready;
} xtlshandshakereader;
```

| 字段 | 类型 | 语义 |
|---|---|---|
| `Data` | `bytes` | 数据 |
| `Size` | `size_t` | 字节数 |
| `Capacity` | `size_t` | 容量 |
| `Required` | `size_t` | 是否必需 |
| `Limit` | `size_t` | 上限 |
| `Retain` | `size_t` | Retain |
| `HeaderSize` | `uint8` | HeaderSize |
| `Ready` | `bool` | Ready |

### `xtlscertificatemessage`

Certificate 消息视图保留版本、请求上下文和完整条目向量。

```c
typedef struct xtlscertificatemessage {
	xtlsversion Version;
	xbytesview RequestContext;
	xbytesview Entries;
} xtlscertificatemessage;
```

| 字段 | 类型 | 语义 |
|---|---|---|
| `Version` | `xtlsversion` | 结构版本 |
| `RequestContext` | `xbytesview` | RequestContext |
| `Entries` | `xbytesview` | 条目数组 |

### `xtlscertificateentry`

证书条目借用 DER 数据；TLS 1.2 条目的 Extensions 为空。

```c
typedef struct xtlscertificateentry {
	xbytesview Data;
	xbytesview Extensions;
} xtlscertificateentry;
```

| 字段 | 类型 | 语义 |
|---|---|---|
| `Data` | `xbytesview` | 数据 |
| `Extensions` | `xbytesview` | Extensions |

### `xtlscertificatecursor`

证书游标不限制链长度，也不复制证书或扩展。

```c
typedef struct xtlscertificatecursor {
	xtlsversion Version;
	xbytesview Data;
	size_t Offset;
} xtlscertificatecursor;
```

| 字段 | 类型 | 语义 |
|---|---|---|
| `Version` | `xtlsversion` | 结构版本 |
| `Data` | `xbytesview` | 数据 |
| `Offset` | `size_t` | 偏移量 |

### `xtlscertificateverify`

CertificateVerify 视图保留未知签名方案和签名字节。

```c
typedef struct xtlscertificateverify {
	uint16 Scheme;
	xbytesview Signature;
} xtlscertificateverify;
```

| 字段 | 类型 | 语义 |
|---|---|---|
| `Scheme` | `uint16` | 协议方案 |
| `Signature` | `xbytesview` | Signature |

### `xtlssessionticket`

NewSessionTicket 统一暴露 TLS 1.2 与 TLS 1.3 字段。

```c
typedef struct xtlssessionticket {
	xtlsversion Version;
	uint32 Lifetime;
	uint32 AgeAdd;
	xbytesview Nonce;
	xbytesview Ticket;
	xbytesview Extensions;
} xtlssessionticket;
```

| 字段 | 类型 | 语义 |
|---|---|---|
| `Version` | `xtlsversion` | 结构版本 |
| `Lifetime` | `uint32` | Lifetime |
| `AgeAdd` | `uint32` | AgeAdd |
| `Nonce` | `xbytesview` | Nonce |
| `Ticket` | `xbytesview` | Ticket |
| `Extensions` | `xbytesview` | Extensions |

### `xtlsauthoritycursor`

证书颁发者游标借用去除外层 16 位长度后的名称条目。

```c
typedef struct xtlsauthoritycursor {
	xbytesview Data;
	size_t Offset;
} xtlsauthoritycursor;
```

| 字段 | 类型 | 语义 |
|---|---|---|
| `Data` | `xbytesview` | 数据 |
| `Offset` | `size_t` | 偏移量 |

### `xtls12certificaterequest`

TLS 1.2 CertificateRequest 保留证书类型、签名方案和完整颁发者向量。

```c
typedef struct xtls12certificaterequest {
	xbytesview CertificateTypes;
	xtlsids Signatures;
	xbytesview AuthorityData;
} xtls12certificaterequest;
```

| 字段 | 类型 | 语义 |
|---|---|---|
| `CertificateTypes` | `xbytesview` | CertificateTypes |
| `Signatures` | `xtlsids` | Signatures |
| `AuthorityData` | `xbytesview` | AuthorityData |

### `xtls13certificaterequest`

TLS 1.3 CertificateRequest 保留原始扩展和常用认证选择的便捷视图。

```c
typedef struct xtls13certificaterequest {
	xbytesview RequestContext;
	xbytesview Extensions;
	xtlsids Signatures;
	xtlsids CertificateSignatures;
	xbytesview AuthorityData;
} xtls13certificaterequest;
```

| 字段 | 类型 | 语义 |
|---|---|---|
| `RequestContext` | `xbytesview` | RequestContext |
| `Extensions` | `xbytesview` | Extensions |
| `Signatures` | `xtlsids` | Signatures |
| `CertificateSignatures` | `xtlsids` | CertificateSignatures |
| `AuthorityData` | `xbytesview` | AuthorityData |

### `xtls12serverkeyexchange`

TLS 1.2 ECDHE ServerKeyExchange 公开可直接参与验签的参数切片。

```c
typedef struct xtls12serverkeyexchange {
	uint16 Group;
	xbytesview PublicKey;
	xbytesview Parameters;
	xtlscertificateverify Verify;
} xtls12serverkeyexchange;
```

| 字段 | 类型 | 语义 |
|---|---|---|
| `Group` | `uint16` | Group |
| `PublicKey` | `xbytesview` | PublicKey |
| `Parameters` | `xbytesview` | Parameters |
| `Verify` | `xtlscertificateverify` | Verify |

### `xtlscertificatestatusmessage`

CertificateStatus 保留状态类型和不透明响应。

```c
typedef struct xtlscertificatestatusmessage {
	uint8 Type;
	xbytesview Response;
} xtlscertificatestatusmessage;
```

| 字段 | 类型 | 语义 |
|---|---|---|
| `Type` | `uint8` | 类型 |
| `Response` | `xbytesview` | Response |

### `xtlscompressedcertificate`

CompressedCertificate 只描述线路对象，解压和协商属于会话层。

```c
typedef struct xtlscompressedcertificate {
	uint16 Algorithm;
	size_t UncompressedSize;
	xbytesview Data;
} xtlscompressedcertificate;
```

| 字段 | 类型 | 语义 |
|---|---|---|
| `Algorithm` | `uint16` | Algorithm |
| `UncompressedSize` | `size_t` | UncompressedSize |
| `Data` | `xbytesview` | 数据 |

### `xtlscontext`

共享 TLS 上下文（不透明）：持有策略、身份与信任库，可跨线程复用。


```c
typedef struct xtlscontext xtlscontext;
```

不透明句柄或别名；生命周期与所有权见各使用方 API 节。

### `xtlsclientconfig`

配置在创建期间借用全部对象和视图；成功会话持有共享对象并深拷贝视图。 ServerName 只用于线路 SNI，VerifyName 用于证书身份验证和恢复票据绑定。 VerifyName 为空时继承 ServerName，允许 DNS 名称保持一字段常用写法； 连接 IP 字面量时应只设置 VerifyName，避免发送协议不允许的 IP SNI。

```c
typedef struct xtlsclientconfig {
	const xtlscontext* Context;
	xstrview ServerName;
	xstrview VerifyName;
	const xstrview* Protocols;
	size_t ProtocolCount;
	const xtlsverifier* Verifier;
	const xtlsresume* Resume;
	size_t ResumeLimit;
	/* 必须接受所给票据；禁止回退完整证书握手。 */
	bool ResumeOnly;
} xtlsclientconfig;
```

| 字段 | 类型 | 语义 |
|---|---|---|
| `Context` | `const xtlscontext*` | 回调上下文 |
| `ServerName` | `xstrview` | ServerName |
| `VerifyName` | `xstrview` | VerifyName |
| `Protocols` | `const xstrview*` | Protocols |
| `ProtocolCount` | `size_t` | ProtocolCount |
| `Verifier` | `const xtlsverifier*` | Verifier |
| `Resume` | `const xtlsresume*` | Resume |
| `ResumeLimit` | `size_t` | ResumeLimit |
| `ResumeOnly` | `bool` | ResumeOnly |

### `xtlsverifier`

对端验证器（不透明）：持有信任库与主机名策略，可被多个握手共享。


```c
typedef struct xtlsverifier xtlsverifier;
```

不透明句柄或别名；生命周期与所有权见各使用方 API 节。

### `xtlsresume`

会话恢复对象（不透明）：持有票据与参数，供客户端恢复握手。


```c
typedef struct xtlsresume xtlsresume;
```

不透明句柄或别名；生命周期与所有权见各使用方 API 节。

### `xtlsidentityconfig`

自定义身份配置借用输入证书和回调，仅在创建成功后接管 Context。 创建后的身份是不可变快照，外部签名器必须允许并发调用。

```c
typedef struct xtlsidentityconfig {
	const xbytesview* Certificates;
	size_t CertificateCount;
	xtlsidentitytype Type;
	xtlsidentitysupportsproc Supports;
	xtlsidentitysignproc Sign;
	xtlsidentityreleaseproc Release;
	ptr Context;
} xtlsidentityconfig;
```

| 字段 | 类型 | 语义 |
|---|---|---|
| `Certificates` | `const xbytesview*` | Certificates |
| `CertificateCount` | `size_t` | CertificateCount |
| `Type` | `xtlsidentitytype` | 类型 |
| `Supports` | `xtlsidentitysupportsproc` | Supports |
| `Sign` | `xtlsidentitysignproc` | 符号 |
| `Release` | `xtlsidentityreleaseproc` | Release |
| `Context` | `ptr` | 回调上下文 |

### `xtlsidentity`

TLS 身份（不透明）：证书链 + 私钥 + 可签名方案。


```c
typedef struct xtlsidentity xtlsidentity;
```

不透明句柄或别名；生命周期与所有权见各使用方 API 节。

### `xtlsidentitysupportsproc`

外部签名器可以进一步限制同一密钥类型实际支持的协议签名方案。

```c
typedef bool (*xtlsidentitysupportsproc)(
	ptr pContext,
	xtlsversion Version,
	xtlssignature Signature
);
```

回调类型；参数与返回语义见签名及各使用方 API 节。

### `xtlsidentitysignproc`

签名器接收完整 TLS 待签内容，内部负责协议方案要求的摘要与编码。 pOutput 为空且容量为零时只查询所需长度；失败时不得修改输出和长度。

```c
typedef bool (*xtlsidentitysignproc)(
	ptr pContext,
	xtlsversion Version,
	xtlssignature Signature,
	xbytesview Message,
	void* pOutput,
	size_t iCapacity,
	size_t* pSize
);
```

回调类型；参数与返回语义见签名及各使用方 API 节。

### `xtlsidentityreleaseproc`

身份释放过程只管理外部签名器上下文，不管理已经深复制的证书链。

```c
typedef void (*xtlsidentityreleaseproc)(ptr pContext);
```

回调类型；参数与返回语义见签名及各使用方 API 节。

### `xtlsresumeconfig`

恢复配置在创建期间借用全部视图；成功后对象持有一份不可变深拷贝。 PeerIdentity 是调用方定义的已认证对端标识，可为空，但不参与线路编码。

```c
typedef struct xtlsresumeconfig {
	xtlsversion Version;
	xtlscipher Cipher;
	xbytesview Ticket;
	xbytesview Secret;
	xstrview ServerName;
	xbytesview Protocol;
	xbytesview PeerIdentity;
	uint32 Lifetime;
	uint32 AgeAdd;
	uint32 MaxEarlyData;
	xtime IssuedAt;
} xtlsresumeconfig;
```

| 字段 | 类型 | 语义 |
|---|---|---|
| `Version` | `xtlsversion` | 结构版本 |
| `Cipher` | `xtlscipher` | Cipher |
| `Ticket` | `xbytesview` | Ticket |
| `Secret` | `xbytesview` | Secret |
| `ServerName` | `xstrview` | ServerName |
| `Protocol` | `xbytesview` | Protocol |
| `PeerIdentity` | `xbytesview` | PeerIdentity |
| `Lifetime` | `uint32` | Lifetime |
| `AgeAdd` | `uint32` | AgeAdd |
| `MaxEarlyData` | `uint32` | MaxEarlyData |
| `IssuedAt` | `xtime` | IssuedAt |

### `xtlsresumeinfo`

信息快照中的视图由恢复对象持有，只能在对象引用存活期间借用。

```c
typedef struct xtlsresumeinfo {
	xtlsversion Version;
	xtlscipher Cipher;
	xbytesview Ticket;
	xbytesview Secret;
	xstrview ServerName;
	xbytesview Protocol;
	xbytesview PeerIdentity;
	uint32 Lifetime;
	uint32 AgeAdd;
	uint32 MaxEarlyData;
	xtime IssuedAt;
	xtime ExpiresAt;
} xtlsresumeinfo;
```

| 字段 | 类型 | 语义 |
|---|---|---|
| `Version` | `xtlsversion` | 结构版本 |
| `Cipher` | `xtlscipher` | Cipher |
| `Ticket` | `xbytesview` | Ticket |
| `Secret` | `xbytesview` | Secret |
| `ServerName` | `xstrview` | ServerName |
| `Protocol` | `xbytesview` | Protocol |
| `PeerIdentity` | `xbytesview` | PeerIdentity |
| `Lifetime` | `uint32` | Lifetime |
| `AgeAdd` | `uint32` | AgeAdd |
| `MaxEarlyData` | `uint32` | MaxEarlyData |
| `IssuedAt` | `xtime` | IssuedAt |
| `ExpiresAt` | `xtime` | ExpiresAt |

### `xtlsserverrequest`

选择请求中的视图只在回调期间借用，Protocols 是完整 ALPN 扩展负载。

```c
typedef struct xtlsserverrequest {
	xbytesview ServerName;
	xbytesview Protocols;
} xtlsserverrequest;
```

| 字段 | 类型 | 语义 |
|---|---|---|
| `ServerName` | `xbytesview` | ServerName |
| `Protocols` | `xbytesview` | Protocols |

### `xtlsserverchoice`

选择结果默认带入静态身份、ALPN 和零 Cookie，回调可替换这些结果。

```c
typedef struct xtlsserverchoice {
	const xtlsidentity* Identity;
	size_t Protocol;
	uint64 Cookie;
} xtlsserverchoice;
```

| 字段 | 类型 | 语义 |
|---|---|---|
| `Identity` | `const xtlsidentity*` | Identity |
| `Protocol` | `size_t` | Protocol |
| `Cookie` | `uint64` | Cookie |

### `xtlsserverresumerequest`

票据查找请求中的全部视图仅在回调期间借用。

```c
typedef struct xtlsserverresumerequest {
	xbytesview ServerName;
	xbytesview Protocols;
	xbytesview Ticket;
	uint32 Age;
} xtlsserverresumerequest;
```

| 字段 | 类型 | 语义 |
|---|---|---|
| `ServerName` | `xbytesview` | ServerName |
| `Protocols` | `xbytesview` | Protocols |
| `Ticket` | `xbytesview` | Ticket |
| `Age` | `uint32` | Age |

### `xtlsserverconfig`

创建期间借用配置；会话持有身份、深复制协议，两个回调上下文借用到首航结束。

```c
typedef struct xtlsserverconfig {
	const xtlscontext* Context;
	const xtlsidentity* Identity;
	const xstrview* Protocols;
	size_t ProtocolCount;
	xtlsserverselectproc Select;
	ptr SelectContext;
	bool RequireProtocol;
	xtlsserverresumeproc Resume;
	ptr ResumeContext;
	uint32 ResumeAgeTolerance;
} xtlsserverconfig;
```

| 字段 | 类型 | 语义 |
|---|---|---|
| `Context` | `const xtlscontext*` | 回调上下文 |
| `Identity` | `const xtlsidentity*` | Identity |
| `Protocols` | `const xstrview*` | Protocols |
| `ProtocolCount` | `size_t` | ProtocolCount |
| `Select` | `xtlsserverselectproc` | Select |
| `SelectContext` | `ptr` | SelectContext |
| `RequireProtocol` | `bool` | RequireProtocol |
| `Resume` | `xtlsserverresumeproc` | Resume |
| `ResumeContext` | `ptr` | ResumeContext |
| `ResumeAgeTolerance` | `uint32` | ResumeAgeTolerance |

### `xtlsserverselectproc`

同步选择器用于 SNI、多身份和租户路由；返回 false 会拒绝握手。

```c
typedef bool (*xtlsserverselectproc)(
	ptr pContext,
	const xtlsserverrequest* pRequest,
	xtlsserverchoice* pChoice
);
```

回调类型；参数与返回语义见签名及各使用方 API 节。

### `xtlsserverresumeproc`

返回借用恢复对象；服务器会在回调返回后立即 retain，再读取其不可变快照。

```c
typedef const xtlsresume* (*xtlsserverresumeproc)(
	ptr pContext,
	const xtlsserverresumerequest* pRequest
);
```

回调类型；参数与返回语义见签名及各使用方 API 节。

### `xtlswait`

等待原因是可组合位；驱动器据此决定继续读、写或等待应用动作。

```c
typedef enum xtlswait {
	XTLS_WAIT_NONE = 0,
	XTLS_WAIT_INPUT = (1u << 0),
	XTLS_WAIT_OUTPUT = (1u << 1),
	XTLS_WAIT_APPLICATION = (1u << 2),
	XTLS_WAIT_IDENTITY = (1u << 3),
	XTLS_WAIT_VERIFY = (1u << 4)
} xtlswait;
```

| 值 | 语义 |
|---|---|
| `XTLS_WAIT_NONE` | 无 |
| `XTLS_WAIT_INPUT` | 输入 |
| `XTLS_WAIT_OUTPUT` | 输出失败 |
| `XTLS_WAIT_APPLICATION` | APPLICATION |
| `XTLS_WAIT_IDENTITY` | IDENTITY |

### `xtlssession`

TLS 会话（不透明）：记录层与握手状态机载体。


```c
typedef struct xtlssession xtlssession;
```

不透明句柄或别名；生命周期与所有权见各使用方 API 节。

### `xtlsstreamconfig`

两个超时都使用微秒；零值显式关闭对应计时器。 AsyncBytesLimit 和 AsyncCountLimit 是未完成操作的独立硬边界， AsyncBatch 限制一次 Worker 轮转完成的操作数。

```c
typedef struct xtlsstreamconfig {
	uint64 HandshakeTimeout;
	uint64 CloseTimeout;
	size_t AsyncBytesLimit;
	uint32 AsyncCountLimit;
	uint32 AsyncBatch;
} xtlsstreamconfig;
```

| 字段 | 类型 | 语义 |
|---|---|---|
| `HandshakeTimeout` | `uint64` | HandshakeTimeout |
| `CloseTimeout` | `uint64` | CloseTimeout |
| `AsyncBytesLimit` | `size_t` | AsyncBytesLimit |
| `AsyncCountLimit` | `uint32` | AsyncCountLimit |
| `AsyncBatch` | `uint32` | AsyncBatch |

### `xtlsstreamstate`

FAILED 保存 TLS 或传输根因；CLOSED 只表示完成认证关闭。

```c
typedef enum xtlsstreamstate {
	XTLS_STREAM_CONNECTING = 0,
	XTLS_STREAM_HANDSHAKE,
	XTLS_STREAM_OPEN,
	XTLS_STREAM_CLOSING,
	XTLS_STREAM_CLOSED,
	XTLS_STREAM_FAILED
} xtlsstreamstate;
```

| 值 | 语义 |
|---|---|
| `XTLS_STREAM_CONNECTING` | 连接中 |
| `XTLS_STREAM_HANDSHAKE` | 握手阶段 |
| `XTLS_STREAM_OPEN` | 开放（握手完成） |
| `XTLS_STREAM_CLOSING` | 关闭中 |
| `XTLS_STREAM_CLOSED` | 已关闭 |

### `xtlsstreamwait`

条件 Future 是水平条件；END 表示收到认证 close_notify， CLOSE 表示底层传输和 TLS 组合对象进入最终终态。

```c
typedef enum xtlsstreamwait {
	XTLS_STREAM_WAIT_OPEN = 0,
	XTLS_STREAM_WAIT_READ,
	XTLS_STREAM_WAIT_WRITE,
	XTLS_STREAM_WAIT_DRAIN,
	XTLS_STREAM_WAIT_END,
	XTLS_STREAM_WAIT_CLOSE
} xtlsstreamwait;
```

| 值 | 语义 |
|---|---|
| `XTLS_STREAM_WAIT_OPEN` | 等待开放 |
| `XTLS_STREAM_WAIT_READ` | 读方向 |
| `XTLS_STREAM_WAIT_WRITE` | 写方向 |
| `XTLS_STREAM_WAIT_DRAIN` | 排空策略 |
| `XTLS_STREAM_WAIT_END` | 等待关闭完成 |

### `xtlsdialstate`

Dial 状态区分名称解析、TCP 连接和 TLS 握手三个可取消阶段。

```c
typedef enum xtlsdialstate {
	XTLS_DIAL_RESOLVING = 0,
	XTLS_DIAL_CONNECTING,
	XTLS_DIAL_HANDSHAKE,
	XTLS_DIAL_CONNECTED,
	XTLS_DIAL_FAILED,
	XTLS_DIAL_CANCELLED
} xtlsdialstate;
```

| 值 | 语义 |
|---|---|
| `XTLS_DIAL_RESOLVING` | 解析中 |
| `XTLS_DIAL_CONNECTING` | 连接中 |
| `XTLS_DIAL_HANDSHAKE` | 握手阶段 |
| `XTLS_DIAL_CONNECTED` | 已连接 |
| `XTLS_DIAL_FAILED` | 已失败 |

### `xtlsdialconfig`

Timeout 覆盖 DNS、TCP 和 TLS 全过程；零值只保留各阶段超时。

```c
typedef struct xtlsdialconfig {
	xnetdialconfig Transport;
	xtlsstreamconfig Stream;
	uint64 Timeout;
	bool ServerNameFromHost;
} xtlsdialconfig;
```

| 字段 | 类型 | 语义 |
|---|---|---|
| `Transport` | `xnetdialconfig` | Transport |
| `Stream` | `xtlsstreamconfig` | 流选择 |
| `Timeout` | `uint64` | 超时（微秒） |
| `ServerNameFromHost` | `bool` | ServerNameFromHost |

### `xtlsstreamevents`

全部回调都在底层 TCP Stream 所属 Worker 上串行执行。

```c
typedef struct xtlsstreamevents {
	void (*Open)(xtlsstream* pStream, ptr pData);
	void (*Read)(xtlsstream* pStream,
		const xnetbuf* pBuffer, ptr pData);
	void (*End)(xtlsstream* pStream, ptr pData);
	void (*Writable)(xtlsstream* pStream, ptr pData);
	void (*Drain)(xtlsstream* pStream, ptr pData);
	void (*Close)(xtlsstream* pStream, xnetresult Result,
		const xerror* pError, ptr pData);
	/*
		客户端恢复队列新增票据时发布边沿；未启用恢复实现时不会调用。
		回调使用 xrtTlsClientTakeResume 接管一张或全部票据。
	*/
	void (*Ticket)(xtlsstream* pStream, ptr pData);
} xtlsstreamevents;
```

| 字段 | 类型 | 语义 |
|---|---|---|

### `xtlslistenerstate`

Listener 只发布已经完成 TLS 握手的 Stream，关闭监听不会隐式关闭已发布连接。

```c
typedef enum xtlslistenerstate {
	XTLS_LISTENER_OPEN = 0,
	XTLS_LISTENER_CLOSING,
	XTLS_LISTENER_CLOSED
} xtlslistenerstate;
```

| 值 | 语义 |
|---|---|
| `XTLS_LISTENER_OPEN` | 监听中 |
| `XTLS_LISTENER_CLOSING` | 关闭中 |

### `xtlslistenerevents`

Accept 在目标 Stream 的 Worker 上执行，返回 true 后接管一个 Stream 引用。 Error 只报告监听层错误；单连接握手失败通过 HandshakeError 独立报告。

```c
typedef struct xtlslistenerevents {
	bool (*Accept)(xtlslistener* pListener,
		xtlsstream* pStream, ptr pData);
	void (*HandshakeError)(xtlslistener* pListener,
		const xerror* pError, ptr pData);
	void (*Error)(xtlslistener* pListener,
		const xerror* pError, ptr pData);
	void (*Close)(xtlslistener* pListener, ptr pData);
} xtlslistenerevents;
```

| 字段 | 类型 | 语义 |
|---|---|---|

### `xtlslistenerconfig`

Listen 负责 TCP 接入，Tls 和 Stream 负责每条连接的 TLS 会话与组合层限制。 AcceptQueueLimit 只限制完成握手但尚未被 pull/Future 消费的连接； HandshakeLimit 在分配 TLS 会话前硬性限制并发握手数。 初始化默认完成队列 1024 条、并发握手 128 条，均可显式调整。

```c
typedef struct xtlslistenerconfig {
	xnetlistenconfig Listen;
	xtlsserverconfig Tls;
	xtlsstreamconfig Stream;
	uint32 AcceptQueueLimit;
	uint32 HandshakeLimit;
} xtlslistenerconfig;
```

| 字段 | 类型 | 语义 |
|---|---|---|
| `Listen` | `xnetlistenconfig` | Listen |
| `Tls` | `xtlsserverconfig` | Tls |
| `Stream` | `xtlsstreamconfig` | 流选择 |
| `AcceptQueueLimit` | `uint32` | AcceptQueueLimit |
| `HandshakeLimit` | `uint32` | HandshakeLimit |

### `xtlslistenerstats`

统计值均为并发快照，累计计数在关闭后仍可读取。

```c
typedef struct xtlslistenerstats {
	xtlslistenerstate State;
	uint64 Handshakes;
	uint64 Accepted;
	uint64 Rejected;
	uint64 HandshakeErrors;
	uint32 ActiveHandshakes;
	uint32 PeakHandshakes;
	uint32 QueuedAccepts;
	uint32 PeakQueuedAccepts;
	uint32 AcceptWaiters;
} xtlslistenerstats;
```

| 字段 | 类型 | 语义 |
|---|---|---|
| `State` | `xtlslistenerstate` | 状态 |
| `Handshakes` | `uint64` | Handshakes |
| `Accepted` | `uint64` | Accepted |
| `Rejected` | `uint64` | Rejected |
| `HandshakeErrors` | `uint64` | HandshakeErrors |
| `ActiveHandshakes` | `uint32` | ActiveHandshakes |
| `PeakHandshakes` | `uint32` | PeakHandshakes |
| `QueuedAccepts` | `uint32` | QueuedAccepts |
| `PeakQueuedAccepts` | `uint32` | PeakQueuedAccepts |
| `AcceptWaiters` | `uint32` | AcceptWaiters |

### `xtlsstream`

公开句柄声明不随 TLS Stream 实现裁剪变化。

```c
typedef struct xtlsstream xtlsstream;
```

不透明句柄或别名；生命周期与所有权见各使用方 API 节。

### `xtlslistener`

TLS 监听器（不透明）：在 TCP 监听器上完成 TLS 接受。


```c
typedef struct xtlslistener xtlslistener;
```

不透明句柄或别名；生命周期与所有权见各使用方 API 节。

### `xtlsdial`

托管 TLS 拨号对象（不透明）：串联 TCP 拨号与 TLS 握手。


```c
typedef struct xtlsdial xtlsdial;
```

不透明句柄或别名；生命周期与所有权见各使用方 API 节。

### `xtlsdialproc`

成功回调接管 TLS Stream 引用；失败时 Stream 为空且 Error 只在回调期间借用。

```c
typedef void (*xtlsdialproc)(
	xtlsdial* pDial,
	xnetresult Result,
	xtlsstream* pStream,
	const xerror* pError,
	ptr pData
);
```

回调类型；参数与返回语义见签名及各使用方 API 节。

### `xtlsverifydecision`

自定义验证过程可以接管信任决策，也可以回退到不可变信任库。

```c
typedef enum xtlsverifydecision {
	XTLS_VERIFY_ERROR = -1,
	XTLS_VERIFY_DEFAULT = 0,
	XTLS_VERIFY_ACCEPT,
	XTLS_VERIFY_REJECT
} xtlsverifydecision;
```

| 值 | 语义 |
|---|---|
| `XTLS_VERIFY_ERROR` | 失败 |
| `XTLS_VERIFY_DEFAULT` | 默认值 |
| `XTLS_VERIFY_ACCEPT` | ACCEPT |

### `xtlspeer`

对端证书视图仅在验证调用期间有效，证书按叶到根的线路顺序排列。

```c
typedef struct xtlspeer {
	xtlsrole Role;
	xstrview Name;
	xtime Time;
	const xx509cert* Certificates;
	size_t CertificateCount;
} xtlspeer;
```

| 字段 | 类型 | 语义 |
|---|---|---|
| `Role` | `xtlsrole` | 角色 |
| `Name` | `xstrview` | 名称 |
| `Time` | `xtime` | 时间戳（Unix 微秒） |
| `Certificates` | `const xx509cert*` | Certificates |
| `CertificateCount` | `size_t` | CertificateCount |

### `xtlsverifiedpeer`

已验证路径按叶到根排列但不包含独立 trust anchor，全部视图只在策略回调期间有效。

```c
typedef struct xtlsverifiedpeer {
	const xtlspeer* Peer;
	const xx509cert* const* Path;
	size_t PathCount;
	const xx509anchor* Anchor;
} xtlsverifiedpeer;
```

| 字段 | 类型 | 语义 |
|---|---|---|
| `Peer` | `const xtlspeer*` | Peer |
| `Path` | `const xx509cert* const*` | 路径 |
| `PathCount` | `size_t` | PathCount |
| `Anchor` | `const xx509anchor*` | Anchor |

### `xtlsverifierconfig`

创建时深复制可选信任库并借用回调；成功后接管上下文。

```c
typedef struct xtlsverifierconfig {
	const xx509store* Store;
	xtlsverifyproc Verify;
	xtlsverifypolicyproc Policy;
	xtlsverifytimeproc Time;
	xtlsverifyreleaseproc Release;
	ptr Context;
	/* 默认 false；只在必须兼容历史证书链时显式允许 SHA-1。 */
	bool AllowSha1;
} xtlsverifierconfig;
```

| 字段 | 类型 | 语义 |
|---|---|---|
| `Store` | `const xx509store*` | Store |
| `Verify` | `xtlsverifyproc` | Verify |
| `Policy` | `xtlsverifypolicyproc` | 策略 |
| `Time` | `xtlsverifytimeproc` | 时间戳（Unix 微秒） |
| `Release` | `xtlsverifyreleaseproc` | Release |
| `Context` | `ptr` | 回调上下文 |
| `AllowSha1` | `bool` | AllowSha1 |

### `xtlsverifyproc`

自定义验证过程必须允许并发调用，不负责 TLS 握手签名验证。

```c
typedef xtlsverifydecision (*xtlsverifyproc)(
	const xtlspeer* pPeer,
	ptr pContext
);
```

回调类型；参数与返回语义见签名及各使用方 API 节。

### `xtlsverifypolicyproc`

默认链和身份验证成功后执行附加策略；返回 false 时可以设置结构化原因。

```c
typedef bool (*xtlsverifypolicyproc)(
	const xtlsverifiedpeer* pPeer,
	ptr pContext
);
```

回调类型；参数与返回语义见签名及各使用方 API 节。

### `xtlsverifytimeproc`

自定义时间源用于确定性测试、重放验证和受控时钟环境。

```c
typedef xtime (*xtlsverifytimeproc)(ptr pContext);
```

回调类型；参数与返回语义见签名及各使用方 API 节。

### `xtlsverifyreleaseproc`

最后一个验证器引用释放时清理调用方上下文。

```c
typedef void (*xtlsverifyreleaseproc)(ptr pContext);
```

回调类型；参数与返回语义见签名及各使用方 API 节。

### 常量总表

| 常量 | 值 | 语义 |
|---|---|---|
| `XTLS_EXTENSION_DATA_MAX` | `65535u` | 单个 TLS 扩展负载受线路 16 位长度字段限制。 |
| `XTLS13_TICKET_LIFETIME_MAX` | `604800u` | TLS 1.3 会话票据最长只能在七天内恢复。 |
| `XTLS_HANDSHAKE_HEADER_SIZE` | `4u` | TLS 握手消息头由类型和 24 位正文长度组成。 |
| `XTLS_HANDSHAKE_BODY_MAX` | `16777215u` | TLS 握手正文受线路 24 位长度字段限制。 |
| `XTLS_HANDSHAKE_LIMIT_DEFAULT` | `1048576u` | 默认单条握手消息上限为 1 MiB，远低于 24 位线路极限。 |
| `XTLS_EXTENSION_HEADER_SIZE` | `4u` | TLS 扩展头由 16 位类型和 16 位负载长度组成。 |
| `XTLS_HANDSHAKE_RETAIN_DEFAULT` | `16384u` | 默认只跨消息保留最多 16 KiB 已分配缓冲。 |
| `XTLS_RANDOM_SIZE` | `32u` | ClientHello 与 ServerHello 的 random 字段固定为 32 字节。 |
| `XTLS_SESSION_ID_MAX` | `32u` | 兼容会话标识受 TLS 握手线路格式限制为最多 32 字节。 |
| `XTLS_FALLBACK_SCSV` | `UINT16_C(0x5600)` | TLS_FALLBACK_SCSV 是 ClientHello 中的信号值，不属于可协商密码套件。 |
| `XTLS_FEED_LIMIT_DEFAULT` | `262144u` | 默认队列上限容纳多个完整 TLS 记录，但不会按连接预分配。 |
| `XTLS_SEND_LIMIT_DEFAULT` | `262144u` | XTLSSEND超限默认值 |
| `XTLS_PLAIN_LIMIT_DEFAULT` | `262144u` | XTLSPLAIN超限默认值 |
| `XTLS_DRIVE_RECORD_BUDGET_DEFAULT` | `64u` | 单次驱动预算限制一个连接连续占用事件循环的工作量。 |
| `XTLS_DRIVE_HANDSHAKE_BUDGET_DEFAULT` | `64u` | XTLSDRIVE握手阶段BUDGET默认值 |
| `XTLS_CLIENT_RESUME_LIMIT_DEFAULT` | `4u` | 客户端默认保留四张票据，兼顾并行恢复与每连接常驻内存。 |
| `XTLS_CLIENT_RESUME_LIMIT_MAX` | `64u` | 显式队列上限避免不可信服务端用连续票据放大内存占用。 |
| `XTLS_SERVER_RESUME_AGE_TOLERANCE_DEFAULT` | `10000u` | XTLS服务端角色RESUMEAGETOLERANCE默认值 |
| `XTLS_SERVER_TICKET_LIFETIME_DEFAULT` | `86400u` | XTLS服务端角色TICKETLIFETIME默认值 |
| `XTLS_SERVER_TICKET_SIZE_DEFAULT` | `32u` | XTLS服务端角色TICKET尺寸默认值 |
| `XTLS_STREAM_HANDSHAKE_TIMEOUT_DEFAULT` | `UINT64_C(10000000)` | XTLSSTREAM握手阶段超时默认值 |
| `XTLS_STREAM_CLOSE_TIMEOUT_DEFAULT` | `UINT64_C(5000000)` | XTLSSTREAMCLOSE超时默认值 |
| `XTLS_STREAM_ASYNC_BYTES_DEFAULT` | `((size_t)1048576u)` | XTLSSTREAMASYNCBYTES默认值 |
| `XTLS_STREAM_ASYNC_COUNT_DEFAULT` | `UINT32_C(1024)` | XTLSSTREAMASYNC数量默认值 |
| `XTLS_STREAM_ASYNC_BATCH_DEFAULT` | `UINT32_C(64)` | XTLSSTREAMASYNCBATCH默认值 |

## 裁剪

启用宏：

```c
#define XRT_FEATURE_TLS
```

直接依赖核心模块中的结构化错误体系；核心模块不可裁剪，因此不需要额外启用宏。

记录保护层继续细分：

| 宏 | 功能 | 直接依赖 |
| --- | --- | --- |
| `XRT_FEATURE_TLS_SESSION` | 无 socket 的公共会话生命周期、惰性有界队列、所有权输入与 Span 输出 | `tls_context`、`tls_record`、`tls_messages`、`tls_schedule`、`net_buffer`、`crypto_core` |
| `XRT_FEATURE_TLS_CLIENT` | 独立客户端配置、真实后端能力过滤和初始 ClientHello | `tls_session`、`tls_hello_write`、`tls_handshake_reader`、`tls_schedule`、`tls_key_exchange`、`random_secure` |
| `XRT_FEATURE_TLS_SERVER` | 独立服务端配置、SNI/ALPN、身份选择、TLS 1.3 证书航班和 READY 状态 | `tls_session`、`tls_hello_write`、`tls_messages_write`、`tls_schedule`、`tls_key_exchange`、`tls_identity`、`random_secure` |
| `XRT_FEATURE_TLS_VERIFY` | 不可变信任快照、默认对端验证、自定义信任决策和 TLS 1.3 握手验签 | `tls_negotiate`、`x509_store`、`x509_identity`、`x509_verify`、`crypto_core` |
| `XRT_FEATURE_TLS_CLIENT_VERIFY` | 客户端 Certificate 与 CertificateVerify 状态 | `tls_client`、`tls_verify` 及所选 X.509 密码后端 |
| `XRT_FEATURE_TLS_RESUME` | 不可变 TLS 1.3 ticket/PSK 恢复资产、有效期与票据年龄 | `tls`、`time`、`crypto_core` |
| `XRT_FEATURE_TLS_CLIENT_RESUME` | 客户端恢复主密钥、票据 PSK 派生、ClientHello binder 和有界显式交接 | `tls_client_verify`、`tls_resume`、`tls_psk_write`、`crypto_sha256` |
| `XRT_FEATURE_TLS_SERVER_RESUME` | 服务端外部票据查找、严格 binder 校验、票据签发和恢复对象交接 | `tls_server`、`tls_resume`、`tls_psk`、`tls_psk_write` |
| `XRT_FEATURE_TLS_STREAM` | TCP 与 TLS 会话组合流、双层背压、阶段超时和认证关闭 | `net_tcp`、`tls_client`、`tls_server` |
| `XRT_FEATURE_TLS_STREAM_FUTURE` | TLS Stream 的发送、接收和条件等待 Future | `tls_stream`、`future` |
| `XRT_FEATURE_TLS_STREAM_DIAL` | 主机解析、TCP 地址竞速和 TLS 握手组成的受管客户端拨号 | `tls_stream`、`net_tcp_dial` |
| `XRT_FEATURE_TLS_STREAM_DIAL_FUTURE` | 受管 TLS Dial 的 Future 结果与协作取消 | `tls_stream_dial`、`future` |
| `XRT_FEATURE_TLS_IDENTITY` | 不可变证书链与外部签名器核心 | `tls_negotiate`、`x509_parse`、`crypto_core` |
| `XRT_FEATURE_TLS_RECORD` | 单向密钥、序列号、nonce、TLS 1.2/1.3 记录保护骨架 | `tls`、`crypto_core` |
| `XRT_FEATURE_TLS_RECORD_AES` | AES-128-GCM、AES-256-GCM 记录后端 | `tls_record`、`crypto_aes_gcm` |
| `XRT_FEATURE_TLS_RECORD_CHACHA` | ChaCha20-Poly1305 记录后端 | `tls_record`、`crypto_chacha20_poly1305` |
| `XRT_FEATURE_TLS_HANDSHAKE` | 握手消息与扩展 framing | `tls` |
| `XRT_FEATURE_TLS_HANDSHAKE_READER` | 跨记录握手消息的有界自适应重组 | `tls_handshake` |
| `XRT_FEATURE_TLS_HELLO` | 扩展游标、SNI、ALPN、版本、组、签名、key_share 与 Hello 严格解析 | `tls_handshake` |
| `XRT_FEATURE_TLS_NEGOTIATE` | 无状态版本、套件、签名与 key_share 选择 | `tls_hello` |
| `XRT_FEATURE_TLS_POLICY` | 客户端与服务端共享的有序协议偏好及完整校验 | `tls_negotiate`、`tls_key_exchange` |
| `XRT_FEATURE_TLS_CONTEXT` | 深拷贝策略、硬限制与公平性预算的共享只读快照 | `tls_policy` |
| `XRT_FEATURE_TLS_KEY_EXCHANGE` | 命名组元数据、后端探测与调用方缓冲密钥交换骨架 | `tls` |
| `XRT_FEATURE_TLS_KEY_EXCHANGE_X25519` | X25519 临时密钥与共享秘密后端 | `tls_key_exchange`、`crypto_x25519_keypair` |
| `XRT_FEATURE_TLS_KEY_EXCHANGE_X448` | X448 临时密钥与共享秘密后端 | `tls_key_exchange`、`crypto_x448_keypair` |
| `XRT_FEATURE_TLS_KEY_EXCHANGE_P256` | secp256r1 临时密钥与共享秘密后端 | `tls_key_exchange`、`crypto_p256_keypair` |
| `XRT_FEATURE_TLS_KEY_EXCHANGE_P384` | secp384r1 临时密钥与共享秘密后端 | `tls_key_exchange`、`crypto_p384_keypair` |
| `XRT_FEATURE_TLS_HELLO_WRITE` | 增量扩展 writer 与 ClientHello/ServerHello 正文编码 | `tls_hello` |
| `XRT_FEATURE_TLS_MESSAGES` | Certificate、EncryptedExtensions、CertificateVerify、Finished、KeyUpdate 与票据严格解析 | `tls_hello` |
| `XRT_FEATURE_TLS_MESSAGES_WRITE` | 握手语义消息的调用方缓冲编码 | `tls_messages` |
| `XRT_FEATURE_TLS_AUTH_MESSAGES` | CertificateRequest、TLS 1.2 ECDHE、OCSP 状态与压缩证书 framing | `tls_messages` |
| `XRT_FEATURE_TLS_AUTH_MESSAGES_WRITE` | 认证消息与证书颁发者向量的调用方缓冲编码 | `tls_auth_messages` |
| `XRT_FEATURE_TLS_SCHEDULE` | transcript、TLS 1.2 PRF、TLS 1.3 密钥调度骨架 | `tls`、`crypto_core` |
| `XRT_FEATURE_TLS_SCHEDULE_SHA256` | SHA-256 transcript、HMAC 与 HKDF 后端 | `tls_schedule`、`crypto_hkdf_sha256` |
| `XRT_FEATURE_TLS_SCHEDULE_SHA384` | SHA-384 transcript、HMAC 与 HKDF 后端 | `tls_schedule`、`crypto_hkdf_sha512` |

只需要通用容器、文件或明文网络时可以完全裁掉 TLS。会话、握手、恢复、TCP 适配和主机名拨号都使用独立细粒度宏，不会被基础记录解析强制带入。

## 协议范围

XRT 只协商 TLS 1.2 和 TLS 1.3，默认优先 TLS 1.3，不提供 SSLv2、SSLv3、TLS 1.0 或 TLS 1.1 兼容路径。记录头中的 `legacy_version` 是线路兼容字段，不等于最终协商版本。

当前密码范围只保留 AEAD 与前向保密路径：TLS 1.3 支持 AES-128-GCM、AES-256-GCM 和 ChaCha20-Poly1305；TLS 1.2 支持 ECDHE-ECDSA/ECDHE-RSA 配合上述 AEAD。静态 RSA、CBC、RC4、3DES 和 TLS 压缩不进入新体系。`xrtTlsCipherName()` 返回可用于日志的标准套件名。

`xrtTlsCipherInfo()` 返回进程期只读的 `xtlscipherinfo`，把每个套件唯一的版本、摘要、AEAD、TLS 1.2 认证类型、密钥、静态 IV、显式 nonce 和认证标签长度集中表达。未知套件返回 `NULL` 且不设置错误，适合策略探测；调用方不能释放或修改返回对象。TLS 1.3 的 `Authentication` 固定为 `XTLS_CIPHER_AUTH_INDEPENDENT`，因为证书或 PSK 认证不再由密码套件决定。

```c
const xtlscipherinfo* Info = xrtTlsCipherInfo(
	XTLS_AES_128_GCM_SHA256
);

if ( (Info != NULL) && (Info->Version == XTLS_VERSION_13) ) {
	/* Info->KeySize 为 16，Info->IvSize 为 12。 */
}
```

`xtlshash` 和 `xtlsaead` 描述协议选择，不要求启用具体密码后端；`xtlscipherauth` 只表达 TLS 1.2 套件认证约束。记录保护、密钥调度和协商层共享这份元数据，避免各自维护一套容易漂移的套件表。

基础限制：

| 常量 | 含义 |
| --- | --- |
| `XTLS_RECORD_HEADER_SIZE` | 固定 5 字节记录头 |
| `XTLS_RECORD_PLAINTEXT_MAX` | 16384 字节明文上限 |
| `XTLS12_RECORD_CIPHERTEXT_MAX` | 18432 字节 TLS 1.2 密文上限 |
| `XTLS13_RECORD_CIPHERTEXT_MAX` | 16640 字节 TLS 1.3 密文上限 |
| `XTLS13_INNER_PLAINTEXT_MAX` | 16385 字节内容、类型与零填充总上限 |
| `XTLS_AES_GCM_RECORD_LIMIT` | 单组 AES-GCM 流量密钥最多处理 `2^24` 条记录 |

## 结果与错误

`xtlsresult` 把正常控制流与错误分开：

- `XTLS_OK`：操作完成。
- `XTLS_AGAIN`：输入尚不完整，继续喂入数据即可，不设置线程错误。
- `XTLS_CLOSED`：收到或完成协议级干净关闭。
- `XTLS_ERROR`：协议或调用失败，使用 `xrtGetError()` 读取 `xrt.tls` 结构化错误。

`xtlserror` 进一步区分版本、记录类型、记录长度、Alert、状态、密码、证书、校验和恢复阶段。调用方不需要解析错误字符串。

### `xrtTlsRecordEncode`

把一条记录编码到调用方缓冲；输入与输出允许重叠。

```c
bool xrtTlsRecordEncode(xtlsrecordtype Type, uint16 iLegacyVersion, xbytesview Payload, ptr pOutput, size_t iOutputSize)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `Type` | 输入 | — | 类型标识 |
| `iLegacyVersion` | 输入 | — | 旧版版本号 |
| `Payload` | 输入 | — | 载荷 |
| `pOutput` | 输入 | — | 输出缓冲 |
| `iOutputSize` | 输入 | — | 接收输出字节数 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 成功 | — |
| `false` | 失败 | 见错误 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_PROTOCOL`（TLS 语义） — DER 或消息结构非法
- `XERR_RANGE` — 容量不足，不写半个结果

#### 范例

[record](../../examples/tls/record/main.c) · record

```c
	if ( !xrtTlsRecordEncode(
		XTLS_RECORD_APPLICATION_DATA,
		UINT16_C(0x0303),
		XRT_BYTES_LITERAL("example"),
		Buffer,
		sizeof(Buffer)
	) ) {
```

### `xrtTlsRecordName`

返回记录内容类型的稳定英文名称，未知值返回 unknown。

```c
cstr xrtTlsRecordName(xtlsrecordtype Type)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `Type` | 输入 | — | 类型标识 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | 静态名称；未知为 `"UNKNOWN"` | — |

#### 错误

- 无 — 纯查询，不设置错误

#### 范例

[record](../../examples/tls/record/main.c) · record

```c
		xrtTlsRecordName(Record.Type),
```

### `xrtTlsRecordParse`

解析输入开头的一条完整记录。输入不足返回 XTLS_AGAIN，Required 返回继续解析所需的总字节数； 只有返回 XTLS_OK 时才写入 Record。

```c
xtlsresult xrtTlsRecordParse(xbytesview Input, xtlsrecord* pRecord, size_t* pRequired)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `Input` | 输入 | — | 输入数据 |
| `pRecord` | 输入 | 非空 | 记录描述 |
| `pRequired` | 输入 | 非空 | 接收必需类型 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `XTLS_OK` | 成功 | — |
| `XTLS_AGAIN` | 暂不可推进 | 不设错误 |
| `XTLS_CLOSED` | 已关闭 | 不设错误 |
| `XTLS_ERROR` | 失败 | 见错误 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_PROTOCOL`（TLS 语义） — DER 或消息结构非法

#### 范例

[record](../../examples/tls/record/main.c) · record

```c
	if ( xrtTlsRecordParse(
		(xbytesview) { Buffer, 12 }, &Record, NULL
	) != XTLS_OK ) {
```

### `xrtTlsRecordSize`

返回给定负载所需的完整记录长度，负载越界时返回零并设置错误。

```c
size_t xrtTlsRecordSize(size_t iPayloadSize)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `iPayloadSize` | 输入 | — | 载荷长度 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `>= 0` | 数量或字节数 | — |

#### 错误

- 无 — 纯查询，不设置错误

#### 范例

[message_tour](../../examples/tls/message_tour/main.c) · record

```c
	if ( (xrtTlsRecordSize(100u) != 105u) ) {
```

## 记录解析

```c
xtlsrecord Record;
size_t Required;
xtlsresult Result = xrtTlsRecordParse(Input, &Record, &Required);
```

解析器适合任意分片方式：

- 输入不足 5 字节时，返回 `XTLS_AGAIN`，`Required` 为 5。
- 已有完整头但负载不足时，返回 `XTLS_AGAIN`，`Required` 为整条记录长度。
- 只有 `XTLS_OK` 会写入 `Record`。
- `Record.Payload` 借用输入内存，不分配、不复制。
- 输入中包含多条记录时只返回第一条，`EncodedSize` 给出应消费的字节数。

未知内容类型、非法兼容版本和超过 TLS 1.2 密文硬上限的长度会立即返回协议错误，不会等待攻击者声明的超大负载。

## 记录编码

```c
uint8 Buffer[64];

bool OK = xrtTlsRecordEncode(
	XTLS_RECORD_APPLICATION_DATA,
	UINT16_C(0x0303),
	XRT_BYTES_LITERAL("hello"),
	Buffer,
	sizeof(Buffer)
);
```

编码器允许输入与输出重叠，适合在已有明文前原地腾出记录头。容量不足时输出保持不变。`xrtTlsRecordSize()` 可提前计算总长度。

记录编码器是协议工具，不会绕过后续会话层的加密、序列号和状态检查；应用数据应优先通过 TLS 会话写入。

### `xrtTlsHandshakeEncode`

把握手类型和正文编码到调用方缓冲，允许输入输出重叠。

```c
bool xrtTlsHandshakeEncode(xtlshandshaketype Type, xbytesview Body, void* pOutput, size_t iOutputSize)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `Type` | 输入 | — | 类型标识 |
| `Body` | 输入 | — | 消息体 |
| `pOutput` | 输入 | 非空 | 输出缓冲 |
| `iOutputSize` | 输入 | — | 接收输出字节数 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 成功 | — |
| `false` | 失败 | 见错误 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_PROTOCOL`（TLS 语义） — DER 或消息结构非法
- `XERR_RANGE` — 容量不足，不写半个结果

#### 范例

[message_tour](../../examples/tls/message_tour/main.c) · handshake

```c
	if ( !xrtTlsHandshakeEncode(XTLS_HANDSHAKE_CLIENT_HELLO,
			(xbytesview) { arrBody, 4u }, arrOut, 8u) ) {
```

### `xrtTlsHandshakeName`

返回握手类型的稳定英文名称，未知线路值返回 unknown_handshake。

```c
cstr xrtTlsHandshakeName(xtlshandshaketype Type)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `Type` | 输入 | — | 类型标识 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | 静态名称；未知为 `"UNKNOWN"` | — |

#### 错误

- 无 — 纯查询，不设置错误

#### 范例

[message_tour](../../examples/tls/message_tour/main.c) · handshake

```c
		(strcmp(xrtTlsHandshakeName((xtlshandshaketype)9999u),
			"unknown_handshake") != 0) ||
```

### `xrtTlsHandshakeParse`

分片感知地解析输入开头的一条握手消息，仅成功时发布借用视图。

```c
xtlsresult xrtTlsHandshakeParse(xbytesview Input, xtlshandshake* pHandshake, size_t* pRequired)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `Input` | 输入 | — | 输入数据 |
| `pHandshake` | 输入 | 非空 | 握手消息描述 |
| `pRequired` | 输入 | 非空 | 接收必需类型 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `XTLS_OK` | 成功 | — |
| `XTLS_AGAIN` | 暂不可推进 | 不设错误 |
| `XTLS_CLOSED` | 已关闭 | 不设错误 |
| `XTLS_ERROR` | 失败 | 见错误 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_PROTOCOL`（TLS 语义） — DER 或消息结构非法

#### 范例

[message_tour](../../examples/tls/message_tour/main.c) · handshake

```c
		if ( (xrtTlsHandshakeParse(
				(xbytesview) { arrOut, 3u },
				&Handshake, &iRequired) != XTLS_AGAIN) ||
			(iRequired != 4u) ) {
```

### `xrtTlsHandshakeReaderConfigInit`

填充安全的默认握手 reader 配置。

```c
void xrtTlsHandshakeReaderConfigInit(xtlshandshakereaderconfig* pConfig)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pConfig` | 输入 | — | 配置 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 无 | — | — |

#### 错误

- 无 — 初始化不失败

#### 范例

[message_tour](../../examples/tls/message_tour/main.c) · handshake

```c
	xrtTlsHandshakeReaderConfigInit(&ReaderConfig);
```

### `xrtTlsHandshakeReaderInit`

初始化 reader；Config 为空时使用默认上限与保留容量。

```c
bool xrtTlsHandshakeReaderInit(xtlshandshakereader* pReader, const xtlshandshakereaderconfig* pConfig)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pReader` | 输入/输出 | 非空 | 握手读取器 |
| `pConfig` | 输入 | — | 配置 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 成功 | — |
| `false` | 失败 | 见错误 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_PROTOCOL`（TLS 语义） — DER 或消息结构非法

#### 范例

[message_tour](../../examples/tls/message_tour/main.c) · handshake

```c
	if ( !xrtTlsHandshakeReaderInit(&Reader, &ReaderConfig) ) {
```

### `xrtTlsHandshakeReaderRead`

读取至多一条握手消息并返回本次消费的输入字节数。完整单片消息直接借用 Input；跨分片消息借用 Reader，直到下次 Read、Reset 或 Unit。

```c
xtlsresult xrtTlsHandshakeReaderRead(xtlshandshakereader* pReader, xbytesview Input, size_t* pConsumed, xtlshandshake* pMessage)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pReader` | 输入/输出 | 非空 | 握手读取器 |
| `Input` | 输入 | — | 输入数据 |
| `pConsumed` | 输入 | 非空 | 接收消费字节数 |
| `pMessage` | 输入 | 非空 | 握手消息 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `XTLS_OK` | 成功 | — |
| `XTLS_AGAIN` | 暂不可推进 | 不设错误 |
| `XTLS_CLOSED` | 已关闭 | 不设错误 |
| `XTLS_ERROR` | 失败 | 见错误 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_PROTOCOL`（TLS 语义） — DER 或消息结构非法

#### 范例

[message_tour](../../examples/tls/message_tour/main.c) · handshake

```c
	if ( (xrtTlsHandshakeReaderRead(&Reader,
			(xbytesview) { arrHandshake, 1u },
			&iConsumed, &ReaderMsg) != XTLS_AGAIN) ||
		(iConsumed != 1u) ||
		(xrtTlsHandshakeReaderRequired(&Reader) != 4u) ) {
```

### `xrtTlsHandshakeReaderRequired`

返回完成当前消息所需的完整编码长度；只有部分头时返回 4。

```c
size_t xrtTlsHandshakeReaderRequired(const xtlshandshakereader* pReader)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pReader` | 输入/输出 | 非空 | 握手读取器 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `>= 0` | 数量或字节数 | — |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法

#### 范例

[message_tour](../../examples/tls/message_tour/main.c) · handshake

```c
		(xrtTlsHandshakeReaderRequired(&Reader) != 4u) ) {
```

### `xrtTlsHandshakeReaderReset`

丢弃当前消息，保留不超过配置阈值的缓冲。

```c
bool xrtTlsHandshakeReaderReset(xtlshandshakereader* pReader)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pReader` | 输入/输出 | 非空 | 握手读取器 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 成功 | — |
| `false` | 失败 | 见错误 |

#### 错误

- 无 — 释放或重置不失败（Reset 系列见参数约束）

#### 范例

[message_tour](../../examples/tls/message_tour/main.c) · handshake

```c
	if ( !xrtTlsHandshakeReaderReset(&Reader) ) {
```

### `xrtTlsHandshakeReaderUnit`

释放 reader 持有的重组缓冲并清零结构。

```c
void xrtTlsHandshakeReaderUnit(xtlshandshakereader* pReader)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pReader` | 输入/输出 | 非空 | 握手读取器 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 无 | — | — |

#### 错误

- 无 — 释放或重置不失败（Reset 系列见参数约束）

#### 范例

[message_tour](../../examples/tls/message_tour/main.c) · handshake

```c
	xrtTlsHandshakeReaderUnit(&Reader);
```

### `xrtTlsHandshakeSize`

返回给定正文所需的完整握手消息长度，越界时返回零。

```c
size_t xrtTlsHandshakeSize(size_t iBodySize)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `iBodySize` | 输入 | — | 消息体长度 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `>= 0` | 数量或字节数 | — |

#### 错误

- 无 — 纯查询，不设置错误

#### 范例

[message_tour](../../examples/tls/message_tour/main.c) · handshake

```c
	if ( (xrtTlsHandshakeSize(4u) != 8u) ||
		(xrtTlsHandshakeSize(SIZE_MAX) != 0u) ) {
```

## 握手与扩展 Framing

`tls_handshake` 公开握手消息和单个扩展的零分配线路原语。它只处理 framing，不在这一层判断某种消息或扩展能否出现在当前握手状态：

```c
xtlshandshake Message;
size_t Required;
xtlsresult Result = xrtTlsHandshakeParse(Input, &Message, &Required);
```

- 握手头固定 4 字节，正文长度是 24 位；扩展头固定 4 字节，负载长度是 16 位。
- 输入不足返回 `XTLS_AGAIN` 和继续解析所需的精确总长度，不设置错误，也不修改输出。
- 成功结果借用输入，只消费开头第一条消息或第一个扩展，便于上层连续处理聚合记录。
- 未知握手类型和未知扩展类型会保留线路数值。后续状态机负责按版本、角色和阶段决定忽略、拒绝或交给扩展处理器。
- 编码允许正文或负载与输出精确重叠，容量不足和不可编码长度不会修改输出。
- `xrtTlsHandshakeSize()` 与 `xrtTlsExtensionSize()` 在分配或预留缓冲前执行 24 位、16 位线路上限检查。

通用 framing 不分配声明长度对应的内存。会话层必须另外配置实际握手消息上限，避免对端用接近 16 MiB 的合法 24 位长度占用过多连接内存。ClientHello、ServerHello、SNI、ALPN、supported_versions、key_share 等语义解析建立在这些原语之上，并负责重复扩展和上下文约束。

## 握手消息 Reader

`tls_handshake_reader` 解决握手消息跨 TLS record、跨 socket 读取分片的问题，同时不把每个连接变成固定大缓冲：

```c
xtlshandshakereader Reader;
xrtTlsHandshakeReaderInit(&Reader, NULL);

size_t Consumed;
xtlshandshake Message;
xtlsresult Result = xrtTlsHandshakeReaderRead(
	&Reader, Input, &Consumed, &Message
);
```

- 完整消息已经位于单个 `Input` 时直接返回借用输入的视图，不分配也不复制。
- 1 到 3 字节的分片头只写入 reader 内联的 4 字节 Header；确认正文需要跨输入后才分配重组区。
- 重组区按实际收到的字节以约 1.5 倍增长，不会在看到 24 位声明长度时立即分配整条消息。
- 默认完整编码消息上限是 1 MiB，可通过 `xtlshandshakereaderconfig.Limit` 在 4 字节到线路最大值之间调整。超限头零消费、零分配并保持 reader 不变。
- 默认只跨消息保留不超过 16 KiB 的容量；证书链等大消息在下一次 `Read()` 或 `Reset()` 时释放，不会永久放大每连接常驻内存。
- 一次只发布一条消息，`Consumed` 不跨过下一条聚合消息。跨分片结果借用 reader，生命周期到下一次 `Read()`、`Reset()` 或 `Unit()`；单片结果借用本次 Input。
- reader 自己的可移动分配区不能作为下一段 Input，API 会在 `realloc` 前显式拒绝别名。

`xrtTlsHandshakeReaderRequired()` 返回当前完整消息长度；只有部分头或空闲时返回 4。正常分片返回 `XTLS_AGAIN` 且不设置错误，容量不足、配置错误和超限消息通过结构化 TLS 错误表达。

### `xrtTlsClientHelloEncode`

失败原子地编码 ClientHello 正文；输入字段不得与输出区域重叠。

```c
bool xrtTlsClientHelloEncode(const xtlsclienthello* pHello, void* pOutput, size_t iOutputSize)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pHello` | 输入 | 非空 | Hello 消息 |
| `pOutput` | 输入 | 非空 | 输出缓冲 |
| `iOutputSize` | 输入 | — | 接收输出字节数 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 成功 | — |
| `false` | 失败 | 见错误 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_PROTOCOL`（TLS 语义） — DER 或消息结构非法
- `XERR_RANGE` — 容量不足，不写半个结果

#### 范例

[writer_tour](../../examples/tls/writer_tour/main.c) · extension tour

```c
	if ( !xrtTlsClientHelloEncode(&ClientHello, arrHelloBuf,
			iSize) ||
		!xrtTlsClientHelloParse(
			(xbytesview) { arrHelloBuf, iSize }, &Parsed) ||
		(Parsed.LegacyVersion != 0x0303u) ||
		(Parsed.Random.Size != 32u) ||
		(Parsed.SessionId.Size != 4u) ||
		(Parsed.Extensions.Size == 0u) ) {
```

### `xrtTlsClientHelloParse`

严格解析一条 ClientHello 正文并发布零拷贝字段视图。

```c
bool xrtTlsClientHelloParse(xbytesview Body, xtlsclienthello* pHello)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `Body` | 输入 | — | 消息体 |
| `pHello` | 输入 | 非空 | Hello 消息 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 成功 | — |
| `false` | 失败 | 见错误 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_PROTOCOL`（TLS 语义） — DER 或消息结构非法

#### 范例

[writer_tour](../../examples/tls/writer_tour/main.c) · extension tour

```c
		!xrtTlsClientHelloParse(
			(xbytesview) { arrHelloBuf, iSize }, &Parsed) ||
```

### `xrtTlsClientHelloSize`

返回编码 ClientHello 正文所需的精确长度。

```c
size_t xrtTlsClientHelloSize(const xtlsclienthello* pHello)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pHello` | 输入 | 非空 | Hello 消息 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `>= 0` | 数量或字节数 | — |

#### 错误

- 无 — 纯查询，不设置错误

#### 范例

[writer_tour](../../examples/tls/writer_tour/main.c) · extension tour

```c
	iSize = xrtTlsClientHelloSize(&ClientHello);
```

### `xrtTlsExtensionEncode`

把扩展类型和负载编码到调用方缓冲，允许输入输出重叠。

```c
bool xrtTlsExtensionEncode(xtlsextensiontype Type, xbytesview Data, void* pOutput, size_t iOutputSize)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `Type` | 输入 | — | 类型标识 |
| `Data` | 输入 | — | 数据 |
| `pOutput` | 输入 | 非空 | 输出缓冲 |
| `iOutputSize` | 输入 | — | 接收输出字节数 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 成功 | — |
| `false` | 失败 | 见错误 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_PROTOCOL`（TLS 语义） — DER 或消息结构非法
- `XERR_RANGE` — 容量不足，不写半个结果

#### 范例

[message_tour](../../examples/tls/message_tour/main.c) · extension tour

```c
			!xrtTlsExtensionEncode(
				XTLS_EXTENSION_SUPPORTED_GROUPS,
				(xbytesview) { arrData, 3u }, arrOut,
				7u) ||
```

### `xrtTlsExtensionName`

返回扩展类型的稳定英文名称，未知线路值返回 unknown_extension。

```c
cstr xrtTlsExtensionName(xtlsextensiontype Type)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `Type` | 输入 | — | 类型标识 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | 静态名称；未知为 `"UNKNOWN"` | — |

#### 错误

- 无 — 纯查询，不设置错误

#### 范例

[message_tour](../../examples/tls/message_tour/main.c) · extension tour

```c
		(strcmp(xrtTlsExtensionName(
			XTLS_EXTENSION_SUPPORTED_GROUPS),
			"supported_groups") != 0) ) {
```

### `xrtTlsExtensionParse`

分片感知地解析输入开头的一个扩展，仅成功时发布借用视图。

```c
xtlsresult xrtTlsExtensionParse(xbytesview Input, xtlsextension* pExtension, size_t* pRequired)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `Input` | 输入 | — | 输入数据 |
| `pExtension` | 输入 | 非空 | 扩展描述 |
| `pRequired` | 输入 | 非空 | 接收必需类型 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `XTLS_OK` | 成功 | — |
| `XTLS_AGAIN` | 暂不可推进 | 不设错误 |
| `XTLS_CLOSED` | 已关闭 | 不设错误 |
| `XTLS_ERROR` | 失败 | 见错误 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_PROTOCOL`（TLS 语义） — DER 或消息结构非法

#### 范例

[message_tour](../../examples/tls/message_tour/main.c) · extension tour

```c
			(xrtTlsExtensionParse(
				(xbytesview) { arrOut, 2u },
				&Extension, &iRequired) != XTLS_AGAIN) ||
```

### `xrtTlsExtensionSize`

返回给定负载所需的完整扩展长度，越界时返回零。

```c
size_t xrtTlsExtensionSize(size_t iDataSize)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `iDataSize` | 输入 | — | 数据长度 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `>= 0` | 数量或字节数 | — |

#### 错误

- 无 — 纯查询，不设置错误

#### 范例

[message_tour](../../examples/tls/message_tour/main.c) · extension tour

```c
		if ( (xrtTlsExtensionSize(3u) != 7u) ||
			!xrtTlsExtensionEncode(
				XTLS_EXTENSION_SUPPORTED_GROUPS,
				(xbytesview) { arrData, 3u }, arrOut,
				7u) ||
			(xrtTlsExtensionParse(
				(xbytesview) { arrOut, 2u },
				&Extension, &iRequired) != XTLS_AGAIN) ||
			(iRequired != 4u) ||
			(xrtTlsExtensionParse(
				(xbytesview) { arrOut, 5u },
				&Extension, &iRequired) != XTLS_AGAIN) ||
```

### `xrtTlsExtensionsFind`

完整验证后查找唯一扩展，未找到返回 XTLS_ITEM_DONE。

```c
xtlsitemresult xrtTlsExtensionsFind(xbytesview Extensions, xtlsextensiontype Type, xtlsextension* pExtension)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `Extensions` | 输入 | — | 扩展列表 |
| `Type` | 输入 | — | 类型标识 |
| `pExtension` | 输入 | 非空 | 扩展描述 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 枚举值 | 当前取值 | — |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法

#### 范例

[extension_tour](../../examples/tls/extension_tour/main.c) · extension tour

```c
		(xrtTlsExtensionsFind((xbytesview) { arrExtBlock, 31u },
			XTLS_EXTENSION_SUPPORTED_GROUPS,
			&Extension) != XTLS_ITEM_VALUE) ||
```

### `xrtTlsExtensionsInit`

初始化借用完整扩展向量的游标，不预先扫描输入。

```c
bool xrtTlsExtensionsInit(xtlsextensioncursor* pCursor, xbytesview Extensions)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pCursor` | 输入/输出 | 非空 | 游标 |
| `Extensions` | 输入 | — | 扩展列表 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 成功 | — |
| `false` | 失败 | 见错误 |

#### 错误

- 无 — 初始化不失败

#### 范例

[extension_tour](../../examples/tls/extension_tour/main.c) · extension tour

```c
	if ( !xrtTlsExtensionsInit(&ExtCursor,
			(xbytesview) { arrExtBlock, 31u }) ) {
```

### `xrtTlsExtensionsRead`

读取下一扩展并拒绝任何重复类型；失败时游标与输出保持不变。

```c
xtlsitemresult xrtTlsExtensionsRead(xtlsextensioncursor* pCursor, xtlsextension* pExtension)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pCursor` | 输入/输出 | 非空 | 游标 |
| `pExtension` | 输入 | 非空 | 扩展描述 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 枚举值 | 当前取值 | — |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_PROTOCOL`（TLS 语义） — DER 或消息结构非法

#### 范例

[extension_tour](../../examples/tls/extension_tour/main.c) · extension tour

```c
	while ( xrtTlsExtensionsRead(&ExtCursor, &Extension) ==
		XTLS_ITEM_VALUE ) {
```

### `xrtTlsExtensionsValidate`

完整验证扩展向量的 framing 与类型唯一性。

```c
bool xrtTlsExtensionsValidate(xbytesview Extensions)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `Extensions` | 输入 | — | 扩展列表 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 成功 | — |
| `false` | 失败 | 见错误 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_PROTOCOL`（TLS 语义） — DER 或消息结构非法

#### 范例

[extension_tour](../../examples/tls/extension_tour/main.c) · extension tour

```c
	if ( !xrtTlsExtensionsValidate(
			(xbytesview) { arrExtBlock, 31u }) ||
		(xrtTlsExtensionsFind((xbytesview) { arrExtBlock, 31u },
			XTLS_EXTENSION_SUPPORTED_GROUPS,
			&Extension) != XTLS_ITEM_VALUE) ||
		(Extension.Data.Size != 8u) ) {
```

### `xrtTlsServerHelloEncode`

失败原子地编码 ServerHello 正文；输入字段不得与输出区域重叠。

```c
bool xrtTlsServerHelloEncode(const xtlsserverhello* pHello, void* pOutput, size_t iOutputSize)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pHello` | 输入 | 非空 | Hello 消息 |
| `pOutput` | 输入 | 非空 | 输出缓冲 |
| `iOutputSize` | 输入 | — | 接收输出字节数 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 成功 | — |
| `false` | 失败 | 见错误 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_PROTOCOL`（TLS 语义） — DER 或消息结构非法
- `XERR_RANGE` — 容量不足，不写半个结果

#### 范例

[writer_tour](../../examples/tls/writer_tour/main.c) · extension tour

```c
	if ( !xrtTlsServerHelloEncode(&ServerHello, arrHelloBuf,
			iSize) ||
		!xrtTlsServerHelloParse(
			(xbytesview) { arrHelloBuf, iSize },
			&ServerParsed) ||
		(ServerParsed.CipherSuite != 0x1301u) ||
		(ServerParsed.Retry) ||
		(ServerParsed.Extensions.Size == 0u) ) {
```

### `xrtTlsServerHelloParse`

严格解析一条 ServerHello 或 HelloRetryRequest 正文。

```c
bool xrtTlsServerHelloParse(xbytesview Body, xtlsserverhello* pHello)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `Body` | 输入 | — | 消息体 |
| `pHello` | 输入 | 非空 | Hello 消息 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 成功 | — |
| `false` | 失败 | 见错误 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_PROTOCOL`（TLS 语义） — DER 或消息结构非法

#### 范例

[writer_tour](../../examples/tls/writer_tour/main.c) · extension tour

```c
		!xrtTlsServerHelloParse(
			(xbytesview) { arrHelloBuf, iSize },
			&ServerParsed) ||
```

### `xrtTlsServerHelloSize`

返回编码 ServerHello 或 HelloRetryRequest 正文所需的精确长度。

```c
size_t xrtTlsServerHelloSize(const xtlsserverhello* pHello)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pHello` | 输入 | 非空 | Hello 消息 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `>= 0` | 数量或字节数 | — |

#### 错误

- 无 — 纯查询，不设置错误

#### 范例

[writer_tour](../../examples/tls/writer_tour/main.c) · extension tour

```c
	iSize = xrtTlsServerHelloSize(&ServerHello);
```

## Hello 与核心扩展

`tls_hello` 在 framing 之上提供三层能力：

1. `xtlsextensioncursor`、`xrtTlsExtensionsRead()` 和 `xrtTlsExtensionsFind()` 遍历完整扩展向量，保留未知类型，并严格拒绝任何重复扩展。
2. `xrtTlsServerNames()`、`xrtTlsProtocols()`、`xrtTlsClientVersions()`、`xrtTlsGroups()`、`xrtTlsSignatures()` 和 key_share API 解析独立扩展数据。游标和结果都借用输入，不把 SNI、ALPN 或公钥复制到固定数组。
3. `xrtTlsClientHelloParse()` 与 `xrtTlsServerHelloParse()` 一次验证完整正文并发布易用视图。声明长度必须恰好消费正文，已知扩展采用精确长度规则，HelloRetryRequest 会由固定 random 自动识别。

```c
xtlsclienthello Hello;

if ( !xrtTlsClientHelloParse(Handshake.Body, &Hello) ) {
	return false;
}

xtlsextension Sni;
if ( xrtTlsExtensionsFind(
	Hello.Extensions, XTLS_EXTENSION_SERVER_NAME, &Sni
) == XTLS_ITEM_VALUE ) {
	xbytesview Host;
	if ( xrtTlsHostName(Sni.Data, &Host) == XTLS_ITEM_VALUE ) {
		/* Host 借用 ClientHello 输入。 */
	}
}
```

核心语义边界：

- 扩展、SNI 名称类型、supported_versions、supported_groups、signature_algorithms 和客户端 key_share 都检查重复项。
- ALPN `ProtocolName` 是非空不透明字节，不强加 ASCII、逗号分隔或末尾零字符限制；服务端选择必须恰好包含一个协议。
- `xrtTlsProtocolFind()` 区分找到、未找到和畸形列表；`xrtTlsProtocolSelect()` 按服务端偏好列表顺序选择双方第一个交集，不内置 HTTP 协议优先级。
- ClientHello 密码套件向量必须非空且为偶数字节，压缩列表必须包含 null compression；只要声明支持 TLS 1.3，兼容压缩字段就必须严格为单个零。
- TLS 1.3 ClientHello 的 `pre_shared_key` 必须是最后一个扩展。
- ServerHello key_share 必须恰好包含一个非空密钥；HelloRetryRequest key_share 必须恰好包含一个两字节命名组，不能用宽松的“大于等于长度”规则接受尾随数据。
- `xrtTlsRetryCookie()` 与 `xrtTlsWriterRetryCookie()` 公开 HRR/第二个 ClientHello 共用的 16 位非空 Cookie 向量；解析结果借用输入，长度声明必须精确消费扩展数据。
- Hello 解析器只负责线路结构和已知核心扩展的局部语义。密码套件选择、版本协商、扩展出现位置、SNI 证书选择和 ALPN 策略属于后续状态机。

扩展唯一性检测不分配 8K 类型位图。游标只保存 32 字节桶状态；发生桶碰撞时最多回看同桶的 16 位类型，因此正常路径为线性扫描，最坏回看次数也受 16 位类型空间约束。

### `xrtTls12CertificateRequestEncode`

失败原子地编码 TLS 1.2 CertificateRequest 正文。

```c
bool xrtTls12CertificateRequestEncode(const xtls12certificaterequest* pRequest, void* pOutput, size_t iOutputSize)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pRequest` | 输入 | 非空 | 证书请求 |
| `pOutput` | 输入 | 非空 | 输出缓冲 |
| `iOutputSize` | 输入 | — | 接收输出字节数 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 成功 | — |
| `false` | 失败 | 见错误 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_PROTOCOL`（TLS 语义） — DER 或消息结构非法
- `XERR_RANGE` — 容量不足，不写半个结果

#### 范例

[message_tour](../../examples/tls/message_tour/main.c) · negotiate

```c
			!xrtTls12CertificateRequestEncode(&Req12,
				arrOut, iSize) ||
```

### `xrtTls12CertificateRequestParse`

严格解析 TLS 1.2 CertificateRequest 正文。

```c
bool xrtTls12CertificateRequestParse(xbytesview Body, xtls12certificaterequest* pRequest)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `Body` | 输入 | — | 消息体 |
| `pRequest` | 输入 | 非空 | 证书请求 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 成功 | — |
| `false` | 失败 | 见错误 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_PROTOCOL`（TLS 语义） — DER 或消息结构非法

#### 范例

[message_tour](../../examples/tls/message_tour/main.c) · negotiate

```c
			!xrtTls12CertificateRequestParse(
				(xbytesview) { arrOut, iSize },
				&Req12Parsed) ||
```

### `xrtTls12CertificateRequestSize`

返回编码 TLS 1.2 CertificateRequest 正文所需长度。

```c
size_t xrtTls12CertificateRequestSize(const xtls12certificaterequest* pRequest)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pRequest` | 输入 | 非空 | 证书请求 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `>= 0` | 数量或字节数 | — |

#### 错误

- 无 — 纯查询，不设置错误

#### 范例

[message_tour](../../examples/tls/message_tour/main.c) · negotiate

```c
		iSize = xrtTls12CertificateRequestSize(&Req12);
```

### `xrtTls12ClientKeyExchangeEncode`

编码 TLS 1.2 ECDHE ClientKeyExchange，允许公钥与输出重叠。

```c
bool xrtTls12ClientKeyExchangeEncode(xbytesview PublicKey, void* pOutput, size_t iOutputSize)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `PublicKey` | 输入 | — | 公钥字节 |
| `pOutput` | 输入 | 非空 | 输出缓冲 |
| `iOutputSize` | 输入 | — | 接收输出字节数 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 成功 | — |
| `false` | 失败 | 见错误 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_PROTOCOL`（TLS 语义） — DER 或消息结构非法
- `XERR_RANGE` — 容量不足，不写半个结果

#### 范例

[message_tour](../../examples/tls/message_tour/main.c) · negotiate

```c
			!xrtTls12ClientKeyExchangeEncode(
				(xbytesview) { arrKey, 4u }, arrOut,
				5u) ||
```

### `xrtTls12ClientKeyExchangeParse`

严格解析 TLS 1.2 ECDHE ClientKeyExchange 公钥。

```c
bool xrtTls12ClientKeyExchangeParse(xbytesview Body, xbytesview* pPublicKey)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `Body` | 输入 | — | 消息体 |
| `pPublicKey` | 输入 | 非空 | 公钥描述 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 成功 | — |
| `false` | 失败 | 见错误 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_PROTOCOL`（TLS 语义） — DER 或消息结构非法

#### 范例

[message_tour](../../examples/tls/message_tour/main.c) · negotiate

```c
			!xrtTls12ClientKeyExchangeParse(
				(xbytesview) { arrOut, 5u },
				&PublicKey) ||
```

### `xrtTls12ClientKeyExchangeSize`

返回编码 TLS 1.2 ECDHE ClientKeyExchange 正文所需长度。

```c
size_t xrtTls12ClientKeyExchangeSize(xbytesview PublicKey)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `PublicKey` | 输入 | — | 公钥字节 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `>= 0` | 数量或字节数 | — |

#### 错误

- 无 — 纯查询，不设置错误

#### 范例

[message_tour](../../examples/tls/message_tour/main.c) · negotiate

```c
		if ( (xrtTls12ClientKeyExchangeSize(
				(xbytesview) { arrKey, 4u }) != 5u) ||
			!xrtTls12ClientKeyExchangeEncode(
				(xbytesview) { arrKey, 4u }, arrOut,
				5u) ||
			!xrtTls12ClientKeyExchangeParse(
				(xbytesview) { arrOut, 5u },
				&PublicKey) ||
			(PublicKey.Size != 4u) ) {
```

### `xrtTls12ServerKeyExchangeEncode`

失败原子地编码 TLS 1.2 ECDHE ServerKeyExchange 正文。

```c
bool xrtTls12ServerKeyExchangeEncode(uint16 iGroup, xbytesview PublicKey, const xtlscertificateverify* pVerify, void* pOutput, size_t iOutputSize)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `iGroup` | 输入 | — | 命名组标识 |
| `PublicKey` | 输入 | — | 公钥字节 |
| `pVerify` | 输入 | 非空 | 验证数据 |
| `pOutput` | 输入 | 非空 | 输出缓冲 |
| `iOutputSize` | 输入 | — | 接收输出字节数 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 成功 | — |
| `false` | 失败 | 见错误 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_PROTOCOL`（TLS 语义） — DER 或消息结构非法
- `XERR_RANGE` — 容量不足，不写半个结果

#### 范例

[auth_messages](../../examples/tls/auth_messages/main.c) · negotiate

```c
	if ( (iBodySize == 0) || !xrtTls12ServerKeyExchangeEncode(
		XTLS_GROUP_X25519,
		(xbytesview) { PublicKey, sizeof(PublicKey) }, &Verify,
		Body, sizeof(Body)
	) || !xrtTls12ServerKeyExchangeParse(
		(xbytesview) { Body, iBodySize }, &Exchange
	) ) {
```

### `xrtTls12ServerKeyExchangeParse`

严格解析 TLS 1.2 ECDHE ServerKeyExchange 正文。

```c
bool xrtTls12ServerKeyExchangeParse(xbytesview Body, xtls12serverkeyexchange* pExchange)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `Body` | 输入 | — | 消息体 |
| `pExchange` | 输入 | 非空 | 密钥交换参数 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 成功 | — |
| `false` | 失败 | 见错误 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_PROTOCOL`（TLS 语义） — DER 或消息结构非法

#### 范例

[auth_messages](../../examples/tls/auth_messages/main.c) · negotiate

```c
	) || !xrtTls12ServerKeyExchangeParse(
```

### `xrtTls12ServerKeyExchangeSize`

返回编码 TLS 1.2 ECDHE ServerKeyExchange 正文所需长度。

```c
size_t xrtTls12ServerKeyExchangeSize(uint16 iGroup, xbytesview PublicKey, const xtlscertificateverify* pVerify)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `iGroup` | 输入 | — | 命名组标识 |
| `PublicKey` | 输入 | — | 公钥字节 |
| `pVerify` | 输入 | 非空 | 验证数据 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `>= 0` | 数量或字节数 | — |

#### 错误

- 无 — 纯查询，不设置错误

#### 范例

[auth_messages](../../examples/tls/auth_messages/main.c) · negotiate

```c
	iBodySize = xrtTls12ServerKeyExchangeSize(
		XTLS_GROUP_X25519,
		(xbytesview) { PublicKey, sizeof(PublicKey) }, &Verify
	);
```

### `xrtTls12ServerKeyExchangeVerify`

验证 TLS 1.2 ECDHE ServerKeyExchange；签名覆盖双方随机数和原始参数。

```c
bool xrtTls12ServerKeyExchangeVerify(xtlssignature Scheme, xbytesview ClientRandom, xbytesview ServerRandom, xbytesview Parameters, xbytesview Signature, const xx509pubkey* pPublicKey)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `Scheme` | 输入 | — | 签名方案 |
| `ClientRandom` | 输入 | — | 客户端随机数 |
| `ServerRandom` | 输入 | — | 服务端随机数 |
| `Parameters` | 输入 | — | 参数 |
| `Signature` | 输入 | — | 签名值 |
| `pPublicKey` | 输入 | 非空 | 公钥描述 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 成功 | — |
| `false` | 失败 | 见错误 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_PROTOCOL`（TLS 语义） — DER 或消息结构非法

#### 范例

[handshake_extra](../../examples/tls/handshake_extra/main.c) · negotiate

```c
			!xrtTls12ServerKeyExchangeVerify(
				XTLS_SIGNATURE_ECDSA_SECP256R1_SHA256,
				(xbytesview) { arrClientRandom, 32u },
				(xbytesview) { arrServerRandom, 32u },
				(xbytesview) { arrParams, 8u },
				(xbytesview) { arrDer, iDerSize },
				&LeafKey) ) {
```

### `xrtTls13CertificateRequestEncode`

失败原子地编码 TLS 1.3 CertificateRequest 正文。

```c
bool xrtTls13CertificateRequestEncode(xbytesview RequestContext, xbytesview Extensions, void* pOutput, size_t iOutputSize)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `RequestContext` | 输入 | — | 请求上下文 |
| `Extensions` | 输入 | — | 扩展列表 |
| `pOutput` | 输入 | 非空 | 输出缓冲 |
| `iOutputSize` | 输入 | — | 接收输出字节数 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 成功 | — |
| `false` | 失败 | 见错误 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_PROTOCOL`（TLS 语义） — DER 或消息结构非法
- `XERR_RANGE` — 容量不足，不写半个结果

#### 范例

[message_tour](../../examples/tls/message_tour/main.c) · negotiate

```c
			!xrtTls13CertificateRequestEncode(
				(xbytesview) { arrCtx, 1u },
				(xbytesview) { arr13Ext, 10u }, arrOut,
				14u) ||
```

### `xrtTls13CertificateRequestParse`

严格解析 TLS 1.3 CertificateRequest 正文和认证扩展。

```c
bool xrtTls13CertificateRequestParse(xbytesview Body, xtls13certificaterequest* pRequest)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `Body` | 输入 | — | 消息体 |
| `pRequest` | 输入 | 非空 | 证书请求 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 成功 | — |
| `false` | 失败 | 见错误 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_PROTOCOL`（TLS 语义） — DER 或消息结构非法

#### 范例

[message_tour](../../examples/tls/message_tour/main.c) · negotiate

```c
			!xrtTls13CertificateRequestParse(
				(xbytesview) { arrOut, 14u },
				&Req13Parsed) ||
```

### `xrtTls13CertificateRequestSize`

返回编码 TLS 1.3 CertificateRequest 正文所需长度。

```c
size_t xrtTls13CertificateRequestSize(xbytesview RequestContext, xbytesview Extensions)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `RequestContext` | 输入 | — | 请求上下文 |
| `Extensions` | 输入 | — | 扩展列表 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `>= 0` | 数量或字节数 | — |

#### 错误

- 无 — 纯查询，不设置错误

#### 范例

[message_tour](../../examples/tls/message_tour/main.c) · negotiate

```c
		iSize = xrtTls13CertificateRequestSize(
			(xbytesview) { arrCtx, 1u },
			(xbytesview) { arr13Ext, 10u });
```

### `xrtTlsAuthorities`

严格解析带 16 位总长的证书颁发者名称向量并初始化游标。

```c
bool xrtTlsAuthorities(xbytesview Data, xtlsauthoritycursor* pCursor)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `Data` | 输入 | — | 数据 |
| `pCursor` | 输入/输出 | 非空 | 游标 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 成功 | — |
| `false` | 失败 | 见错误 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_PROTOCOL`（TLS 语义） — DER 或消息结构非法

#### 范例

[message_tour](../../examples/tls/message_tour/main.c) · negotiate

```c
		if ( !xrtTlsAuthorities(
				(xbytesview) { arrGood, 13u },
				&AuthCursor) ) {
```

### `xrtTlsAuthoritiesEncode`

失败原子地编码带 16 位总长的证书颁发者名称向量。

```c
bool xrtTlsAuthoritiesEncode(const xbytesview* pNames, size_t iCount, void* pOutput, size_t iOutputSize)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pNames` | 输入 | 非空 | 名称数组 |
| `iCount` | 输入 | — | 数量 |
| `pOutput` | 输入 | 非空 | 输出缓冲 |
| `iOutputSize` | 输入 | — | 接收输出字节数 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 成功 | — |
| `false` | 失败 | 见错误 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_PROTOCOL`（TLS 语义） — DER 或消息结构非法
- `XERR_RANGE` — 容量不足，不写半个结果

#### 范例

[message_tour](../../examples/tls/message_tour/main.c) · negotiate

```c
			!xrtTlsAuthoritiesEncode(arrTwo, 2u,
				arrOut, 13u) ||
```

### `xrtTlsAuthoritiesRead`

读取下一项非空 DER DistinguishedName。

```c
xtlsitemresult xrtTlsAuthoritiesRead(xtlsauthoritycursor* pCursor, xbytesview* pName)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pCursor` | 输入/输出 | 非空 | 游标 |
| `pName` | 输入 | 非空 | 接收名称 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 枚举值 | 当前取值 | — |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_PROTOCOL`（TLS 语义） — DER 或消息结构非法

#### 范例

[message_tour](../../examples/tls/message_tour/main.c) · negotiate

```c
		while ( xrtTlsAuthoritiesRead(&AuthCursor,
				&Name) == XTLS_ITEM_VALUE ) {
```

### `xrtTlsAuthoritiesSize`

返回编码证书颁发者名称向量所需长度。

```c
size_t xrtTlsAuthoritiesSize(const xbytesview* pNames, size_t iCount)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pNames` | 输入 | 非空 | 名称数组 |
| `iCount` | 输入 | — | 数量 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `>= 0` | 数量或字节数 | — |

#### 错误

- 无 — 纯查询，不设置错误

#### 范例

[message_tour](../../examples/tls/message_tour/main.c) · negotiate

```c
		if ( (xrtTlsAuthoritiesSize(arrTwo, 2u) != 13u) ||
			!xrtTlsAuthoritiesEncode(arrTwo, 2u,
				arrOut, 13u) ||
			!xrtTlsAuthorities(
				(xbytesview) { arrOut, 13u }, &AuthCursor) ) {
```

### `xrtTlsCertificatesRead`

读取下一证书条目；结束、值和错误使用三态结果区分。

```c
xtlsitemresult xrtTlsCertificatesRead(xtlscertificatecursor* pCursor, xtlscertificateentry* pEntry)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pCursor` | 输入/输出 | 非空 | 游标 |
| `pEntry` | 输入 | 非空 | 接收条目 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 枚举值 | 当前取值 | — |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_PROTOCOL`（TLS 语义） — DER 或消息结构非法

#### 范例

[messages](../../examples/tls/messages/main.c) · negotiate

```c
	while ( (Result = xrtTlsCertificatesRead(
		&Cursor, &Entry
	)) == XTLS_ITEM_VALUE ) {
```

### `xrtTlsCompressedCertificateEncode`

编码 CompressedCertificate，允许压缩数据与输出重叠。

```c
bool xrtTlsCompressedCertificateEncode(const xtlscompressedcertificate* pCertificate, void* pOutput, size_t iOutputSize)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pCertificate` | 输入 | 非空 | 证书视图 |
| `pOutput` | 输入 | 非空 | 输出缓冲 |
| `iOutputSize` | 输入 | — | 接收输出字节数 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 成功 | — |
| `false` | 失败 | 见错误 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_PROTOCOL`（TLS 语义） — DER 或消息结构非法
- `XERR_RANGE` — 容量不足，不写半个结果

#### 范例

[message_tour](../../examples/tls/message_tour/main.c) · negotiate

```c
		!xrtTlsCompressedCertificateEncode(&Compressed,
			arrOut, 9u) ||
```

### `xrtTlsCompressedCertificateParse`

严格解析 TLS 1.3 CompressedCertificate 正文。

```c
bool xrtTlsCompressedCertificateParse(xbytesview Body, xtlscompressedcertificate* pCertificate)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `Body` | 输入 | — | 消息体 |
| `pCertificate` | 输入 | 非空 | 证书视图 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 成功 | — |
| `false` | 失败 | 见错误 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_PROTOCOL`（TLS 语义） — DER 或消息结构非法

#### 范例

[message_tour](../../examples/tls/message_tour/main.c) · negotiate

```c
		!xrtTlsCompressedCertificateParse(
			(xbytesview) { arrOut, 9u },
			&CompressedParsed) ||
```

### `xrtTlsCompressedCertificateSize`

返回编码 CompressedCertificate 正文所需长度。

```c
size_t xrtTlsCompressedCertificateSize(const xtlscompressedcertificate* pCertificate)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pCertificate` | 输入 | 非空 | 证书视图 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `>= 0` | 数量或字节数 | — |

#### 错误

- 无 — 纯查询，不设置错误

#### 范例

[message_tour](../../examples/tls/message_tour/main.c) · negotiate

```c
	iSize = xrtTlsCompressedCertificateSize(&Compressed);
```

### `xrtTlsEncryptedExtensionsEncode`

失败原子地编码 EncryptedExtensions 正文。

```c
bool xrtTlsEncryptedExtensionsEncode(xbytesview Extensions, void* pOutput, size_t iOutputSize)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `Extensions` | 输入 | — | 扩展列表 |
| `pOutput` | 输入 | 非空 | 输出缓冲 |
| `iOutputSize` | 输入 | — | 接收输出字节数 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 成功 | — |
| `false` | 失败 | 见错误 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_PROTOCOL`（TLS 语义） — DER 或消息结构非法
- `XERR_RANGE` — 容量不足，不写半个结果

#### 范例

[message_tour](../../examples/tls/message_tour/main.c) · negotiate

```c
			!xrtTlsEncryptedExtensionsEncode(
				(xbytesview) { NULL, 0u }, arrOut, 2u) ||
```

### `xrtTlsEncryptedExtensionsParse`

严格解析 TLS 1.3 EncryptedExtensions 正文。

```c
bool xrtTlsEncryptedExtensionsParse(xbytesview Body, xbytesview* pExtensions)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `Body` | 输入 | — | 消息体 |
| `pExtensions` | 输入 | 非空 | 扩展数组 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 成功 | — |
| `false` | 失败 | 见错误 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_PROTOCOL`（TLS 语义） — DER 或消息结构非法

#### 范例

[message_tour](../../examples/tls/message_tour/main.c) · negotiate

```c
			!xrtTlsEncryptedExtensionsParse(
				(xbytesview) { arrOut, 2u }, &Extensions) ||
```

### `xrtTlsEncryptedExtensionsSize`

返回编码 EncryptedExtensions 正文所需长度。

```c
size_t xrtTlsEncryptedExtensionsSize(xbytesview Extensions)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `Extensions` | 输入 | — | 扩展列表 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `>= 0` | 数量或字节数 | — |

#### 错误

- 无 — 纯查询，不设置错误

#### 范例

[message_tour](../../examples/tls/message_tour/main.c) · negotiate

```c
		if ( (xrtTlsEncryptedExtensionsSize(
				(xbytesview) { NULL, 0u }) != 2u) ||
			!xrtTlsEncryptedExtensionsEncode(
				(xbytesview) { NULL, 0u }, arrOut, 2u) ||
			!xrtTlsEncryptedExtensionsParse(
				(xbytesview) { arrOut, 2u }, &Extensions) ||
			(Extensions.Size != 0u) ||
			(xrtTlsEncryptedExtensionsSize(
				(xbytesview) { arrAck, 4u }) != 6u) ||
			!xrtTlsEncryptedExtensionsEncode(
				(xbytesview) { arrAck, 4u }, arrOut, 6u) ||
			!xrtTlsEncryptedExtensionsParse(
```

### `xrtTlsFinishedEncode`

编码非空 Finished 验证数据正文。

```c
bool xrtTlsFinishedEncode(xbytesview VerifyData, void* pOutput, size_t iOutputSize)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `VerifyData` | 输入 | — | 验证数据 |
| `pOutput` | 输入 | 非空 | 输出缓冲 |
| `iOutputSize` | 输入 | — | 接收输出字节数 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 成功 | — |
| `false` | 失败 | 见错误 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_PROTOCOL`（TLS 语义） — DER 或消息结构非法
- `XERR_RANGE` — 容量不足，不写半个结果

#### 范例

[message_tour](../../examples/tls/message_tour/main.c) · negotiate

```c
	if ( !xrtTlsFinishedEncode((xbytesview) { arrBody, 12u },
			arrOut, 12u) ||
		!xrtTlsFinishedParse((xbytesview) { arrOut, 12u },
			12u, &VerifyData) ||
		(VerifyData.Size != 12u) ) {
```

### `xrtTlsFinishedParse`

按调用方给出的协商长度严格解析 Finished 验证数据。

```c
bool xrtTlsFinishedParse(xbytesview Body, size_t iExpectedSize, xbytesview* pVerifyData)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `Body` | 输入 | — | 消息体 |
| `iExpectedSize` | 输入 | — | 期望长度 |
| `pVerifyData` | 输入 | 非空 | 验证数据 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 成功 | — |
| `false` | 失败 | 见错误 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_PROTOCOL`（TLS 语义） — DER 或消息结构非法

#### 范例

[message_tour](../../examples/tls/message_tour/main.c) · negotiate

```c
		!xrtTlsFinishedParse((xbytesview) { arrOut, 12u },
			12u, &VerifyData) ||
```

### `xrtTlsWriterClientKeyShares`

追加 ClientHello key_share 扩展，允许写出空列表以请求 Retry。

```c
bool xrtTlsWriterClientKeyShares(xtlswriter* pWriter, const xtlskeyshare* pShares, size_t iCount)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pWriter` | 输入/输出 | 非空 | Hello 写出器 |
| `pShares` | 输入 | 非空 | 份额数组 |
| `iCount` | 输入 | — | 数量 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 成功 | — |
| `false` | 失败 | 见错误 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_PROTOCOL`（TLS 语义） — DER 或消息结构非法

#### 范例

[writer_tour](../../examples/tls/writer_tour/main.c) · negotiate

```c
	if ( !xrtTlsWriterClientKeyShares(&Writer, &Share, 1u) ) {
```

### `xrtTlsWriterClientPsks`

追加 ClientHello PSK identities 与等量 binders，调用方保证它是末项。

```c
bool xrtTlsWriterClientPsks(xtlswriter* pWriter, const xtlspsk* pPsks, size_t iCount)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pWriter` | 输入/输出 | 非空 | Hello 写出器 |
| `pPsks` | 输入 | 非空 | PSK 数组 |
| `iCount` | 输入 | — | 数量 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 成功 | — |
| `false` | 失败 | 见错误 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_PROTOCOL`（TLS 语义） — DER 或消息结构非法

#### 范例

[writer_tour](../../examples/tls/writer_tour/main.c) · negotiate

```c
		if ( !xrtTlsWriterClientPsks(&Writer, Psks, 1u) ) {
```

### `xrtTlsWriterClientVersions`

追加 ClientHello supported_versions 扩展。

```c
bool xrtTlsWriterClientVersions(xtlswriter* pWriter, const uint16* pVersions, size_t iCount)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pWriter` | 输入/输出 | 非空 | Hello 写出器 |
| `pVersions` | 输入 | 非空 | 版本数组 |
| `iCount` | 输入 | — | 数量 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 成功 | — |
| `false` | 失败 | 见错误 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_PROTOCOL`（TLS 语义） — DER 或消息结构非法

#### 范例

[writer_tour](../../examples/tls/writer_tour/main.c) · negotiate

```c
	if ( !xrtTlsWriterClientVersions(&Writer, arrGroups, 2u) ) {
```

### `xrtTlsWriterData`

返回 writer 已完成区域的借用视图。

```c
xbytesview xrtTlsWriterData(const xtlswriter* pWriter)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pWriter` | 输入/输出 | 非空 | Hello 写出器 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 枚举值 | 当前取值 | — |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法

#### 范例

[writer_tour](../../examples/tls/writer_tour/main.c) · negotiate

```c
	Written = xrtTlsWriterData(&Writer);
```

### `xrtTlsWriterExtension`

失败原子地追加一个原始扩展，负载允许与 writer 缓冲重叠。

```c
bool xrtTlsWriterExtension(xtlswriter* pWriter, xtlsextensiontype Type, xbytesview Data)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pWriter` | 输入/输出 | 非空 | Hello 写出器 |
| `Type` | 输入 | — | 类型标识 |
| `Data` | 输入 | — | 数据 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 成功 | — |
| `false` | 失败 | 见错误 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_PROTOCOL`（TLS 语义） — DER 或消息结构非法

#### 范例

[writer_tour](../../examples/tls/writer_tour/main.c) · negotiate

```c
		if ( !xrtTlsWriterExtension(&Writer,
				XTLS_EXTENSION_STATUS_REQUEST,
				(xbytesview) { arrStatus, 1u }) ) {
```

### `xrtTlsWriterHostName`

追加只包含一个 host_name 的 SNI 扩展。

```c
bool xrtTlsWriterHostName(xtlswriter* pWriter, xbytesview Host)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pWriter` | 输入/输出 | 非空 | Hello 写出器 |
| `Host` | 输入 | — | 主机名 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 成功 | — |
| `false` | 失败 | 见错误 |

#### 错误

- 无 — 纯查询，不设置错误

#### 范例

[writer_tour](../../examples/tls/writer_tour/main.c) · negotiate

```c
	if ( !xrtTlsWriterHostName(&Writer,
			(xbytesview) { (cbytes)"api", 3u }) ) {
```

### `xrtTlsWriterIds`

追加带 16 位长度前缀的标识列表扩展，适用于组和签名方案。

```c
bool xrtTlsWriterIds(xtlswriter* pWriter, xtlsextensiontype Type, const uint16* pValues, size_t iCount)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pWriter` | 输入/输出 | 非空 | Hello 写出器 |
| `Type` | 输入 | — | 类型标识 |
| `pValues` | 输入 | 非空 | 值数组 |
| `iCount` | 输入 | — | 数量 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 成功 | — |
| `false` | 失败 | 见错误 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_PROTOCOL`（TLS 语义） — DER 或消息结构非法

#### 范例

[writer_tour](../../examples/tls/writer_tour/main.c) · negotiate

```c
	if ( !xrtTlsWriterIds(&Writer,
			XTLS_EXTENSION_SUPPORTED_GROUPS, arrGroups, 2u) ) {
```

### `xrtTlsWriterInit`

初始化一个空的调用方缓冲 writer。

```c
bool xrtTlsWriterInit(xtlswriter* pWriter, void* pData, size_t iCapacity)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pWriter` | 输入/输出 | 非空 | Hello 写出器 |
| `pData` | 输入 | — | 用户数据 |
| `iCapacity` | 输入 | — | 容量 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 成功 | — |
| `false` | 失败 | 见错误 |

#### 错误

- 无 — 初始化不失败

#### 范例

[writer_tour](../../examples/tls/writer_tour/main.c) · negotiate

```c
	if ( !xrtTlsWriterInit(&Writer, arrExtBuf, sizeof(arrExtBuf)) ) {
```

### `xrtTlsWriterProtocols`

追加完整 ALPN 协议列表；协议名称按输入顺序保留。

```c
bool xrtTlsWriterProtocols(xtlswriter* pWriter, const xbytesview* pProtocols, size_t iCount)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pWriter` | 输入/输出 | 非空 | Hello 写出器 |
| `pProtocols` | 输入 | 非空 | 协议数组 |
| `iCount` | 输入 | — | 数量 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 成功 | — |
| `false` | 失败 | 见错误 |

#### 错误

- 无 — 纯查询，不设置错误

#### 范例

[writer_tour](../../examples/tls/writer_tour/main.c) · negotiate

```c
	if ( !xrtTlsWriterProtocols(&Writer, arrProtocols, 2u) ) {
```

### `xrtTlsWriterPskModes`

追加非空且不重复的 PSK 密钥交换模式列表。

```c
bool xrtTlsWriterPskModes(xtlswriter* pWriter, const uint8* pModes, size_t iCount)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pWriter` | 输入/输出 | 非空 | Hello 写出器 |
| `pModes` | 输入 | 非空 | PSK 模式数组 |
| `iCount` | 输入 | — | 数量 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 成功 | — |
| `false` | 失败 | 见错误 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_PROTOCOL`（TLS 语义） — DER 或消息结构非法

#### 范例

[writer_tour](../../examples/tls/writer_tour/main.c) · negotiate

```c
		if ( !xrtTlsWriterPskModes(&Writer, arrModes, 1u) ) {
```

### `xrtTlsWriterReset`

清空 writer 的逻辑内容，不擦除调用方缓冲。

```c
bool xrtTlsWriterReset(xtlswriter* pWriter)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pWriter` | 输入/输出 | 非空 | Hello 写出器 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 成功 | — |
| `false` | 失败 | 见错误 |

#### 错误

- 无 — 释放或重置不失败（Reset 系列见参数约束）

#### 范例

[writer_tour](../../examples/tls/writer_tour/main.c) · negotiate

```c
	if ( !xrtTlsWriterReset(&Writer) ) {
```

### `xrtTlsWriterRetryCookie`

追加 HelloRetryRequest 或 ClientHello 使用的非空 cookie 扩展。

```c
bool xrtTlsWriterRetryCookie(xtlswriter* pWriter, xbytesview Cookie)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pWriter` | 输入/输出 | 非空 | Hello 写出器 |
| `Cookie` | 输入 | — | Cookie 数据 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 成功 | — |
| `false` | 失败 | 见错误 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_PROTOCOL`（TLS 语义） — DER 或消息结构非法

#### 范例

[writer_tour](../../examples/tls/writer_tour/main.c) · negotiate

```c
		!xrtTlsWriterRetryCookie(&Writer,
			(xbytesview) { arrKey1, 4u }) ) {
```

### `xrtTlsWriterRetryGroup`

追加 HelloRetryRequest 选择组形式的 key_share 扩展。

```c
bool xrtTlsWriterRetryGroup(xtlswriter* pWriter, uint16 iGroup)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pWriter` | 输入/输出 | 非空 | Hello 写出器 |
| `iGroup` | 输入 | — | 命名组标识 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 成功 | — |
| `false` | 失败 | 见错误 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_PROTOCOL`（TLS 语义） — DER 或消息结构非法

#### 范例

[writer_tour](../../examples/tls/writer_tour/main.c) · negotiate

```c
	if ( !xrtTlsWriterRetryGroup(&Writer, 0x001Du) ||
		!xrtTlsWriterRetryCookie(&Writer,
			(xbytesview) { arrKey1, 4u }) ) {
```

### `xrtTlsWriterServerKeyShare`

追加普通 ServerHello 的单个 key_share 扩展。

```c
bool xrtTlsWriterServerKeyShare(xtlswriter* pWriter, const xtlskeyshare* pShare)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pWriter` | 输入/输出 | 非空 | Hello 写出器 |
| `pShare` | 输入 | 非空 | 密钥份额 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 成功 | — |
| `false` | 失败 | 见错误 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_PROTOCOL`（TLS 语义） — DER 或消息结构非法

#### 范例

[writer_tour](../../examples/tls/writer_tour/main.c) · negotiate

```c
	if ( !xrtTlsWriterServerKeyShare(&Writer, &Share) ) {
```

### `xrtTlsWriterServerPsk`

追加 ServerHello 选择的单一 PSK identity 索引。

```c
bool xrtTlsWriterServerPsk(xtlswriter* pWriter, uint16 iSelected)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pWriter` | 输入/输出 | 非空 | Hello 写出器 |
| `iSelected` | 输入 | — | 接收选中项 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 成功 | — |
| `false` | 失败 | 见错误 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_PROTOCOL`（TLS 语义） — DER 或消息结构非法

#### 范例

[writer_tour](../../examples/tls/writer_tour/main.c) · negotiate

```c
		if ( !xrtTlsWriterServerPsk(&Writer, 0u) ) {
```

### `xrtTlsWriterServerVersion`

追加 ServerHello 选择单一版本的 supported_versions 扩展。

```c
bool xrtTlsWriterServerVersion(xtlswriter* pWriter, uint16 iVersion)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pWriter` | 输入/输出 | 非空 | Hello 写出器 |
| `iVersion` | 输入 | — | TLS 版本 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 成功 | — |
| `false` | 失败 | 见错误 |

#### 错误

- 无 — 纯查询，不设置错误

#### 范例

[writer_tour](../../examples/tls/writer_tour/main.c) · negotiate

```c
	if ( !xrtTlsWriterServerVersion(&Writer, 0x0304u) ) {
```

## 无状态协商

`tls_negotiate` 只选择协议参数，不生成随机数、密钥或签名，也不修改会话状态。所有偏好数组都由调用方提供并按本地顺序解释，因此服务端、客户端、硬件能力探测和应用策略可以共享同一套选择器，不受库内硬编码优先级限制。

```c
static const xtlsversion Versions[] = {
	XTLS_VERSION_13, XTLS_VERSION_12
};
static const xtlscipher Ciphers[] = {
	XTLS_CHACHA20_POLY1305_SHA256,
	XTLS_AES_128_GCM_SHA256,
	XTLS_AES_256_GCM_SHA384
};

xtlsversion Version;
xtlscipher Cipher;

xrtTlsVersionSelect(&OfferedVersions, Versions, 2, &Version);
xrtTlsCipherSelect(
	Version, &Hello.CipherSuites, XTLS_IDENTITY_RSA,
	Ciphers, 3, &Cipher
);
```

API 分层如下：

- `xrtTlsIdsSelect()` 按本地偏好求任意 16 位标识列表的交集，未知和私有线路值不会被截断。`xrtTlsKeyShareFind()` 在完整客户端 key_share 负载中查找指定组。
- `xrtTlsVersionSelect()` 选择已解析的 `supported_versions`；`xrtTlsClientVersionSelect()` 进一步处理扩展缺失语义，此时只能选择 TLS 1.2，绝不会根据 `legacy_version` 猜测 TLS 1.3。
- `xrtTlsCipherSelect()` 使用 `xrtTlsCipherInfo()` 和 `xtlsidentitytype` 过滤版本与 TLS 1.2 认证约束。TLS 1.3 套件与证书或 PSK 身份独立；TLS 1.2 ECDHE_RSA 只接受 RSA 身份，ECDHE_ECDSA 按 RFC 8422 接受 ECDSA 或 EdDSA 身份。
- `xrtTlsSignatureInfo()` 返回方案要求的身份类别、摘要长度和协议版本范围。元数据是进程期只读对象，未知线路值返回 `NULL` 且不设置错误；会话策略、身份选择和签名实现不再分别维护方案表。
- `xrtTlsSignatureSelect()` 区分版本规则。TLS 1.3 RSA 握手签名只选择 PSS，ECDSA 方案必须匹配身份曲线；TLS 1.2 的 ECDSA 线路值仍是哈希/签名对，不把名称中的曲线错误地强加到证书密钥。
- `xrtTlsKeyShareSelect()` 先验证 key_share 是 `supported_groups` 的同序子序列。`XTLS_KEY_SHARE_PREFER_GROUP` 严格保留本地组优先级，首选组没有 share 时发布 `Retry=true`；`XTLS_KEY_SHARE_PREFER_READY` 优先选择已经携带 share 的共同组以避免额外往返，没有可用 share 时才请求首选共同组。

所有选择器使用 `XTLS_ITEM_VALUE`、`XTLS_ITEM_DONE` 和 `XTLS_ITEM_ERROR`：无交集是正常 `DONE`，不设置错误且不修改输出；畸形对端输入或非法本地配置返回结构化 `xrt.tls` 协商错误。结果中的 key-share 公钥借用 ClientHello 输入，生命周期不超过原输入。

`xtlsidentitytype` 只描述握手签名公钥类别，不等同于后续持有证书链、私钥和策略的身份对象。这一层故意不判断密码后端是否编入、RSA-PSS 密钥参数和模数长度、证书链是否满足对端 `signature_algorithms_cert`、组公钥是否在曲线上，也不决定 PSK、SNI 或 ALPN。会话配置根据实际启用的密码后端和密钥能力构造偏好数组，身份选择发生在 SNI 之后，密钥交换层再验证组专用公钥并计算共享秘密；自定义组或签名仍可使用通用标识 API 和原始扩展视图实现。

### `xrtTlsCipherCompatible`

判断密码套件能否用于指定版本和握手身份。

```c
bool xrtTlsCipherCompatible(xtlsversion Version, xtlscipher Cipher, xtlsidentitytype Identity)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `Version` | 输入 | — | TLS 版本 |
| `Cipher` | 输入 | — | 密码套件 |
| `Identity` | 输入 | — | 身份标识 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 成功 | — |
| `false` | 失败 | 见错误 |

#### 错误

- 无 — 纯查询，不设置错误

#### 范例

[extension_tour](../../examples/tls/extension_tour/main.c) · policy

```c
	if ( !xrtTlsCipherCompatible(XTLS_VERSION_13,
			XTLS_AES_128_GCM_SHA256,
			XTLS_IDENTITY_ECDSA_P256) ||
		!xrtTlsCipherCompatible((xtlsversion)0x0303u,
			XTLS_ECDHE_RSA_AES_128_GCM_SHA256,
			XTLS_IDENTITY_RSA) ||
		xrtTlsCipherCompatible((xtlsversion)0x0303u,
			XTLS_ECDHE_RSA_AES_128_GCM_SHA256,
			XTLS_IDENTITY_ECDSA_P256) ||
		xrtTlsCipherCompatible(XTLS_VERSION_13,
			XTLS_ECDHE_RSA_AES_128_GCM_SHA256,
			XTLS_IDENTITY_RSA) ) {
```

### `xrtTlsCipherInfo`

返回只读密码套件元数据，未知套件返回空指针且不设置错误。

```c
const xtlscipherinfo* xrtTlsCipherInfo(xtlscipher Cipher)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `Cipher` | 输入 | 非空 | 密码套件 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | 对象或借用 | — |
| `NULL` | 失败或不适用 | 见错误 |

#### 错误

- 无 — 纯查询，不设置错误

#### 范例

[cipher_backends](../../examples/tls/cipher_backends/main.c) · policy

```c
		const xtlscipherinfo* pInfo = xrtTlsCipherInfo(
			Ciphers[i]
		);
```

### `xrtTlsCipherName`

返回密码套件的稳定英文名称，未知值返回 unknown。

```c
cstr xrtTlsCipherName(xtlscipher Cipher)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `Cipher` | 输入 | — | 密码套件 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | 静态名称；未知为 `"UNKNOWN"` | — |

#### 错误

- 无 — 纯查询，不设置错误

#### 范例

[cipher_backends](../../examples/tls/cipher_backends/main.c) · policy

```c
			xrtTlsCipherName(Ciphers[i]),
```

### `xrtTlsCipherSelect`

按本地偏好选择版本、身份和对端都接受的密码套件。

```c
xtlsitemresult xrtTlsCipherSelect(xtlsversion Version, const xtlsids* pOffered, xtlsidentitytype Identity, const xtlscipher* pPreferred, size_t iPreferredCount, xtlscipher* pSelected)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `Version` | 输入 | — | TLS 版本 |
| `pOffered` | 输入 | 非空 | 对端提供数组 |
| `Identity` | 输入 | — | 身份标识 |
| `pPreferred` | 输入 | 非空 | 偏好数组 |
| `iPreferredCount` | 输入 | — | 偏好数量 |
| `pSelected` | 输入 | 非空 | 接收选中结果 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 枚举值 | 当前取值 | — |

#### 错误

- 无 — 纯查询，不设置错误

#### 范例

[negotiate](../../examples/tls/negotiate/main.c) · policy

```c
	if ( xrtTlsCipherSelect(
		Version, &OfferedCiphers, XTLS_IDENTITY_RSA,
		Ciphers, sizeof(Ciphers) / sizeof(Ciphers[0]), &Cipher
	) != XTLS_ITEM_VALUE ) {
```

### `xrtTlsGroupAvailable`

判断命名组的密码后端是否已编译进当前 XRT。

```c
bool xrtTlsGroupAvailable(uint16 iGroup)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `iGroup` | 输入 | — | 命名组标识 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 成功 | — |
| `false` | 失败 | 见错误 |

#### 错误

- 无 — 纯查询，不设置错误

#### 范例

[key_exchange](../../examples/tls/key_exchange/main.c) · policy

```c
		if ( xrtTlsGroupAvailable(Groups[i]) ) {
```

### `xrtTlsGroupInfo`

返回协议已知组的只读元数据；未知或未实现组返回空指针且不设置错误。

```c
const xtlsgroupinfo* xrtTlsGroupInfo(uint16 iGroup)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `iGroup` | 输入 | 非空 | 命名组标识 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | 对象或借用 | — |
| `NULL` | 失败或不适用 | 见错误 |

#### 错误

- 无 — 纯查询，不设置错误

#### 范例

[key_exchange](../../examples/tls/key_exchange/main.c) · policy

```c
			return xrtTlsGroupInfo(Groups[i]);
```

### `xrtTlsGroups`

严格解析 supported_groups 扩展数据。

```c
bool xrtTlsGroups(xbytesview Data, xtlsids* pGroups)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `Data` | 输入 | — | 数据 |
| `pGroups` | 输入 | 非空 | 命名组数组 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 成功 | — |
| `false` | 失败 | 见错误 |

#### 错误

- 无 — 纯查询，不设置错误

#### 范例

[extension_tour](../../examples/tls/extension_tour/main.c) · policy

```c
		!xrtTlsGroups(Extension.Data, &Ids) ||
```

### `xrtTlsHostName`

查找 SNI host_name；没有该名称类型时返回 XTLS_ITEM_DONE。

```c
xtlsitemresult xrtTlsHostName(xbytesview Data, xbytesview* pHost)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `Data` | 输入 | — | 数据 |
| `pHost` | 输入 | 非空 | 接收主机名 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 枚举值 | 当前取值 | — |

#### 错误

- 无 — 纯查询，不设置错误

#### 范例

[extension_tour](../../examples/tls/extension_tour/main.c) · policy

```c
		(xrtTlsHostName((xbytesview) { arrSni, 8u },
			&Host) != XTLS_ITEM_VALUE) ||
```

### `xrtTlsIdsContain`

判断 16 位标识列表是否包含给定线路值。

```c
bool xrtTlsIdsContain(const xtlsids* pIds, uint16 iValue)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pIds` | 输入 | 非空 | 标识数组 |
| `iValue` | 输入 | — | 数值 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 成功 | — |
| `false` | 失败 | 见错误 |

#### 错误

- 无 — 纯查询，不设置错误

#### 范例

[extension_tour](../../examples/tls/extension_tour/main.c) · policy

```c
		!xrtTlsIdsContain(&Ids, 0x0017u) ||
```

### `xrtTlsIdsCount`

返回 16 位标识列表的元素数量。

```c
size_t xrtTlsIdsCount(const xtlsids* pIds)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pIds` | 输入 | 非空 | 标识数组 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `>= 0` | 数量或字节数 | — |

#### 错误

- 无 — 纯查询，不设置错误

#### 范例

[extension_tour](../../examples/tls/extension_tour/main.c) · policy

```c
	if ( (xrtTlsIdsCount(&Ids) != 3u) ||
		!xrtTlsIdsGet(&Ids, 2u, &iValue) ||
		(iValue != 0x0304u) ||
		xrtTlsIdsGet(&Ids, 3u, &iValue) ||
		!xrtTlsIdsContain(&Ids, 0x0017u) ||
		xrtTlsIdsContain(&Ids, 0x00FFu) ) {
```

### `xrtTlsIdsGet`

按索引读取 16 位标识；越界时不修改输出。

```c
bool xrtTlsIdsGet(const xtlsids* pIds, size_t iIndex, uint16* pValue)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pIds` | 输入 | 非空 | 标识数组 |
| `iIndex` | 输入 | — | 索引 |
| `pValue` | 输入 | 非空 | 值 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 成功 | — |
| `false` | 失败 | 见错误 |

#### 错误

- 无 — 纯查询，不设置错误

#### 范例

[extension_tour](../../examples/tls/extension_tour/main.c) · policy

```c
		!xrtTlsIdsGet(&Ids, 2u, &iValue) ||
```

### `xrtTlsIdsSelect`

按本地偏好顺序选择 16 位标识交集，未知线路值保持可扩展。

```c
xtlsitemresult xrtTlsIdsSelect(const xtlsids* pOffered, const uint16* pPreferred, size_t iPreferredCount, uint16* pSelected)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pOffered` | 输入 | 非空 | 对端提供数组 |
| `pPreferred` | 输入 | 非空 | 偏好数组 |
| `iPreferredCount` | 输入 | — | 偏好数量 |
| `pSelected` | 输入 | 非空 | 接收选中结果 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 枚举值 | 当前取值 | — |

#### 错误

- 无 — 纯查询，不设置错误

#### 范例

[extension_tour](../../examples/tls/extension_tour/main.c) · policy

```c
		if ( (xrtTlsIdsSelect(&Ids, arrPreferred, 2u,
				&iPicked) != XTLS_ITEM_VALUE) ||
			(iPicked != 0x0017u) ) {
```

### `xrtTlsLimitsInit`

初始化适合通用客户端和服务端的有界队列与驱动预算。

```c
void xrtTlsLimitsInit(xtlslimits* pLimits)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pLimits` | 输入 | 非空 | 资源上限 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 无 | — | — |

#### 错误

- 无 — 初始化不失败

#### 范例

[extension_tour](../../examples/tls/extension_tour/main.c) · policy

```c
	xrtTlsLimitsInit(&Limits);
```

### `xrtTlsLimitsValid`

验证队列至少能接收一个最大记录，并限制单条握手消息。

```c
bool xrtTlsLimitsValid(const xtlslimits* pLimits)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pLimits` | 输入 | 非空 | 资源上限 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 成功 | — |
| `false` | 失败 | 见错误 |

#### 错误

- 无 — 纯校验，不设置错误

#### 范例

[extension_tour](../../examples/tls/extension_tour/main.c) · policy

```c
	if ( !xrtTlsLimitsValid(&Limits) ) {
```

### `xrtTlsPolicyInit`

初始化覆盖 TLS 1.3/1.2 的通用安全偏好；所有数组都可由调用方替换。

```c
void xrtTlsPolicyInit(xtlspolicy* pPolicy)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pPolicy` | 输入 | 非空 | TLS 策略 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 无 | — | — |

#### 错误

- 无 — 初始化不失败

#### 范例

[context](../../examples/tls/context/main.c) · policy

```c
	xrtTlsPolicyInit(&Policy);
```

### `xrtTlsPolicyValid`

验证偏好指针、唯一性、已知线路值和跨字段可用性。

```c
bool xrtTlsPolicyValid(const xtlspolicy* pPolicy)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pPolicy` | 输入 | 非空 | TLS 策略 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 成功 | — |
| `false` | 失败 | 见错误 |

#### 错误

- 无 — 纯校验，不设置错误

#### 范例

[policy](../../examples/tls/policy/main.c) · policy

```c
	if ( !xrtTlsPolicyValid(&Policy) ) {
```

### `xrtTlsProtocolFind`

在完整 ALPN 列表中查找一个不透明协议名称。

```c
xtlsitemresult xrtTlsProtocolFind(xbytesview Data, xbytesview Protocol)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `Data` | 输入 | — | 数据 |
| `Protocol` | 输入 | — | 协议 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 枚举值 | 当前取值 | — |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法

#### 范例

[extension_tour](../../examples/tls/extension_tour/main.c) · policy

```c
		(xrtTlsProtocolFind((xbytesview) { arrAlpn, 15u },
			(xbytesview) { (cbytes)"h2", 2u }) !=
			XTLS_ITEM_VALUE) ) {
```

### `xrtTlsProtocolSelect`

按 Preferred 的顺序选择双方 ALPN 列表的第一个交集。

```c
xtlsitemresult xrtTlsProtocolSelect(xbytesview Offered, xbytesview Preferred, xbytesview* pProtocol)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `Offered` | 输入 | — | 提供值 |
| `Preferred` | 输入 | — | 偏好值 |
| `pProtocol` | 输入 | 非空 | 接收选中协议 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 枚举值 | 当前取值 | — |

#### 错误

- 无 — 纯查询，不设置错误

#### 范例

[extension_tour](../../examples/tls/extension_tour/main.c) · policy

```c
			if ( (xrtTlsProtocolSelect(
					(xbytesview) { arrAlpn, 15u },
					(xbytesview) { arrFull, 8u },
					&Protocol) != XTLS_ITEM_VALUE) ||
				(Protocol.Size != 2u) ||
				(memcmp(Protocol.Data, "h2", 2u) != 0) ) {
```

### `xrtTlsProtocolSelected`

严格读取服务端必须唯一选择的 ALPN 协议。

```c
bool xrtTlsProtocolSelected(xbytesview Data, xbytesview* pProtocol)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `Data` | 输入 | — | 数据 |
| `pProtocol` | 输入 | 非空 | 接收选中协议 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 成功 | — |
| `false` | 失败 | 见错误 |

#### 错误

- 无 — 纯查询，不设置错误

#### 范例

[extension_tour](../../examples/tls/extension_tour/main.c) · policy

```c
		if ( !xrtTlsProtocolSelected(
				(xbytesview) { arrOne, 4u },
				&Protocol) ||
			(Protocol.Size != 1u) ) {
```

### `xrtTlsProtocols`

严格解析 ALPN ProtocolNameList 并把游标重置到首项。

```c
bool xrtTlsProtocols(xbytesview Data, xtlsprotocolcursor* pCursor)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `Data` | 输入 | — | 数据 |
| `pCursor` | 输入/输出 | 非空 | 游标 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 成功 | — |
| `false` | 失败 | 见错误 |

#### 错误

- 无 — 纯查询，不设置错误

#### 范例

[extension_tour](../../examples/tls/extension_tour/main.c) · policy

```c
	if ( !xrtTlsProtocols((xbytesview) { arrAlpn, 15u },
			&AlpnCursor) ) {
```

### `xrtTlsProtocolsRead`

读取下一项非空 ALPN 协议名称。

```c
xtlsitemresult xrtTlsProtocolsRead(xtlsprotocolcursor* pCursor, xbytesview* pProtocol)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pCursor` | 输入/输出 | 非空 | 游标 |
| `pProtocol` | 输入 | 非空 | 接收选中协议 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 枚举值 | 当前取值 | — |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_PROTOCOL`（TLS 语义） — DER 或消息结构非法

#### 范例

[extension_tour](../../examples/tls/extension_tour/main.c) · policy

```c
	while ( xrtTlsProtocolsRead(&AlpnCursor, &Protocol) ==
		XTLS_ITEM_VALUE ) {
```

### `xrtTlsPskModes`

严格解析非空且不重复的 PSK 密钥交换模式列表。

```c
bool xrtTlsPskModes(xbytesview Data, xbytesview* pModes)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `Data` | 输入 | — | 数据 |
| `pModes` | 输入 | 非空 | PSK 模式数组 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 成功 | — |
| `false` | 失败 | 见错误 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_PROTOCOL`（TLS 语义） — DER 或消息结构非法

#### 范例

[extension_tour](../../examples/tls/extension_tour/main.c) · policy

```c
		if ( !xrtTlsPskModes((xbytesview) { arrModes, 2u },
				&Protocol) ||
			(Protocol.Size != 1u) ||
			(Protocol.Data[0] != 1u) ) {
```

### `xrtTlsPsksRead`

同步读取下一项 identity、混淆年龄和 binder。

```c
xtlsitemresult xrtTlsPsksRead(xtlspskcursor* pCursor, xtlspsk* pPsk)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pCursor` | 输入/输出 | 非空 | 游标 |
| `pPsk` | 输入 | 非空 | PSK |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 枚举值 | 当前取值 | — |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_PROTOCOL`（TLS 语义） — DER 或消息结构非法

#### 范例

[extension_tour](../../examples/tls/extension_tour/main.c) · policy

```c
		if ( (xrtTlsPsksRead(&PskCursor, &Psk) !=
				XTLS_ITEM_VALUE) ||
			(Psk.Identity.Size != 7u) ||
			(Psk.Binder.Size != 32u) ||
			(Psk.Binder.Size != 32u) ||
			(xrtTlsPsksRead(&PskCursor, &Psk) !=
				XTLS_ITEM_DONE) ) {
```

### `xrtTlsRetryCookie`

严格解析 HelloRetryRequest cookie 的 16 位非空字节向量。

```c
bool xrtTlsRetryCookie(xbytesview Data, xbytesview* pCookie)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `Data` | 输入 | — | 数据 |
| `pCookie` | 输入 | 非空 | Cookie |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 成功 | — |
| `false` | 失败 | 见错误 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_PROTOCOL`（TLS 语义） — DER 或消息结构非法

#### 范例

[extension_tour](../../examples/tls/extension_tour/main.c) · policy

```c
			!xrtTlsRetryCookie((xbytesview) { arrRetry + 2,
				3u }, &Protocol) ||
```

### `xrtTlsRetryGroup`

严格解析 HelloRetryRequest 中仅含命名组的 key_share。

```c
bool xrtTlsRetryGroup(xbytesview Data, uint16* pGroup)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `Data` | 输入 | — | 数据 |
| `pGroup` | 输入 | 非空 | 接收命名组 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 成功 | — |
| `false` | 失败 | 见错误 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_PROTOCOL`（TLS 语义） — DER 或消息结构非法

#### 范例

[extension_tour](../../examples/tls/extension_tour/main.c) · policy

```c
		if ( !xrtTlsRetryGroup((xbytesview) { arrRetry, 2u },
				&iGroup) ||
			(iGroup != 0x001Du) ||
			!xrtTlsRetryCookie((xbytesview) { arrRetry + 2,
				3u }, &Protocol) ||
			(Protocol.Size != 1u) ) {
```

### `xrtTlsSignatureCompatible`

判断签名方案能否用于指定版本和握手身份。

```c
bool xrtTlsSignatureCompatible(xtlsversion Version, xtlssignature Signature, xtlsidentitytype Identity)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `Version` | 输入 | — | TLS 版本 |
| `Signature` | 输入 | — | 签名值 |
| `Identity` | 输入 | — | 身份标识 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 成功 | — |
| `false` | 失败 | 见错误 |

#### 错误

- 无 — 纯查询，不设置错误

#### 范例

[extension_tour](../../examples/tls/extension_tour/main.c) · policy

```c
		!xrtTlsSignatureCompatible(XTLS_VERSION_13,
			XTLS_SIGNATURE_RSA_PSS_RSAE_SHA256,
			XTLS_IDENTITY_RSA) ) {
```

### `xrtTlsSignatureInfo`

返回签名方案的只读元数据；未知线路值返回 NULL 且不设置错误。

```c
const xtlssignatureinfo* xrtTlsSignatureInfo(xtlssignature Signature)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `Signature` | 输入 | 非空 | 签名值 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | 对象或借用 | — |
| `NULL` | 失败或不适用 | 见错误 |

#### 错误

- 无 — 纯查询，不设置错误

#### 范例

[extension_tour](../../examples/tls/extension_tour/main.c) · policy

```c
	pSigInfo = xrtTlsSignatureInfo(XTLS_SIGNATURE_RSA_PSS_RSAE_SHA256);
```

### `xrtTlsSignatureSelect`

按本地偏好选择版本、身份和对端都接受的握手签名方案。

```c
xtlsitemresult xrtTlsSignatureSelect(xtlsversion Version, const xtlsids* pOffered, xtlsidentitytype Identity, const xtlssignature* pPreferred, size_t iPreferredCount, xtlssignature* pSelected)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `Version` | 输入 | — | TLS 版本 |
| `pOffered` | 输入 | 非空 | 对端提供数组 |
| `Identity` | 输入 | — | 身份标识 |
| `pPreferred` | 输入 | 非空 | 偏好数组 |
| `iPreferredCount` | 输入 | — | 偏好数量 |
| `pSelected` | 输入 | 非空 | 接收选中结果 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 枚举值 | 当前取值 | — |

#### 错误

- 无 — 纯查询，不设置错误

#### 范例

[extension_tour](../../examples/tls/extension_tour/main.c) · policy

```c
		if ( (xrtTlsSignatureSelect(XTLS_VERSION_13, &Ids,
				XTLS_IDENTITY_RSA, arrPref, 2u,
				&Picked) != XTLS_ITEM_VALUE) ||
			(Picked !=
				XTLS_SIGNATURE_RSA_PSS_RSAE_SHA256) ) {
```

### `xrtTlsSignatures`

严格解析 signature_algorithms 类扩展数据。

```c
bool xrtTlsSignatures(xbytesview Data, xtlsids* pSignatures)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `Data` | 输入 | — | 数据 |
| `pSignatures` | 输入 | 非空 | 签名方案数组 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 成功 | — |
| `false` | 失败 | 见错误 |

#### 错误

- 无 — 纯查询，不设置错误

#### 范例

[extension_tour](../../examples/tls/extension_tour/main.c) · policy

```c
		if ( !xrtTlsSignatures((xbytesview) { arrSigData, 6u },
				&Ids) ||
			(xrtTlsIdsCount(&Ids) != 2u) ) {
```

### `xrtTlsVersionName`

返回协议版本的稳定英文名称，未知值返回 unknown。

```c
cstr xrtTlsVersionName(uint16 iVersion)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `iVersion` | 输入 | — | TLS 版本 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | 静态名称；未知为 `"UNKNOWN"` | — |

#### 错误

- 无 — 纯查询，不设置错误

#### 范例

[negotiate](../../examples/tls/negotiate/main.c) · policy

```c
		xrtTlsVersionName(Version), xrtTlsCipherName(Cipher)
```

### `xrtTlsVersionSelect`

按本地版本偏好选择 supported_versions 中的第一个交集。

```c
xtlsitemresult xrtTlsVersionSelect(const xtlsids* pOffered, const xtlsversion* pPreferred, size_t iPreferredCount, xtlsversion* pSelected)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pOffered` | 输入 | 非空 | 对端提供数组 |
| `pPreferred` | 输入 | 非空 | 偏好数组 |
| `iPreferredCount` | 输入 | — | 偏好数量 |
| `pSelected` | 输入 | 非空 | 接收选中结果 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 枚举值 | 当前取值 | — |

#### 错误

- 无 — 纯查询，不设置错误

#### 范例

[negotiate](../../examples/tls/negotiate/main.c) · policy

```c
	if ( xrtTlsVersionSelect(
		&OfferedVersions, Versions, 2, &Version
	) != XTLS_ITEM_VALUE ) {
```

## TLS 策略

`tls_policy` 把原先散落在客户端、服务端和身份分支中的硬编码优先级收敛成一份借用式配置。默认初始化不分配内存，数组指向进程期只读常量；调用方可以逐项替换为生命周期覆盖上下文创建过程的自有数组：

```c
static const xtlsversion Versions[] = { XTLS_VERSION_13 };
static const xtlscipher Ciphers[] = {
	XTLS_CHACHA20_POLY1305_SHA256,
	XTLS_AES_128_GCM_SHA256
};

xtlspolicy Policy;
xrtTlsPolicyInit(&Policy);
Policy.Versions = Versions;
Policy.VersionCount = 1;
Policy.Ciphers = Ciphers;
Policy.CipherCount = 2;
Policy.KeySharePolicy = XTLS_KEY_SHARE_PREFER_GROUP;

if ( !xrtTlsPolicyValid(&Policy) ) {
	return false;
}
```

策略校验不修改输入且不分配内存。版本和套件必须非空；所有列表都必须保持指针/数量一致、元素唯一且属于内建会话能力。每个套件必须对应一个启用版本，每个启用版本也必须至少保留一个套件。组和签名列表可以为空，为恢复会话、PSK 或后续外部认证路径保留扩展空间；非空签名必须能用于至少一个启用版本，因此 TLS 1.2 专用 PKCS#1 方案不会混入纯 TLS 1.3 策略。

策略只描述协议偏好，不承诺当前裁剪构建已经包含对应密码执行后端。后续客户端或服务端会话创建会结合 `xrtTlsGroupAvailable()`、记录 AEAD、身份私钥和验证后端生成实际执行路径；原始策略快照保持不变，便于同一配置服务不同机器和硬件能力。

### `xrtTlsContextConfigInit`

初始化使用默认策略和默认限制的上下文配置。

```c
void xrtTlsContextConfigInit(xtlscontextconfig* pConfig)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pConfig` | 输入 | — | 配置 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 无 | — | — |

#### 错误

- 无 — 初始化不失败

#### 范例

[context](../../examples/tls/context/main.c) · context

```c
	xrtTlsContextConfigInit(&Config);
```

### `xrtTlsContextCreate`

创建可跨线程共享的只读 TLS 配置快照。

```c
xtlscontext* xrtTlsContextCreate(const xtlscontextconfig* pConfig)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pConfig` | 输入 | — | 配置 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | 对象或借用 | — |
| `NULL` | 失败或不适用 | 见错误 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_MEMORY` — 分配失败

#### 范例

[context](../../examples/tls/context/main.c) · context

```c
	pContext = xrtTlsContextCreate(&Config);
```

### `xrtTlsContextLimits`

返回生命周期不超过上下文的只读限制快照。

```c
const xtlslimits* xrtTlsContextLimits(const xtlscontext* pContext)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pContext` | 输入/输出 | 非空 | 共享上下文 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | 对象或借用 | — |
| `NULL` | 失败或不适用 | 见错误 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_MEMORY` — 分配失败

#### 范例

[context](../../examples/tls/context/main.c) · context

```c
		xrtTlsContextLimits(pContext)->PlainLimit
```

### `xrtTlsContextPolicy`

返回生命周期不超过上下文的只读策略快照。

```c
const xtlspolicy* xrtTlsContextPolicy(const xtlscontext* pContext)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pContext` | 输入/输出 | 非空 | 共享上下文 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | 对象或借用 | — |
| `NULL` | 失败或不适用 | 见错误 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_MEMORY` — 分配失败

#### 范例

[context](../../examples/tls/context/main.c) · context

```c
		xrtTlsContextPolicy(pContext)->VersionCount,
```

### `xrtTlsContextRelease`

释放一个上下文引用，空指针无操作。

```c
void xrtTlsContextRelease(xtlscontext* pContext)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pContext` | 输入/输出 | 非空 | 共享上下文 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 无 | — | — |

#### 错误

- 无 — 释放或重置不失败（Reset 系列见参数约束）

#### 范例

[context](../../examples/tls/context/main.c) · context

```c
	xrtTlsContextRelease(pContext);
```

### `xrtTlsContextRetain`

增加上下文引用；会话必须为其借用的上下文持有一个引用。

```c
xtlscontext* xrtTlsContextRetain(const xtlscontext* pContext)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pContext` | 输入/输出 | 非空 | 共享上下文 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | 对象或借用 | — |
| `NULL` | 失败或不适用 | 见错误 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法

#### 范例

[extension_tour](../../examples/tls/extension_tour/main.c) · context

```c
		((pRetained = xrtTlsContextRetain(pContext)) ==
			NULL) ) {
```

## 共享上下文

`tls_context` 把调用方策略转换为可跨线程共享的只读快照。上下文使用一次紧凑分配保存对象和四个有序数组；创建返回后不再借用 `xtlspolicy`、数组或 `xtlscontextconfig`。连接和会话只保留上下文引用，证书或 ticket key 轮换通过创建新上下文并替换上层引用完成，不在活动对象中原地修改策略。

```c
xtlscontextconfig Config;
xtlscontext* Context;

xrtTlsContextConfigInit(&Config);
Config.Policy = &Policy;
Config.Limits.PlainLimit = 512u * 1024u;

Context = xrtTlsContextCreate(&Config);
if ( Context == NULL ) {
	return false;
}

/* 每个会话持有自己的引用。 */
xrtTlsContextRetain(Context);
xrtTlsContextRelease(Context);
xrtTlsContextRelease(Context);
```

`xtlslimits` 不会触发预分配。`FeedLimit`、`SendLimit` 和 `PlainLimit` 只是后续自适应 `xnetbuf` 队列的硬上限；空闲 context 或 session 不常驻 8 KiB 缓冲。三个队列至少容纳一条最大合法记录，避免对端发送标准大小记录后永久停滞。`HandshakeLimit` 限制单条重组消息，默认 1 MiB；`RecordBudget` 和 `HandshakeBudget` 限制一次非阻塞驱动处理的工作量，防止高吞吐连接独占 Worker。

`xrtTlsListenerConfigInit` 默认限制 128 条并发握手和 1024 条已完成待领取连接。握手预算在创建 TLS 会话之前执行，调用方仍可显式设置 `HandshakeLimit` 和 `AcceptQueueLimit`。本次只收紧现有默认值，没有新增资源 profile 或改变缓冲的惰性分配策略。

上下文当前保存协议策略，不把 Worker 专属 `xnetbufpool` 放入共享对象，也不把“协议已知”误当成“当前构建后端可执行”。客户端或服务端会话创建时再将上下文策略与已编入的组、AEAD、身份和验证能力求交集；无法得到完整执行路径时创建失败，而不会静默改写调用方优先级。

### `xrtTlsKeyShareDerive`

从精确长度的私钥和对端公钥派生共享秘密；输出容量可大于元数据要求。

```c
bool xrtTlsKeyShareDerive(uint16 iGroup, xbytesview Private, xbytesview PeerPublic, void* pShared, size_t iSharedCapacity)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `iGroup` | 输入 | — | 命名组标识 |
| `Private` | 输入 | — | 私钥份额 |
| `PeerPublic` | 输入 | — | 对端公钥份额 |
| `pShared` | 输入 | 非空 | 接收共享秘密 |
| `iSharedCapacity` | 输入 | — | 共享容量 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 成功 | — |
| `false` | 失败 | 见错误 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_PROTOCOL`（TLS 语义） — DER 或消息结构非法

#### 范例

[key_exchange](../../examples/tls/key_exchange/main.c) · key exchange

```c
	) || !xrtTlsKeyShareDerive(
```

### `xrtTlsKeyShareFind`

从完整客户端 key_share 扩展负载查找指定组的借用公钥。

```c
xtlsitemresult xrtTlsKeyShareFind(xbytesview KeyShares, uint16 iGroup, xtlskeyshare* pShare)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `KeyShares` | 输入 | — | 密钥份额数组 |
| `iGroup` | 输入 | — | 命名组标识 |
| `pShare` | 输入 | 非空 | 密钥份额 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 枚举值 | 当前取值 | — |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法

#### 范例

[extension_tour](../../examples/tls/extension_tour/main.c) · key exchange

```c
		if ( xrtTlsKeyShareFind(
				(xbytesview) { arrKsEmpty, 2u },
				0x001Du, &Share) != XTLS_ITEM_DONE ) {
```

### `xrtTlsKeyShareGenerate`

为命名组生成临时私钥与线路公钥；输出容量可大于元数据要求。

```c
bool xrtTlsKeyShareGenerate(uint16 iGroup, void* pPrivate, size_t iPrivateCapacity, void* pPublic, size_t iPublicCapacity)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `iGroup` | 输入 | — | 命名组标识 |
| `pPrivate` | 输入 | 非空 | 私钥缓冲 |
| `iPrivateCapacity` | 输入 | — | 私钥容量 |
| `pPublic` | 输入 | 非空 | 公钥缓冲 |
| `iPublicCapacity` | 输入 | — | 公钥容量 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 成功 | — |
| `false` | 失败 | 见错误 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_PROTOCOL`（TLS 语义） — DER 或消息结构非法

#### 范例

[key_exchange](../../examples/tls/key_exchange/main.c) · key exchange

```c
	if ( !xrtTlsKeyShareGenerate(
		pInfo->Group,
		ClientPrivate, sizeof(ClientPrivate),
		ClientPublic, sizeof(ClientPublic)
	) || !xrtTlsKeyShareGenerate(
		pInfo->Group,
		ServerPrivate, sizeof(ServerPrivate),
		ServerPublic, sizeof(ServerPublic)
	) || !xrtTlsKeyShareDerive(
		pInfo->Group,
		(xbytesview) { ClientPrivate, pInfo->PrivateSize },
		(xbytesview) { ServerPublic, pInfo->PublicSize },
```

### `xrtTlsKeyShareSelect`

选择可直接使用或需要 HelloRetryRequest 的共同密钥共享组。

```c
xtlsitemresult xrtTlsKeyShareSelect(const xtlsids* pGroups, xbytesview KeyShares, const uint16* pPreferred, size_t iPreferredCount, xtlskeysharepolicy Policy, xtlskeyshareselection* pSelection)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pGroups` | 输入 | 非空 | 命名组数组 |
| `KeyShares` | 输入 | — | 密钥份额数组 |
| `pPreferred` | 输入 | 非空 | 偏好数组 |
| `iPreferredCount` | 输入 | — | 偏好数量 |
| `Policy` | 输入 | — | 策略 |
| `pSelection` | 输入 | 非空 | 接收选择结果 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 枚举值 | 当前取值 | — |

#### 错误

- 无 — 纯查询，不设置错误

#### 范例

[extension_tour](../../examples/tls/extension_tour/main.c) · key exchange

```c
			if ( xrtTlsKeyShareSelect(&Ids,
					(xbytesview) { arrKsEmpty, 2u },
					arrPreferred, 2u,
					XTLS_KEY_SHARE_PREFER_READY,
					&Selection) == XTLS_ITEM_ERROR ) {
```

### `xrtTlsKeySharesRead`

读取下一项非空且命名组唯一的客户端密钥共享。

```c
xtlsitemresult xrtTlsKeySharesRead(xtlskeysharecursor* pCursor, xtlskeyshare* pShare)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pCursor` | 输入/输出 | 非空 | 游标 |
| `pShare` | 输入 | 非空 | 密钥份额 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 枚举值 | 当前取值 | — |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_PROTOCOL`（TLS 语义） — DER 或消息结构非法

#### 范例

[extension_tour](../../examples/tls/extension_tour/main.c) · key exchange

```c
			(xrtTlsKeySharesRead(&KsCursor, &Share) !=
				XTLS_ITEM_DONE) ) {
```

### `xrtTlsKeyUpdateEncode`

编码 TLS 1.3 单字节 KeyUpdate 请求。

```c
bool xrtTlsKeyUpdateEncode(xtlskeyupdate Request, void* pOutput, size_t iOutputSize)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `Request` | 输入 | — | 请求 |
| `pOutput` | 输入 | 非空 | 输出缓冲 |
| `iOutputSize` | 输入 | — | 接收输出字节数 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 成功 | — |
| `false` | 失败 | 见错误 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_PROTOCOL`（TLS 语义） — DER 或消息结构非法
- `XERR_RANGE` — 容量不足，不写半个结果

#### 范例

[message_tour](../../examples/tls/message_tour/main.c) · key exchange

```c
	if ( !xrtTlsKeyUpdateEncode(XTLS_KEY_UPDATE_REQUESTED,
			arrOut, 1u) ||
		!xrtTlsKeyUpdateParse((xbytesview) { arrOut, 1u },
			&KeyUpdate) ||
		(KeyUpdate != XTLS_KEY_UPDATE_REQUESTED) ) {
```

### `xrtTlsKeyUpdateParse`

严格解析 TLS 1.3 单字节 KeyUpdate 请求。

```c
bool xrtTlsKeyUpdateParse(xbytesview Body, xtlskeyupdate* pRequest)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `Body` | 输入 | — | 消息体 |
| `pRequest` | 输入 | 非空 | 证书请求 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 成功 | — |
| `false` | 失败 | 见错误 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_PROTOCOL`（TLS 语义） — DER 或消息结构非法

#### 范例

[message_tour](../../examples/tls/message_tour/main.c) · key exchange

```c
		!xrtTlsKeyUpdateParse((xbytesview) { arrOut, 1u },
			&KeyUpdate) ||
```

## 命名组与密钥交换

`tls_key_exchange` 把协议组信息与密码后端分开。`xrtTlsGroupInfo()` 对 X25519、X448、secp256r1 和 secp384r1 始终返回进程期只读元数据；`xrtTlsGroupAvailable()` 才反映当前裁剪配置。未知组查询返回 `NULL` 或 `false` 且不设置错误，便于策略层探测；对未知组或已裁剪后端执行生成、派生时返回 `XTLS_ERROR_KEY_EXCHANGE`。

```c
const xtlsgroupinfo* Info = xrtTlsGroupInfo(XTLS_GROUP_X25519);

if ( (Info != NULL) && xrtTlsGroupAvailable(Info->Group) ) {
	uint8 Private[32];
	uint8 Public[32];

	if ( !xrtTlsKeyShareGenerate(
		Info->Group, Private, sizeof(Private), Public, sizeof(Public)
	) ) {
		return false;
	}
}
```

- `PrivateSize`、`PublicSize` 和 `SharedSize` 是精确有效长度；输出容量可以更大，函数只写对应有效区间。
- `xrtTlsKeyShareGenerate()` 要求私钥与公钥有效输出区间不重叠。它先验证组、指针、容量和重叠，再调用随机源；任何失败都不修改两段输出。
- `xrtTlsKeyShareDerive()` 要求本地私钥和对端公钥视图恰好等于元数据长度。输出可以覆盖本地私钥或对端公钥，因为四个后端都在发布结果前完成输入消费；容量不足或密码校验失败时输出不变。
- X25519 与 X448 在派生阶段以常量时间聚合结果并拒绝全零共享秘密；P-256 与 P-384 验证私钥范围、未压缩点格式、坐标范围和曲线成员关系。
- 密码后端错误作为 `xrt.tls` 密钥交换错误的 `Cause` 保留，因此上层宿主和 C 调用方既能按 TLS 阶段处理，也能继续读取具体 `xrt.crypto` 原因。
- API 不分配堆内存，也不保存每连接状态。会话层只需在组选择完成后按元数据申请一组精确缓冲，不再为每个对象常驻四套私钥、公钥或最大共享秘密。

这层只执行传统 ECDHE/XDH。未来新增命名组时，协议解析和协商仍可通过 `uint16` 保留未知线路值；增加密码实现只需新增独立后端宏，不需要修改 Hello parser 的通用逃生路径。

## Hello Writer

`tls_hello_write` 把“高级构建器”和“直接线路控制”放在同一条分层路径上。`xtlswriter` 只引用调用方缓冲，不分配、不扩容，也不包含固定 1KB 或 8KB 数组：

```c
uint8 ExtensionBuffer[512];
xtlswriter Writer;

xrtTlsWriterInit(&Writer, ExtensionBuffer, sizeof(ExtensionBuffer));
xrtTlsWriterHostName(&Writer, XRT_BYTES_LITERAL("example.com"));

xbytesview Protocols[] = {
	XRT_BYTES_LITERAL("h2"),
	XRT_BYTES_LITERAL("http/1.1")
};
xrtTlsWriterProtocols(&Writer, Protocols, 2);

uint16 Versions[] = { XTLS_VERSION_13, XTLS_VERSION_12 };
xrtTlsWriterClientVersions(&Writer, Versions, 2);
```

常见路径可直接使用 `xrtTlsWriterHostName()`、`xrtTlsWriterProtocols()`、`xrtTlsWriterIds()`、版本和 key_share helper。少见或私有扩展使用 `xrtTlsWriterExtension()` 追加原始负载，不必修改协议层。每次追加先验证当前向量、重复类型、线路上限、容量和输入重叠；只有完整扩展写入成功才推进 `Writer.Size`。

`xrtTlsClientHelloSize()` / `xrtTlsServerHelloSize()` 先验证视图和核心扩展语义，再返回精确正文长度；对应 `Encode()` 在容量不足、Retry random 与标记不一致、TLS 1.3 压缩字段错误或输入输出重叠时保持输出不变。正文写好后可直接交给 `xrtTlsHandshakeEncode()` framing，或由后续会话层写入 transcript 和记录层。

重叠规则是显式的：原始 `xrtTlsWriterExtension()` 使用 `memmove`，允许负载来自 writer 缓冲；SNI、ALPN、标识数组、key_share 和完整 Hello 编码拒绝输入与本次目标区域重叠，避免多字段写入覆盖尚未读取的数据。空扩展向量编码时省略可选的两字节扩展总长字段。

大 ClientHello 只受调用方容量和 TLS 线路上限约束。回归包含超过旧版 1024 字节栈缓冲的 255 项 ALPN 列表，并由严格解析器完成 round-trip 验证。

### `xrtTlsCertificateEncode`

失败原子地编码完整 Certificate 正文。

```c
bool xrtTlsCertificateEncode(xtlsversion Version, xbytesview RequestContext, const xtlscertificateentry* pEntries, size_t iCount, void* pOutput, size_t iOutputSize)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `Version` | 输入 | — | TLS 版本 |
| `RequestContext` | 输入 | — | 请求上下文 |
| `pEntries` | 输入 | 非空 | 接收条目 |
| `iCount` | 输入 | — | 数量 |
| `pOutput` | 输入 | 非空 | 输出缓冲 |
| `iOutputSize` | 输入 | — | 接收输出字节数 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 成功 | — |
| `false` | 失败 | 见错误 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_PROTOCOL`（TLS 语义） — DER 或消息结构非法
- `XERR_RANGE` — 容量不足，不写半个结果

#### 范例

[messages](../../examples/tls/messages/main.c) · message tour

```c
	if ( (iBodySize == 0) || !xrtTlsCertificateEncode(
		XTLS_VERSION_13, (xbytesview) { NULL, 0 }, Chain, 2u,
		Body, sizeof(Body)
	) || !xrtTlsCertificateParse(
		XTLS_VERSION_13, (xbytesview) { Body, iBodySize }, &Message
	) || !xrtTlsCertificateEntries(&Message, &Cursor) ) {
```

### `xrtTlsCertificateEntries`

从已验证的 Certificate 消息初始化零拷贝证书游标。

```c
bool xrtTlsCertificateEntries(const xtlscertificatemessage* pMessage, xtlscertificatecursor* pCursor)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pMessage` | 输入 | 非空 | 握手消息 |
| `pCursor` | 输入/输出 | 非空 | 游标 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 成功 | — |
| `false` | 失败 | 见错误 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_PROTOCOL`（TLS 语义） — DER 或消息结构非法

#### 范例

[messages](../../examples/tls/messages/main.c) · message tour

```c
	) || !xrtTlsCertificateEntries(&Message, &Cursor) ) {
```

### `xrtTlsCertificateParse`

严格解析 TLS 1.2 或 TLS 1.3 Certificate 正文并验证全部条目。

```c
bool xrtTlsCertificateParse(xtlsversion Version, xbytesview Body, xtlscertificatemessage* pMessage)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `Version` | 输入 | — | TLS 版本 |
| `Body` | 输入 | — | 消息体 |
| `pMessage` | 输入 | 非空 | 握手消息 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 成功 | — |
| `false` | 失败 | 见错误 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_PROTOCOL`（TLS 语义） — DER 或消息结构非法

#### 范例

[messages](../../examples/tls/messages/main.c) · message tour

```c
	) || !xrtTlsCertificateParse(
```

### `xrtTlsCertificateSize`

返回编码 Certificate 正文所需长度，非法输入返回零。

```c
size_t xrtTlsCertificateSize(xtlsversion Version, xbytesview RequestContext, const xtlscertificateentry* pEntries, size_t iCount)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `Version` | 输入 | — | TLS 版本 |
| `RequestContext` | 输入 | — | 请求上下文 |
| `pEntries` | 输入 | 非空 | 接收条目 |
| `iCount` | 输入 | — | 数量 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `>= 0` | 数量或字节数 | — |

#### 错误

- 无 — 纯查询，不设置错误

#### 范例

[messages](../../examples/tls/messages/main.c) · message tour

```c
	iBodySize = xrtTlsCertificateSize(
		XTLS_VERSION_13, (xbytesview) { NULL, 0 }, Chain, 2u
	);
```

### `xrtTlsCertificateStatusEncode`

编码 OCSP CertificateStatus，允许响应与输出重叠。

```c
bool xrtTlsCertificateStatusEncode(const xtlscertificatestatusmessage* pStatus, void* pOutput, size_t iOutputSize)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pStatus` | 输入 | 非空 | 证书状态 |
| `pOutput` | 输入 | 非空 | 输出缓冲 |
| `iOutputSize` | 输入 | — | 接收输出字节数 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 成功 | — |
| `false` | 失败 | 见错误 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_PROTOCOL`（TLS 语义） — DER 或消息结构非法
- `XERR_RANGE` — 容量不足，不写半个结果

#### 范例

[message_tour](../../examples/tls/message_tour/main.c) · message tour

```c
		!xrtTlsCertificateStatusEncode(&Status, arrOut, 8u) ||
```

### `xrtTlsCertificateStatusParse`

严格解析 OCSP CertificateStatus 正文。

```c
bool xrtTlsCertificateStatusParse(xbytesview Body, xtlscertificatestatusmessage* pStatus)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `Body` | 输入 | — | 消息体 |
| `pStatus` | 输入 | 非空 | 证书状态 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 成功 | — |
| `false` | 失败 | 见错误 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_PROTOCOL`（TLS 语义） — DER 或消息结构非法

#### 范例

[message_tour](../../examples/tls/message_tour/main.c) · message tour

```c
		!xrtTlsCertificateStatusParse(
			(xbytesview) { arrOut, 8u }, &StatusParsed) ||
```

### `xrtTlsCertificateStatusSize`

返回编码 OCSP CertificateStatus 正文所需长度。

```c
size_t xrtTlsCertificateStatusSize(const xtlscertificatestatusmessage* pStatus)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pStatus` | 输入 | 非空 | 证书状态 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `>= 0` | 数量或字节数 | — |

#### 错误

- 无 — 纯查询，不设置错误

#### 范例

[message_tour](../../examples/tls/message_tour/main.c) · message tour

```c
	iSize = xrtTlsCertificateStatusSize(&Status);
```

### `xrtTlsCertificateVerifyEncode`

失败原子地编码 CertificateVerify 正文。

```c
bool xrtTlsCertificateVerifyEncode(const xtlscertificateverify* pVerify, void* pOutput, size_t iOutputSize)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pVerify` | 输入 | 非空 | 验证数据 |
| `pOutput` | 输入 | 非空 | 输出缓冲 |
| `iOutputSize` | 输入 | — | 接收输出字节数 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 成功 | — |
| `false` | 失败 | 见错误 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_PROTOCOL`（TLS 语义） — DER 或消息结构非法
- `XERR_RANGE` — 容量不足，不写半个结果

#### 范例

[message_tour](../../examples/tls/message_tour/main.c) · message tour

```c
		!xrtTlsCertificateVerifyEncode(&Verify, arrOut, 8u) ||
```

### `xrtTlsCertificateVerifyParse`

严格解析 CertificateVerify 的方案与非空签名。

```c
bool xrtTlsCertificateVerifyParse(xbytesview Body, xtlscertificateverify* pVerify)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `Body` | 输入 | — | 消息体 |
| `pVerify` | 输入 | 非空 | 验证数据 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 成功 | — |
| `false` | 失败 | 见错误 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_PROTOCOL`（TLS 语义） — DER 或消息结构非法

#### 范例

[message_tour](../../examples/tls/message_tour/main.c) · message tour

```c
		!xrtTlsCertificateVerifyParse(
			(xbytesview) { arrOut, 8u }, &VerifyParsed) ||
```

### `xrtTlsCertificateVerifySize`

返回编码 CertificateVerify 正文所需长度。

```c
size_t xrtTlsCertificateVerifySize(const xtlscertificateverify* pVerify)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pVerify` | 输入 | 非空 | 验证数据 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `>= 0` | 数量或字节数 | — |

#### 错误

- 无 — 纯查询，不设置错误

#### 范例

[message_tour](../../examples/tls/message_tour/main.c) · message tour

```c
	iSize = xrtTlsCertificateVerifySize(&Verify);
```

## 握手语义消息

`tls_messages` 把握手 framing 与会话状态机之间反复出现的字段切分收敛为一层公共协议 API。它不解析 X.509、不执行签名，也不决定当前状态是否允许某种消息；它只保证正文符合 TLS 1.2 或 TLS 1.3 的精确线路结构。

证书链采用消息视图与游标两级 API：

```c
xtlscertificatemessage Certificates;
xtlscertificatecursor Cursor;
xtlscertificateentry Entry;

if ( !xrtTlsCertificateParse(Version, Handshake.Body, &Certificates) ||
	!xrtTlsCertificateEntries(&Certificates, &Cursor) ) {
	return false;
}

while ( xrtTlsCertificatesRead(&Cursor, &Entry) == XTLS_ITEM_VALUE ) {
	/* Entry.Data 是 DER；TLS 1.3 的 Entry.Extensions 保留条目扩展。 */
}
```

- TLS 1.2 与 TLS 1.3 的请求上下文、列表和条目格式分别解析，不使用“至少还有这些字节”的宽松规则接受尾随数据。
- 证书列表可以为空，由握手角色和认证策略决定该场景是否合法；非空条目的 DER 数据必须至少一字节。
- 游标不保存固定证书指针数组，因此没有 4、8、16 项链上限。单张证书使用 24 位长度，70KB 以上证书不会被错误压缩成 16 位。
- TLS 1.3 每个证书条目的扩展向量都会验证 framing 与类型唯一性，并保留未知扩展供状态机、OCSP 和 SCT 层处理。
- 所有视图借用握手正文；解析和遍历不分配、不复制证书。

其他消息使用短路径 API：

| API | 契约 |
| --- | --- |
| `xrtTlsEncryptedExtensionsParse()` | 16 位扩展总长必须精确，拒绝重复扩展，并校验 ALPN 单选、空确认扩展和常用限制字段 |
| `xrtTlsCertificateVerifyParse()` | 签名方案保留未知线路值，签名非空且 16 位长度必须恰好消费正文 |
| `xrtTlsFinishedParse()` | 调用方传入协商后的验证数据长度；TLS 1.2 通常为 12，TLS 1.3 为当前 transcript 摘要长度 |
| `xrtTlsKeyUpdateParse()` | 正文必须恰好一字节且只能是 `not_requested` 或 `requested` |
| `xrtTlsSessionTicketParse()` | 区分 TLS 1.2 与 TLS 1.3 格式；TLS 1.3 票据必须非空、寿命不超过七天，early_data 扩展必须恰好四字节 |

未知扩展继续保留，协议状态机再验证它是否曾由对端提供、能否出现在当前消息和是否受本地策略允许。这样协议工具既不会提前封死未来扩展，也不会把重复类型、畸形长度或已知字段的局部错误拖到密码阶段。

### `xrtTlsAlertEncode`

编码一个两字节 Alert 负载。

```c
bool xrtTlsAlertEncode(xtlsalertlevel Level, xtlsalert Alert, void* pOutput, size_t iOutputSize)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `Level` | 输入 | — | 级别 |
| `Alert` | 输入 | — | 告警 |
| `pOutput` | 输入 | 非空 | 输出缓冲 |
| `iOutputSize` | 输入 | — | 接收输出字节数 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 成功 | — |
| `false` | 失败 | 见错误 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_PROTOCOL`（TLS 语义） — DER 或消息结构非法
- `XERR_RANGE` — 容量不足，不写半个结果

#### 范例

[message_tour](../../examples/tls/message_tour/main.c) · record

```c
	if ( !xrtTlsAlertEncode(XTLS_ALERT_WARNING,
			XTLS_ALERT_CLOSE_NOTIFY, arrOut, 2u) ||
		!xrtTlsAlertParse((xbytesview) { arrOut, 2u },
			&AlertLevel, &Alert) ||
		(AlertLevel != XTLS_ALERT_WARNING) ||
		(Alert != XTLS_ALERT_CLOSE_NOTIFY) ) {
```

### `xrtTlsAlertName`

返回 Alert 的稳定英文名称，未知值返回 unknown_alert。

```c
cstr xrtTlsAlertName(xtlsalert Alert)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `Alert` | 输入 | — | 告警 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | 静态名称；未知为 `"UNKNOWN"` | — |

#### 错误

- 无 — 纯查询，不设置错误

#### 范例

[message_tour](../../examples/tls/message_tour/main.c) · record

```c
	if ( (strcmp(xrtTlsAlertName(XTLS_ALERT_CLOSE_NOTIFY),
			"close_notify") != 0) ||
		(strcmp(xrtTlsHandshakeName((xtlshandshaketype)9999u),
			"unknown_handshake") != 0) ||
		(strcmp(xrtTlsHandshakeName(
			XTLS_HANDSHAKE_CLIENT_HELLO),
			"client_hello") != 0) ||
		(strcmp(xrtTlsExtensionName(
			XTLS_EXTENSION_SUPPORTED_GROUPS),
			"supported_groups") != 0) ) {
```

### `xrtTlsAlertParse`

解析恰好一个两字节 Alert 负载。

```c
bool xrtTlsAlertParse(xbytesview Payload, xtlsalertlevel* pLevel, xtlsalert* pAlert)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `Payload` | 输入 | — | 载荷 |
| `pLevel` | 输入 | 非空 | 接收告警级别 |
| `pAlert` | 输入 | 非空 | 接收告警 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 成功 | — |
| `false` | 失败 | 见错误 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_PROTOCOL`（TLS 语义） — DER 或消息结构非法

#### 范例

[message_tour](../../examples/tls/message_tour/main.c) · record

```c
		!xrtTlsAlertParse((xbytesview) { arrOut, 2u },
			&AlertLevel, &Alert) ||
```

### `xrtTlsSessionTicketEncode`

失败原子地编码版本对应的 NewSessionTicket 正文。

```c
bool xrtTlsSessionTicketEncode(const xtlssessionticket* pTicket, void* pOutput, size_t iOutputSize)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pTicket` | 输入 | 非空 | 会话票据 |
| `pOutput` | 输入 | 非空 | 输出缓冲 |
| `iOutputSize` | 输入 | — | 接收输出字节数 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 成功 | — |
| `false` | 失败 | 见错误 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_PROTOCOL`（TLS 语义） — DER 或消息结构非法
- `XERR_RANGE` — 容量不足，不写半个结果

#### 范例

[message_tour](../../examples/tls/message_tour/main.c) · record

```c
		!xrtTlsSessionTicketEncode(&Ticket, arrOut, iSize) ||
```

### `xrtTlsSessionTicketParse`

严格解析版本对应的 NewSessionTicket 正文。

```c
bool xrtTlsSessionTicketParse(xtlsversion Version, xbytesview Body, xtlssessionticket* pTicket)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `Version` | 输入 | — | TLS 版本 |
| `Body` | 输入 | — | 消息体 |
| `pTicket` | 输入 | 非空 | 会话票据 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 成功 | — |
| `false` | 失败 | 见错误 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_PROTOCOL`（TLS 语义） — DER 或消息结构非法

#### 范例

[message_tour](../../examples/tls/message_tour/main.c) · record

```c
		!xrtTlsSessionTicketParse(XTLS_VERSION_13,
			(xbytesview) { arrOut, iSize },
			&TicketParsed) ||
```

### `xrtTlsSessionTicketSize`

返回编码 NewSessionTicket 正文所需长度。

```c
size_t xrtTlsSessionTicketSize(const xtlssessionticket* pTicket)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pTicket` | 输入 | 非空 | 会话票据 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `>= 0` | 数量或字节数 | — |

#### 错误

- 无 — 纯查询，不设置错误

#### 范例

[message_tour](../../examples/tls/message_tour/main.c) · record

```c
	iSize = xrtTlsSessionTicketSize(&Ticket);
```

## 语义消息编码

`tls_messages_write` 为上述消息提供精确 `Size()` 与失败原子的 `Encode()`：

```c
xtlscertificateentry Chain[2] = { 0 };
Chain[0].Data = LeafDer;
Chain[1].Data = IssuerDer;

size_t BodySize = xrtTlsCertificateSize(
	XTLS_VERSION_13, (xbytesview) { NULL, 0 }, Chain, 2
);
if ( (BodySize == 0) || !xrtTlsCertificateEncode(
	XTLS_VERSION_13, (xbytesview) { NULL, 0 }, Chain, 2,
	Body, BodyCapacity
) ) {
	return false;
}
```

- Certificate 与 NewSessionTicket 先验证全部字段、扩展、线路上限、容量和输入重叠，再写第一个字节；失败时输出不变。
- Certificate 接受调用方证书条目数组，不分配完整消息，不只发送叶证书，也不内置固定链数量。
- EncryptedExtensions、CertificateVerify 与 Finished 的单一负载使用 `memmove`，允许原位向前或向后腾出前缀。
- 多字段消息拒绝字段与目标区域重叠；描述结构本身在写入前做局部快照，避免前缀覆盖后续元数据。
- `Size()` 同时充当可编码性验证，不会静默丢弃 TLS 1.2 不存在的请求上下文、条目扩展、nonce 或 `age_add`。

语义编码只产生握手正文。调用方可以继续交给 `xrtTlsHandshakeEncode()` 生成四字节握手头；会话层则会在同一路径中追加 transcript 并交给记录保护层。原始扩展向量始终是逃生口，高级扩展 writer 只是可选构建器。

### `xrtTls13CertificateVerifyContentEncode`

编码 TLS 1.3 CertificateVerify 待签内容，供身份签名与独立验证复用。

```c
bool xrtTls13CertificateVerifyContentEncode(xtlsrole Signer, xbytesview TranscriptHash, void* pOutput, size_t iOutputSize)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `Signer` | 输入 | — | 签名方 |
| `TranscriptHash` | 输入 | — | 脚本哈希 |
| `pOutput` | 输入 | 非空 | 输出缓冲 |
| `iOutputSize` | 输入 | — | 接收输出字节数 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 成功 | — |
| `false` | 失败 | 见错误 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_PROTOCOL`（TLS 语义） — DER 或消息结构非法
- `XERR_RANGE` — 容量不足，不写半个结果

#### 范例

[handshake_extra](../../examples/tls/handshake_extra/main.c) · message tour

```c
		if ( !xrtTls13CertificateVerifyContentEncode(XTLS_SERVER,
				(xbytesview) { arrHash, 32u }, arrContent,
				sizeof(arrContent)) ||
			!xrtSha256(arrContent, 130u, arrDigest) ||
			!xrtEcdsaP256Sign(XCRYPTO_HASH_SHA256, arrDigest,
				EXAMPLE_P256_SCALAR, arrRaw) ||
			!xrtEcdsaDerEncode(arrRaw, 32u, arrDer,
				sizeof(arrDer), &iDerSize) ||
			!xrtTls13CertificateVerifySignature(XTLS_SERVER,
				XTLS_SIGNATURE_ECDSA_SECP256R1_SHA256,
				(xbytesview) { arrHash, 32u },
				(xbytesview) { arrDer, iDerSize },
```

### `xrtTls13CertificateVerifyContentSize`

返回 TLS 1.3 CertificateVerify 待签内容的精确长度。

```c
size_t xrtTls13CertificateVerifyContentSize(xtlsrole Signer, size_t iTranscriptHashSize)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `Signer` | 输入 | — | 签名方 |
| `iTranscriptHashSize` | 输入 | — | 脚本哈希长度 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `>= 0` | 数量或字节数 | — |

#### 错误

- 无 — 纯查询，不设置错误

#### 范例

[message_tour](../../examples/tls/message_tour/main.c) · message tour

```c
		size_t iCvc = xrtTls13CertificateVerifyContentSize(
			XTLS_SERVER, 32u);
```

### `xrtTls13CertificateVerifySignature`

验证 TLS 1.3 对端 CertificateVerify；信任回调不能绕过此步骤。

```c
bool xrtTls13CertificateVerifySignature(xtlsrole Signer, xtlssignature Scheme, xbytesview TranscriptHash, xbytesview Signature, const xx509pubkey* pPublicKey)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `Signer` | 输入 | — | 签名方 |
| `Scheme` | 输入 | — | 签名方案 |
| `TranscriptHash` | 输入 | — | 脚本哈希 |
| `Signature` | 输入 | — | 签名值 |
| `pPublicKey` | 输入 | 非空 | 公钥描述 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 成功 | — |
| `false` | 失败 | 见错误 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_PROTOCOL`（TLS 语义） — DER 或消息结构非法

#### 范例

[handshake_extra](../../examples/tls/handshake_extra/main.c) · message tour

```c
			!xrtTls13CertificateVerifySignature(XTLS_SERVER,
				XTLS_SIGNATURE_ECDSA_SECP256R1_SHA256,
				(xbytesview) { arrHash, 32u },
				(xbytesview) { arrDer, iDerSize },
				&LeafKey) ) {
```

## 认证消息

`tls_auth_messages` 继续拆出认证状态机需要、但不应在客户端和服务端反复手写的线路对象：

| 对象 | 公开契约 |
| --- | --- |
| CertificateRequest | 分别解析 TLS 1.2 与 TLS 1.3 格式；证书类型、签名方案、证书签名方案、请求上下文和颁发者名称都保留为借用视图 |
| TLS 1.2 ECDHE ServerKeyExchange | 发布命名组、公钥、完整 `ServerECDHParams` 和签名；`Parameters` 可直接拼接两个 random 后参与验签 |
| TLS 1.2 ECDHE ClientKeyExchange | 严格解析单个一字节长度 ECPoint |
| CertificateStatus | 当前只接受具有已知正文形状的 OCSP 类型，并发布不透明响应 |
| CompressedCertificate | 发布算法、声明的解压长度和压缩负载，不在 framing 层绑定 zlib、Brotli 或 Zstandard 后端 |

CertificateRequest 的颁发者名称使用公共两级 API：`xrtTlsAuthorities()` 验证完整 16 位总长，`xrtTlsAuthoritiesRead()` 零拷贝遍历任意数量的非空 DER `DistinguishedName`。TLS 1.2 允许空列表；TLS 1.3 一旦携带 `certificate_authorities` 扩展就必须至少有一个名称。TLS 1.3 的 `signature_algorithms` 是必选扩展，`signature_algorithms_cert` 缺失时由后续状态机按协议回退，不在解析结果中伪造一份列表。

```c
xtls12serverkeyexchange Exchange;

if ( !xrtTls12ServerKeyExchangeParse(Handshake.Body, &Exchange) ) {
	return false;
}

/* 验签输入是 client_random || server_random || Exchange.Parameters。 */
```

语法层不根据当前内建密码后端限制 `Group`、签名方案或压缩算法。未知线路值继续发布，协商状态机再检查它是否由本端提供、是否与证书和密码套件匹配，以及公钥长度是否符合所选组。这样新增命名组或外置压缩后端不需要修改基础 parser。OCSP `CertificateStatus` 是例外：未知状态类型没有标准化的可跳过正文形状，因此 parser 明确拒绝。

压缩证书 parser 只接受非空负载和 24 位非零声明长度。后续解压层必须在分配前检查每连接配置上限，要求实际输出恰好等于 `UncompressedSize`，再把输出交给普通 `xrtTlsCertificateParse()`；不能信任对端声明直接分配接近 16 MiB 的缓冲。

`tls_auth_messages_write` 为每个对象提供精确 `Size()` 与失败原子的 `Encode()`。CertificateRequest 和 ServerKeyExchange 是多字段消息，字段不得与目标写入区重叠；ClientKeyExchange、CertificateStatus 和 CompressedCertificate 只有一个不透明负载，使用 `memmove` 支持原位腾出线路前缀。`xrtTlsAuthoritiesSize()` / `Encode()` 接受名称视图数组，不限制条目数量，只受 16 位向量总长限制。

认证消息 API 不执行签名、验签、密钥交换、OCSP ASN.1 解析或解压。这些能力由会话、X.509、crypto 和可选压缩后端逐层组合，避免协议 framing 与算法实现形成不可裁剪的强耦合。

## 记录保护内部契约

记录保护层是握手与会话的内部底座，不额外公开一套让应用直接管理流量密钥的 API。它具备以下已压实边界：

- 每个 `record-key` 只管理一个发送或接收方向，序列号不会跨方向共享。
- TLS 1.3 使用静态 IV 异或 64 位序列号，外层类型固定为 `application_data`，打开后去除内层类型与零填充。
- TLS 1.2 AES-GCM 使用 4 字节静态盐和 8 字节显式序列号；ChaCha20-Poly1305 不在线路上发送显式 nonce。
- 只有认证成功才递增接收序列号；认证失败不会写出明文，也不会改变类型结果。
- TLS 1.3 角色会话写入应用数据时，在 AES-GCM 的 `2^24 - 1`（ChaCha20-Poly1305 的 `UINT64_MAX - 1`）计数处先发送 KeyUpdate，再用新密钥发送数据，为旧密钥保留最后一个记录号。输出背压可以重试，不会重复提交 epoch。控制消息密集但不写应用数据的连接仍可主动 KeyUpdate；TLS 1.2 和底层记录接口在硬上限处拒绝继续使用密钥，由应用关闭或重建连接。
- TLS 1.3 单条记录可选零填充；TLS 1.2 AEAD 记录不接受这一参数。
- 明文与密文缓冲允许精确原位处理；TLS 1.2 AES 打开时明文起点是显式 nonce 后的密文起点。

记录后端不分配堆内存。会话层后续使用网络自适应缓冲承接密文和明文，不为每个连接预留固定 8K 记录数组。

## Transcript 与密钥调度

调度层是握手状态机的内部密码底座，不公开应用直接传入 traffic secret 的 API。它把摘要算法选择与握手解析分开，并保证以下契约：

- 一个 `xtlstranscript` 只保存协商密码套件实际使用的 SHA-256 或 SHA-384 状态，不同时计算两种摘要。
- transcript 支持任意分块追加和不结束状态的摘要快照；失败不会把未初始化或错误长度结果当成有效摘要。
- HelloRetryRequest 使用 RFC 8446 规定的 synthetic `message_hash` 重建 transcript，替换过程成功前不修改原状态。
- TLS 1.3 提供 HKDF-Extract、HKDF-Expand-Label、Derive-Secret 和 Finished 内部原语；secret、transcript hash 与输出长度都按所选摘要严格检查。
- `HkdfLabel` 支持协议允许的 249 字节调用方 label 和 255 字节 context，完整编码最大 514 字节，不沿用旧实现的 256 字节固定缓冲限制。
- TLS 1.2 PRF 流式处理 `label || seed`，不复制到固定数组；大于旧版 256 字节限制的 seed 仍可正常派生。
- SHA-256 和 SHA-384 是独立裁剪后端。只启用公共调度骨架时，所有算法请求以结构化 unsupported 错误拒绝。
- 调度原语不分配堆内存；摘要、HMAC 中间状态和临时 secret 在退出前清零。

TLS 1.3 Expand-Label 输出同时受 16 位线路长度和 HKDF 的 255 个摘要块限制。TLS 1.2 PRF 单次内部派生上限为 65535 字节；握手状态机仍应只请求协议实际需要的几十到数百字节。

## Alert

`xrtTlsAlertParse()` 和 `xrtTlsAlertEncode()` 处理严格的两字节 Alert 负载。`xrtTlsAlertName()` 返回稳定英文名称，便于日志和结构化诊断。

未知 Alert 描述仍会保留其线路数值，由会话状态机按照协议决定是否终止；非法 Alert 级别和错误长度会被拒绝。

## 所有权与线程

- 基础记录和 Alert API 不分配内存。
- 所有输入和解析结果都是借用视图。
- 纯编解码函数没有共享可变状态，可以并发调用。
- 当前线程已有错误不会因 `XTLS_AGAIN` 被清除或覆盖。

## 参考标准

- [RFC 8446: TLS 1.3](https://www.rfc-editor.org/rfc/rfc8446.html)
- [RFC 5246: TLS 1.2](https://www.rfc-editor.org/rfc/rfc5246.html)
- [RFC 6066: TLS 扩展与 SNI](https://www.rfc-editor.org/rfc/rfc6066.html)
- [RFC 7301: ALPN](https://www.rfc-editor.org/rfc/rfc7301.html)
- [RFC 8422: TLS 1.2 椭圆曲线密码](https://www.rfc-editor.org/rfc/rfc8422.html)
- [RFC 8879: TLS 1.3 证书压缩](https://www.rfc-editor.org/rfc/rfc8879.html)
- [RFC 9325: TLS 安全部署建议](https://www.rfc-editor.org/rfc/rfc9325.html)

## 范例与测试

- `examples/tls/messages/main.c`
- `examples/tls/auth_messages/main.c`
- `examples/tls/key_exchange/main.c`
- `examples/tls/context/main.c`
- `examples/tls/record/main.c`
- `tests/tls/test_tls.c`
- `tests/tls/test_tls_mutation.c`
- `tests/tls/test_tls_record.c`
- `tests/tls/test_tls_record_aes.c`
- `tests/tls/test_tls_record_chacha.c`
- `tests/tls/test_tls_handshake.c`
- `tests/tls/test_tls_handshake_mutation.c`
- `tests/tls/test_tls_handshake_reader.c`
- `tests/tls/test_tls_handshake_reader_limits.c`
- `tests/tls/test_tls_hello.c`
- `tests/tls/test_tls_hello_negative.c`
- `tests/tls/test_tls_hello_mutation.c`
- `tests/tls/test_tls_key_exchange.c`
- `tests/tls/test_tls_key_exchange_negative.c`
- `tests/tls/test_tls_context.c`
- `tests/tls/test_tls_context_negative.c`
- `tests/tls/test_tls_context_oom.c`
- `tests/tls/test_tls_hello_write.c`
- `tests/tls/test_tls_hello_write_limits.c`
- `tests/tls/test_tls_messages.c`
- `tests/tls/test_tls_messages_negative.c`
- `tests/tls/test_tls_messages_mutation.c`
- `tests/tls/test_tls_messages_write.c`
- `tests/tls/test_tls_messages_write_limits.c`
- `tests/tls/test_tls_auth_messages.c`
- `tests/tls/test_tls_auth_messages_negative.c`
- `tests/tls/test_tls_auth_messages_mutation.c`
- `tests/tls/test_tls_auth_messages_write.c`
- `tests/tls/test_tls_auth_messages_write_limits.c`
- `tests/tls/test_tls_schedule.c`
- `tests/tls/test_tls_schedule_sha256.c`
- `tests/tls/test_tls_schedule_sha384.c`
- `tests/single/test_single_tls.c`
- `tests/single/test_single_tls_auth_messages.c`
- `tests/single/test_single_tls_auth_messages_write.c`
- `tests/single/test_single_tls_key_exchange.c`
- `tests/single/test_single_tls_context.c`
- `tests/tls/test_tls_session.c`
- `tests/tls/test_tls_session_limits.c`
- `tests/tls/test_tls_session_negative.c`
- `tests/tls/test_tls_session_oom.c`
- `tests/tls/test_tls_session_record.c`
- `tests/tls/test_tls_session_record_oom.c`
- `tests/single/test_single_tls_session.c`
- `tests/single/test_single_tls_session_record.c`

### `xrtTlsSessionCipher`

返回协商后的密码套件；握手尚未选定套件时返回零且不设置错误。

```c
xtlscipher xrtTlsSessionCipher(const xtlssession* pSession)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pSession` | 输入/输出 | 非空 | TLS 会话 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 枚举值 | 当前取值 | — |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法

#### 范例

[session_tour](../../examples/tls/session_tour/main.c) · session tour

```c
		(xrtTlsSessionCipher(pClient) == 0u) ||
```

### `xrtTlsSessionClose`

排队一次 close_notify，并等待密文排空和对端认证关闭。

```c
xtlsresult xrtTlsSessionClose(xtlssession* pSession)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pSession` | 输入/输出 | 非空 | TLS 会话 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `XTLS_OK` | 成功 | — |
| `XTLS_AGAIN` | 暂不可推进 | 不设错误 |
| `XTLS_CLOSED` | 已关闭 | 不设错误 |
| `XTLS_ERROR` | 失败 | 见错误 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_STATE` — 状态非法（顺序、已关闭、非所属线程）
- `XERR_PROTOCOL` — close_notify 写出失败

#### 范例

[session_tour](../../examples/tls/session_tour/main.c) · session tour

```c
	if ( xrtTlsSessionClose(pClient) != XTLS_OK ) {
```

### `xrtTlsSessionContext`

借用会话持有的只读上下文；返回值不得超过会话生命周期使用。

```c
const xtlscontext* xrtTlsSessionContext(const xtlssession* pSession)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pSession` | 输入/输出 | 非空 | TLS 会话 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | 对象或借用 | — |
| `NULL` | 失败或不适用 | 见错误 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_MEMORY` — 分配失败

#### 范例

[session_tour](../../examples/tls/session_tour/main.c) · session tour

```c
		(xrtTlsSessionContext(pClient) == NULL) ||
```

### `xrtTlsSessionDestroy`

销毁会话、释放队列并归还上下文引用；空指针无操作。

```c
void xrtTlsSessionDestroy(xtlssession* pSession)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pSession` | 输入/输出 | 非空 | TLS 会话 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 无 | — | — |

#### 错误

- 无 — 释放或重置不失败（Reset 系列见参数约束）

#### 范例

[client_resume](../../examples/tls/client_resume/main.c) · session tour

```c
	xrtTlsSessionDestroy(pSession);
```

### `xrtTlsSessionEof`

通知底层传输已到 EOF；缺少 close_notify 时报告截断错误。

```c
xtlsresult xrtTlsSessionEof(xtlssession* pSession)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pSession` | 输入/输出 | 非空 | TLS 会话 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `XTLS_OK` | 成功 | — |
| `XTLS_AGAIN` | 暂不可推进 | 不设错误 |
| `XTLS_CLOSED` | 已关闭 | 不设错误 |
| `XTLS_ERROR` | 失败 | 见错误 |

#### 错误

- 无 — 纯查询，不设置错误

#### 范例

[session_tour](../../examples/tls/session_tour/main.c) · session tour

```c
		if ( (xrtTlsSessionEof(pServer) != XTLS_OK) ||
			!xrtTlsSessionPeerAlert(pServer, &Level, &Alert) ||
			(Level != XTLS_ALERT_WARNING) ||
			(Alert != XTLS_ALERT_CLOSE_NOTIFY) ) {
```

### `xrtTlsSessionFeed`

复制一段收到的 TLS 密文；达到输入硬上限时返回 XTLS_AGAIN。

```c
xtlsresult xrtTlsSessionFeed(xtlssession* pSession, const void* pData, size_t iSize)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pSession` | 输入/输出 | 非空 | TLS 会话 |
| `pData` | 输入 | — | 用户数据 |
| `iSize` | 输入 | — | 字节数 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `XTLS_OK` | 成功 | — |
| `XTLS_AGAIN` | 暂不可推进 | 不设错误 |
| `XTLS_CLOSED` | 已关闭 | 不设错误 |
| `XTLS_ERROR` | 失败 | 见错误 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_STATE` — 状态非法（顺序、已关闭、非所属线程）
- `XERR_PROTOCOL` — 记录或握手解密失败（详见会话错误）

#### 范例

[server](../../examples/tls/server/main.c) · session tour

```c
			(Span.Size == 0) || (xrtTlsSessionFeed(
				pTarget, Span.Data, Span.Size
			) != XTLS_OK) || !xrtTlsSessionSendConsume(
				pSource, Span.Size
			) ) {
```

### `xrtTlsSessionFeedBorrow`

借用一段收到的密文，调用方须保持其存活到会话消费或销毁。

```c
xtlsresult xrtTlsSessionFeedBorrow(xtlssession* pSession, const void* pData, size_t iSize)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pSession` | 输入/输出 | 非空 | TLS 会话 |
| `pData` | 输入 | — | 用户数据 |
| `iSize` | 输入 | — | 字节数 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `XTLS_OK` | 成功 | — |
| `XTLS_AGAIN` | 暂不可推进 | 不设错误 |
| `XTLS_CLOSED` | 已关闭 | 不设错误 |
| `XTLS_ERROR` | 失败 | 见错误 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法

#### 范例

[handshake_extra](../../examples/tls/handshake_extra/main.c) · session tour

```c
		Result = xrtTlsSessionFeedBorrow(pTarget, Span.Data,
			Span.Size);
```

### `xrtTlsSessionFeedBuffer`

零复制接管一条密文缓冲链；AGAIN 或失败时源缓冲保持不变。

```c
xtlsresult xrtTlsSessionFeedBuffer(xtlssession* pSession, xnetbuf* pBuffer)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pSession` | 输入/输出 | 非空 | TLS 会话 |
| `pBuffer` | 输入 | 非空 | 缓冲 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `XTLS_OK` | 成功 | — |
| `XTLS_AGAIN` | 暂不可推进 | 不设错误 |
| `XTLS_CLOSED` | 已关闭 | 不设错误 |
| `XTLS_ERROR` | 失败 | 见错误 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法

#### 范例

[session_tour](../../examples/tls/session_tour/main.c) · session tour

```c
				Result = xrtTlsSessionFeedBuffer(pTarget,
					&Chain);
```

### `xrtTlsSessionFeedRef`

接管带释放过程的密文引用；失败时不会调用释放过程。

```c
xtlsresult xrtTlsSessionFeedRef(xtlssession* pSession, const void* pData, size_t iSize, xnetreleaseproc pRelease, ptr pContext)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pSession` | 输入/输出 | 非空 | TLS 会话 |
| `pData` | 输入 | — | 用户数据 |
| `iSize` | 输入 | — | 字节数 |
| `pRelease` | 输入 | — | 释放回调 |
| `pContext` | 输入/输出 | — | 共享上下文 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `XTLS_OK` | 成功 | — |
| `XTLS_AGAIN` | 暂不可推进 | 不设错误 |
| `XTLS_CLOSED` | 已关闭 | 不设错误 |
| `XTLS_ERROR` | 失败 | 见错误 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法

#### 范例

[session_tour](../../examples/tls/session_tour/main.c) · session tour

```c
				Result = xrtTlsSessionFeedRef(pTarget,
					Spans[0].Data, Spans[0].Size,
					exampleRelease, (ptr)pReleased);
```

### `xrtTlsSessionFeedSize`

返回尚未由协议状态机消费的密文字节数。

```c
size_t xrtTlsSessionFeedSize(const xtlssession* pSession)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pSession` | 输入/输出 | 非空 | TLS 会话 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `>= 0` | 数量或字节数 | — |

#### 错误

- 无 — 纯查询，不设置错误

#### 范例

[session_tour](../../examples/tls/session_tour/main.c) · session tour

```c
		(xrtTlsSessionFeedSize(pClient) != 0u) ) {
```

### `xrtTlsSessionFeedTake`

接管一段由 xrtMalloc 家族分配的密文；失败时所有权仍归调用方。

```c
xtlsresult xrtTlsSessionFeedTake(xtlssession* pSession, ptr pData, size_t iSize)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pSession` | 输入/输出 | 非空 | TLS 会话 |
| `pData` | 输入 | — | 用户数据 |
| `iSize` | 输入 | — | 字节数 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `XTLS_OK` | 成功 | — |
| `XTLS_AGAIN` | 暂不可推进 | 不设错误 |
| `XTLS_CLOSED` | 已关闭 | 不设错误 |
| `XTLS_ERROR` | 失败 | 见错误 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法

#### 范例

[session_tour](../../examples/tls/session_tour/main.c) · session tour

```c
				Result = xrtTlsSessionFeedTake(pTarget,
					pCopy, Spans[0].Size);
```

### `xrtTlsSessionPeerAlert`

查询最后收到的对端 Alert；尚未收到时返回 false 且不设置错误。

```c
bool xrtTlsSessionPeerAlert(const xtlssession* pSession, xtlsalertlevel* pLevel, xtlsalert* pAlert)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pSession` | 输入/输出 | 非空 | TLS 会话 |
| `pLevel` | 输入 | 非空 | 接收告警级别 |
| `pAlert` | 输入 | 非空 | 接收告警 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 成功 | — |
| `false` | 失败 | 见错误 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_PROTOCOL`（TLS 语义） — DER 或消息结构非法

#### 范例

[session_tour](../../examples/tls/session_tour/main.c) · session tour

```c
			!xrtTlsSessionPeerAlert(pServer, &Level, &Alert) ||
```

### `xrtTlsSessionPlainConsume`

精确消费应用已经处理的明文字节，禁止静默过量消费。

```c
bool xrtTlsSessionPlainConsume(xtlssession* pSession, size_t iSize)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pSession` | 输入/输出 | 非空 | TLS 会话 |
| `iSize` | 输入 | — | 字节数 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 成功 | — |
| `false` | 失败 | 见错误 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_PROTOCOL`（TLS 语义） — DER 或消息结构非法

#### 范例

[session_tour](../../examples/tls/session_tour/main.c) · session tour

```c
				!xrtTlsSessionPlainConsume(pClient, 5u) ||
```

### `xrtTlsSessionPlainFront`

借用明文读取队列的第一个连续 Span；空队列返回空 Span。

```c
bool xrtTlsSessionPlainFront(const xtlssession* pSession, xnetspan* pSpan)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pSession` | 输入/输出 | 非空 | TLS 会话 |
| `pSpan` | 输入 | 非空 | 接收分片 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 成功 | — |
| `false` | 失败 | 见错误 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_PROTOCOL`（TLS 语义） — DER 或消息结构非法

#### 范例

[session_tour](../../examples/tls/session_tour/main.c) · session tour

```c
			!xrtTlsSessionPlainFront(pClient, &Spans[0]) ||
```

### `xrtTlsSessionPlainSize`

返回等待应用读取的明文字节数。

```c
size_t xrtTlsSessionPlainSize(const xtlssession* pSession)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pSession` | 输入/输出 | 非空 | TLS 会话 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `>= 0` | 数量或字节数 | — |

#### 错误

- 无 — 纯查询，不设置错误

#### 范例

[session_tour](../../examples/tls/session_tour/main.c) · session tour

```c
		(xrtTlsSessionPlainSize(pServer) != 4u) ||
```

### `xrtTlsSessionPlainSpanCount`

返回明文读取队列当前非空 Span 数。

```c
size_t xrtTlsSessionPlainSpanCount(const xtlssession* pSession)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pSession` | 输入/输出 | 非空 | TLS 会话 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `>= 0` | 数量或字节数 | — |

#### 错误

- 无 — 纯查询，不设置错误

#### 范例

[session_tour](../../examples/tls/session_tour/main.c) · session tour

```c
		iCount = xrtTlsSessionPlainSpanCount(pClient);
```

### `xrtTlsSessionPlainSpans`

借用最多给定数量的明文 Span，适合无复制协议解析。

```c
size_t xrtTlsSessionPlainSpans(const xtlssession* pSession, xnetspan* pSpans, size_t iCapacity)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pSession` | 输入/输出 | 非空 | TLS 会话 |
| `pSpans` | 输入 | 非空 | 分片数组 |
| `iCapacity` | 输入 | — | 容量 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `>= 0` | 数量或字节数 | — |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法

#### 范例

[session_tour](../../examples/tls/session_tour/main.c) · session tour

```c
			(xrtTlsSessionPlainSpans(pClient, Spans, 4u) !=
				iCount) ) {
```

### `xrtTlsSessionProtocol`

借用协商后的 ALPN 协议；尚未选择协议时返回 false 且不设置错误。

```c
bool xrtTlsSessionProtocol(const xtlssession* pSession, xbytesview* pProtocol)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pSession` | 输入/输出 | 非空 | TLS 会话 |
| `pProtocol` | 输入 | 非空 | 接收选中协议 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 成功 | — |
| `false` | 失败 | 见错误 |

#### 错误

- 无 — 纯查询，不设置错误

#### 范例

[server](../../examples/tls/server/main.c) · session tour

```c
		!xrtTlsSessionProtocol(pServer, &Protocol) ) {
```

### `xrtTlsSessionRead`

复制并消费明文；无数据返回 AGAIN，认证关闭后返回 CLOSED。

```c
xtlsresult xrtTlsSessionRead(xtlssession* pSession, void* pOutput, size_t iCapacity, size_t* pRead)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pSession` | 输入/输出 | 非空 | TLS 会话 |
| `pOutput` | 输入 | 非空 | 输出缓冲 |
| `iCapacity` | 输入 | — | 容量 |
| `pRead` | 输入 | 非空 | 接收读取游标 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `XTLS_OK` | 成功 | — |
| `XTLS_AGAIN` | 暂不可推进 | 不设错误 |
| `XTLS_CLOSED` | 已关闭 | 不设错误 |
| `XTLS_ERROR` | 失败 | 见错误 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_PROTOCOL`（TLS 语义） — DER 或消息结构非法

#### 范例

[server](../../examples/tls/server/main.c) · session tour

```c
	return (xrtTlsSessionRead(
		pTarget, Output, sizeof(Output), &iRead
	) == XTLS_OK) && (iRead == iSize) &&
```

### `xrtTlsSessionRole`

返回会话的客户端或服务端角色；失败返回零并设置错误。

```c
xtlsrole xrtTlsSessionRole(const xtlssession* pSession)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pSession` | 输入/输出 | 非空 | TLS 会话 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 枚举值 | 当前取值 | — |

#### 错误

- 无 — 纯查询，不设置错误

#### 范例

[handshake_extra](../../examples/tls/handshake_extra/main.c) · session tour

```c
	return (xrtTlsSessionRole(pSession) == XTLS_SERVER) ?
```

### `xrtTlsSessionSendConsume`

精确消费已经由底层传输发送的密文字节，禁止静默过量消费。

```c
bool xrtTlsSessionSendConsume(xtlssession* pSession, size_t iSize)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pSession` | 输入/输出 | 非空 | TLS 会话 |
| `iSize` | 输入 | — | 字节数 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 成功 | — |
| `false` | 失败 | 见错误 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_PROTOCOL`（TLS 语义） — DER 或消息结构非法

#### 范例

[handshake_extra](../../examples/tls/handshake_extra/main.c) · session tour

```c
		if ( !xrtTlsSessionSendConsume(pSource, Span.Size) ) {
```

### `xrtTlsSessionSendFront`

借用密文发送队列的第一个连续 Span；空队列返回空 Span。

```c
bool xrtTlsSessionSendFront(const xtlssession* pSession, xnetspan* pSpan)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pSession` | 输入/输出 | 非空 | TLS 会话 |
| `pSpan` | 输入 | 非空 | 接收分片 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 成功 | — |
| `false` | 失败 | 见错误 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_PROTOCOL`（TLS 语义） — DER 或消息结构非法

#### 范例

[handshake_extra](../../examples/tls/handshake_extra/main.c) · session tour

```c
		if ( !xrtTlsSessionSendFront(pSource, &Span) ||
			(Span.Size == 0u) ) {
```

### `xrtTlsSessionSendSize`

返回等待底层传输发送的密文字节数。

```c
size_t xrtTlsSessionSendSize(const xtlssession* pSession)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pSession` | 输入/输出 | 非空 | TLS 会话 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `>= 0` | 数量或字节数 | — |

#### 错误

- 无 — 纯查询，不设置错误

#### 范例

[client_resume](../../examples/tls/client_resume/main.c) · session tour

```c
		xrtTlsSessionSendSize(pSession),
```

### `xrtTlsSessionSendSpanCount`

返回密文发送队列当前非空 Span 数。

```c
size_t xrtTlsSessionSendSpanCount(const xtlssession* pSession)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pSession` | 输入/输出 | 非空 | TLS 会话 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `>= 0` | 数量或字节数 | — |

#### 错误

- 无 — 纯查询，不设置错误

#### 范例

[session_tour](../../examples/tls/session_tour/main.c) · session tour

```c
		if ( (xrtTlsSessionSendSpanCount(pClient) != 0u) ) {
```

### `xrtTlsSessionSendSpans`

借用最多给定数量的密文发送 Span，供 scatter/gather 发送。

```c
size_t xrtTlsSessionSendSpans(const xtlssession* pSession, xnetspan* pSpans, size_t iCapacity)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pSession` | 输入/输出 | 非空 | TLS 会话 |
| `pSpans` | 输入 | 非空 | 分片数组 |
| `iCapacity` | 输入 | — | 容量 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `>= 0` | 数量或字节数 | — |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法

#### 范例

[session_tour](../../examples/tls/session_tour/main.c) · session tour

```c
		if ( (xrtTlsSessionSendSpans(pSource, Spans, 8u) == 0u) ||
			(Spans[0].Size == 0u) ) {
```

### `xrtTlsSessionState`

返回公开生命周期状态；空会话返回 FAILED 并设置错误。

```c
xtlsstate xrtTlsSessionState(const xtlssession* pSession)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pSession` | 输入/输出 | 非空 | TLS 会话 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 枚举值 | 当前取值 | — |

#### 错误

- 无 — 纯查询，不设置错误

#### 范例

[handshake_extra](../../examples/tls/handshake_extra/main.c) · session tour

```c
		if ( (xrtTlsSessionState(pClient) == XTLS_STATE_READY) &&
			(xrtTlsSessionState(pServer) == XTLS_STATE_READY) ) {
```

### `xrtTlsSessionVersion`

返回协商后的协议版本；握手尚未选定版本时返回零且不设置错误。

```c
xtlsversion xrtTlsSessionVersion(const xtlssession* pSession)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pSession` | 输入/输出 | 非空 | TLS 会话 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 枚举值 | 当前取值 | — |

#### 错误

- 无 — 纯查询，不设置错误

#### 范例

[session_tour](../../examples/tls/session_tour/main.c) · session tour

```c
		(xrtTlsSessionVersion(pClient) != XTLS_VERSION_13) ||
```

### `xrtTlsSessionWait`

返回当前等待原因位；没有等待原因时返回 XTLS_WAIT_NONE。

```c
uint32 xrtTlsSessionWait(const xtlssession* pSession)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pSession` | 输入/输出 | 非空 | TLS 会话 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 数值 | 当前取值 | — |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法

#### 范例

[session_tour](../../examples/tls/session_tour/main.c) · session tour

```c
		(xrtTlsSessionWait(pClient) == 0u) ||
```

### `xrtTlsSessionWrite`

把明文按记录边界加入有界发送队列；允许成功短写。

```c
xtlsresult xrtTlsSessionWrite(xtlssession* pSession, const void* pData, size_t iSize, size_t* pWritten)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pSession` | 输入/输出 | 非空 | TLS 会话 |
| `pData` | 输入 | — | 用户数据 |
| `iSize` | 输入 | — | 字节数 |
| `pWritten` | 输入 | 非空 | 接收写出数量 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `XTLS_OK` | 成功 | — |
| `XTLS_AGAIN` | 暂不可推进 | 不设错误 |
| `XTLS_CLOSED` | 已关闭 | 不设错误 |
| `XTLS_ERROR` | 失败 | 见错误 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_STATE` — 状态非法（顺序、已关闭、非所属线程）
- `XERR_RANGE` — 发送缓冲已满

#### 范例

[server](../../examples/tls/server/main.c) · session tour

```c
	if ( (iSize > sizeof(Output)) || (xrtTlsSessionWrite(
		pSource, sText, iSize, &iWritten
	) != XTLS_OK) || (iWritten != iSize) ||
		!exampleTlsMove(pSource, pTarget) ) {
```

## 公共会话底座

`tls_session` 位于纯协议层与客户端/服务端状态机之间，公共声明单独放在 `<xrt/tls_session.h>`。这个头文件明确组合 `<xrt/tls.h>` 与 `<xrt/net.h>`，而纯协议头 `<xrt/tls.h>` 不反向依赖网络。会话本身不调用 socket；后续客户端、服务端和 TCP 适配器共享同一对象与队列，不再维护第二套 TLS 实现。

会话创建入口分别属于后续 `tls_client` 和 `tls_server`。公共底座不发布 `bool is_server` 式构造器，避免角色配置混用；创建后的 `xtlssession` 由一个线程或 Worker 独占驱动，并持有一份 `xtlscontext` 引用。

三个持久队列都是惰性 `xnetbuf`：

- `Feed` 保存尚未处理的输入密文，提供复制、借用、接管、自定义释放和完整 `xnetbuf` 块链接管五种所有权入口。
- `Send` 保存等待底层传输发送的密文，通过 `Front` / `Spans` 借用视图并由完成路径精确消费。
- `Plain` 保存等待应用读取的明文，既支持 Span 零复制解析，也支持 `xrtTlsSessionRead()` 复制式便捷读取。

收到完整受保护记录时，会话按该记录的精确长度惰性取得一块临时 `Scratch`。认证成功后，应用数据块直接移动到 `Plain`，不再复制；认证失败、OOM 或会话销毁都会先擦除临时明文。`Scratch` 不是每连接常驻的第四个固定缓冲。

创建空会话不会分配任何队列块。`FeedLimit`、`SendLimit` 和 `PlainLimit` 是追加前检查的硬上限；达到上限返回 `XTLS_AGAIN`，队列、引用所有权、记录序号和当前线程错误都保持不变。`xrtTlsSessionFeedBuffer()` 在通过上限检查后零复制移动源 `xnetbuf` 的全部块，成功后源缓冲为空并可复用；`XTLS_AGAIN`、活动写预留或其他失败都保持源缓冲不变。会话与缓冲必须由同一线程或 Worker 驱动，借用块的原始存活约束不会因移动而改变。所有 `Consume` API 拒绝超过待处理字节数的完成通知，不会用静默截断掩盖适配器错误。会话销毁时，其拥有的明文块会在归还 Worker 缓冲池前安全擦除；借用或引用的外部输入永不被写回。

`xrtTlsSessionWrite()` 是应用明文写入入口。它按 TLS 记录上限拆分输入，并在 `SendLimit` 只容纳部分记录时返回 `XTLS_OK` 和实际短写长度；完全没有进展时才返回 `XTLS_AGAIN`。每条记录要么完整进入 `Send` 并消耗一个写序号，要么队列和序号均不改变。底层传输通过 `xrtTlsSessionSendFront()` 或 `xrtTlsSessionSendSpans()` 发送，再以 `xrtTlsSessionSendConsume()` 报告精确完成量。

`xrtTlsSessionClose()` 只排队一次 `close_notify`。收到对端 `close_notify` 时会话自动排队一次回应，等待本地密文排空后进入 `CLOSED`；重复调用不会产生重复 Alert。`xrtTlsSessionPeerAlert()` 可查询最后收到的 Alert。底层传输遇到 EOF 必须调用 `xrtTlsSessionEof()`：只有此前收到认证的 `close_notify` 才是正常关闭，否则报告 `XTLS_ERROR_TRUNCATED`，避免把截断攻击误认为正常 EOF。

客户端或服务端发现协议失败时，会话会按根错误映射并尽力排队一次 fatal Alert：握手密钥尚未建立时使用明文记录，建立后使用当前写 epoch 保护。Alert 排队成功与否不改变失败结果；OOM、发送硬上限或已经结束的传输都不能覆盖最初的结构化错误。会话随后进入 `FAILED`，调用方仍可通过 `SendFront` / `SendSpans` 排空已经生成的 fatal Alert，再关闭底层传输。

`xrtTlsSessionProtocol()` 借用返回状态机已经严格确认的 ALPN 选择，视图有效到会话销毁。尚未协商或对端没有选择 ALPN 时返回 `false`，不设置错误，也不修改输出；空会话或空输出参数才是 `XTLS_ERROR_ARGUMENT`。应用可以在会话进入 `READY` 后查询，协议适配器也可以在内部握手阶段据此选择 HTTP/1.1、HTTP/2 或自定义上层协议。

`xtlswait` 是等待原因位集合：`INPUT`、`OUTPUT`、`APPLICATION`、`IDENTITY` 和 `VERIFY` 可以组合。公开状态只包含 `NEW`、`HANDSHAKE`、`READY`、`CLOSING`、`CLOSED` 与 `FAILED`，内部握手步骤不进入 ABI。`XTLS_AGAIN` 和 `XTLS_CLOSED` 是正常控制结果，不创建、清除或覆盖 `xerror`。

### `xrtTlsResumeConfigInit`

初始化 TLS 1.3 恢复配置，并把签发时间设为当前墙钟时间。

```c
void xrtTlsResumeConfigInit(xtlsresumeconfig* pConfig)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pConfig` | 输入 | — | 配置 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 无 | — | — |

#### 错误

- 无 — 初始化不失败

#### 范例

[client_resume](../../examples/tls/client_resume/main.c) · resume tour

```c
	xrtTlsResumeConfigInit(&ResumeConfig);
```

### `xrtTlsResumeCreate`

创建单次精确分配、深拷贝且可跨线程共享的恢复对象。

```c
xtlsresume* xrtTlsResumeCreate(const xtlsresumeconfig* pConfig)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pConfig` | 输入 | — | 配置 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | 对象或借用 | — |
| `NULL` | 失败或不适用 | 见错误 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_MEMORY` — 分配失败

#### 范例

[client_resume](../../examples/tls/client_resume/main.c) · resume tour

```c
	pResume = xrtTlsResumeCreate(&ResumeConfig);
```

### `xrtTlsResumeInfo`

发布恢复对象的只读信息快照；输出视图不得超过对象引用生命周期。

```c
bool xrtTlsResumeInfo(const xtlsresume* pResume, xtlsresumeinfo* pInfo)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pResume` | 输入/输出 | 非空 | 会话恢复对象 |
| `pInfo` | 输入 | 非空 | 接收信息 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 成功 | — |
| `false` | 失败 | 见错误 |

#### 错误

- 无 — 纯查询，不设置错误

#### 范例

[resume](../../examples/tls/resume/main.c) · resume tour

```c
	if ( (pResume == NULL) || !xrtTlsResumeInfo(pResume, &Info) ) {
```

### `xrtTlsResumeRelease`

释放恢复对象，并在最后一个引用结束时清除票据、PSK 与全部元数据。

```c
void xrtTlsResumeRelease(xtlsresume* pResume)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pResume` | 输入/输出 | 非空 | 会话恢复对象 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 无 | — | — |

#### 错误

- 无 — 释放或重置不失败（Reset 系列见参数约束）

#### 范例

[client_resume](../../examples/tls/client_resume/main.c) · resume tour

```c
	xrtTlsResumeRelease(pResume);
```

### `xrtTlsResumeRetain`

增加恢复对象引用；对象内容在全部引用之间保持只读。

```c
xtlsresume* xrtTlsResumeRetain(const xtlsresume* pResume)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pResume` | 输入/输出 | 非空 | 会话恢复对象 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | 对象或借用 | — |
| `NULL` | 失败或不适用 | 见错误 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法

#### 范例

[resume_tour](../../examples/tls/resume_tour/main.c) · resume tour

```c
		((pRetained = xrtTlsResumeRetain(pResume)) == NULL) ||
```

### `xrtTlsResumeTicketAge`

计算 TLS 1.3 ClientHello 使用的混淆票据年龄，过期时不修改输出。

```c
bool xrtTlsResumeTicketAge(const xtlsresume* pResume, xtime iNow, uint32* pAge)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pResume` | 输入/输出 | 非空 | 会话恢复对象 |
| `iNow` | 输入 | — | 当前时刻 |
| `pAge` | 输入 | 非空 | 票据年龄 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 成功 | — |
| `false` | 失败 | 见错误 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_PROTOCOL`（TLS 语义） — DER 或消息结构非法

#### 范例

[resume_tour](../../examples/tls/resume_tour/main.c) · resume tour

```c
		!xrtTlsResumeTicketAge(pResume, xrtNow(), &iAge) ) {
```

### `xrtTlsResumeValidAt`

判断给定墙钟时刻是否位于票据的半开有效区间内。

```c
bool xrtTlsResumeValidAt(const xtlsresume* pResume, xtime iNow)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pResume` | 输入/输出 | 非空 | 会话恢复对象 |
| `iNow` | 输入 | — | 当前时刻 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 成功 | — |
| `false` | 失败 | 见错误 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_PROTOCOL`（TLS 语义） — DER 或消息结构非法

#### 范例

[resume_tour](../../examples/tls/resume_tour/main.c) · resume tour

```c
		!xrtTlsResumeValidAt(pResume, xrtNow()) ||
```

## TLS 会话恢复对象

`<xrt/tls_resume.h>` 提供独立可裁剪的 `xtlsresume`，用于在 TLS 状态机、调用方缓存和后续连接之间传递恢复资产。对象创建时深拷贝 ticket、PSK、SNI、ALPN 和可选对端身份，一次精确分配后保持不可变；引用计数允许跨线程共享，最后一个引用释放前会清除整块内存。协议客户端不拥有全局缓存，应用可以直接保存对象，也可以在后续组合显式缓存策略。

```c
xtlsresumeconfig Config;
xtlsresumeinfo Info;
xtlsresume* Resume;

xrtTlsResumeConfigInit(&Config);
Config.Cipher = XTLS_AES_128_GCM_SHA256;
Config.Ticket = Ticket;
Config.Secret = Psk;
Config.ServerName = XRT_STR_LITERAL("example.com");
Config.Protocol = XRT_BYTES_LITERAL("h2");
Config.Lifetime = 3600;
Resume = xrtTlsResumeCreate(&Config);

if ( Resume != NULL ) {
	(void)xrtTlsResumeInfo(Resume, &Info);
	xrtTlsResumeRelease(Resume);
}
```

当前对象契约只接受 TLS 1.3：ticket 必须为 1 到 65535 字节，PSK 长度必须等于密码套件摘要长度，寿命必须位于 1 到 604800 秒。ALPN 最多 255 字节，SNI 拒绝内嵌空字节；可选 `PeerIdentity` 是不参与线路编码的调用方身份域，可保存证书或信任策略摘要。`xrtTlsResumeInfo()` 发布的全部视图只在对象引用存活期间有效，其中 `Secret` 是敏感只读视图，调用方不得修改或无保护地记录。

有效期使用 `[IssuedAt, ExpiresAt)` 半开区间；墙钟回退到签发时刻以前时对象安全失效，不会延长票据寿命。`xrtTlsResumeTicketAge()` 先验证有效期，再按毫秒向下取整并与 `AgeAdd` 执行协议规定的 32 位模加，失败时不修改输出。对象层只表达材料和时间，不决定票据替换、容量、并发或淘汰策略。

对象生命周期示例位于 `examples/tls/resume/main.c`，下一连接 ClientHello 示例位于 `examples/tls/client_resume/main.c`；深拷贝、引用、时间边界、非法字段、OOM 和单头门禁位于 `tests/tls/test_tls_resume*.c` 与 `tests/single/test_single_tls_resume.c`。对象释放路径直接复用已经独立验证的 `xrtSecureZero()` 清零原语。

恢复对象可以直接交给下一条客户端连接。`Resume` 在创建期间由配置借用；创建成功后会话持有独立引用，因此调用方可以立即释放自己的引用：

```c
xtlsclientconfig ClientConfig;

xrtTlsClientConfigInit(&ClientConfig);
ClientConfig.Resume = Resume;
ClientConfig.Verifier = Verifier; /* 允许票据被拒后回退到证书握手。 */
Session = xrtTlsClientCreate(&ClientConfig, Pool);
xrtTlsResumeRelease(Resume);
```

省略 `ServerName` 和 ALPN 列表时，客户端从恢复对象精确继承两者。显式 SNI 必须完全匹配票据绑定；显式 ALPN 列表必须包含票据协议，票据未绑定 ALPN 时则不能为恢复连接额外提供协议。过期、尚未生效、套件被当前策略禁用或路由域不匹配的对象都在会话分配前拒绝。



### `xrtTlsClientCertificate`

借用一张已解析对端证书，视图稳定到客户端会话销毁。

```c
const xx509cert* xrtTlsClientCertificate(const xtlssession* pSession, size_t iIndex)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pSession` | 输入/输出 | 非空 | TLS 会话 |
| `iIndex` | 输入 | — | 索引 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | 对象或借用 | — |
| `NULL` | 失败或不适用 | 见错误 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_MEMORY` — 分配失败

#### 范例

[handshake_extra](../../examples/tls/handshake_extra/main.c) · context

```c
			((pPeerCert = xrtTlsClientCertificate(pClient, 0u)) ==
				NULL) ||
```

### `xrtTlsClientCertificateCount`

返回完整握手中已经深复制并验证的对端证书数量。

```c
size_t xrtTlsClientCertificateCount(const xtlssession* pSession)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pSession` | 输入/输出 | 非空 | TLS 会话 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `>= 0` | 数量或字节数 | — |

#### 错误

- 无 — 纯查询，不设置错误

#### 范例

[handshake_extra](../../examples/tls/handshake_extra/main.c) · context

```c
		if ( (xrtTlsClientCertificateCount(pClient) != 2u) ||
			((pPeerCert = xrtTlsClientCertificate(pClient, 0u)) ==
				NULL) ||
			!xrtX509Parse(exampleLeafDer,
				sizeof(exampleLeafDer), &Leaf) ||
			(pPeerCert->Raw.Size != Leaf.Raw.Size) ||
			!xrtTlsServerName(pServer, &ServerName) ||
			(ServerName.Size != 9u) ||
			(memcmp(ServerName.Data, "localhost", 9u) != 0) ) {
```

### `xrtTlsClientConfigInit`

初始化使用默认共享策略、无 SNI 和无 ALPN 的客户端配置。

```c
void xrtTlsClientConfigInit(xtlsclientconfig* pConfig)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pConfig` | 输入 | — | 配置 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 无 | — | — |

#### 错误

- 无 — 初始化不失败

#### 范例

[client_resume](../../examples/tls/client_resume/main.c) · context

```c
	xrtTlsClientConfigInit(&ClientConfig);
```

### `xrtTlsClientCreate`

创建客户端会话；默认要求 Verifier，无验证器时必须启用 ResumeOnly。

```c
xtlssession* xrtTlsClientCreate(const xtlsclientconfig* pConfig, xnetbufpool* pPool)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pConfig` | 输入 | — | 配置 |
| `pPool` | 输入 | 非空 | 任务池 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | 对象或借用 | — |
| `NULL` | 失败或不适用 | 见错误 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_MEMORY` — 分配失败

#### 范例

[client_resume](../../examples/tls/client_resume/main.c) · context

```c
	pSession = xrtTlsClientCreate(&ClientConfig, NULL);
```

### `xrtTlsClientDrive`

在公平性预算内消费已喂入记录并推进客户端握手状态。

```c
xtlsresult xrtTlsClientDrive(xtlssession* pSession)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pSession` | 输入/输出 | 非空 | TLS 会话 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `XTLS_OK` | 成功 | — |
| `XTLS_AGAIN` | 暂不可推进 | 不设错误 |
| `XTLS_CLOSED` | 已关闭 | 不设错误 |
| `XTLS_ERROR` | 失败 | 见错误 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_STATE` — 状态非法（顺序、已关闭、非所属线程）
- `XERR_PROTOCOL` — 握手失败（错误经会话报告）

#### 范例

[handshake_extra](../../examples/tls/handshake_extra/main.c) · context

```c
		xrtTlsClientDrive(pSession);
```

### `xrtTlsClientKeyShares`

严格解析 ClientHello key_share 列表并把游标重置到首项。

```c
bool xrtTlsClientKeyShares(xbytesview Data, xtlskeysharecursor* pCursor)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `Data` | 输入 | — | 数据 |
| `pCursor` | 输入/输出 | 非空 | 游标 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 成功 | — |
| `false` | 失败 | 见错误 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_PROTOCOL`（TLS 语义） — DER 或消息结构非法

#### 范例

[extension_tour](../../examples/tls/extension_tour/main.c) · context

```c
		if ( !xrtTlsClientKeyShares(
				(xbytesview) { arrKsEmpty, 2u },
				&KsCursor) ||
			(xrtTlsKeySharesRead(&KsCursor, &Share) !=
				XTLS_ITEM_DONE) ) {
```

### `xrtTlsClientKeyUpdate`

用当前 TLS 1.3 写 epoch 排队 KeyUpdate；TLS 1.2 会话返回不支持。

```c
xtlsresult xrtTlsClientKeyUpdate(xtlssession* pSession, xtlskeyupdate Request)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pSession` | 输入/输出 | 非空 | TLS 会话 |
| `Request` | 输入 | — | 请求 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `XTLS_OK` | 成功 | — |
| `XTLS_AGAIN` | 暂不可推进 | 不设错误 |
| `XTLS_CLOSED` | 已关闭 | 不设错误 |
| `XTLS_ERROR` | 失败 | 见错误 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法

#### 范例

[handshake_extra](../../examples/tls/handshake_extra/main.c) · context

```c
		(xrtTlsClientKeyUpdate(pClient,
			XTLS_KEY_UPDATE_REQUESTED) != XTLS_OK) ||
```

### `xrtTlsClientPsks`

严格解析 ClientHello PSK 列表并重置同步游标。

```c
bool xrtTlsClientPsks(xbytesview Data, xtlspskcursor* pCursor)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `Data` | 输入 | — | 数据 |
| `pCursor` | 输入/输出 | 非空 | 游标 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 成功 | — |
| `false` | 失败 | 见错误 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_PROTOCOL`（TLS 语义） — DER 或消息结构非法

#### 范例

[extension_tour](../../examples/tls/extension_tour/main.c) · context

```c
		if ( !xrtTlsClientPsks((xbytesview) { arrPsk, 50u },
				&PskCursor) ) {
```

### `xrtTlsClientResumeCount`

返回等待调用方接管的恢复对象数量。

```c
size_t xrtTlsClientResumeCount(const xtlssession* pSession)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pSession` | 输入/输出 | 非空 | TLS 会话 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `>= 0` | 数量或字节数 | — |

#### 错误

- 无 — 纯查询，不设置错误

#### 范例

[resume_tour](../../examples/tls/resume_tour/main.c) · context

```c
		(xrtTlsClientResumeCount(pClient) == 0u); i++ ) {
```

### `xrtTlsClientResumeDropped`

返回因队列关闭、容量淘汰或可选缓存 OOM 而未保留的有效票据总数。

```c
uint64 xrtTlsClientResumeDropped(const xtlssession* pSession)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pSession` | 输入/输出 | 非空 | TLS 会话 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 数值 | 当前取值 | — |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法

#### 范例

[resume_tour](../../examples/tls/resume_tour/main.c) · context

```c
			(xrtTlsClientResumeDropped(pClient2) !=
				xrtTlsClientResumeDropped(pClient2)) ) {
```

### `xrtTlsClientResumed`

返回服务端是否接受了本次创建时提供的恢复对象。

```c
bool xrtTlsClientResumed(const xtlssession* pSession)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pSession` | 输入/输出 | 非空 | TLS 会话 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 成功 | — |
| `false` | 失败 | 见错误 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_PROTOCOL`（TLS 语义） — DER 或消息结构非法

#### 范例

[client_resume](../../examples/tls/client_resume/main.c) · context

```c
		xrtTlsClientResumed(pSession) ? "yes" : "not yet"
```

### `xrtTlsClientTakeResume`

从队首取出一张恢复票据并把唯一会话引用转移给调用方。

```c
xtlsresume* xrtTlsClientTakeResume(xtlssession* pSession)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pSession` | 输入/输出 | 非空 | TLS 会话 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | 对象或借用 | — |
| `NULL` | 失败或不适用 | 见错误 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_MEMORY` — 分配失败

#### 范例

[resume_tour](../../examples/tls/resume_tour/main.c) · context

```c
		((pResume = xrtTlsClientTakeResume(pClient)) == NULL) ||
```

### `xrtTlsClientVersionSelect`

从 ClientHello 选择版本，并在扩展缺失时只允许 TLS 1.2。

```c
xtlsitemresult xrtTlsClientVersionSelect(const xtlsclienthello* pHello, const xtlsversion* pPreferred, size_t iPreferredCount, xtlsversion* pSelected)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pHello` | 输入 | 非空 | Hello 消息 |
| `pPreferred` | 输入 | 非空 | 偏好数组 |
| `iPreferredCount` | 输入 | — | 偏好数量 |
| `pSelected` | 输入 | 非空 | 接收选中结果 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 枚举值 | 当前取值 | — |

#### 错误

- 无 — 纯查询，不设置错误

#### 范例

[message_tour](../../examples/tls/message_tour/main.c) · context

```c
		if ( (xrtTlsClientVersionSelect(&Hello, arrPref, 2u,
				&Selected) != XTLS_ITEM_VALUE) ||
			(Selected != XTLS_VERSION_13) ) {
```

### `xrtTlsClientVersions`

严格解析 ClientHello supported_versions 扩展数据。

```c
bool xrtTlsClientVersions(xbytesview Data, xtlsids* pVersions)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `Data` | 输入 | — | 数据 |
| `pVersions` | 输入 | 非空 | 版本数组 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 成功 | — |
| `false` | 失败 | 见错误 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_PROTOCOL`（TLS 语义） — DER 或消息结构非法

#### 范例

[message_tour](../../examples/tls/message_tour/main.c) · context

```c
		if ( !xrtTlsClientVersions(
				(xbytesview) { arrCv, 5u },
				&Ids) ||
			(xrtTlsIdsCount(&Ids) != 2u) ) {
```

## TLS 客户端

`<xrt/tls_client.h>` 提供独立客户端构造入口，不把客户端字段塞进服务端配置，也不调用 socket。`xtlsclientconfig` 在创建期间借用共享上下文、SNI、ALPN 数组、可选 verifier 和可选恢复对象；创建成功后，会话持有这些共享对象的引用，并在同一块角色尾部内存中保存名称、协议名、临时密钥、扩展工作区和初始 ClientHello 的独立快照。

```c
static const xstrview Protocols[] = {
	{ "h2", sizeof("h2") - 1u },
	{ "http/1.1", sizeof("http/1.1") - 1u }
};
xtlsclientconfig Config;
xtlssession* Session;

xrtTlsClientConfigInit(&Config);
Config.ServerName = XRT_STR_LITERAL("example.com");
Config.Protocols = Protocols;
Config.ProtocolCount = sizeof(Protocols) / sizeof(Protocols[0]);
Config.Verifier = Verifier; /* 使用下文创建的证书验证器。 */
Session = xrtTlsClientCreate(&Config, Pool);
```

初始化配置使用默认共享策略、无 SNI 和无 ALPN；完整证书握手必须显式绑定 verifier，否则在创建时失败。构造器只发布当前裁剪构建可以执行的参数：套件需要对应 AEAD 和摘要调度，组需要密钥交换后端，签名需要本构建的验签后端。TLS 1.2/1.3 均已接入；TLS 1.2 要求 EMS 和 RFC 5746 的空初始绑定，不提供重新协商。Ed448 不会发布；P-521/SHA-512 不会出现在允许 TLS 1.3 的实际 offer 中。TLS 1.2-only 的 `0x0603` 是 SHA-512/ECDSA 组合，仍可用于已有 P-256/P-384 后端。纯 PSK 恢复构建没有验签后端时可以省略签名扩展，不因此新增密码算法。

创建成功会立即生成密码安全随机数、兼容 session ID 和首选可用组的一份 key share，并排队一条完整明文 ClientHello 记录。SNI、ALPN、版本、组、签名和 key_share 均使用公共 Hello writer 编码，不保留固定 1024 字节构建缓冲；发送队列仍受上下文 `SendLimit` 约束。此时会话进入 `XTLS_STATE_HANDSHAKE`，等待原因同时包含输入和输出，传输层可直接发送 `xrtTlsSessionSendFront()` 或 `xrtTlsSessionSendSpans()` 返回的密文。

`xrtTlsClientDrive()` 在上下文记录与握手预算内消费已经 Feed 的线路记录。普通 TLS 1.3 ServerHello 必须精确回显兼容 session ID，只能选择实际写入 ClientHello 的套件和 key share，并同时携带唯一的 `supported_versions` 与 `key_share`。ServerHello 可以跨任意数量的明文记录重组；兼容 CCS 只接受单字节 `1`，Alert、意外明文应用数据、尾随握手消息和未 offer 的选择都不会穿过状态机。

有效 ServerHello 先在临时状态中完成 transcript、ECDHE、handshake secret、双向 traffic secret 和记录 key/IV 派生，再消费记录并一次替换收发 epoch。任一步失败都会进入 `XTLS_STATE_FAILED`，不会发布半组密钥；成功后立即擦除客户端临时私钥，接收方向使用服务端流量秘密，发送方向使用客户端流量秘密。`XTLS_AGAIN` 只表示需要更多输入或后续阶段尚未推进，不覆盖线程错误。

握手密钥切换后，EncryptedExtensions 必须位于 TLS 1.3 受保护记录中。状态机允许 SNI 确认、`supported_groups` 和 ALPN，拒绝 `early_data`、未知扩展、未请求的 SNI/ALPN 和不在客户端真实线路 offer 中的协议。消息支持跨加密记录重组；一条记录内的后续握手消息不会被提前消费，而是以稳定记录偏移留给下一状态。只有整条 EE 通过解析、出现位置和 offer 校验后，transcript 与 ALPN 才会一起提交。

Certificate 使用严格公共 parser，拒绝非空请求上下文和客户端未请求的每个 CertificateEntry 扩展。状态机先精确测量完整线路链，再用一次分配保存证书视图与独立 DER；证书视图数组按目标 ABI 的 `xx509cert` 对齐，不能假设父对象尾地址天然满足 64 位字段对齐。链长度没有固定张数上限，只受握手消息和整数上限约束。信任验证和 transcript 更新都成功后才发布对端快照。CertificateVerify 必须使用 ClientHello 真实提供的方案，并以消息加入 transcript 之前的摘要验证 RFC 8446 上下文签名；自定义信任回调不能绕过此步骤。

启用客户端验证后，`xrtTlsClientCertificateCount()` 返回当前已经认证的链长度，`xrtTlsClientCertificate()` 按索引借用稳定到会话销毁的 `xx509cert`；原始 DER 位于 `Certificate.Raw`，不需要重复的 DER 指针和长度 API。握手尚未收到证书或本次使用 PSK 恢复时数量为零，因为恢复线路没有重传证书；按索引查询此时返回状态错误，不能把票据中的身份摘要伪装成证书。

CertificateVerify 通过后，客户端严格按服务端握手流量秘密校验 Finished。校验使用加入服务端 Finished 之前的 transcript；成功后先把服务端 Finished 纳入临时 transcript，再派生主秘密和双向应用流量秘密，并以仍在生效的客户端握手写 epoch 排队客户端 Finished。只有该记录完整进入有界 Send 队列后，状态机才一次提交 transcript、应用收发 epoch 和 `XTLS_STATE_READY`；输出空间不足返回 `XTLS_AGAIN`，不会消费服务端 Finished 或提前切换密钥。

进入 READY 后，同一个 `xrtTlsClientDrive()` 继续处理应用记录、Alert 与后握手消息。应用明文以 Scratch 块零拷贝移动到 Plain 队列；达到 `PlainLimit` 时返回 `XTLS_AGAIN` 并发布 `XTLS_WAIT_APPLICATION`，挂起记录不会重复解密或再次递增接收序列号。收到 `close_notify` 后客户端自动排队一次响应，忽略对端随后发送的应用数据，并在本地响应排空后进入 `CLOSED`。

TLS 1.3 KeyUpdate 必须独占一条记录。客户端收到 `update_not_requested` 时只替换读取 secret/key；收到 `update_requested` 时先用旧写 epoch 排队 `not_requested` 应答，再一次提交新收发 epoch。强制应答遇到 `SendLimit` 会保持活动 secret、写序列号和挂起记录不变，待输出排空后重试。应用也可以主动轮换写方向：

```c
xtlsresult Result = xrtTlsClientKeyUpdate(
	Session,
	XTLS_KEY_UPDATE_REQUESTED
);
```

主动消息同样使用旧写 epoch，只有完整记录入队后才替换客户端写 secret/key；`XTLS_AGAIN` 不会让连接进入半更新状态。线路队列保证已经排队的旧 epoch 数据、KeyUpdate 和随后新 epoch 数据保持调用顺序。

启用 `XRT_FEATURE_TLS_CLIENT_RESUME` 后，客户端从包含客户端 Finished 的最终 transcript 派生 resumption master secret，再按每张 `NewSessionTicket` 的 nonce 派生独立 PSK。发布对象深拷贝 ticket、实际 SNI、实际协商 ALPN 和已验证叶证书 SHA-256 身份；`MaxEarlyData` 只保存服务端限制，不代表当前客户端支持发送 0-RTT。

`ResumeLimit` 默认为 4，可设为 0 禁用保存，最大为 64。队列槽位包含在客户端角色的单块尾部内存中，不为节点单独分配；满队列淘汰最旧票据并保留最新票据。`xrtTlsClientResumeCount()` 查询数量，`xrtTlsClientResumeDropped()` 统计显式禁用、容量淘汰和恢复对象 OOM，`xrtTlsClientTakeResume()` 按接收顺序把会话持有的引用转移给调用方。空队列返回 `NULL` 且不改变错误状态；对象缓存、目标选择和跨连接并发仍由调用方拥有。已进入 `READY` 的连接不会因为可选票据缓存 OOM 失败，该票据只计入丢弃统计且不会泄漏临时线程错误。

`NewSessionTicket` 支持跨记录重组和同记录多消息，畸形寿命、长度、空票据或已知扩展会使客户端失败。下一条 ClientHello 最多提供一张外部票据，同时发送 `psk_dhe_ke`、正常 X25519 key share 和位于扩展列表末尾的 `pre_shared_key`。binder 从最终编码的 ClientHello 截断 transcript、票据 PSK 和套件摘要独立计算；当前不发送 0-RTT。

服务端可以不选择 PSK，此时客户端安全回退到完整证书握手。服务端选择时只接受 identity `0`，套件摘要必须与票据一致且仍必须提供有效 key share；接受后客户端跳过 Certificate/CertificateVerify，直接校验服务端 Finished，再按正常路径发送客户端 Finished 并切换应用 epoch。恢复连接的 ALPN 必须与票据绑定完全一致，后续新票据继承原来已经认证的 `PeerIdentity`。`xrtTlsClientResumed()` 在服务端 ServerHello 接受 identity `0` 后返回 `true`；它报告协商选择，不把后续握手成功状态混入同一个查询。

当前门禁已经覆盖证书认证和 PSK+DHE 恢复型 TLS 1.3 客户端从 ClientHello 到持续 READY 的主路径：独立复算 binder、双方 Finished、应用流量秘密、resumption master 与 ticket PSK，验证恢复回退、错误 identity、缺失 key share、ALPN 绑定、双向应用数据、认证关闭、KeyUpdate 两种请求、主动更新、旧/新 epoch 顺序、票据分片/聚合/有界淘汰/深拷贝路由域、应用及输出背压、非法消息、目标 OOM、GCC、TinyCC x86 和单头文件。客户端同时支持带 extended master secret 的 TLS 1.2 ECDHE 证书完整握手、ALPN、双向应用数据与认证关闭；TLS 1.2 不提供会话恢复、重新协商或 KeyUpdate。非 ResumeOnly 配置缺少 verifier 时在创建期拒绝。客户端与服务端支持单次 HRR；TLS 1.3 应用写入会在记录用量临界点自动换钥。CertificateRequest、客户端证书认证和 0-RTT 未实现。对应门禁位于 `tests/tls/test_tls_client.c`、`tests/tls/test_tls_client_server_hello.c`、`tests/tls/test_tls_server12.c`、`tests/tls/test_tls_client_oom.c`、`tests/single/test_single_tls_client.c` 和 `tests/single/test_single_tls_client_server_hello.c`。



### `xrtTlsServerConfigInit`

初始化默认上下文、无身份、无 ALPN 和无动态选择器的服务端配置。

```c
void xrtTlsServerConfigInit(xtlsserverconfig* pConfig)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pConfig` | 输入 | — | 配置 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 无 | — | — |

#### 错误

- 无 — 初始化不失败

#### 范例

[handshake_extra](../../examples/tls/handshake_extra/main.c) · listener tour

```c
	xrtTlsServerConfigInit(&ServerConfig);
```

### `xrtTlsServerCookie`

返回选择器为本次握手保存的不透明宿主 Cookie；未设置时返回零。

```c
bool xrtTlsServerCookie(const xtlssession* pSession, uint64* pCookie)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pSession` | 输入/输出 | 非空 | TLS 会话 |
| `pCookie` | 输入 | 非空 | Cookie |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 成功 | — |
| `false` | 失败 | 见错误 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_PROTOCOL`（TLS 语义） — DER 或消息结构非法

#### 范例

[handshake_extra](../../examples/tls/handshake_extra/main.c) · listener tour

```c
		(void)xrtTlsServerCookie(pServer, &Cookie);
```

### `xrtTlsServerCreate`

创建等待 ClientHello 的服务端会话；必须提供静态身份或选择器。

```c
xtlssession* xrtTlsServerCreate(const xtlsserverconfig* pConfig, xnetbufpool* pPool)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pConfig` | 输入 | — | 配置 |
| `pPool` | 输入 | 非空 | 任务池 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | 对象或借用 | — |
| `NULL` | 失败或不适用 | 见错误 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_MEMORY` — 分配失败

#### 范例

[handshake_extra](../../examples/tls/handshake_extra/main.c) · listener tour

```c
	pServer = xrtTlsServerCreate(&ServerConfig, NULL);
```

### `xrtTlsServerDrive`

在公平性预算内消费输入并推进服务端握手和后握手状态。

```c
xtlsresult xrtTlsServerDrive(xtlssession* pSession)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pSession` | 输入/输出 | 非空 | TLS 会话 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `XTLS_OK` | 成功 | — |
| `XTLS_AGAIN` | 暂不可推进 | 不设错误 |
| `XTLS_CLOSED` | 已关闭 | 不设错误 |
| `XTLS_ERROR` | 失败 | 见错误 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_STATE` — 状态非法（顺序、已关闭、非所属线程）
- `XERR_PROTOCOL` — 握手失败（错误经会话报告）

#### 范例

[handshake_extra](../../examples/tls/handshake_extra/main.c) · listener tour

```c
		xrtTlsServerDrive(pSession) :
```

### `xrtTlsServerKeyShare`

严格解析普通 ServerHello 中唯一的 key_share。

```c
bool xrtTlsServerKeyShare(xbytesview Data, xtlskeyshare* pShare)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `Data` | 输入 | — | 数据 |
| `pShare` | 输入 | 非空 | 密钥份额 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 成功 | — |
| `false` | 失败 | 见错误 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_PROTOCOL`（TLS 语义） — DER 或消息结构非法

#### 范例

[message_tour](../../examples/tls/message_tour/main.c) · listener tour

```c
		if ( !xrtTlsServerKeyShare(
				(xbytesview) { arrKs, 8u }, &Share) ||
			(Share.Group != 0x001Du) ||
			(Share.Key.Size != 4u) ) {
```

### `xrtTlsServerKeyUpdate`

用当前 TLS 1.3 写 epoch 排队 KeyUpdate；TLS 1.2 会话返回不支持。

```c
xtlsresult xrtTlsServerKeyUpdate(xtlssession* pSession, xtlskeyupdate Request)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pSession` | 输入/输出 | 非空 | TLS 会话 |
| `Request` | 输入 | — | 请求 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `XTLS_OK` | 成功 | — |
| `XTLS_AGAIN` | 暂不可推进 | 不设错误 |
| `XTLS_CLOSED` | 已关闭 | 不设错误 |
| `XTLS_ERROR` | 失败 | 见错误 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法

#### 范例

[handshake_extra](../../examples/tls/handshake_extra/main.c) · listener tour

```c
	if ( (xrtTlsServerKeyUpdate(pServer,
			XTLS_KEY_UPDATE_NOT_REQUESTED) != XTLS_OK) ||
		!exampleMove(pServer, pClient) ||
		(xrtTlsClientKeyUpdate(pClient,
			XTLS_KEY_UPDATE_REQUESTED) != XTLS_OK) ||
		!exampleMove(pClient, pServer) ) {
```

### `xrtTlsServerName`

借用服务端从 ClientHello 深复制的 SNI；尚未收到名称时返回 false。

```c
bool xrtTlsServerName(const xtlssession* pSession, xbytesview* pServerName)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pSession` | 输入/输出 | 非空 | TLS 会话 |
| `pServerName` | 输入 | 非空 | 服务器名称 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 成功 | — |
| `false` | 失败 | 见错误 |

#### 错误

- 无 — 纯查询，不设置错误

#### 范例

[handshake_extra](../../examples/tls/handshake_extra/main.c) · listener tour

```c
			!xrtTlsServerName(pServer, &ServerName) ||
```

### `xrtTlsServerNames`

严格解析 SNI 名称列表并把游标重置到首项。

```c
bool xrtTlsServerNames(xbytesview Data, xtlsservernamecursor* pCursor)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `Data` | 输入 | — | 数据 |
| `pCursor` | 输入/输出 | 非空 | 游标 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 成功 | — |
| `false` | 失败 | 见错误 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_PROTOCOL`（TLS 语义） — DER 或消息结构非法

#### 范例

[extension_tour](../../examples/tls/extension_tour/main.c) · listener tour

```c
	if ( !xrtTlsServerNames((xbytesview) { arrSni, 8u },
			&SniCursor) ||
		(xrtTlsServerNamesRead(&SniCursor, &ServerName) !=
			XTLS_ITEM_VALUE) ||
		(ServerName.Type != 0u) ||
		(ServerName.Name.Size != 3u) ||
		(memcmp(ServerName.Name.Data, "api", 3u) != 0) ||
		(xrtTlsServerNamesRead(&SniCursor, &ServerName) !=
			XTLS_ITEM_DONE) ||
		(xrtTlsHostName((xbytesview) { arrSni, 8u },
			&Host) != XTLS_ITEM_VALUE) ||
		(Host.Size != 3u) ) {
```

### `xrtTlsServerNamesRead`

读取下一 SNI 名称；失败时游标与输出保持不变。

```c
xtlsitemresult xrtTlsServerNamesRead(xtlsservernamecursor* pCursor, xtlsservername* pName)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pCursor` | 输入/输出 | 非空 | 游标 |
| `pName` | 输入 | 非空 | 接收名称 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 枚举值 | 当前取值 | — |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_PROTOCOL`（TLS 语义） — DER 或消息结构非法

#### 范例

[extension_tour](../../examples/tls/extension_tour/main.c) · listener tour

```c
		(xrtTlsServerNamesRead(&SniCursor, &ServerName) !=
			XTLS_ITEM_VALUE) ||
```

### `xrtTlsServerPsk`

严格解析 ServerHello 选择的 PSK identity 索引。

```c
bool xrtTlsServerPsk(xbytesview Data, uint16* pSelected)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `Data` | 输入 | — | 数据 |
| `pSelected` | 输入 | 非空 | 接收选中结果 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 成功 | — |
| `false` | 失败 | 见错误 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_PROTOCOL`（TLS 语义） — DER 或消息结构非法

#### 范例

[message_tour](../../examples/tls/message_tour/main.c) · listener tour

```c
		if ( !xrtTlsServerPsk((xbytesview) { arrPsk, 2u },
				&iVersion) ) {
```

### `xrtTlsServerResumed`

返回本次连接是否接受了客户端提供的 TLS 1.3 会话票据。

```c
bool xrtTlsServerResumed(const xtlssession* pSession)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pSession` | 输入/输出 | 非空 | TLS 会话 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 成功 | — |
| `false` | 失败 | 见错误 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_PROTOCOL`（TLS 语义） — DER 或消息结构非法

#### 范例

[handshake_extra](../../examples/tls/handshake_extra/main.c) · listener tour

```c
		if ( (Cookie != 0u) || xrtTlsServerResumed(pServer) ) {
```

### `xrtTlsServerTicket`

用调用方票据签发 TLS 1.3 NewSessionTicket；TLS 1.2 会话返回不支持。

```c
xtlsresult xrtTlsServerTicket(xtlssession* pSession, xbytesview Ticket, uint32 iLifetime, xtlsresume** ppResume)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pSession` | 输入/输出 | 非空 | TLS 会话 |
| `Ticket` | 输入 | — | 会话票据 |
| `iLifetime` | 输入 | — | 生命周期秒数 |
| `ppResume` | 输入 | 非空 | 接收恢复对象 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `XTLS_OK` | 成功 | — |
| `XTLS_AGAIN` | 暂不可推进 | 不设错误 |
| `XTLS_CLOSED` | 已关闭 | 不设错误 |
| `XTLS_ERROR` | 失败 | 见错误 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法

#### 范例

[resume_tour](../../examples/tls/resume_tour/main.c) · listener tour

```c
		if ( (xrtTlsServerTicket(pServer,
				(xbytesview) { arrTicket, 16u }, 600u,
				&pSecond) != XTLS_OK) ||
			(pSecond == NULL) ||
			!xrtTlsResumeValidAt(pSecond, xrtNow()) ) {
```

### `xrtTlsServerTicketNew`

使用默认随机票据和有效期完成一次 TLS 1.3 签发。

```c
xtlsresult xrtTlsServerTicketNew(xtlssession* pSession, xtlsresume** ppResume)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pSession` | 输入/输出 | 非空 | TLS 会话 |
| `ppResume` | 输入 | 非空 | 接收恢复对象 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `XTLS_OK` | 成功 | — |
| `XTLS_AGAIN` | 暂不可推进 | 不设错误 |
| `XTLS_CLOSED` | 已关闭 | 不设错误 |
| `XTLS_ERROR` | 失败 | 见错误 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法

#### 范例

[resume_tour](../../examples/tls/resume_tour/main.c) · listener tour

```c
	if ( (xrtTlsServerTicketNew(pServer, &pCustom) != XTLS_OK) ||
		((g_pServerResume = pCustom) == NULL) ||
		!exampleMove(pServer, pClient) ) {
```

### `xrtTlsServerVersion`

严格解析 ServerHello selected_version 扩展数据。

```c
bool xrtTlsServerVersion(xbytesview Data, uint16* pVersion)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `Data` | 输入 | — | 数据 |
| `pVersion` | 输入 | 非空 | 接收版本 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 成功 | — |
| `false` | 失败 | 见错误 |

#### 错误

- 无 — 纯查询，不设置错误

#### 范例

[message_tour](../../examples/tls/message_tour/main.c) · listener tour

```c
		if ( !xrtTlsServerVersion((xbytesview) { arrTwo, 2u },
				&iVersion) ||
			(iVersion != 0x0304u) ) {
```

## TLS 服务端

`<xrt/tls_server.h>` 提供独立服务端入口，不把服务端身份、SNI 路由或票据缓存塞进客户端配置，也不直接持有 socket。`xtlsserverconfig` 在创建期间借用共享上下文和静态身份；成功后会话持有引用并深复制 ALPN 列表。至少必须提供静态 `Identity` 或同步 `Select`，从而保证未知 SNI 或恢复回退仍有明确认证路径。

```c
static const xstrview Protocols[] = {
	XRT_STR_LITERAL("h2"),
	XRT_STR_LITERAL("http/1.1")
};
xtlsserverconfig Config;
xtlssession* Session;

xrtTlsServerConfigInit(&Config);
Config.Identity = Identity;
Config.Protocols = Protocols;
Config.ProtocolCount = sizeof(Protocols) / sizeof(Protocols[0]);
Config.RequireProtocol = true;
Session = xrtTlsServerCreate(&Config, Pool);
```

会话创建时不生成输出，只进入 `HANDSHAKE` 并等待 ClientHello。传输层把收到的密文交给 `xrtTlsSessionFeed*()`，调用 `xrtTlsServerDrive()`，再从公共 `Send` 队列发送服务端航班；应用数据、关闭、等待原因和所有权接口与客户端共享。TLS 1.3 完整证书航班依次生成 ServerHello、EncryptedExtensions、Certificate、CertificateVerify 和 Finished，验证客户端 Finished 后原子切换到应用 epoch 与 `READY`。TLS 1.2 路径要求 extended master secret，执行 ECDHE 证书完整握手并在 ChangeCipherSpec 边界原子切换记录 epoch；它支持 SNI、ALPN、双向应用数据和认证关闭，不提供会话恢复、重新协商或 KeyUpdate。

服务端首航的扩展表、证书条目、签名输入和临时编码流来自会话内的惰性临时 Arena。
空闲连接和仅创建未握手的会话不分配 Arena 块；首航完成后立即安全清零并释放全部块，
不会为每个连接保留固定握手缓冲。TLS 1.2 与 TLS 1.3 复用同一个 Arena 和清理路径，
错误、OOM 与销毁也执行相同的安全释放；最终需要排队发送的记录仍由公共 `Send` 队列
独立拥有，不借用已经清理的临时内存。

`Select` 在 ClientHello 完成严格解析、SNI/ALPN 提取之后，任何服务端输出生成之前执行。请求中的 SNI 和完整 ALPN 扩展负载只在回调期间借用；`xtlsserverchoice` 初始包含静态身份、按服务端偏好计算的协议下标和零值 `Cookie`，回调可以替换身份或协议，并写入一个 XRT 不解释的 64 位宿主路由标识。返回身份按共享对象处理，选择结果在回调返回后由会话持有。`xrtTlsServerCookie()` 在成功选择后返回该标识，未设置或选择尚未发生时返回零，适合让传输适配层把已完成握手的连接关联回选择身份时的配置代。`XTLS_SERVER_PROTOCOL_NONE` 表示不协商 ALPN；`RequireProtocol` 为 `true` 时没有共同协议会明确失败。回调上下文只借用到首航完成，不能递归驱动同一会话。

`xrtTlsServerName()` 返回服务端从 ClientHello 深复制的 SNI，视图稳定到会话销毁；`xrtTlsSessionProtocol()` 返回最终 ALPN。进入 READY 后，`xrtTlsServerKeyUpdate()` 与收到的 KeyUpdate 都遵循“旧 epoch 完整排队，新 epoch 一次提交”的顺序，发送背压和分配失败不会发布半更新状态。

启用 `XRT_FEATURE_TLS_SERVER_RESUME` 后，`Resume` 回调接收 SNI、完整 ALPN 负载、不透明 ticket 和客户端计算的混淆年龄。回调返回借用的不可变 `xtlsresume`；会话在回调返回后立即增加引用，再校验版本、套件、SNI、ALPN、有效期和年龄容差。未找到票据或路由绑定不匹配会安全回退到完整证书握手；已经匹配同一票据元数据但 binder 错误属于认证失败，必须发送 fatal Alert，不能降级绕过认证。当前恢复始终保留 ECDHE key share，只支持 PSK+DHE，不支持纯 PSK 或 0-RTT。

READY 服务端使用 `xrtTlsServerTicket()` 把调用方给出的非空不透明 ticket 和寿命编码为 NewSessionTicket，并把对应服务端恢复对象的所有权交给调用方；`xrtTlsServerTicketNew()` 使用 32 字节密码安全随机 ticket 和 86400 秒默认寿命。XRT 不维护进程全局票据缓存，调用方可以按租户、容量、过期和持久化需求选择 Map、分片缓存或外部存储。发送硬上限会在随机数、派生和分配之前预检；返回 `XTLS_AGAIN` 时输出对象为 `NULL`，写序号、traffic secret 和恢复状态均不变，排空输出后可原样重试。

服务端门禁覆盖随机小分片 TLS 1.2/1.3 证书握手、SNI/ALPN 动态选择、双向应用数据、认证关闭、TLS 1.3 主动和被动 KeyUpdate、明文与受保护 fatal Alert、票据签发、第二连接 PSK+DHE 恢复、未知或过期票据回退、SNI/ALPN/年龄绑定、坏 binder、畸形 PSK 扩展、发送背压、定向 OOM 和单头文件。完整示例位于 `examples/tls/server/main.c`，测试位于 `tests/tls/test_tls_server*.c` 与 `tests/single/test_single_tls_server.c`。已支持单次 HRR 和 TLS 1.3 应用写入自动换钥。当前没有客户端证书认证、0-RTT、TLS 1.2 会话恢复、重新协商或异步身份选择；TCP 组合入口由后文独立的 `tls_stream` 裁剪单元提供。

### `xrtTlsPeerVerify`

使用显式时间、证书和借用信任库执行默认路径与身份验证。

```c
bool xrtTlsPeerVerify(const xtlspeer* pPeer, const xx509store* pStore, xtlsverifypolicyproc pPolicy, ptr pContext)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pPeer` | 输入 | 非空 | 对端 |
| `pStore` | 输入 | 非空 | 信任库 |
| `pPolicy` | 输入 | — | TLS 策略 |
| `pContext` | 输入/输出 | — | 共享上下文 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 成功 | — |
| `false` | 失败 | 见错误 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_PROTOCOL` — 证书链验证失败（原因保留在验证器结果中）
- `XERR_UNSUPPORTED` — 算法、版本或能力不受支持

#### 范例

[handshake_extra](../../examples/tls/handshake_extra/main.c) · verify

```c
		xrtTlsPeerVerify(pPeer, g_pPeerStore, NULL, NULL) ) {
```

### `xrtTlsVerifierConfigInit`

初始化尚未绑定信任库、回调或自定义时钟的验证器配置。

```c
void xrtTlsVerifierConfigInit(xtlsverifierconfig* pConfig)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pConfig` | 输入 | — | 配置 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 无 | — | — |

#### 错误

- 无 — 初始化不失败

#### 范例

[dial](../../examples/tls/dial/main.c) · verify

```c
	xrtTlsVerifierConfigInit(&VerifierConfig);
```

### `xrtTlsVerifierCreate`

创建可跨线程共享的验证器；成功后接管自定义上下文。

```c
xtlsverifier* xrtTlsVerifierCreate(const xtlsverifierconfig* pConfig)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pConfig` | 输入 | — | 配置 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | 对象或借用 | — |
| `NULL` | 失败或不适用 | 见错误 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_MEMORY` — 分配失败

#### 范例

[dial](../../examples/tls/dial/main.c) · verify

```c
	pVerifier = xrtTlsVerifierCreate(&VerifierConfig);
```

### `xrtTlsVerifierRelease`

释放验证器引用；空指针无操作。

```c
void xrtTlsVerifierRelease(xtlsverifier* pVerifier)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pVerifier` | 输入/输出 | 非空 | 对端验证器 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 无 | — | — |

#### 错误

- 无 — 释放或重置不失败（Reset 系列见参数约束）

#### 范例

[dial](../../examples/tls/dial/main.c) · verify

```c
	xrtTlsVerifierRelease(pVerifier);
```

### `xrtTlsVerifierRetain`

增加不可变验证器引用。

```c
xtlsverifier* xrtTlsVerifierRetain(const xtlsverifier* pVerifier)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pVerifier` | 输入/输出 | 非空 | 对端验证器 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | 对象或借用 | — |
| `NULL` | 失败或不适用 | 见错误 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法

#### 范例

[handshake_extra](../../examples/tls/handshake_extra/main.c) · verify

```c
		xtlsverifier* pRetainedVerifier = xrtTlsVerifierRetain(
			pVerifier);
```

### `xrtTlsVerifierVerify`

执行自定义或默认信任决策；证书和名称只在调用期间借用。

```c
bool xrtTlsVerifierVerify(const xtlsverifier* pVerifier, xtlsrole Role, xstrview Name, const xx509cert* pCertificates, size_t iCertificateCount)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pVerifier` | 输入/输出 | 非空 | 对端验证器 |
| `Role` | 输入 | — | 角色 |
| `Name` | 输入 | — | 名称 |
| `pCertificates` | 输入 | 非空 | 证书视图数组 |
| `iCertificateCount` | 输入 | — | 证书数量 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 成功 | — |
| `false` | 失败 | 见错误 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_PROTOCOL` — 证书链验证失败

#### 范例

[handshake_extra](../../examples/tls/handshake_extra/main.c) · verify

```c
			!xrtTlsVerifierVerify(pVerifier, XTLS_SERVER,
				XRT_STR_LITERAL("localhost"), &Leaf, 1u) ) {
```

## TLS 对端验证

`<xrt/tls_verify.h>` 把信任决策从客户端状态机和 X.509 原语之间独立出来。`xrtTlsVerifierCreate()` 深复制可选 `xx509store`，因此创建后可以释放或修改来源 store；验证器本体不可变、引用计数共享，并要求自定义回调可以并发执行。没有回调时必须提供至少一个 trust anchor，空 store 会在创建期失败。

`xrtTlsVerifierRetain()` 增加共享引用，`xrtTlsVerifierRelease()` 释放引用并在最后一次释放时销毁快照。`xrtTlsVerifierVerify()` 使用配置的时间源、信任决策和附加策略完成一次对端验证；同一个验证器可以被多个 TLS 会话并发调用。

默认 `xrtTlsPeerVerify()` 使用调用方显式给出的时间验证路径，并按角色检查 `digitalSignature` KeyUsage 和 `serverAuth` / `clientAuth` EKU；服务端角色还按 RFC 9525 匹配请求名称。线路链按叶到根排列，trust anchor 来自 store 且不要求出现在对端链中。调用方可以传入 `xtlsverifypolicyproc`，在这些默认检查全部成功后读取 `xtlsverifiedpeer`：`Path` 按叶到根排列但不包含独立 `Anchor`，全部视图只在回调期间借用。策略可以直接组合 CRL、OCSP、CT、证书固定或企业规则，不必重复建链和身份验证。

TLS 核心不会隐式加载系统根、下载 CRL、发起 OCSP 请求或提交 CT 查询；数据加载和联网属于独立可裁剪组合层，基础客户端不会在验证期间偷偷阻塞文件或系统 API。同步策略返回 `false` 时可以设置结构化错误，验证器会把它包装为 `XTLS_ERROR_VERIFY` 的 cause；没有设置错误时得到明确的 `XERR_PERMISSION` 拒绝。

```c
xtlsverifierconfig VerifyConfig;
xtlsclientconfig ClientConfig;
xtlsverifier* Verifier;

xrtTlsVerifierConfigInit(&VerifyConfig);
VerifyConfig.Store = TrustStore;
Verifier = xrtTlsVerifierCreate(&VerifyConfig);

xrtTlsClientConfigInit(&ClientConfig);
ClientConfig.ServerName = XRT_STR_LITERAL("example.com");
ClientConfig.Verifier = Verifier;
Session = xrtTlsClientCreate(&ClientConfig, Pool);
xrtTlsVerifierRelease(Verifier);
```

`xtlsverifydecision` 是自定义 `xtlsverifyproc` 的完整结果域：`XTLS_VERIFY_ACCEPT` 接管信任，`XTLS_VERIFY_REJECT` 明确拒绝，`XTLS_VERIFY_DEFAULT` 回退到不可变 store，`XTLS_VERIFY_ERROR` 报告本次回调产生的结构化错误。回退时没有 store 会明确失败；错误结果必须在本次回调内设置错误，验证器隔离回调前后的错误槽并把本次错误保留为 cause。接受结果不会再调用只适用于默认路径的 `Policy`，但仍不能跳过 CertificateVerify 和 Finished。

`xtlsverifytimeproc` 为路径和附加策略提供同一个确定时间；省略时使用 `xrtNow()`。`Verify`、`Policy`、`Time` 可以并发调用，不能依赖线程局部的可变共享状态。配置的 `Context` 只在 `xrtTlsVerifierCreate()` 成功后转移所有权；最后一个引用释放时，`xtlsverifyreleaseproc` 恰好调用一次。

`xrtTls13CertificateVerifySignature()` 是公开的协议验签原语。它构造标准的 64 个空格、角色上下文、零分隔符和 transcript hash，严格区分 `rsae` / `pss` 密钥、P-256 / P-384 方案和 Ed25519，并拒绝 TLS 1.3 禁止的 PKCS#1 方案。它不执行证书路径验证，调用方若直接使用原语必须先完成信任决策。

完整示例位于 `examples/tls/verify/main.c`；基础契约、默认真实 RSA 路径、CRL 路径策略、OOM、客户端端到端航班、TinyCC x86 和单头门禁分别位于 `tests/tls/test_tls_verify*.c`、`tests/tls/test_tls_client_server_hello.c` 与 `tests/single/test_single_tls_verify.c`。`tests/tls/test_tls_verify_policy_rsa.c` 同时展示无 CRL、空 CRL、吊销和过期 CRL 的完整组合方式。

### `xrtTlsIdentityCanSign`

判断身份后端是否支持指定 TLS 版本和签名方案。

```c
bool xrtTlsIdentityCanSign(const xtlsidentity* pIdentity, xtlsversion Version, xtlssignature Signature)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pIdentity` | 输入/输出 | 非空 | 身份对象 |
| `Version` | 输入 | — | TLS 版本 |
| `Signature` | 输入 | — | 签名值 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 成功 | — |
| `false` | 失败 | 见错误 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_PROTOCOL`（TLS 语义） — DER 或消息结构非法

#### 范例

[handshake_extra](../../examples/tls/handshake_extra/main.c) · identity

```c
		!xrtTlsIdentityCanSign(pIdentity, XTLS_VERSION_13,
			XTLS_SIGNATURE_ECDSA_SECP256R1_SHA256) ||
```

### `xrtTlsIdentityCertificate`

借用指定位置的完整 DER 证书；视图随身份最后一个引用失效。

```c
bool xrtTlsIdentityCertificate(const xtlsidentity* pIdentity, size_t iIndex, xbytesview* pCertificate)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pIdentity` | 输入/输出 | 非空 | 身份对象 |
| `iIndex` | 输入 | — | 索引 |
| `pCertificate` | 输入 | 非空 | 证书视图 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 成功 | — |
| `false` | 失败 | 见错误 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_PROTOCOL`（TLS 语义） — DER 或消息结构非法

#### 范例

[handshake_extra](../../examples/tls/handshake_extra/main.c) · identity

```c
		!xrtTlsIdentityCertificate(pIdentity, 0u, &Stored) ||
```

### `xrtTlsIdentityCertificateCount`

返回按 TLS 发送顺序保存的证书数量，第一张证书是叶证书。

```c
size_t xrtTlsIdentityCertificateCount(const xtlsidentity* pIdentity)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pIdentity` | 输入/输出 | 非空 | 身份对象 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `>= 0` | 数量或字节数 | — |

#### 错误

- 无 — 纯查询，不设置错误

#### 范例

[handshake_extra](../../examples/tls/handshake_extra/main.c) · identity

```c
		(xrtTlsIdentityCertificateCount(pIdentity) != 2u) ||
```

### `xrtTlsIdentityCreate`

创建深复制证书链、接管外部签名上下文的自定义共享身份。

```c
xtlsidentity* xrtTlsIdentityCreate(const xtlsidentityconfig* pConfig)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pConfig` | 输入 | — | 配置 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | 对象或借用 | — |
| `NULL` | 失败或不适用 | 见错误 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_MEMORY` — 分配失败

#### 范例

[handshake_extra](../../examples/tls/handshake_extra/main.c) · identity

```c
	pIdentity = xrtTlsIdentityCreate(&IdentityConfig);
```

### `xrtTlsIdentityEd25519`

从 32 字节种子、DER OCTET 或未加密 PKCS#8 私钥创建 Ed25519 身份。

```c
xtlsidentity* xrtTlsIdentityEd25519(const xbytesview* pCertificates, size_t iCertificateCount, xbytesview PrivateKey)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pCertificates` | 输入 | 非空 | 证书视图数组 |
| `iCertificateCount` | 输入 | — | 证书数量 |
| `PrivateKey` | 输入 | — | 私钥 DER |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | 对象或借用 | — |
| `NULL` | 失败或不适用 | 见错误 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_MEMORY` — 分配失败

#### 范例

[identity](../../examples/tls/identity/main.c) · identity

```c
			pIdentity = xrtTlsIdentityEd25519(arrCertificates, 1u, PrivateKey);
```

### `xrtTlsIdentityP256`

从原始标量、SEC1 或未加密 PKCS#8 私钥创建 P-256 身份。

```c
xtlsidentity* xrtTlsIdentityP256(const xbytesview* pCertificates, size_t iCertificateCount, xbytesview PrivateKey)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pCertificates` | 输入 | 非空 | 证书视图数组 |
| `iCertificateCount` | 输入 | — | 证书数量 |
| `PrivateKey` | 输入 | — | 私钥 DER |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | 对象或借用 | — |
| `NULL` | 失败或不适用 | 见错误 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_MEMORY` — 分配失败

#### 范例

[identity](../../examples/tls/identity/main.c) · identity

```c
			pIdentity = xrtTlsIdentityP256(arrCertificates, 1u, PrivateKey);
```

### `xrtTlsIdentityP384`

从原始标量、SEC1 或未加密 PKCS#8 私钥创建 P-384 身份。

```c
xtlsidentity* xrtTlsIdentityP384(const xbytesview* pCertificates, size_t iCertificateCount, xbytesview PrivateKey)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pCertificates` | 输入 | 非空 | 证书视图数组 |
| `iCertificateCount` | 输入 | — | 证书数量 |
| `PrivateKey` | 输入 | — | 私钥 DER |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | 对象或借用 | — |
| `NULL` | 失败或不适用 | 见错误 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_MEMORY` — 分配失败

#### 范例

[identity](../../examples/tls/identity/main.c) · identity

```c
			pIdentity = xrtTlsIdentityP384(arrCertificates, 1u, PrivateKey);
```

### `xrtTlsIdentityPublicKey`

借用已经严格解析并与身份类型匹配的叶证书公钥。

```c
bool xrtTlsIdentityPublicKey(const xtlsidentity* pIdentity, xx509pubkey* pPublicKey)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pIdentity` | 输入/输出 | 非空 | 身份对象 |
| `pPublicKey` | 输入 | 非空 | 公钥描述 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 成功 | — |
| `false` | 失败 | 见错误 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_PROTOCOL`（TLS 语义） — DER 或消息结构非法

#### 范例

[handshake_extra](../../examples/tls/handshake_extra/main.c) · identity

```c
		!xrtTlsIdentityPublicKey(pIdentity, &LeafKey) ||
```

### `xrtTlsIdentityRelease`

释放身份，并在最后一个引用结束时释放签名器和清除内部存储。

```c
void xrtTlsIdentityRelease(xtlsidentity* pIdentity)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pIdentity` | 输入/输出 | 非空 | 身份对象 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 无 | — | — |

#### 错误

- 无 — 释放或重置不失败（Reset 系列见参数约束）

#### 范例

[handshake_extra](../../examples/tls/handshake_extra/main.c) · identity

```c
	xrtTlsIdentityRelease(pRetained);
```

### `xrtTlsIdentityRetain`

增加共享身份引用；身份本体和证书视图均保持只读。

```c
xtlsidentity* xrtTlsIdentityRetain(const xtlsidentity* pIdentity)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pIdentity` | 输入/输出 | 非空 | 身份对象 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | 对象或借用 | — |
| `NULL` | 失败或不适用 | 见错误 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法

#### 范例

[handshake_extra](../../examples/tls/handshake_extra/main.c) · identity

```c
		((pRetained = xrtTlsIdentityRetain(pIdentity)) == NULL) ||
```

### `xrtTlsIdentityRsa`

从 PKCS#1 或未加密 PKCS#8 DER 私钥创建 RSA 身份。构造过程核对叶证书公钥、完整指数和 CRT 五参数，不借用私钥输入。

```c
xtlsidentity* xrtTlsIdentityRsa(const xbytesview* pCertificates, size_t iCertificateCount, xbytesview PrivateKey)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pCertificates` | 输入 | 非空 | 证书视图数组 |
| `iCertificateCount` | 输入 | — | 证书数量 |
| `PrivateKey` | 输入 | — | 私钥 DER |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | 对象或借用 | — |
| `NULL` | 失败或不适用 | 见错误 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_MEMORY` — 分配失败

#### 范例

[identity](../../examples/tls/identity/main.c) · identity

```c
			pIdentity = xrtTlsIdentityRsa(arrCertificates, 1u, PrivateKey);
```

### `xrtTlsIdentitySign`

签署完整 TLS 待签内容；空输出查询精确长度，容量不足时不调用签名器。外部签名器失败会保留为 TLS 身份错误的原因链。

```c
bool xrtTlsIdentitySign(const xtlsidentity* pIdentity, xtlsversion Version, xtlssignature Signature, xbytesview Message, void* pOutput, size_t iCapacity, size_t* pSize)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pIdentity` | 输入/输出 | 非空 | 身份对象 |
| `Version` | 输入 | — | TLS 版本 |
| `Signature` | 输入 | — | 签名值 |
| `Message` | 输入 | — | 消息 |
| `pOutput` | 输入 | 非空 | 输出缓冲 |
| `iCapacity` | 输入 | — | 容量 |
| `pSize` | 输入 | 非空 | 接收写出字节数 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 成功 | — |
| `false` | 失败 | 见错误 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_UNSUPPORTED` — 算法、版本或能力不受支持
- `XERR_PROTOCOL` — 签名计算失败

#### 范例

[identity](../../examples/tls/identity/main.c) · identity

```c
	if ( (Signature == 0) || !xrtTlsIdentitySign(
		pIdentity, XTLS_VERSION_13, Signature,
		(xbytesview) { Message, sizeof(Message) - 1u },
		NULL, 0, &iSignatureSize
	) ) {
```

### `xrtTlsIdentityType`

返回叶证书对应的握手签名密钥类型。

```c
xtlsidentitytype xrtTlsIdentityType(const xtlsidentity* pIdentity)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pIdentity` | 输入/输出 | 非空 | 身份对象 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 枚举值 | 当前取值 | — |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法

#### 范例

[identity](../../examples/tls/identity/main.c) · identity

```c
	switch ( xrtTlsIdentityType(pIdentity) ) {
```

## TLS 身份

`<xrt/tls_identity.h>` 提供与网络和角色状态机解耦的共享身份对象。身份创建时深复制完整 DER 证书链，第一张证书必须是叶证书；`xrtTlsIdentityCertificate()`、`xrtTlsIdentityPublicKey()` 返回的借用视图一直有效到最后一个身份引用释放。身份创建后不可变，可由多个服务端配置和 Worker 并发共享。

| 构造器 | 私钥输入 | 验证 |
| --- | --- | --- |
| `xrtTlsIdentityRsa()` | PKCS#1 或未加密 PKCS#8 DER | 模数、指数、全部 CRT 参数、叶 SPKI 和 RSA-PSS 限制 |
| `xrtTlsIdentityP256()` | 32 字节标量、SEC1 或未加密 PKCS#8 DER | 标量范围、曲线 OID、SEC1 可选公钥和叶 P-256 SPKI |
| `xrtTlsIdentityP384()` | 48 字节标量、SEC1 或未加密 PKCS#8 DER | 标量范围、曲线 OID、SEC1 可选公钥和叶 P-384 SPKI |
| `xrtTlsIdentityEd25519()` | 32 字节种子、单/双层 DER OCTET 或 RFC 8410 PKCS#8 | 算法参数缺省、派生公钥和叶 Ed25519 SPKI |

构造器不借用私钥输入。内置身份与证书链保存在一块紧凑分配中，释放前安全清零；RSA 保留完整 CRT 视图，Ed25519 保留一次展开的签名密钥，避免每次握手重复派生。PEM、文件和系统证书库不属于身份核心依赖，调用方可先用对应层取得 DER，后续再由独立便捷加载模块组合。

`xrtTlsIdentityCanSign()` 同时检查 TLS 版本、证书身份类型、标准签名方案和后端限制。TLS 1.3 不会接受 RSA-PKCS#1 CertificateVerify，ECDSA 方案必须与 P-256/P-384 证书曲线一致；TLS 1.2 的 ECDSA 线路值仍按摘要/签名对解释，因此两条已实现曲线都可按对端选择使用 SHA-256、SHA-384 或 SHA-512。`rsaEncryption` 与 `RSASSA-PSS` 证书分别只进入 `rsa_pss_rsae_*` 和 `rsa_pss_pss_*` 路径。受限 RSA-PSS 密钥还必须满足证书与私钥两侧的摘要、MGF1、最小盐长和 trailer 约束；没有任何共同 TLS 方案的身份在构造时直接拒绝。

`xrtTlsIdentitySign()` 接收完整 TLS 待签内容。空输出查询精确签名长度，容量不足时不会调用签名器；内置后端通过临时结果或底层原子发布契约保证失败不发布部分签名。RSA-PSS 使用密码安全随机盐，ECDSA 按线路摘要使用 RFC 6979 确定性 low-S 签名，Ed25519 使用纯 Ed25519 模式。

```c
size_t iSignatureSize = 0;

if ( !xrtTlsIdentitySign(
	Identity, XTLS_VERSION_13,
	XTLS_SIGNATURE_ED25519, Content,
	NULL, 0, &iSignatureSize
) ) {
	return false;
}
```

`xrtTlsIdentityCreate()` 是真实扩展接口，用于 HSM、系统密钥库或远程签名器。配置中的证书链仍由 XRT 深复制；签名上下文只在创建成功后转移所有权，最后释放时调用 `Release`。`Supports` 与 `Sign` 必须允许并发调用；`Supports` 是不设置错误的能力谓词，`Sign` 失败时必须保持输出和长度不变。没有硬件或系统密钥需求时应使用内置强类型构造器。

身份层错误使用 `xrt.tls` 的 identity 错误码，并把 DER、X.509 或 crypto 失败保留为 `Cause`。因此上层宿主可以按 TLS 阶段映射，C 调用方也能继续读取底层格式或密码原因。

裁剪宏相互独立：

- `XRT_FEATURE_TLS_IDENTITY`：共享对象、证书快照、引用和外部签名器。
- `XRT_FEATURE_TLS_IDENTITY_RSA`：RSA PKCS#1/PKCS#8、PKCS#1 与 PSS 签名。
- `XRT_FEATURE_TLS_IDENTITY_EC`：共享 SEC1/PKCS#8 EC 格式解析，不单独发布构造器。
- `XRT_FEATURE_TLS_IDENTITY_P256`、`XRT_FEATURE_TLS_IDENTITY_P384`：各自曲线身份。
- `XRT_FEATURE_TLS_IDENTITY_ED25519`：Ed25519 身份。

完整示例位于 `examples/tls/identity/main.c`；模块化、负向、OOM、组合和单头门禁位于 `tests/tls/test_tls_identity*.c` 与 `tests/single/test_single_tls_identity*.c`。

### `xrtTlsDial`

解析主机、竞争 TCP 地址并完成 TLS 握手；成功 Stream 引用转移给完成回调。

```c
xtlsdial* xrtTlsDial(xnetengine* pEngine, xnetresolver* pResolver, cstr sHost, uint16 iPort, const xtlsclientconfig* pTls, const xtlsdialconfig* pConfig, const xtlsstreamevents* pStreamEvents, ptr pStreamData, xtlsdialproc pDone, ptr pDoneData)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pEngine` | 输入 | 非空 | 网络 Engine |
| `pResolver` | 输入 | 非空 | 名称解析器 |
| `sHost` | 输入 | — | 主机名 |
| `iPort` | 输入 | — | 端口 |
| `pTls` | 输入 | 非空 | TLS 对象 |
| `pConfig` | 输入 | — | 配置 |
| `pStreamEvents` | 输入 | 非空 | 流事件表 |
| `pStreamData` | 输入 | — | 流用户数据 |
| `pDone` | 输入 | — | 完成回调 |
| `pDoneData` | 输入 | — | 回调数据 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | 对象或借用 | — |
| `NULL` | 失败或不适用 | 见错误 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_UNSUPPORTED` — 算法、版本或能力不受支持
- `XERR_MEMORY` — 分配失败

#### 范例

[dial](../../examples/tls/dial/main.c) · stream tour

```c
	pDial = xrtTlsDial(
		pEngine,
		pResolver,
		sHost,
		(uint16)iPort,
		&TlsConfig,
		&DialConfig,
		&Events,
		&Example,
		exampleTlsDialDone,
		&Example
	);
```

### `xrtTlsDialAsync`

以 Future 接收完成握手的 TLS Stream；Open 先于成功终态发布。Future 持有一个 Stream 引用，取消请求协作终止 DNS、TCP 或 TLS 当前阶段。

```c
xfuture* xrtTlsDialAsync(xnetengine* pEngine, xnetresolver* pResolver, cstr sHost, uint16 iPort, const xtlsclientconfig* pTls, const xtlsdialconfig* pConfig, const xtlsstreamevents* pStreamEvents, ptr pStreamData)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pEngine` | 输入 | 非空 | 网络 Engine |
| `pResolver` | 输入 | 非空 | 名称解析器 |
| `sHost` | 输入 | — | 主机名 |
| `iPort` | 输入 | — | 端口 |
| `pTls` | 输入 | 非空 | TLS 对象 |
| `pConfig` | 输入 | — | 配置 |
| `pStreamEvents` | 输入 | 非空 | 流事件表 |
| `pStreamData` | 输入 | — | 流用户数据 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | 对象或借用 | — |
| `NULL` | 失败或不适用 | 见错误 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_MEMORY` — 分配失败

#### 范例

[dial_future](../../examples/tls/dial_future/main.c) · stream tour

```c
	pFuture = xrtTlsDialAsync(
		pEngine,
		pResolver,
		sHost,
		(uint16)iPort,
		&TlsConfig,
		&DialConfig,
		NULL,
		NULL
	);
```

### `xrtTlsDialCancel`

原子受理取消；返回真保证最终结果不会再变为成功。

```c
bool xrtTlsDialCancel(xtlsdial* pDial)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pDial` | 输入 | 非空 | 托管拨号对象 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 成功 | — |
| `false` | 失败 | 见错误 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_PROTOCOL`（TLS 语义） — DER 或消息结构非法

#### 范例

[listener_tour](../../examples/tls/listener_tour/main.c) · stream tour

```c
			bool bCancelled = xrtTlsDialCancel(pMidair);
```

### `xrtTlsDialConfigInit`

初始化 TCP 拨号、TLS Stream 和总超时策略。

```c
void xrtTlsDialConfigInit(xtlsdialconfig* pConfig)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pConfig` | 输入 | — | 配置 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 无 | — | — |

#### 错误

- 无 — 初始化不失败

#### 范例

[dial](../../examples/tls/dial/main.c) · stream tour

```c
	xrtTlsDialConfigInit(&DialConfig);
```

### `xrtTlsDialDestroy`

释放 TLS Dial 引用；空指针视为空操作。

```c
void xrtTlsDialDestroy(xtlsdial* pDial)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pDial` | 输入 | 非空 | 托管拨号对象 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 无 | — | — |

#### 错误

- 无 — 释放或重置不失败（Reset 系列见参数约束）

#### 范例

[dial](../../examples/tls/dial/main.c) · stream tour

```c
	xrtTlsDialDestroy(pDial);
```

### `xrtTlsDialError`

失败或取消后借用完整错误原因链。

```c
const xerror* xrtTlsDialError(const xtlsdial* pDial)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pDial` | 输入 | 非空 | 托管拨号对象 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | 对象或借用 | — |
| `NULL` | 失败或不适用 | 见错误 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_MEMORY` — 分配失败

#### 范例

[listener_tour](../../examples/tls/listener_tour/main.c) · stream tour

```c
		xrtTlsDialError(pDial) == NULL ? "(none)" : "err");
```

### `xrtTlsDialRef`

增加 TLS Dial 引用并返回原指针。

```c
xtlsdial* xrtTlsDialRef(xtlsdial* pDial)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pDial` | 输入 | 非空 | 托管拨号对象 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | 对象或借用 | — |
| `NULL` | 失败或不适用 | 见错误 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法

#### 范例

[listener_tour](../../examples/tls/listener_tour/main.c) · stream tour

```c
		(xrtTlsDialRef(pDial) != pDial) ) {
```

### `xrtTlsDialState`

返回当前拨号阶段或不可变终态。

```c
xtlsdialstate xrtTlsDialState(const xtlsdial* pDial)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pDial` | 输入 | 非空 | 托管拨号对象 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 枚举值 | 当前取值 | — |

#### 错误

- 无 — 纯查询，不设置错误

#### 范例

[listener_tour](../../examples/tls/listener_tour/main.c) · stream tour

```c
	pSlot->DialState = xrtTlsDialState(pDial);
```

### `xrtTlsDialTransportStats`

取得底层 TCP Dial 统计；TLS 握手阶段仍保留获胜地址信息。

```c
bool xrtTlsDialTransportStats(const xtlsdial* pDial, xnetdialstats* pStats)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pDial` | 输入 | 非空 | 托管拨号对象 |
| `pStats` | 输入 | 非空 | 接收统计快照 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 成功 | — |
| `false` | 失败 | 见错误 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_PROTOCOL`（TLS 语义） — DER 或消息结构非法

#### 范例

[listener_tour](../../examples/tls/listener_tour/main.c) · stream tour

```c
	if ( !xrtTlsDialTransportStats(pDial, &TransportStats) ||
		(TransportStats.AttemptsStarted < 1u) ) {
```

### `xrtTlsListenerAccept`

pull 模式下非阻塞取得一个已完成握手的 Stream；空队列返回空指针。

```c
xtlsstream* xrtTlsListenerAccept(xtlslistener* pListener)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pListener` | 输入 | 非空 | TLS 监听器 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | 对象或借用 | — |
| `NULL` | 失败或不适用 | 见错误 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_MEMORY` — 分配失败

#### 范例

[listener_tour](../../examples/tls/listener_tour/main.c) · stream tour

```c
		pServerA = xrtTlsListenerAccept(pListener);
```

### `xrtTlsListenerAcceptAsync`

pull 模式下异步接受一个已完成握手的 Stream；Future 持有结果引用。

```c
xfuture* xrtTlsListenerAcceptAsync(xtlslistener* pListener)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pListener` | 输入 | 非空 | TLS 监听器 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | 对象或借用 | — |
| `NULL` | 失败或不适用 | 见错误 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_MEMORY` — 分配失败

#### 范例

[listener_tour](../../examples/tls/listener_tour/main.c) · stream tour

```c
	pAcceptFuture = xrtTlsListenerAcceptAsync(pListener);
```

### `xrtTlsListenerAcceptWait`

阻塞接受一个已完成握手的 Stream；禁止从该 Engine 的 Worker 调用。

```c
xtlsstream* xrtTlsListenerAcceptWait(xtlslistener* pListener, xdeadline iDeadline, xcancel* pCancel)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pListener` | 输入 | 非空 | TLS 监听器 |
| `iDeadline` | 输入 | — | 单调截止时间 |
| `pCancel` | 输入 | — | 取消令牌 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | 对象或借用 | — |
| `NULL` | 失败或不适用 | 见错误 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_MEMORY` — 分配失败

#### 范例

[listener_tour](../../examples/tls/listener_tour/main.c) · stream tour

```c
	pServerC = xrtTlsListenerAcceptWait(pListener,
		xrtDeadlineAfter(EXAMPLE_DEADLINE_US), NULL);
```

### `xrtTlsListenerClose`

原子停止接入并丢弃尚未交付的连接；已交付连接保持独立生命周期。

```c
bool xrtTlsListenerClose(xtlslistener* pListener)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pListener` | 输入 | 非空 | TLS 监听器 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 成功 | — |
| `false` | 失败 | 见错误 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_PROTOCOL`（TLS 语义） — DER 或消息结构非法

#### 范例

[listener_tour](../../examples/tls/listener_tour/main.c) · stream tour

```c
		(void)xrtTlsListenerClose(pListener);
```

### `xrtTlsListenerConfigInit`

初始化单 IPv4 动态端口、有界握手与有界完成队列。

```c
void xrtTlsListenerConfigInit(xtlslistenerconfig* pConfig)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pConfig` | 输入 | — | 配置 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 无 | — | — |

#### 错误

- 无 — 初始化不失败

#### 范例

[listener_tour](../../examples/tls/listener_tour/main.c) · stream tour

```c
	xrtTlsListenerConfigInit(&ListenerConfig);
```

### `xrtTlsListenerData`

返回创建时保存的用户数据快照。

```c
ptr xrtTlsListenerData(const xtlslistener* pListener)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pListener` | 输入 | 非空 | TLS 监听器 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 值 | 当前取值 | — |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法

#### 范例

[listener_tour](../../examples/tls/listener_tour/main.c) · stream tour

```c
		(xrtTlsListenerData(pListener) != &EngineConfig) ) {
```

### `xrtTlsListenerDestroy`

释放 Listener 引用；不会隐式关闭仍在监听的对象。

```c
void xrtTlsListenerDestroy(xtlslistener* pListener)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pListener` | 输入 | 非空 | TLS 监听器 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 无 | — | — |

#### 错误

- 无 — 释放或重置不失败（Reset 系列见参数约束）

#### 范例

[listener_tour](../../examples/tls/listener_tour/main.c) · stream tour

```c
	xrtTlsListenerDestroy(pListenerRef);
```

### `xrtTlsListenerLocal`

复制监听 Socket 的实际本地地址，支持动态端口。

```c
bool xrtTlsListenerLocal(xtlslistener* pListener, xnetaddr* pAddress)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pListener` | 输入 | 非空 | TLS 监听器 |
| `pAddress` | 输入 | 非空 | 接收地址 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 成功 | — |
| `false` | 失败 | 见错误 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_PROTOCOL`（TLS 语义） — DER 或消息结构非法

#### 范例

[listener_tour](../../examples/tls/listener_tour/main.c) · stream tour

```c
		!xrtTlsListenerLocal(pListener, &Address) ||
```

### `xrtTlsListenerRef`

增加 Listener 引用并返回原指针。

```c
xtlslistener* xrtTlsListenerRef(xtlslistener* pListener)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pListener` | 输入 | 非空 | TLS 监听器 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | 对象或借用 | — |
| `NULL` | 失败或不适用 | 见错误 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法

#### 范例

[listener_tour](../../examples/tls/listener_tour/main.c) · stream tour

```c
	pListenerRef = xrtTlsListenerRef(pListener);
```

### `xrtTlsListenerStart`

同步完成 TCP 绑定并开始异步接入；配置数组只在调用期间借用。Listener 会保留 Context、Identity，并深复制 ALPN 协议列表。SelectContext 与 ResumeContext 由调用方持有，必须存活到 Listener 关闭回调结束。

```c
xtlslistener* xrtTlsListenerStart(xnetengine* pEngine, const xtlslistenerconfig* pConfig, const xtlslistenerevents* pEvents, const xtlsstreamevents* pStreamEvents, ptr pData)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pEngine` | 输入 | 非空 | 网络 Engine |
| `pConfig` | 输入 | — | 配置 |
| `pEvents` | 输入 | — | 事件表 |
| `pStreamEvents` | 输入 | 非空 | 流事件表 |
| `pData` | 输入 | — | 用户数据 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | 对象或借用 | — |
| `NULL` | 失败或不适用 | 见错误 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_MEMORY` — 分配失败

#### 范例

[listener_tour](../../examples/tls/listener_tour/main.c) · stream tour

```c
	pListener = xrtTlsListenerStart(pEngine, &ListenerConfig, NULL,
		NULL, &EngineConfig);
```

### `xrtTlsListenerState`

返回 Listener 当前生命周期状态。

```c
xtlslistenerstate xrtTlsListenerState(const xtlslistener* pListener)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pListener` | 输入 | 非空 | TLS 监听器 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 枚举值 | 当前取值 | — |

#### 错误

- 无 — 纯查询，不设置错误

#### 范例

[listener_tour](../../examples/tls/listener_tour/main.c) · stream tour

```c
		(xrtTlsListenerState(pListener) != XTLS_LISTENER_OPEN) ||
```

### `xrtTlsListenerStats`

复制 Listener 的并发统计快照。

```c
bool xrtTlsListenerStats(const xtlslistener* pListener, xtlslistenerstats* pStats)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pListener` | 输入 | 非空 | TLS 监听器 |
| `pStats` | 输入 | 非空 | 接收统计快照 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 成功 | — |
| `false` | 失败 | 见错误 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_PROTOCOL`（TLS 语义） — DER 或消息结构非法

#### 范例

[listener_tour](../../examples/tls/listener_tour/main.c) · stream tour

```c
	if ( !xrtTlsListenerStats(pListener, &ListenerStats) ||
		(ListenerStats.Accepted < 3u) ||
		(ListenerStats.Handshakes < 3u) ) {
```

### `xrtTlsStreamAbort`

从任意线程立即放弃 TLS 与 TCP 会话。失败收尾尚未完成时仍会中止 TCP，但不会覆盖已经保存的首个根因。

```c
bool xrtTlsStreamAbort(xtlsstream* pStream)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pStream` | 输入/输出 | 非空 | TLS 组合流 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 成功 | — |
| `false` | 失败 | 见错误 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_PROTOCOL`（TLS 语义） — DER 或消息结构非法

#### 范例

[dial](../../examples/tls/dial/main.c) · stream tour

```c
			(void)xrtTlsStreamAbort(pStream);
```

### `xrtTlsStreamAccept`

在 TCP Accept 回调内接管 Stream；返回值应直接作为该回调结果。

```c
bool xrtTlsStreamAccept(xnetstream* pTransport, const xtlsserverconfig* pTls, const xtlsstreamconfig* pConfig, const xtlsstreamevents* pEvents, ptr pData, xtlsstream** ppStream)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pTransport` | 输入 | 非空 | 传输 TCP 流 |
| `pTls` | 输入 | 非空 | TLS 对象 |
| `pConfig` | 输入 | — | 配置 |
| `pEvents` | 输入 | — | 事件表 |
| `pData` | 输入 | — | 用户数据 |
| `ppStream` | 输入 | 非空 | 接收流对象 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 成功 | — |
| `false` | 失败 | 见错误 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_STATE` — 状态非法（顺序、已关闭、非所属线程）
- `XERR_PROTOCOL` — 接受的字节不是有效 TLS 记录
- `XERR_MEMORY` — 分配失败

#### 范例

[stream](../../examples/tls/stream/main.c) · stream tour

```c
	bAccepted = xrtTlsStreamAccept(
		pTransport,
		&pExample->ServerConfig,
		&pExample->StreamConfig,
		&pExample->StreamEvents,
		pExample,
		&pStream
	);
```

### `xrtTlsStreamAsyncBytes`

返回尚未由所属 Worker 终结的异步发送负载字节数。

```c
size_t xrtTlsStreamAsyncBytes(const xtlsstream* pStream)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pStream` | 输入/输出 | 非空 | TLS 组合流 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `>= 0` | 数量或字节数 | — |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法

#### 范例

[stream_tour](../../examples/tls/stream_tour/main.c) · stream tour

```c
	(void)xrtTlsStreamAsyncBytes(pStream);
```

### `xrtTlsStreamAsyncCount`

返回异步发送、接收和条件等待的合计操作数。

```c
uint32 xrtTlsStreamAsyncCount(const xtlsstream* pStream)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pStream` | 输入/输出 | 非空 | TLS 组合流 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 数值 | 当前取值 | — |

#### 错误

- 无 — 纯查询，不设置错误

#### 范例

[stream_tour](../../examples/tls/stream_tour/main.c) · stream tour

```c
	(void)xrtTlsStreamAsyncCount(pStream);
```

### `xrtTlsStreamAttach`

在已公开的 TCP Stream 所属 Worker 上接管 Transport 和 Session。Transport 必须仍可双向收发，调用方必须停止直接操作其 IO。成功时接管两者的调用方引用；失败时所有权、Session 分配归属和 Transport 事件均保持不变，输出清空。

```c
bool xrtTlsStreamAttach(xnetstream* pTransport, xtlssession* pSession, const xtlsstreamconfig* pConfig, const xtlsstreamevents* pEvents, ptr pData, xtlsstream** ppStream)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pTransport` | 输入 | 非空 | 传输 TCP 流 |
| `pSession` | 输入/输出 | 非空 | TLS 会话 |
| `pConfig` | 输入 | — | 配置 |
| `pEvents` | 输入 | — | 事件表 |
| `pData` | 输入 | — | 用户数据 |
| `ppStream` | 输入 | 非空 | 接收流对象 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 成功 | — |
| `false` | 失败 | 见错误 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_PROTOCOL`（TLS 语义） — DER 或消息结构非法

#### 范例

[stream_tour](../../examples/tls/stream_tour/main.c) · stream tour

```c
		pTask->bOk = xrtTlsStreamAttach(pTask->pTcp,
			pTask->pSession, pTask->pStream, pTask->pEvents,
			pTask->pData, &pTask->pTls);
```

### `xrtTlsStreamAvailable`

返回当前待应用消费明文字节数的并发快照。

```c
size_t xrtTlsStreamAvailable(const xtlsstream* pStream)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pStream` | 输入/输出 | 非空 | TLS 组合流 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `>= 0` | 数量或字节数 | — |

#### 错误

- 无 — 纯查询，不设置错误

#### 范例

[dial](../../examples/tls/dial/main.c) · stream tour

```c
	while ( xrtTlsStreamAvailable(pStream) != 0 ) {
```

### `xrtTlsStreamBuffer`

在所属 Worker 上借用明文块链，借用期不超过本次回调。默认在当前明文消费前暂停底层读取；增量协议解析器可显式请求 ReadMore。

```c
const xnetbuf* xrtTlsStreamBuffer(xtlsstream* pStream)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pStream` | 输入/输出 | 非空 | TLS 组合流 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | 对象或借用 | — |
| `NULL` | 失败或不适用 | 见错误 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_MEMORY` — 分配失败

#### 范例

[dial](../../examples/tls/dial/main.c) · stream tour

```c
		const xnetbuf* pPlain = xrtTlsStreamBuffer(pStream);
```

### `xrtTlsStreamClient`

在已连接 TCP Stream 上创建 TLS 客户端。适用于代理隧道、STARTTLS 和自定义拨号；成功时接管 Transport 引用。

```c
bool xrtTlsStreamClient(xnetstream* pTransport, const xtlsclientconfig* pTls, const xtlsstreamconfig* pConfig, const xtlsstreamevents* pEvents, ptr pData, xtlsstream** ppStream)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pTransport` | 输入 | 非空 | 传输 TCP 流 |
| `pTls` | 输入 | 非空 | TLS 对象 |
| `pConfig` | 输入 | — | 配置 |
| `pEvents` | 输入 | — | 事件表 |
| `pData` | 输入 | — | 用户数据 |
| `ppStream` | 输入 | 非空 | 接收流对象 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 成功 | — |
| `false` | 失败 | 见错误 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_PROTOCOL`（TLS 语义） — DER 或消息结构非法

#### 范例

[stream_tour](../../examples/tls/stream_tour/main.c) · stream tour

```c
		pTask->bOk = xrtTlsStreamClient(pTask->pTcp,
			pTask->pClient, pTask->pStream, pTask->pEvents,
			pTask->pData, &pTask->pTls);
```

### `xrtTlsStreamClose`

从任意线程请求 close_notify、等待对端认证关闭并排空 TCP。调用前已接纳的异步发送会先按 FIFO 完成；调用后的新发送不再接纳。

```c
bool xrtTlsStreamClose(xtlsstream* pStream)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pStream` | 输入/输出 | 非空 | TLS 组合流 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 成功 | — |
| `false` | 失败 | 见错误 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_PROTOCOL`（TLS 语义） — DER 或消息结构非法

#### 范例

[dial](../../examples/tls/dial/main.c) · stream tour

```c
		if ( !xrtTlsStreamClose(pStream) ) {
```

### `xrtTlsStreamConfigInit`

初始化握手与认证关闭超时。

```c
void xrtTlsStreamConfigInit(xtlsstreamconfig* pConfig)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pConfig` | 输入 | — | 配置 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 无 | — | — |

#### 错误

- 无 — 初始化不失败

#### 范例

[stream](../../examples/tls/stream/main.c) · stream tour

```c
	xrtTlsStreamConfigInit(&Example.StreamConfig);
```

### `xrtTlsStreamConnect`

创建 TLS 客户端并异步连接数字 TCP 地址。

```c
xtlsstream* xrtTlsStreamConnect(xnetengine* pEngine, const xnetaddr* pRemote, uint64 iAffinity, const xnetstreamconfig* pTransport, const xtlsclientconfig* pTls, const xtlsstreamconfig* pConfig, const xtlsstreamevents* pEvents, ptr pData)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pEngine` | 输入 | 非空 | 网络 Engine |
| `pRemote` | 输入 | 非空 | 远端地址 |
| `iAffinity` | 输入 | — | 亲和 Worker 标识 |
| `pTransport` | 输入 | 非空 | 传输 TCP 流 |
| `pTls` | 输入 | 非空 | TLS 对象 |
| `pConfig` | 输入 | — | 配置 |
| `pEvents` | 输入 | — | 事件表 |
| `pData` | 输入 | — | 用户数据 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | 对象或借用 | — |
| `NULL` | 失败或不适用 | 见错误 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_STATE` — 状态非法（顺序、已关闭、非所属线程）
- `xrt.net` 域错误 — TCP 连接提交失败
- `XERR_MEMORY` — 分配失败

#### 范例

[listener_tour](../../examples/tls/listener_tour/main.c) · stream tour

```c
	pClientB = xrtTlsStreamConnect(pEngine, &Address, 0, NULL,
		&ClientConfigB, NULL, &ClientEvents, &ClientB);
```

### `xrtTlsStreamConsume`

在所属 Worker 上安全消费精确数量的明文。

```c
bool xrtTlsStreamConsume(xtlsstream* pStream, size_t iSize)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pStream` | 输入/输出 | 非空 | TLS 组合流 |
| `iSize` | 输入 | — | 字节数 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 成功 | — |
| `false` | 失败 | 见错误 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_PROTOCOL`（TLS 语义） — DER 或消息结构非法

#### 范例

[dial](../../examples/tls/dial/main.c) · stream tour

```c
		if ( !xrtTlsStreamConsume(pStream, Span.Size) ) {
```

### `xrtTlsStreamData`

返回线程安全的用户数据指针快照，不延长目标生命周期。

```c
ptr xrtTlsStreamData(const xtlsstream* pStream)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pStream` | 输入/输出 | 非空 | TLS 组合流 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 值 | 当前取值 | — |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法

#### 范例

[stream_tour](../../examples/tls/stream_tour/main.c) · stream tour

```c
	pClient->bDataOk = xrtTlsStreamData(pStream) == pClient;
```

### `xrtTlsStreamDestroy`

释放 TLS Stream 引用；关闭必须另行请求。

```c
void xrtTlsStreamDestroy(xtlsstream* pStream)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pStream` | 输入/输出 | 非空 | TLS 组合流 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 无 | — | — |

#### 错误

- 无 — 释放或重置不失败（Reset 系列见参数约束）

#### 范例

[dial](../../examples/tls/dial/main.c) · stream tour

```c
	xrtTlsStreamDestroy(Example.Stream);
```

### `xrtTlsStreamError`

终态失败时借用保存的 TLS 或传输根因。

```c
const xerror* xrtTlsStreamError(const xtlsstream* pStream)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pStream` | 输入/输出 | 非空 | TLS 组合流 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | 对象或借用 | — |
| `NULL` | 失败或不适用 | 见错误 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_MEMORY` — 分配失败

#### 范例

[stream_tour](../../examples/tls/stream_tour/main.c) · stream tour

```c
		xrtTlsStreamError(pClientA) == NULL ? "(none)" : "err");
```

### `xrtTlsStreamPending`

返回 TLS 密文暂存与底层 TCP 队列的总待发字节并发快照。

```c
size_t xrtTlsStreamPending(const xtlsstream* pStream)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pStream` | 输入/输出 | 非空 | TLS 组合流 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `>= 0` | 数量或字节数 | — |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法

#### 范例

[stream_tour](../../examples/tls/stream_tour/main.c) · stream tour

```c
	while ( xrtTlsStreamPending(pClientB) != 0u ) {
```

### `xrtTlsStreamPullup`

在所属 Worker 上把精确明文前缀按需连续化并返回借用视图。不消费明文；视图在下一次明文缓冲修改或消费前有效，零长度和越界请求失败。

```c
bool xrtTlsStreamPullup(xtlsstream* pStream, size_t iSize, xnetspan* pSpan)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pStream` | 输入/输出 | 非空 | TLS 组合流 |
| `iSize` | 输入 | — | 字节数 |
| `pSpan` | 输入 | 非空 | 接收分片 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 成功 | — |
| `false` | 失败 | 见错误 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_PROTOCOL`（TLS 语义） — DER 或消息结构非法

#### 范例

[stream_tour](../../examples/tls/stream_tour/main.c) · stream tour

```c
		if ( !xrtTlsStreamPullup(pStream, 3u, &Span) ||
			(Span.Size < 3u) ) {
```

### `xrtTlsStreamRead`

在所属 Worker 上复制并安全消费明文。

```c
xtlsresult xrtTlsStreamRead(xtlsstream* pStream, void* pOutput, size_t iCapacity, size_t* pRead)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pStream` | 输入/输出 | 非空 | TLS 组合流 |
| `pOutput` | 输入 | 非空 | 输出缓冲 |
| `iCapacity` | 输入 | — | 容量 |
| `pRead` | 输入 | 非空 | 接收读取游标 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `XTLS_OK` | 成功 | — |
| `XTLS_AGAIN` | 暂不可推进 | 不设错误 |
| `XTLS_CLOSED` | 已关闭 | 不设错误 |
| `XTLS_ERROR` | 失败 | 见错误 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_STATE` — 状态非法（顺序、已关闭、非所属线程）
- `XERR_PROTOCOL` — 记录解密失败

#### 范例

[stream_tour](../../examples/tls/stream_tour/main.c) · stream tour

```c
	(void)xrtTlsStreamRead(pStream, pClient->ReadOut, 4u,
		&pClient->iRead);
```

### `xrtTlsStreamReadMore`

在 Read 回调保留现有明文时，请求继续解密并在明文增长后再次发布 Read。累积量受 Context PlainLimit 硬约束，并必须为一条最大明文 record 留出空间。普通消费者无需调用，重复请求是幂等的；请求待完成时不能替换事件接收者。

```c
bool xrtTlsStreamReadMore(xtlsstream* pStream)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pStream` | 输入/输出 | 非空 | TLS 组合流 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 成功 | — |
| `false` | 失败 | 见错误 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_PROTOCOL`（TLS 语义） — DER 或消息结构非法

#### 范例

[stream_tour](../../examples/tls/stream_tour/main.c) · stream tour

```c
		pClient->bReadMore = xrtTlsStreamReadMore(pStream);
```

### `xrtTlsStreamRecvAsync`

在拉取模式下复制并消费当前可用明文。零上限表示读取全部当前明文；成功值是由 Future 持有的 xnetbytes。

```c
xfuture* xrtTlsStreamRecvAsync(xtlsstream* pStream, size_t iMaxBytes)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pStream` | 输入/输出 | 非空 | TLS 组合流 |
| `iMaxBytes` | 输入 | — | 最多字节数 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | 对象或借用 | — |
| `NULL` | 失败或不适用 | 见错误 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_MEMORY` — 分配失败

#### 范例

[stream_future](../../examples/tls/stream_future/main.c) · stream tour

```c
	xfuture* pFuture = xrtTlsStreamRecvAsync(
		pStream,
		64u * 1024u
	);
```

### `xrtTlsStreamRef`

增加 TLS Stream 引用并返回原指针；引用耗尽时返回空并设置状态错误。

```c
xtlsstream* xrtTlsStreamRef(xtlsstream* pStream)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pStream` | 输入/输出 | 非空 | TLS 组合流 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | 对象或借用 | — |
| `NULL` | 失败或不适用 | 见错误 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法

#### 范例

[dial_future](../../examples/tls/dial_future/main.c) · stream tour

```c
	pStream = xrtTlsStreamRef(
		(xtlsstream*)xrtFutureValue(pFuture)
	);
```

### `xrtTlsStreamSend`

在所属 Worker 上把明文编码为记录；允许成功短写。

```c
xtlsresult xrtTlsStreamSend(xtlsstream* pStream, const void* pData, size_t iSize, size_t* pWritten)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pStream` | 输入/输出 | 非空 | TLS 组合流 |
| `pData` | 输入 | — | 用户数据 |
| `iSize` | 输入 | — | 字节数 |
| `pWritten` | 输入 | 非空 | 接收写出数量 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `XTLS_OK` | 成功 | — |
| `XTLS_AGAIN` | 暂不可推进 | 不设错误 |
| `XTLS_CLOSED` | 已关闭 | 不设错误 |
| `XTLS_ERROR` | 失败 | 见错误 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_RANGE` — 发送预算已满
- `XERR_STATE` — 握手未完成或已关闭
- `XERR_PROTOCOL` — 记录保护失败

#### 范例

[dial](../../examples/tls/dial/main.c) · stream tour

```c
		xtlsresult Result = xrtTlsStreamSend(
			pStream,
			pExample->Request + pExample->Sent,
			pExample->RequestSize - pExample->Sent,
			&iWritten
		);
```

### `xrtTlsStreamSendAsync`

从任意线程复制并按 FIFO 提交一段完整明文。Future 在全部明文被 TLS 会话受理时完成；排空必须另行等待 DRAIN。取消只在首个字节受理前有效，已开始的发送保持完整和有序。Close 线性化前已接纳的发送保证先完成，之后的发送以 STATE 拒绝。

```c
xfuture* xrtTlsStreamSendAsync(xtlsstream* pStream, const void* pData, size_t iSize)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pStream` | 输入/输出 | 非空 | TLS 组合流 |
| `pData` | 输入 | — | 用户数据 |
| `iSize` | 输入 | — | 字节数 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | 对象或借用 | — |
| `NULL` | 失败或不适用 | 见错误 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_MEMORY` — 分配失败

#### 范例

[stream_future](../../examples/tls/stream_future/main.c) · stream tour

```c
	if ( !exampleTlsFutureResolved(xrtTlsStreamSendAsync(
		pStream,
		pData,
		iSize
	)) ) {
```

### `xrtTlsStreamSendBound`

在所属 Worker 上返回一次明文发送产生的精确密文线路字节数。结果包含记录头、显式 nonce、内层类型和认证标签，失败不修改 pBound。pBound 不得与 Stream 或其 Session 对象存储重叠。

```c
bool xrtTlsStreamSendBound(xtlsstream* pStream, size_t iPlainSize, size_t* pBound)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pStream` | 输入/输出 | 非空 | TLS 组合流 |
| `iPlainSize` | 输入 | — | 明文长度 |
| `pBound` | 输入 | 非空 | 接收输出上界 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 成功 | — |
| `false` | 失败 | 见错误 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_PROTOCOL`（TLS 语义） — DER 或消息结构非法

#### 范例

[stream_tour](../../examples/tls/stream_tour/main.c) · stream tour

```c
	(void)xrtTlsStreamSendBound(pStream, 64u, &pClient->iBound);
```

### `xrtTlsStreamSendVec`

在所属 Worker 上依次编码明文片段；返回跨片段的连续受理前缀。

```c
xtlsresult xrtTlsStreamSendVec(xtlsstream* pStream, const xnetspan* pSpans, size_t iCount, size_t* pWritten)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pStream` | 输入/输出 | 非空 | TLS 组合流 |
| `pSpans` | 输入 | 非空 | 分片数组 |
| `iCount` | 输入 | — | 数量 |
| `pWritten` | 输入 | 非空 | 接收写出数量 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `XTLS_OK` | 成功 | — |
| `XTLS_AGAIN` | 暂不可推进 | 不设错误 |
| `XTLS_CLOSED` | 已关闭 | 不设错误 |
| `XTLS_ERROR` | 失败 | 见错误 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法

#### 范例

[stream_tour](../../examples/tls/stream_tour/main.c) · stream tour

```c
	(void)xrtTlsStreamSendVec(pStream, Vec, 2, &iWritten);
```

### `xrtTlsStreamSendVecAsync`

从任意线程复制片段并按 FIFO 提交为一段连续明文。全部片段在返回前完成校验和复制，失败不会发布部分操作。

```c
xfuture* xrtTlsStreamSendVecAsync(xtlsstream* pStream, const xnetspan* pSpans, size_t iCount)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pStream` | 输入/输出 | 非空 | TLS 组合流 |
| `pSpans` | 输入 | 非空 | 分片数组 |
| `iCount` | 输入 | — | 数量 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | 对象或借用 | — |
| `NULL` | 失败或不适用 | 见错误 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_MEMORY` — 分配失败

#### 范例

[stream_tour](../../examples/tls/stream_tour/main.c) · stream tour

```c
	pSendFuture = xrtTlsStreamSendVecAsync(pClientB, AsyncVec, 2);
```

### `xrtTlsStreamSession`

在所属 Worker 上借用协议会话，供 ALPN、票据等高级查询。

```c
xtlssession* xrtTlsStreamSession(xtlsstream* pStream)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pStream` | 输入/输出 | 非空 | TLS 组合流 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | 对象或借用 | — |
| `NULL` | 失败或不适用 | 见错误 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_MEMORY` — 分配失败

#### 范例

[stream_tour](../../examples/tls/stream_tour/main.c) · stream tour

```c
	pClient->bSessionOk = xrtTlsStreamSession(pStream) != NULL;
```

### `xrtTlsStreamSetEvents`

在所属 Worker 上替换已打开 TLS Stream 的事件与用户数据。不会自动重放当前明文缓冲，协议升级层必须显式处理已有后缀。

```c
bool xrtTlsStreamSetEvents(xtlsstream* pStream, const xtlsstreamevents* pEvents, ptr pData)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pStream` | 输入/输出 | 非空 | TLS 组合流 |
| `pEvents` | 输入 | — | 事件表 |
| `pData` | 输入 | — | 用户数据 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 成功 | — |
| `false` | 失败 | 见错误 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_PROTOCOL`（TLS 语义） — DER 或消息结构非法

#### 范例

[stream_tour](../../examples/tls/stream_tour/main.c) · stream tour

```c
	pTask->bOk = xrtTlsStreamSetEvents(pTask->pStream,
		pTask->pEvents, NULL);
```

### `xrtTlsStreamState`

返回组合 Stream 状态的并发快照。

```c
xtlsstreamstate xrtTlsStreamState(const xtlsstream* pStream)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pStream` | 输入/输出 | 非空 | TLS 组合流 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 枚举值 | 当前取值 | — |

#### 错误

- 无 — 纯查询，不设置错误

#### 范例

[dial_future](../../examples/tls/dial_future/main.c) · stream tour

```c
	while ( (xrtTlsStreamState(pStream) != XTLS_STREAM_CLOSED) &&
		(xrtTlsStreamState(pStream) != XTLS_STREAM_FAILED) ) {
```

### `xrtTlsStreamTransport`

借用底层 TCP Stream，调用方不得改变其 IO 状态机。

```c
xnetstream* xrtTlsStreamTransport(const xtlsstream* pStream)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pStream` | 输入/输出 | 非空 | TLS 组合流 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | 对象或借用 | — |
| `NULL` | 失败或不适用 | 见错误 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_MEMORY` — 分配失败

#### 范例

[listener_tour](../../examples/tls/listener_tour/main.c) · stream tour

```c
		xrtNetStreamWorker(xrtTlsStreamTransport(pServerA)),
```

### `xrtTlsStreamWaitAsync`

建立 OPEN、READ、WRITE、DRAIN、END 或 CLOSE 条件 Future。WRITE 要求发送 FIFO 清空且当前至少可受理明文；DRAIN 还要求 TLS 与 TCP 两级发送队列归零。END 在已认证明文全部交付后完成。取消只移除本次等待。

```c
xfuture* xrtTlsStreamWaitAsync(xtlsstream* pStream, xtlsstreamwait Wait)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pStream` | 输入/输出 | 非空 | TLS 组合流 |
| `Wait` | 输入 | 非空 | 等待条件 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | 对象或借用 | — |
| `NULL` | 失败或不适用 | 见错误 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_MEMORY` — 分配失败

#### 范例

[stream_future](../../examples/tls/stream_future/main.c) · stream tour

```c
	return exampleTlsFutureResolved(xrtTlsStreamWaitAsync(
		pStream,
		XTLS_STREAM_WAIT_DRAIN
	));
```

## TLS-over-TCP 组合流

`<xrt/tls_stream.h>` 以 `XRT_FEATURE_TLS_STREAM` 独立裁剪，把公开 TCP Stream 与公开 TLS 客户端/服务端会话组合成事件驱动明文字节流。它不复制握手、验证、身份或记录状态机，也不让 TLS 原语反向依赖 socket；高级用户仍可直接使用传输无关会话层。

客户端连接数字地址使用 `xrtTlsStreamConnect()`。已经通过代理、自定义拨号或
明文协议协商得到 TCP Stream 时，在其所属 Worker 上调用
`xrtTlsStreamClient()`；它成功后接管 TCP 引用，后续只从 TLS Stream 读写。
需要自行创建角色 Session 的底层用户可以调用 `xrtTlsStreamAttach()`，成功时
同时转移 Session 与 TCP 引用。服务端在 TCP `Accept` 回调内调用
`xrtTlsStreamAccept()`，并把其布尔结果直接作为 Accept 结果返回：

```c
static bool acceptTls(
	xnetlistener* pListener,
	xnetstream* pTransport,
	ptr pData
)
{
	server* pServer = (server*)pData;

	(void)pListener;
	return xrtTlsStreamAccept(
		pTransport,
		&pServer->Tls,
		&pServer->Stream,
		&pServer->Events,
		pServer,
		NULL
	);
}
```

`xrtTlsStreamAccept()` 成功后接管 TCP Accept 交付的调用方引用；失败时保持原
Accept 失败回收规则。`xrtTlsStreamAttach()` 和 `xrtTlsStreamClient()` 只接受
已经发布 Open、尚未读结束、写结束、Close 或 Abort 的双向 Stream，必须在该
Stream 的 Worker 上调用。调用方必须在升级点停止直接收发和改变 TCP 状态。
失败时输出清空，不接管输入引用，不改变 Session 的缓冲池归属，也不替换 TCP
事件或用户数据；修正配置后可用同一 Session 与 Transport 重试。它们会立即
处理 TCP 缓冲中已有的密文，因此代理响应后的尾随 TLS 记录不会丢失。成功时
`ppStream` 返回独立调用方引用。
`xrtTlsStreamConnect()` 成功同样返回调用方引用。`xrtTlsStreamDestroy()` 只释放
引用，不隐式关闭；运行时引用会保持对象活到唯一 `Close` 回调结束。

完成握手后，可以在所属 Worker 上调用
`xrtTlsStreamSetEvents(pStream, pEvents, pData)` 原子替换事件表和用户数据。
它用于 HTTPS Upgrade、WebSocket 和应用协议协商后的处理器接管；不会再次发布
`Open`，也不会重放切换前已经留在明文缓冲中的数据。接管层必须在切换点显式
取得并处理协议余量。原始 TCP 仍由 TLS 组合流独占，不能借此绕过 TLS 接管。

`xrtTlsStreamData()` 以 acquire 语义返回当前用户数据的借用指针快照；事件切换
在所属 Worker 上保持顺序，但查询可以来自任意线程。快照不延长目标生命周期，
指针所指对象的存活期与并发访问仍由调用方管理。`xrtTlsStreamRef()` 在引用计数
耗尽时返回空并设置状态错误，不会让已经释放或饱和的对象重新进入生命周期。

需要主机名时启用 `XRT_FEATURE_TLS_STREAM_DIAL` 并调用 `xrtTlsDial()`。该可选层
直接复用 `xrtNetDial()` 的 Resolver、双栈候选竞速、回退、取消和统计，不复制
另一套 DNS 或 TCP 连接器。数字地址使用 `xrtTlsStreamConnect()`；已有 TCP
Stream 使用 `xrtTlsStreamClient()` 或 `xrtTlsStreamAttach()`；服务端 Accept
使用 `xrtTlsStreamAccept()`；完全自定义传输仍可直接使用会话层。

```c
xtlsdialconfig Config;
xtlsdial* pDial;

xrtTlsDialConfigInit(&Config);
Config.Timeout = 15000000u;
pDial = xrtTlsDial(
	Engine,
	Resolver,
	"example.com",
	443,
	&Tls,
	&Config,
	&Events,
	App,
	dialDone,
	App
);
```

`Config.Transport` 控制 DNS、地址族、Happy Eyeballs、候选数和 TCP 阶段超时；`Config.Stream` 控制 TLS 握手、认证关闭和缓冲硬上限；`Config.Timeout` 是从提交开始覆盖 DNS、TCP 与 TLS 的全过程硬上限，零值表示只使用各阶段超时。`ServerNameFromHost` 默认开启，仅在 `Tls.ServerName` 为空时用 `sHost` 填充 SNI 与证书名称；显式名称始终优先，关闭该选项则不做自动填充。

通过配置校验后，受管 TLS Dial 会先取得一份 Engine 初始化租约，再创建客户端会话、组合 Stream、全过程 Timer 和底层 TCP Dial。只有这些活动对象已经接管 Engine 生命周期后，临时租约才会释放；任一创建步骤失败则在返回前完整回滚并保留原始 cause。并发 `xrtNetEngineDestroy()` 不能跨过这段尚未返回对象的初始化窗口。

成功时，最终 Stream 的用户 `Open` 先执行，随后 `xtlsdialproc` 以 `XNET_RESULT_OK` 交付同一 Stream 的调用方引用。握手完成前失败只发布一次 Dial 完成回调；半初始化 TLS Stream 在回调前释放，也不额外调用用户 Stream `Close`。`xrtTlsDialCancel()` 可以在解析、TCP 连接或 TLS 握手阶段竞争取消；返回真表示取消已经赢得唯一终态门，之后不能再发布成功。安全 Stream 已赢得发布权或 Dial 已终止时返回假。终态由 `xrtTlsDialState()` 与 `xrtTlsDialError()` 固定保存；底层 TCP Dial 已经终结 Engine 活动占用，但其只读快照保留到 TLS Dial 销毁，因此 `xrtTlsDialTransportStats()` 在 TLS 阶段和终态继续提供候选、尝试、失败和获胜地址统计。

启用 `XRT_FEATURE_TLS_STREAM_DIAL_FUTURE` 后，可以用同一受管拨号状态机直接取得
Future：

```c
xfuture* pDial = xrtTlsDialAsync(
	Engine,
	Resolver,
	"example.com",
	443,
	&Tls,
	&Config,
	&Events,
	App
);
```

成功 Future 持有一个已经 `OPEN` 的 `xtlsstream` 引用，用户 `Open` 回调仍先于
`XFUTURE_RESOLVED` 发布。`xrtFutureValue()` 返回借用指针；需要在销毁 Future 后
继续使用时，先调用 `xrtTlsStreamRef()` 保留独立引用。Future 值析构只释放它所
持有的引用，不隐式关闭其他调用方引用。

`xrtFutureCancel()` 把协作取消传递到当前 DNS、TCP 或 TLS 阶段，只有底层完成
清理后 Future 才进入 `XFUTURE_CANCELLED`。连接失败与全过程超时进入
`XFUTURE_FAILED`，`xrtFutureError()` 保留 TLS 包装错误和底层原因链。需要观察
候选统计或在完成前查询阶段时继续使用回调式 `xrtTlsDial()`；Future 入口是常见
连接路径的轻量适配器，不复制 Dial 状态机。

`Open` 只在 TCP 已连接、TLS 已到 `READY` 且 SNI/ALPN/证书验证全部完成后发布。客户端与服务端事件都在底层 TCP 所属 Worker 上串行执行。`Send`、`SendBound`、`Buffer`、`Read`、`Consume` 和 `Session` 必须在该 Worker 上调用；`Close`、`Abort`、`State`、`Available`、`Pending`、`Transport`、`Data` 和终态 `Error` 支持并发调用或快照读取。

```c
size_t iWritten = 0;
xtlsresult Result = xrtTlsStreamSend(
	Stream,
	Data,
	Size,
	&iWritten
);

size_t WireSize;
bool Sized = xrtTlsStreamSendBound(Stream, Size, &WireSize);
```

`Send` 允许成功短写。`XTLS_OK` 且 `iWritten < Size` 表示该前缀已原子受理，剩余数据由调用方保留；`XTLS_AGAIN` 保证 `iWritten == 0`。`xrtTlsStreamSendVec()` 先校验全部 Span 与总长度，再直接逐片生成记录，不分配一块拼接副本；`iWritten` 是跨 Span 的连续受理前缀，输入无效或总长度溢出时保持为零且不会修改会话。两种发送入口使用相同的背压契约。

`xrtTlsStreamSendBound()` 在所属 Worker 上返回一次 `Send` 对指定明文产生的精确密文线路长度，包含每条记录的头、显式 nonce、TLS 1.3 内层类型和认证标签。它不修改会话、序列号或发送队列；零长度返回零，算术溢出或输出与 Stream/Session 重叠时失败且不修改输出。协议适配层可用它在接受明文前把自身队列预算换算成真实 TLS 线路成本。

背压会登记一个边沿，TLS 发送队列和 TCP 队列重新具备容量后发布 `Writable`。应用必须在 `Writable` 中从未受理偏移继续发送，不能重发已经计入 `iWritten` 的前缀。每次至少受理一个字节后会登记 `Drain`；只有 TLS 密文队列与 TCP 用户态发送预算同时归零才发布该事件。`Writable` 与 `Drain` 均不会重入尚未返回的 `xrtTlsStreamSend()` 或 `xrtTlsStreamSendVec()`，因此调用方可以在返回后再统一提交 `iWritten`。

`xrtTlsStreamPending()` 返回 TLS Session 尚未转移的密文与底层 TCP 用户态发送
队列的饱和相加快照。它不包含已被操作系统接受的内核缓冲字节，也不代表对端
已经读取；成功短写后允许立即为零。该查询可从任意线程用于统计和限流，
`Drain` 回调发布时它必须为零。

适配器要求 TCP `WriteLimit >= TLS SendLimit`，从而把一批完整 TLS 密文块链全有或全无地转移给 TCP。常规路径不复制密文；TCP ReadBuffer 可以整体移动进 TLS Feed。当一次 TCP 输入大于 Feed 剩余容量时，只复制可容纳的前部 Span，消费后继续，避免以固定 8 KiB 缓冲或无界增长掩盖压力。成功短写与同步 TCP 低水位回调之间有重入门；同步产生的 `Writable`/`Drain` 会转为同一 Worker 的内部命令，外层发送返回后才允许进入应用。

`Read` 回调借用 `const xnetbuf*`。应用可以用 `xrtTlsStreamRead()` 复制并消费，也可以检查 `xrtTlsStreamBuffer()` 后以 `xrtTlsStreamConsume()` 精确确认已处理字节。默认通知采用受控边沿语义：当前明文没有全部消费前暂停新的 TCP 接收。增量协议解析器在保留不完整前缀时可调用 `xrtTlsStreamReadMore()`；TLS 会在 `PlainLimit` 内继续解密，只在明文增长后再次发布 `Read`，并要求限制中仍能容纳一条最大明文 record，避免有空间但无法取得下一条完整记录的永久停滞。请求待完成期间不能替换事件接收者。`xrtTlsStreamPullup()` 只按需连续化精确前缀，不消费明文。借用不能保存到下一次回调；消费到零后恢复普通残留密文处理和底层读取。

### TLS Stream Future

启用 `XRT_FEATURE_TLS_STREAM_FUTURE` 后，同一个 TLS Stream 可以从任意线程使用
Future 入口，不需要为同步等待或协程再建立一套连接对象：

```c
xfuture* pSend = xrtTlsStreamSendAsync(Stream, Data, Size);
xfuture* pDrain = xrtTlsStreamWaitAsync(
	Stream,
	XTLS_STREAM_WAIT_DRAIN
);
xfuture* pReceive = xrtTlsStreamRecvAsync(Stream, 64u * 1024u);
```

`xrtTlsStreamSendAsync()` 和 `xrtTlsStreamSendVecAsync()` 在提交期间复制完整输入，
调用返回后不再借用原数据。多个发送保持严格 FIFO；成功 Future 表示全部明文
已经被 TLS 会话受理，不表示密文已经离开 TCP 用户态队列，更不表示对端已经
读取。需要本地排空时另行等待 `XTLS_STREAM_WAIT_DRAIN`。

`xrtTlsStreamClose()` 与异步发送接纳共享一个线性化门。关闭调用之前已经成功
返回 Future 的发送会先保持 FIFO 完整受理，随后才生成 `close_notify`；关闭门
生效后的新发送立即以 `XERR_STATE/XTLS_ERROR_STATE` 拒绝。构造期间的 OOM
会完整归还预算并继续被延迟的关闭，不会让连接永久停在 OPEN。零字节发送是
合法的 FIFO 节点，在轮到它时成功完成且不产生 TLS 应用记录。

发送取消只在首个字节被 TLS 会话受理前有效。尚未开始的节点确认
`XFUTURE_CANCELLED` 并完整归还预算；已经发生成功短写的节点忽略后续取消请求，
继续按原顺序发送剩余后缀，最终成功或报告真实连接终态。这个规则避免取消把
一个应用消息静默截成线路前缀。

`xrtTlsStreamRecvAsync()` 是 pull 模式入口。它在所属 Worker 上先为结果分配独立
存储，再消费当前可用明文；成功值是由 Future 持有的 `xnetbytes`。读取内容统一
调用 `xrtNetBytesView()`；通过 `xrtNetBytesRef()` 增加引用后，结果可以越过 Future
生命周期继续使用。`iMaxBytes == 0` 读取当前全部明文。结果分配失败不会
消费任何字节，恢复内存后可以重试。一个 Stream 不能同时安装 `Read` 回调并
登记 READ/Recv Future；双向切换都以 `XERR_STATE/XTLS_ERROR_STATE` 拒绝，防止
两条路径竞争消费同一明文。

已经通过 TLS 记录认证并解密的明文不会因为随后发生 TCP 截断、协议失败或本地 Abort
而被丢弃。终态 Stream 的 `RecvAsync` 先返回这些缓冲字节；缓冲耗尽后，下一次接收
才返回稳定的失败或关闭结果。Stream 的 `FAILED` 状态和 `xrtTlsStreamError()` 在读取
期间保持不变，因此截断敏感的上层协议仍能明确拒绝不完整消息。

`xrtTlsStreamWaitAsync()` 提供六个水平条件：

| 条件 | 完成点 |
| --- | --- |
| `OPEN` | TCP、TLS 握手和认证全部完成 |
| `READ` | 至少一个明文字节可消费 |
| `WRITE` | 异步发送 FIFO 为空且当前可受理明文 |
| `DRAIN` | 异步发送 FIFO、TLS 密文和 TCP 用户态发送队列全部为空 |
| `END` | 收到并认证对端 `close_notify`，且此前明文已经全部交付 |
| `CLOSE` | TLS Stream 到达最终传输终态 |

`xtlsstreamconfig` 的 `AsyncBytesLimit`、`AsyncCountLimit` 是独立硬边界，
`AsyncBatch` 限制一次 Worker 轮转最多完成的节点数。三个值都必须非零。
超出字节边界返回 `XERR_RANGE/XTLS_ERROR_LIMIT`，并发操作数饱和返回
`XERR_AGAIN/XTLS_ERROR_LIMIT`；失败提交不会残留节点或预算。
`xrtTlsStreamAsyncBytes()` 与 `xrtTlsStreamAsyncCount()` 提供无锁并发快照。

所有 Promise 终态都由所属 Worker 确认；已经终止并失去 Worker 的对象允许在
调用线程立即返回固定结果。认证关闭使 END/CLOSE 成功，普通对端 EOF 或 TLS
协议错误使挂起操作失败并保留 `xrtTlsStreamError()` 根因，本地主动 Abort 使
挂起操作进入 `XFUTURE_CANCELLED`。通用 `xrtFutureWait*()` 和
`xrtFutureAwait*()` 可以直接消费这些 Future，不增加 TLS 专用协程 API。
已认证应用明文始终先于 END 和接收侧 `XFUTURE_CLOSED` 交付；即使最后一个
应用记录与 `close_notify` 同批到达，也不会出现先观察 EOF、后出现残留明文的
窗口。

固定大小等待、接收元数据和总分配不超过 1 KiB 的发送节点共享所属 Worker 的
`NodeCacheBytes` 预算；较大发送保持一次普通堆分配，不把载荷塞入小节点缓存。活动
缓存节点持有临时 Engine 租约，节点归还后才释放。TLS Stream 发布最终 Close 后，
新建 Future 使用独立堆且不访问底层 TCP Worker，因此调用方保留的终态 TLS Stream
可以晚于 Engine 销毁，并继续取得 Close、EOF 或固定失败结果。

`xrtTlsStreamClose()` 排队一次 `close_notify`，排空 TCP，并等待对端经过认证的 `close_notify`。收到对端通知时先发布一次 `End`，双向认证关闭和 TCP 终态都完成后才发布 `CLOSED/XNET_RESULT_OK`。握手超时默认 10 秒，认证关闭超时默认 5 秒；配置值为零显式禁用对应 Timer。对端直接 EOF 映射为 `XTLS_ERROR_TRUNCATED`，关闭等待到期映射为 `XERR_TIMEOUT/XTLS_ERROR_CLOSED`。`Abort` 不生成 Alert 并立即放弃 TCP；若 TLS 已经失败但仍在发送 fatal Alert 或等待传输收尾，`Abort` 仍会加速关闭，同时保留原来的失败结果和根因。只有尚无失败根因的主动 Abort 才发布取消结果。

第一个 TLS、验证、内存、Timer 或传输根因保存在对象中，不会被后续关闭错误覆盖。失败 `Close` 回调中的 `pError` 和 `xrtTlsStreamError()` 都借用该稳定原因；底层 TCP 失败以 TLS 组合错误包装并保留原 Cause。正常 `CLOSED` 的 `xrtTlsStreamError()` 始终为空。

`xrtTlsStreamSession()` 是 Worker 内高级查询入口，可读取 ALPN、恢复票据等会话资产。`xrtTlsStreamTransport()` 借用原始 TCP Stream，只用于地址、统计和标准库尚未覆盖的只读/安全选项；应用不得关闭、收发、切换阻塞模式、替换事件或直接消费其缓冲。确实需要自定义传输行为时应回到公开会话层，而不是破坏组合对象状态机。

启用 `XRT_FEATURE_TLS_CLIENT_RESUME` 时，`xtlsstreamevents.Ticket` 在客户端
恢复队列新增 ticket 后于所属 Worker 发布。回调只表示“现在有票据可取”，
不转移 ticket，也不延迟 `Open`、HTTP 完成或连接关闭；处理器应通过
`xrtTlsStreamSession()` 取得会话，再循环调用 `xrtTlsClientTakeResume()`，
直到队列为空。队列已满并用新 ticket 替换最旧项时，长度虽然不变，仍会发布
新的边沿。切换 `xrtTlsStreamSetEvents()` 后，后续 ticket 只通知新的
处理器；已经在队列中的票据不会重放事件，接管层应在切换时主动排空一次。

完整 Echo 服务示例位于 `examples/tls/stream/main.c`，Future 用法位于
`examples/tls/stream_future/main.c`，使用系统信任库的主机名客户端位于
`examples/tls/dial/main.c`，Future Dial 位于
`examples/tls/dial_future/main.c`。select、IOCP 与 io_uring 共用同一份 TLS
Stream 和 TLS Dial 测试主体；后端文件只选择端口实现，不复制握手、背压、超时
或关闭断言。生命周期、Attach 失败原子性、失败后 Abort 根因保持、截断 EOF、
握手超时、认证关闭超时、单段和向量发送、向量失败原子校验、成功短写、
`Writable`/`Drain` 非重入、小 FeedLimit 大 TCP Read、延迟明文消费、组合对象
OOM、Timer 调度拒绝回滚、会话恢复、非法参数和单头文件真实传输门禁位于
`tests/tls/test_tls_stream*.c` 与 `tests/single/test_single_tls_stream*.c`。
Future 门禁另外覆盖 callback/pull 排他、并发硬预算、锁外错误构造、构造与结果
OOM、构造预留回滚、零字节发送、开始前取消、成功短写后的取消、关闭门之前
4 MiB 发送完整交付、关闭后的发送拒绝、同批应用记录先于 END、认证关闭、Abort、
异常关闭后的已认证明文、Worker 节点缓存复用、Engine 销毁后的终态 Future、
GCC/TinyCC、Select/IOCP 和通用协程恢复。主机名、验证名称、IPv6 到 IPv4 回退、
TCP 耗尽、解析期取消、全过程
超时、Timer 拒绝恢复、传输统计和 Open-before-Done 顺序由
`tests/tls/test_tls_stream_dial*.c` 压实。Engine Timer 扩容 OOM 的独立边界由
`tests/network/test_net_engine_oom.c` 压实。Future Dial 另外验证成功 Stream 在
Future 发布前已经 OPEN、保留引用后销毁 Future 不关闭连接、DNS/TCP/TLS 各阶段
协作取消、全过程超时、结构化原因链、Select/IOCP/io_uring 后端包装与通用协程
Await；公共桥接器的监听安装 OOM 由
`tests/concurrency/test_future_bridge_oom.c` 确定性覆盖。
