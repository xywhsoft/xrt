#ifndef XRT_TLS_H
#define XRT_TLS_H

#include <xrt/core.h>



#if defined(XRT_FEATURE_TLS_RECORD) && !defined(XRT_FEATURE_TLS)
	#error "XRT TLS record protection requires XRT_FEATURE_TLS"
#endif

#if defined(XRT_FEATURE_TLS_RECORD) && !defined(XRT_FEATURE_CRYPTO_CORE)
	#error "XRT TLS record protection requires XRT_FEATURE_CRYPTO_CORE"
#endif

#if defined(XRT_FEATURE_TLS_RECORD_AES) && \
	(!defined(XRT_FEATURE_TLS_RECORD) || \
	 !defined(XRT_FEATURE_CRYPTO_AES_GCM))
	#error "XRT TLS AES record protection requires TLS records and AES-GCM"
#endif

#if defined(XRT_FEATURE_TLS_RECORD_CHACHA) && \
	(!defined(XRT_FEATURE_TLS_RECORD) || \
	 !defined(XRT_FEATURE_CRYPTO_CHACHA20_POLY1305))
	#error "XRT TLS ChaCha record protection requires TLS records and ChaCha20-Poly1305"
#endif

#if defined(XRT_FEATURE_TLS_SCHEDULE) && \
	(!defined(XRT_FEATURE_TLS) || !defined(XRT_FEATURE_CRYPTO_CORE))
	#error "XRT TLS key schedule requires TLS and crypto core"
#endif

#if defined(XRT_FEATURE_TLS_SCHEDULE_SHA256) && \
	(!defined(XRT_FEATURE_TLS_SCHEDULE) || \
	 !defined(XRT_FEATURE_CRYPTO_HKDF_SHA256))
	#error "XRT TLS SHA-256 schedule requires TLS schedule and HKDF-SHA256"
#endif

#if defined(XRT_FEATURE_TLS_SCHEDULE_SHA384) && \
	(!defined(XRT_FEATURE_TLS_SCHEDULE) || \
	 !defined(XRT_FEATURE_CRYPTO_HKDF_SHA512))
	#error "XRT TLS SHA-384 schedule requires TLS schedule and HKDF-SHA384"
#endif

#if defined(XRT_FEATURE_TLS_HANDSHAKE) && !defined(XRT_FEATURE_TLS)
	#error "XRT TLS handshake framing requires XRT_FEATURE_TLS"
#endif

#if defined(XRT_FEATURE_TLS_HANDSHAKE_READER) && \
	!defined(XRT_FEATURE_TLS_HANDSHAKE)
	#error "XRT TLS handshake reader requires XRT_FEATURE_TLS_HANDSHAKE"
#endif

#if defined(XRT_FEATURE_TLS_HELLO) && !defined(XRT_FEATURE_TLS_HANDSHAKE)
	#error "XRT TLS hello parsing requires XRT_FEATURE_TLS_HANDSHAKE"
#endif

#if defined(XRT_FEATURE_TLS_HELLO_WRITE) && !defined(XRT_FEATURE_TLS_HELLO)
	#error "XRT TLS hello writing requires XRT_FEATURE_TLS_HELLO"
#endif

#if defined(XRT_FEATURE_TLS_PSK) && !defined(XRT_FEATURE_TLS_HELLO)
	#error "XRT TLS PSK parsing requires XRT_FEATURE_TLS_HELLO"
#endif

#if defined(XRT_FEATURE_TLS_PSK_WRITE) && \
	(!defined(XRT_FEATURE_TLS_PSK) || \
	 !defined(XRT_FEATURE_TLS_HELLO_WRITE))
	#error "XRT TLS PSK writing requires PSK parsing and hello writing"
#endif

#if defined(XRT_FEATURE_TLS_NEGOTIATE) && !defined(XRT_FEATURE_TLS_HELLO)
	#error "XRT TLS negotiation requires XRT_FEATURE_TLS_HELLO"
#endif

#if defined(XRT_FEATURE_TLS_POLICY) && \
	(!defined(XRT_FEATURE_TLS_NEGOTIATE) || \
	 !defined(XRT_FEATURE_TLS_KEY_EXCHANGE))
	#error "XRT TLS policy requires negotiation and key exchange metadata"
#endif

#if defined(XRT_FEATURE_TLS_CONTEXT) && !defined(XRT_FEATURE_TLS_POLICY)
	#error "XRT TLS context requires XRT_FEATURE_TLS_POLICY"
#endif

#if defined(XRT_FEATURE_TLS_KEY_EXCHANGE) && !defined(XRT_FEATURE_TLS)
	#error "XRT TLS key exchange requires XRT_FEATURE_TLS"
#endif

#if defined(XRT_FEATURE_TLS_KEY_EXCHANGE_X25519) && \
	(!defined(XRT_FEATURE_TLS_KEY_EXCHANGE) || \
	 !defined(XRT_FEATURE_CRYPTO_X25519_KEYPAIR))
	#error "XRT TLS X25519 key exchange requires TLS key exchange and X25519 key pairs"
#endif

#if defined(XRT_FEATURE_TLS_KEY_EXCHANGE_X448) && \
	(!defined(XRT_FEATURE_TLS_KEY_EXCHANGE) || \
	 !defined(XRT_FEATURE_CRYPTO_X448_KEYPAIR))
	#error "XRT TLS X448 key exchange requires TLS key exchange and X448 key pairs"
#endif

#if defined(XRT_FEATURE_TLS_KEY_EXCHANGE_P256) && \
	(!defined(XRT_FEATURE_TLS_KEY_EXCHANGE) || \
	 !defined(XRT_FEATURE_CRYPTO_P256_KEYPAIR))
	#error "XRT TLS P-256 key exchange requires TLS key exchange and P-256 key pairs"
#endif

#if defined(XRT_FEATURE_TLS_KEY_EXCHANGE_P384) && \
	(!defined(XRT_FEATURE_TLS_KEY_EXCHANGE) || \
	 !defined(XRT_FEATURE_CRYPTO_P384_KEYPAIR))
	#error "XRT TLS P-384 key exchange requires TLS key exchange and P-384 key pairs"
#endif

#if defined(XRT_FEATURE_TLS_MESSAGES) && !defined(XRT_FEATURE_TLS_HELLO)
	#error "XRT TLS semantic messages require XRT_FEATURE_TLS_HELLO"
#endif

#if defined(XRT_FEATURE_TLS_MESSAGES_WRITE) && \
	!defined(XRT_FEATURE_TLS_MESSAGES)
	#error "XRT TLS semantic message writing requires XRT_FEATURE_TLS_MESSAGES"
#endif

#if defined(XRT_FEATURE_TLS_AUTH_MESSAGES) && \
	!defined(XRT_FEATURE_TLS_MESSAGES)
	#error "XRT TLS authentication messages require XRT_FEATURE_TLS_MESSAGES"
#endif

#if defined(XRT_FEATURE_TLS_AUTH_MESSAGES_WRITE) && \
	!defined(XRT_FEATURE_TLS_AUTH_MESSAGES)
	#error "XRT TLS authentication message writing requires XRT_FEATURE_TLS_AUTH_MESSAGES"
#endif



#if defined(XRT_FEATURE_TLS)

/* TLS 记录头固定为类型、兼容版本和 16 位负载长度。 */
#define XTLS_RECORD_HEADER_SIZE 5u

/* 明文记录最多承载 2^14 字节。 */
#define XTLS_RECORD_PLAINTEXT_MAX 16384u

/* TLS 1.2 密文记录允许在明文上限之外增加最多 2048 字节。 */
#define XTLS12_RECORD_CIPHERTEXT_MAX 18432u

/* TLS 1.3 密文记录允许在明文上限之外增加最多 256 字节。 */
#define XTLS13_RECORD_CIPHERTEXT_MAX 16640u

/* 当前 AEAD 的 TLSInnerPlaintext 最多包含 2^14 字节内容和一个类型字节。 */
#define XTLS13_INNER_PLAINTEXT_MAX 16385u

/* AES-GCM 在单组流量密钥下采用保守的 2^24 条记录使用上限。 */
#define XTLS_AES_GCM_RECORD_LIMIT UINT64_C(16777216)

/* 单个 TLS 扩展负载受线路 16 位长度字段限制。 */
#define XTLS_EXTENSION_DATA_MAX 65535u

/* TLS 1.3 会话票据最长只能在七天内恢复。 */
#define XTLS13_TICKET_LIFETIME_MAX 604800u



#if defined(XRT_FEATURE_TLS_HANDSHAKE)

/* TLS 握手消息头由类型和 24 位正文长度组成。 */
#define XTLS_HANDSHAKE_HEADER_SIZE 4u

/* TLS 握手正文受线路 24 位长度字段限制。 */
#define XTLS_HANDSHAKE_BODY_MAX 16777215u

/* 默认单条握手消息上限为 1 MiB，远低于 24 位线路极限。 */
#define XTLS_HANDSHAKE_LIMIT_DEFAULT 1048576u

/* TLS 扩展头由 16 位类型和 16 位负载长度组成。 */
#define XTLS_EXTENSION_HEADER_SIZE 4u

#endif



#if defined(XRT_FEATURE_TLS_HANDSHAKE_READER)

/* 默认只跨消息保留最多 16 KiB 已分配缓冲。 */
#define XTLS_HANDSHAKE_RETAIN_DEFAULT 16384u

#endif



#if defined(XRT_FEATURE_TLS_HELLO)

/* ClientHello 与 ServerHello 的 random 字段固定为 32 字节。 */
#define XTLS_RANDOM_SIZE 32u

/* 兼容会话标识受 TLS 握手线路格式限制为最多 32 字节。 */
#define XTLS_SESSION_ID_MAX 32u

#endif



/* TLS 协议操作把正常控制结果与结构化错误分开表达。 */
typedef enum xtlsresult {
	XTLS_ERROR = -1,
	XTLS_OK = 0,
	XTLS_AGAIN,
	XTLS_CLOSED
} xtlsresult;



/* XRT 只协商 TLS 1.2 和 TLS 1.3。 */
typedef enum xtlsversion {
	XTLS_VERSION_12 = 0x0303,
	XTLS_VERSION_13 = 0x0304
} xtlsversion;



/* XRT 支持的密码套件全部使用 AEAD，TLS 1.2 只保留前向保密套件。 */
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

/* TLS_FALLBACK_SCSV 是 ClientHello 中的信号值，不属于可协商密码套件。 */
#define XTLS_FALLBACK_SCSV UINT16_C(0x5600)



/* TLS 密码套件使用的摘要算法与密码后端解耦。 */
typedef enum xtlshash {
	XTLS_HASH_SHA256 = 1,
	XTLS_HASH_SHA384
} xtlshash;



/* TLS 记录保护当前只保留两类现代 AEAD。 */
typedef enum xtlsaead {
	XTLS_AEAD_AES_GCM = 1,
	XTLS_AEAD_CHACHA20_POLY1305
} xtlsaead;



/* TLS 1.2 把认证类型编码进套件，TLS 1.3 则与套件独立。 */
typedef enum xtlscipherauth {
	XTLS_CIPHER_AUTH_INDEPENDENT = 0,
	XTLS_CIPHER_AUTH_RSA,
	XTLS_CIPHER_AUTH_ECDSA
} xtlscipherauth;



/* TLS 角色决定握手状态机的方向。 */
typedef enum xtlsrole {
	XTLS_CLIENT = 1,
	XTLS_SERVER = 2
} xtlsrole;



/* 会话状态只描述公开生命周期，不暴露内部握手步骤。 */
typedef enum xtlsstate {
	XTLS_STATE_NEW = 0,
	XTLS_STATE_HANDSHAKE,
	XTLS_STATE_READY,
	XTLS_STATE_CLOSING,
	XTLS_STATE_CLOSED,
	XTLS_STATE_FAILED
} xtlsstate;



/* TLS 记录内容类型使用协议规定的稳定数值。 */
typedef enum xtlsrecordtype {
	XTLS_RECORD_CHANGE_CIPHER_SPEC = 20,
	XTLS_RECORD_ALERT = 21,
	XTLS_RECORD_HANDSHAKE = 22,
	XTLS_RECORD_APPLICATION_DATA = 23
} xtlsrecordtype;



#if defined(XRT_FEATURE_TLS_HANDSHAKE)

/* TLS 1.2 与 TLS 1.3 握手消息类型保留标准线路数值。 */
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



/* 常用 TLS 扩展类型使用 IANA 分配的线路数值。 */
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

#endif



#if defined(XRT_FEATURE_TLS_HELLO) || \
	defined(XRT_FEATURE_TLS_KEY_EXCHANGE)

/* 常用命名组保留 IANA 线路值，未知组仍可由 uint16 视图访问。 */
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

#endif



#if defined(XRT_FEATURE_TLS_HELLO)



/* TLS 1.2 与 TLS 1.3 常用签名方案保留 IANA 线路值。 */
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



/* TLS 游标把正常结束、读取到值和协议错误分开表达。 */
typedef enum xtlsitemresult {
	XTLS_ITEM_ERROR = -1,
	XTLS_ITEM_DONE = 0,
	XTLS_ITEM_VALUE = 1
} xtlsitemresult;

#endif



#if defined(XRT_FEATURE_TLS_MESSAGES)

/* KeyUpdate 请求值保留 TLS 1.3 线路数值。 */
typedef enum xtlskeyupdate {
	XTLS_KEY_UPDATE_NOT_REQUESTED = 0,
	XTLS_KEY_UPDATE_REQUESTED = 1
} xtlskeyupdate;

#endif



#if defined(XRT_FEATURE_TLS_AUTH_MESSAGES)

/* TLS 1.2 CertificateRequest 证书类型保留标准线路数值。 */
typedef enum xtlscertificatetype {
	XTLS_CERTIFICATE_RSA_SIGN = 1,
	XTLS_CERTIFICATE_DSS_SIGN = 2,
	XTLS_CERTIFICATE_ECDSA_SIGN = 64
} xtlscertificatetype;



/* CertificateStatus 当前标准化的正文类型是 OCSP。 */
typedef enum xtlscertificatestatustype {
	XTLS_CERTIFICATE_STATUS_OCSP = 1
} xtlscertificatestatustype;



/* TLS 1.3 压缩证书算法保留 RFC 8879 线路数值。 */
typedef enum xtlscertificatecompression {
	XTLS_CERTIFICATE_COMPRESSION_ZLIB = 1,
	XTLS_CERTIFICATE_COMPRESSION_BROTLI = 2,
	XTLS_CERTIFICATE_COMPRESSION_ZSTD = 3
} xtlscertificatecompression;

#endif



/* Alert 级别保留 TLS 线上的稳定数值。 */
typedef enum xtlsalertlevel {
	XTLS_ALERT_WARNING = 1,
	XTLS_ALERT_FATAL = 2
} xtlsalertlevel;



/* TLS 1.2 和 TLS 1.3 仍可在线路上出现的 Alert 描述。 */
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



/* TLS 错误码稳定描述失败发生的协议阶段。 */
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



/* 记录视图借用输入内存；EncodedSize 是头与负载的总长度。 */
typedef struct xtlsrecord {
	xtlsrecordtype Type;
	uint16 LegacyVersion;
	xbytesview Payload;
	size_t EncodedSize;
} xtlsrecord;



/* 密码套件元数据是进程期只读对象，尺寸字段都以字节计。 */
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



#if defined(XRT_FEATURE_TLS_KEY_EXCHANGE)

/* 命名组类型区分 Montgomery XDH 与未压缩短 Weierstrass ECDH 公钥。 */
typedef enum xtlsgroupkind {
	XTLS_GROUP_KIND_XDH = 1,
	XTLS_GROUP_KIND_ECDH
} xtlsgroupkind;



/* 命名组元数据是进程期只读对象，所有尺寸字段均以字节计。 */
typedef struct xtlsgroupinfo {
	uint16 Group;
	xtlsgroupkind Kind;
	uint16 PrivateSize;
	uint16 PublicSize;
	uint16 SharedSize;
} xtlsgroupinfo;

#endif



#if defined(XRT_FEATURE_TLS_HANDSHAKE)

/* 握手消息视图借用输入，EncodedSize 包含 4 字节头和正文。 */
typedef struct xtlshandshake {
	xtlshandshaketype Type;
	xbytesview Body;
	size_t EncodedSize;
} xtlshandshake;



/* 扩展视图借用输入，EncodedSize 包含类型、长度和扩展负载。 */
typedef struct xtlsextension {
	xtlsextensiontype Type;
	xbytesview Data;
	size_t EncodedSize;
} xtlsextension;

#endif



#if defined(XRT_FEATURE_TLS_HELLO)

/* 扩展游标借用完整扩展向量，并用小型精确桶检测重复类型。 */
typedef struct xtlsextensioncursor {
	xbytesview Data;
	size_t Offset;
	uint64 Seen[4];
} xtlsextensioncursor;



/* 16 位标识列表借用去除线路长度前缀后的偶数字节序列。 */
typedef struct xtlsids {
	xbytesview Data;
} xtlsids;



/* SNI 名称保留名称类型和借用的原始名称字节。 */
typedef struct xtlsservername {
	uint8 Type;
	xbytesview Name;
} xtlsservername;



/* SNI 名称游标在 256 种名称类型上精确检测重复。 */
typedef struct xtlsservernamecursor {
	xbytesview Data;
	size_t Offset;
	uint64 Seen[4];
} xtlsservernamecursor;



/* ALPN 游标借用去除 16 位列表长度后的 ProtocolNameList。 */
typedef struct xtlsprotocolcursor {
	xbytesview Data;
	size_t Offset;
} xtlsprotocolcursor;



/* TLS 1.3 密钥共享保留命名组和借用的线路公钥。 */
typedef struct xtlskeyshare {
	uint16 Group;
	xbytesview Key;
} xtlskeyshare;



/* ClientHello 密钥共享游标借用列表并检测重复命名组。 */
typedef struct xtlskeysharecursor {
	xbytesview Data;
	size_t Offset;
	uint64 Seen[4];
} xtlskeysharecursor;



/* ClientHello 视图只发布已经严格验证且恰好消费完整正文的字段。 */
typedef struct xtlsclienthello {
	uint16 LegacyVersion;
	xbytesview Random;
	xbytesview SessionId;
	xtlsids CipherSuites;
	xbytesview CompressionMethods;
	xbytesview Extensions;
} xtlsclienthello;



/* ServerHello 视图保留 HelloRetryRequest 标记和全部借用字段。 */
typedef struct xtlsserverhello {
	uint16 LegacyVersion;
	xbytesview Random;
	xbytesview SessionId;
	uint16 CipherSuite;
	uint8 CompressionMethod;
	xbytesview Extensions;
	bool Retry;
} xtlsserverhello;

#endif



#if defined(XRT_FEATURE_TLS_PSK)

/* TLS 1.3 PSK 密钥交换模式保留标准线路值。 */
typedef enum xtlspskmode {
	XTLS_PSK_KE = 0,
	XTLS_PSK_DHE_KE = 1
} xtlspskmode;



/* 一项客户端 PSK 同时借用 identity、混淆年龄和对应 binder。 */
typedef struct xtlspsk {
	xbytesview Identity;
	uint32 ObfuscatedAge;
	xbytesview Binder;
} xtlspsk;



/* PSK 游标同步遍历已经验证为等长的 identities 与 binders 列表。 */
typedef struct xtlspskcursor {
	xbytesview Identities;
	xbytesview Binders;
	size_t IdentityOffset;
	size_t BinderOffset;
} xtlspskcursor;

#endif



#if defined(XRT_FEATURE_TLS_NEGOTIATE)

/* 身份类型只描述握手签名密钥，不把协商层绑到 X.509 实现。 */
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



/* 签名方案元数据描述线路方案要求的密钥身份、摘要长度和协议版本范围。 */
typedef struct xtlssignatureinfo {
	xtlssignature Signature;
	xtlsidentitytype Identity;
	uint8 HashSize;
	xtlsversion Minimum;
	xtlsversion Maximum;
} xtlssignatureinfo;



/* 密钥共享策略可选择本地组优先级或避免 HelloRetryRequest。 */
typedef enum xtlskeysharepolicy {
	XTLS_KEY_SHARE_PREFER_GROUP = 0,
	XTLS_KEY_SHARE_PREFER_READY
} xtlskeysharepolicy;



/* 密钥共享选择成功时发布借用公钥；Retry 时 Key 为空。 */
typedef struct xtlskeyshareselection {
	xtlskeyshare Share;
	bool Retry;
} xtlskeyshareselection;

#endif



#if defined(XRT_FEATURE_TLS_POLICY)

/* TLS 策略借用有序偏好数组；默认初始化后的数组具有进程期生命周期。 */
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

#endif



#if defined(XRT_FEATURE_TLS_CONTEXT)

/* 默认队列上限容纳多个完整 TLS 记录，但不会按连接预分配。 */
#define XTLS_FEED_LIMIT_DEFAULT 262144u
#define XTLS_SEND_LIMIT_DEFAULT 262144u
#define XTLS_PLAIN_LIMIT_DEFAULT 262144u

/* 单次驱动预算限制一个连接连续占用事件循环的工作量。 */
#define XTLS_DRIVE_RECORD_BUDGET_DEFAULT 64u
#define XTLS_DRIVE_HANDSHAKE_BUDGET_DEFAULT 64u



typedef struct xtlscontext xtlscontext;



/* TLS 限制只描述硬上限与公平性预算，不会触发预分配。 */
typedef struct xtlslimits {
	size_t FeedLimit;
	size_t SendLimit;
	size_t PlainLimit;
	size_t HandshakeLimit;
	uint32 RecordBudget;
	uint32 HandshakeBudget;
} xtlslimits;



/* 创建配置借用可选策略；创建成功后上下文持有独立快照。 */
typedef struct xtlscontextconfig {
	const xtlspolicy* Policy;
	xtlslimits Limits;
} xtlscontextconfig;

#endif



#if defined(XRT_FEATURE_TLS_HELLO_WRITE)

/* TLS writer 直接使用调用方缓冲，并只在完整追加成功后推进 Size。 */
typedef struct xtlswriter {
	bytes Data;
	size_t Capacity;
	size_t Size;
} xtlswriter;

#endif



#if defined(XRT_FEATURE_TLS_HANDSHAKE_READER)

/* Reader 配置分开控制单消息硬上限和跨消息保留容量。 */
typedef struct xtlshandshakereaderconfig {
	size_t Limit;
	size_t Retain;
} xtlshandshakereaderconfig;



/* Reader 只在消息跨输入分片时渐进分配连续重组缓冲。 */
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

#endif



#if defined(XRT_FEATURE_TLS_MESSAGES)

/* Certificate 消息视图保留版本、请求上下文和完整条目向量。 */
typedef struct xtlscertificatemessage {
	xtlsversion Version;
	xbytesview RequestContext;
	xbytesview Entries;
} xtlscertificatemessage;



/* 证书条目借用 DER 数据；TLS 1.2 条目的 Extensions 为空。 */
typedef struct xtlscertificateentry {
	xbytesview Data;
	xbytesview Extensions;
} xtlscertificateentry;



/* 证书游标不限制链长度，也不复制证书或扩展。 */
typedef struct xtlscertificatecursor {
	xtlsversion Version;
	xbytesview Data;
	size_t Offset;
} xtlscertificatecursor;



/* CertificateVerify 视图保留未知签名方案和签名字节。 */
typedef struct xtlscertificateverify {
	uint16 Scheme;
	xbytesview Signature;
} xtlscertificateverify;



/* NewSessionTicket 统一暴露 TLS 1.2 与 TLS 1.3 字段。 */
typedef struct xtlssessionticket {
	xtlsversion Version;
	uint32 Lifetime;
	uint32 AgeAdd;
	xbytesview Nonce;
	xbytesview Ticket;
	xbytesview Extensions;
} xtlssessionticket;

#endif



#if defined(XRT_FEATURE_TLS_AUTH_MESSAGES)

/* 证书颁发者游标借用去除外层 16 位长度后的名称条目。 */
typedef struct xtlsauthoritycursor {
	xbytesview Data;
	size_t Offset;
} xtlsauthoritycursor;



/* TLS 1.2 CertificateRequest 保留证书类型、签名方案和完整颁发者向量。 */
typedef struct xtls12certificaterequest {
	xbytesview CertificateTypes;
	xtlsids Signatures;
	xbytesview AuthorityData;
} xtls12certificaterequest;



/* TLS 1.3 CertificateRequest 保留原始扩展和常用认证选择的便捷视图。 */
typedef struct xtls13certificaterequest {
	xbytesview RequestContext;
	xbytesview Extensions;
	xtlsids Signatures;
	xtlsids CertificateSignatures;
	xbytesview AuthorityData;
} xtls13certificaterequest;



/* TLS 1.2 ECDHE ServerKeyExchange 公开可直接参与验签的参数切片。 */
typedef struct xtls12serverkeyexchange {
	uint16 Group;
	xbytesview PublicKey;
	xbytesview Parameters;
	xtlscertificateverify Verify;
} xtls12serverkeyexchange;



/* CertificateStatus 保留状态类型和不透明响应。 */
typedef struct xtlscertificatestatusmessage {
	uint8 Type;
	xbytesview Response;
} xtlscertificatestatusmessage;



/* CompressedCertificate 只描述线路对象，解压和协商属于会话层。 */
typedef struct xtlscompressedcertificate {
	uint16 Algorithm;
	size_t UncompressedSize;
	xbytesview Data;
} xtlscompressedcertificate;

#endif



XRT_EXTERN_C_BEGIN



/* 返回协议版本的稳定英文名称，未知值返回 unknown。 */
XRT_API cstr xrtTlsVersionName(uint16 iVersion);



/* 返回密码套件的稳定英文名称，未知值返回 unknown。 */
XRT_API cstr xrtTlsCipherName(xtlscipher Cipher);



/* 返回只读密码套件元数据，未知套件返回空指针且不设置错误。 */
XRT_API const xtlscipherinfo* xrtTlsCipherInfo(xtlscipher Cipher);



#if defined(XRT_FEATURE_TLS_KEY_EXCHANGE)

/* 返回协议已知组的只读元数据；未知或未实现组返回空指针且不设置错误。 */
XRT_API const xtlsgroupinfo* xrtTlsGroupInfo(uint16 iGroup);



/* 判断命名组的密码后端是否已编译进当前 XRT。 */
XRT_API bool xrtTlsGroupAvailable(uint16 iGroup);



/* 为命名组生成临时私钥与线路公钥；输出容量可大于元数据要求。 */
XRT_API bool xrtTlsKeyShareGenerate(
	uint16 iGroup,
	void* pPrivate,
	size_t iPrivateCapacity,
	void* pPublic,
	size_t iPublicCapacity
);



/* 从精确长度的私钥和对端公钥派生共享秘密；输出容量可大于元数据要求。 */
XRT_API bool xrtTlsKeyShareDerive(
	uint16 iGroup,
	xbytesview Private,
	xbytesview PeerPublic,
	void* pShared,
	size_t iSharedCapacity
);

#endif



/* 返回记录内容类型的稳定英文名称，未知值返回 unknown。 */
XRT_API cstr xrtTlsRecordName(xtlsrecordtype Type);



/* 返回 Alert 的稳定英文名称，未知值返回 unknown_alert。 */
XRT_API cstr xrtTlsAlertName(xtlsalert Alert);



#if defined(XRT_FEATURE_TLS_HANDSHAKE)

/* 返回握手类型的稳定英文名称，未知线路值返回 unknown_handshake。 */
XRT_API cstr xrtTlsHandshakeName(xtlshandshaketype Type);



/* 返回扩展类型的稳定英文名称，未知线路值返回 unknown_extension。 */
XRT_API cstr xrtTlsExtensionName(xtlsextensiontype Type);



/* 返回给定正文所需的完整握手消息长度，越界时返回零。 */
XRT_API size_t xrtTlsHandshakeSize(size_t iBodySize);



/* 分片感知地解析输入开头的一条握手消息，仅成功时发布借用视图。 */
XRT_API xtlsresult xrtTlsHandshakeParse(
	xbytesview Input,
	xtlshandshake* pHandshake,
	size_t* pRequired
);



/* 把握手类型和正文编码到调用方缓冲，允许输入输出重叠。 */
XRT_API bool xrtTlsHandshakeEncode(
	xtlshandshaketype Type,
	xbytesview Body,
	void* pOutput,
	size_t iOutputSize
);



/* 返回 TLS 1.3 CertificateVerify 待签内容的精确长度。 */
XRT_API size_t xrtTls13CertificateVerifyContentSize(
	xtlsrole Signer,
	size_t iTranscriptHashSize
);



/* 编码 TLS 1.3 CertificateVerify 待签内容，供身份签名与独立验证复用。 */
XRT_API bool xrtTls13CertificateVerifyContentEncode(
	xtlsrole Signer,
	xbytesview TranscriptHash,
	void* pOutput,
	size_t iOutputSize
);



/* 返回给定负载所需的完整扩展长度，越界时返回零。 */
XRT_API size_t xrtTlsExtensionSize(size_t iDataSize);



/* 分片感知地解析输入开头的一个扩展，仅成功时发布借用视图。 */
XRT_API xtlsresult xrtTlsExtensionParse(
	xbytesview Input,
	xtlsextension* pExtension,
	size_t* pRequired
);



/* 把扩展类型和负载编码到调用方缓冲，允许输入输出重叠。 */
XRT_API bool xrtTlsExtensionEncode(
	xtlsextensiontype Type,
	xbytesview Data,
	void* pOutput,
	size_t iOutputSize
);

#endif



#if defined(XRT_FEATURE_TLS_HELLO)

/* 初始化借用完整扩展向量的游标，不预先扫描输入。 */
XRT_API bool xrtTlsExtensionsInit(
	xtlsextensioncursor* pCursor,
	xbytesview Extensions
);



/* 读取下一扩展并拒绝任何重复类型；失败时游标与输出保持不变。 */
XRT_API xtlsitemresult xrtTlsExtensionsRead(
	xtlsextensioncursor* pCursor,
	xtlsextension* pExtension
);



/* 完整验证扩展向量的 framing 与类型唯一性。 */
XRT_API bool xrtTlsExtensionsValidate(xbytesview Extensions);



/* 完整验证后查找唯一扩展，未找到返回 XTLS_ITEM_DONE。 */
XRT_API xtlsitemresult xrtTlsExtensionsFind(
	xbytesview Extensions,
	xtlsextensiontype Type,
	xtlsextension* pExtension
);



/* 返回 16 位标识列表的元素数量。 */
XRT_API size_t xrtTlsIdsCount(const xtlsids* pIds);



/* 按索引读取 16 位标识；越界时不修改输出。 */
XRT_API bool xrtTlsIdsGet(
	const xtlsids* pIds,
	size_t iIndex,
	uint16* pValue
);



/* 判断 16 位标识列表是否包含给定线路值。 */
XRT_API bool xrtTlsIdsContain(const xtlsids* pIds, uint16 iValue);



/* 严格解析 ClientHello supported_versions 扩展数据。 */
XRT_API bool xrtTlsClientVersions(
	xbytesview Data,
	xtlsids* pVersions
);



/* 严格解析 ServerHello selected_version 扩展数据。 */
XRT_API bool xrtTlsServerVersion(xbytesview Data, uint16* pVersion);



/* 严格解析 supported_groups 扩展数据。 */
XRT_API bool xrtTlsGroups(xbytesview Data, xtlsids* pGroups);



/* 严格解析 signature_algorithms 类扩展数据。 */
XRT_API bool xrtTlsSignatures(xbytesview Data, xtlsids* pSignatures);



/* 严格解析 SNI 名称列表并把游标重置到首项。 */
XRT_API bool xrtTlsServerNames(
	xbytesview Data,
	xtlsservernamecursor* pCursor
);



/* 读取下一 SNI 名称；失败时游标与输出保持不变。 */
XRT_API xtlsitemresult xrtTlsServerNamesRead(
	xtlsservernamecursor* pCursor,
	xtlsservername* pName
);



/* 查找 SNI host_name；没有该名称类型时返回 XTLS_ITEM_DONE。 */
XRT_API xtlsitemresult xrtTlsHostName(
	xbytesview Data,
	xbytesview* pHost
);



/* 严格解析 ALPN ProtocolNameList 并把游标重置到首项。 */
XRT_API bool xrtTlsProtocols(
	xbytesview Data,
	xtlsprotocolcursor* pCursor
);



/* 读取下一项非空 ALPN 协议名称。 */
XRT_API xtlsitemresult xrtTlsProtocolsRead(
	xtlsprotocolcursor* pCursor,
	xbytesview* pProtocol
);



/* 严格读取服务端必须唯一选择的 ALPN 协议。 */
XRT_API bool xrtTlsProtocolSelected(
	xbytesview Data,
	xbytesview* pProtocol
);



/* 在完整 ALPN 列表中查找一个不透明协议名称。 */
XRT_API xtlsitemresult xrtTlsProtocolFind(
	xbytesview Data,
	xbytesview Protocol
);



/* 按 Preferred 的顺序选择双方 ALPN 列表的第一个交集。 */
XRT_API xtlsitemresult xrtTlsProtocolSelect(
	xbytesview Offered,
	xbytesview Preferred,
	xbytesview* pProtocol
);



/* 严格解析 ClientHello key_share 列表并把游标重置到首项。 */
XRT_API bool xrtTlsClientKeyShares(
	xbytesview Data,
	xtlskeysharecursor* pCursor
);



/* 读取下一项非空且命名组唯一的客户端密钥共享。 */
XRT_API xtlsitemresult xrtTlsKeySharesRead(
	xtlskeysharecursor* pCursor,
	xtlskeyshare* pShare
);



/* 严格解析普通 ServerHello 中唯一的 key_share。 */
XRT_API bool xrtTlsServerKeyShare(
	xbytesview Data,
	xtlskeyshare* pShare
);



/* 严格解析 HelloRetryRequest 中仅含命名组的 key_share。 */
XRT_API bool xrtTlsRetryGroup(xbytesview Data, uint16* pGroup);



/* 严格解析 HelloRetryRequest cookie 的 16 位非空字节向量。 */
XRT_API bool xrtTlsRetryCookie(xbytesview Data, xbytesview* pCookie);



/* 严格解析一条 ClientHello 正文并发布零拷贝字段视图。 */
XRT_API bool xrtTlsClientHelloParse(
	xbytesview Body,
	xtlsclienthello* pHello
);



/* 严格解析一条 ServerHello 或 HelloRetryRequest 正文。 */
XRT_API bool xrtTlsServerHelloParse(
	xbytesview Body,
	xtlsserverhello* pHello
);

#endif



#if defined(XRT_FEATURE_TLS_PSK)

/* 严格解析非空且不重复的 PSK 密钥交换模式列表。 */
XRT_API bool xrtTlsPskModes(xbytesview Data, xbytesview* pModes);



/* 严格解析 ClientHello PSK 列表并重置同步游标。 */
XRT_API bool xrtTlsClientPsks(
	xbytesview Data,
	xtlspskcursor* pCursor
);



/* 同步读取下一项 identity、混淆年龄和 binder。 */
XRT_API xtlsitemresult xrtTlsPsksRead(
	xtlspskcursor* pCursor,
	xtlspsk* pPsk
);



/* 严格解析 ServerHello 选择的 PSK identity 索引。 */
XRT_API bool xrtTlsServerPsk(xbytesview Data, uint16* pSelected);

#endif



#if defined(XRT_FEATURE_TLS_NEGOTIATE)

/* 按本地偏好顺序选择 16 位标识交集，未知线路值保持可扩展。 */
XRT_API xtlsitemresult xrtTlsIdsSelect(
	const xtlsids* pOffered,
	const uint16* pPreferred,
	size_t iPreferredCount,
	uint16* pSelected
);



/* 从完整客户端 key_share 扩展负载查找指定组的借用公钥。 */
XRT_API xtlsitemresult xrtTlsKeyShareFind(
	xbytesview KeyShares,
	uint16 iGroup,
	xtlskeyshare* pShare
);



/* 按本地版本偏好选择 supported_versions 中的第一个交集。 */
XRT_API xtlsitemresult xrtTlsVersionSelect(
	const xtlsids* pOffered,
	const xtlsversion* pPreferred,
	size_t iPreferredCount,
	xtlsversion* pSelected
);



/* 从 ClientHello 选择版本，并在扩展缺失时只允许 TLS 1.2。 */
XRT_API xtlsitemresult xrtTlsClientVersionSelect(
	const xtlsclienthello* pHello,
	const xtlsversion* pPreferred,
	size_t iPreferredCount,
	xtlsversion* pSelected
);



/* 判断密码套件能否用于指定版本和握手身份。 */
XRT_API bool xrtTlsCipherCompatible(
	xtlsversion Version,
	xtlscipher Cipher,
	xtlsidentitytype Identity
);



/* 按本地偏好选择版本、身份和对端都接受的密码套件。 */
XRT_API xtlsitemresult xrtTlsCipherSelect(
	xtlsversion Version,
	const xtlsids* pOffered,
	xtlsidentitytype Identity,
	const xtlscipher* pPreferred,
	size_t iPreferredCount,
	xtlscipher* pSelected
);



/* 返回签名方案的只读元数据；未知线路值返回 NULL 且不设置错误。 */
XRT_API const xtlssignatureinfo* xrtTlsSignatureInfo(
	xtlssignature Signature
);



/* 判断签名方案能否用于指定版本和握手身份。 */
XRT_API bool xrtTlsSignatureCompatible(
	xtlsversion Version,
	xtlssignature Signature,
	xtlsidentitytype Identity
);



/* 按本地偏好选择版本、身份和对端都接受的握手签名方案。 */
XRT_API xtlsitemresult xrtTlsSignatureSelect(
	xtlsversion Version,
	const xtlsids* pOffered,
	xtlsidentitytype Identity,
	const xtlssignature* pPreferred,
	size_t iPreferredCount,
	xtlssignature* pSelected
);



/* 选择可直接使用或需要 HelloRetryRequest 的共同密钥共享组。 */
XRT_API xtlsitemresult xrtTlsKeyShareSelect(
	const xtlsids* pGroups,
	xbytesview KeyShares,
	const uint16* pPreferred,
	size_t iPreferredCount,
	xtlskeysharepolicy Policy,
	xtlskeyshareselection* pSelection
);

#endif



#if defined(XRT_FEATURE_TLS_POLICY)

/* 初始化覆盖 TLS 1.3/1.2 的通用安全偏好；所有数组都可由调用方替换。 */
XRT_API void xrtTlsPolicyInit(xtlspolicy* pPolicy);



/* 验证偏好指针、唯一性、已知线路值和跨字段可用性。 */
XRT_API bool xrtTlsPolicyValid(const xtlspolicy* pPolicy);

#endif



#if defined(XRT_FEATURE_TLS_CONTEXT)

/* 初始化适合通用客户端和服务端的有界队列与驱动预算。 */
XRT_API void xrtTlsLimitsInit(xtlslimits* pLimits);



/* 验证队列至少能接收一个最大记录，并限制单条握手消息。 */
XRT_API bool xrtTlsLimitsValid(const xtlslimits* pLimits);



/* 初始化使用默认策略和默认限制的上下文配置。 */
XRT_API void xrtTlsContextConfigInit(xtlscontextconfig* pConfig);



/* 创建可跨线程共享的只读 TLS 配置快照。 */
XRT_API xtlscontext* xrtTlsContextCreate(
	const xtlscontextconfig* pConfig
);



/* 增加上下文引用；会话必须为其借用的上下文持有一个引用。 */
XRT_API xtlscontext* xrtTlsContextRetain(
	const xtlscontext* pContext
);



/* 释放一个上下文引用，空指针无操作。 */
XRT_API void xrtTlsContextRelease(xtlscontext* pContext);



/* 返回生命周期不超过上下文的只读策略快照。 */
XRT_API const xtlspolicy* xrtTlsContextPolicy(
	const xtlscontext* pContext
);



/* 返回生命周期不超过上下文的只读限制快照。 */
XRT_API const xtlslimits* xrtTlsContextLimits(
	const xtlscontext* pContext
);

#endif



#if defined(XRT_FEATURE_TLS_HANDSHAKE_READER)

/* 填充安全的默认握手 reader 配置。 */
XRT_API void xrtTlsHandshakeReaderConfigInit(
	xtlshandshakereaderconfig* pConfig
);



/* 初始化 reader；Config 为空时使用默认上限与保留容量。 */
XRT_API bool xrtTlsHandshakeReaderInit(
	xtlshandshakereader* pReader,
	const xtlshandshakereaderconfig* pConfig
);



/* 释放 reader 持有的重组缓冲并清零结构。 */
XRT_API void xrtTlsHandshakeReaderUnit(
	xtlshandshakereader* pReader
);



/* 丢弃当前消息，保留不超过配置阈值的缓冲。 */
XRT_API bool xrtTlsHandshakeReaderReset(
	xtlshandshakereader* pReader
);



/* 返回完成当前消息所需的完整编码长度；只有部分头时返回 4。 */
XRT_API size_t xrtTlsHandshakeReaderRequired(
	const xtlshandshakereader* pReader
);



/*
	读取至多一条握手消息并返回本次消费的输入字节数。
	完整单片消息直接借用 Input；跨分片消息借用 Reader，直到下次 Read、Reset 或 Unit。
*/
XRT_API xtlsresult xrtTlsHandshakeReaderRead(
	xtlshandshakereader* pReader,
	xbytesview Input,
	size_t* pConsumed,
	xtlshandshake* pMessage
);

#endif



#if defined(XRT_FEATURE_TLS_MESSAGES)

/* 严格解析 TLS 1.2 或 TLS 1.3 Certificate 正文并验证全部条目。 */
XRT_API bool xrtTlsCertificateParse(
	xtlsversion Version,
	xbytesview Body,
	xtlscertificatemessage* pMessage
);



/* 从已验证的 Certificate 消息初始化零拷贝证书游标。 */
XRT_API bool xrtTlsCertificateEntries(
	const xtlscertificatemessage* pMessage,
	xtlscertificatecursor* pCursor
);



/* 读取下一证书条目；结束、值和错误使用三态结果区分。 */
XRT_API xtlsitemresult xrtTlsCertificatesRead(
	xtlscertificatecursor* pCursor,
	xtlscertificateentry* pEntry
);



/* 严格解析 TLS 1.3 EncryptedExtensions 正文。 */
XRT_API bool xrtTlsEncryptedExtensionsParse(
	xbytesview Body,
	xbytesview* pExtensions
);



/* 严格解析 CertificateVerify 的方案与非空签名。 */
XRT_API bool xrtTlsCertificateVerifyParse(
	xbytesview Body,
	xtlscertificateverify* pVerify
);



/* 按调用方给出的协商长度严格解析 Finished 验证数据。 */
XRT_API bool xrtTlsFinishedParse(
	xbytesview Body,
	size_t iExpectedSize,
	xbytesview* pVerifyData
);



/* 严格解析 TLS 1.3 单字节 KeyUpdate 请求。 */
XRT_API bool xrtTlsKeyUpdateParse(
	xbytesview Body,
	xtlskeyupdate* pRequest
);



/* 严格解析版本对应的 NewSessionTicket 正文。 */
XRT_API bool xrtTlsSessionTicketParse(
	xtlsversion Version,
	xbytesview Body,
	xtlssessionticket* pTicket
);

#endif



#if defined(XRT_FEATURE_TLS_AUTH_MESSAGES)

/* 严格解析带 16 位总长的证书颁发者名称向量并初始化游标。 */
XRT_API bool xrtTlsAuthorities(
	xbytesview Data,
	xtlsauthoritycursor* pCursor
);



/* 读取下一项非空 DER DistinguishedName。 */
XRT_API xtlsitemresult xrtTlsAuthoritiesRead(
	xtlsauthoritycursor* pCursor,
	xbytesview* pName
);



/* 严格解析 TLS 1.2 CertificateRequest 正文。 */
XRT_API bool xrtTls12CertificateRequestParse(
	xbytesview Body,
	xtls12certificaterequest* pRequest
);



/* 严格解析 TLS 1.3 CertificateRequest 正文和认证扩展。 */
XRT_API bool xrtTls13CertificateRequestParse(
	xbytesview Body,
	xtls13certificaterequest* pRequest
);



/* 严格解析 TLS 1.2 ECDHE ServerKeyExchange 正文。 */
XRT_API bool xrtTls12ServerKeyExchangeParse(
	xbytesview Body,
	xtls12serverkeyexchange* pExchange
);



/* 严格解析 TLS 1.2 ECDHE ClientKeyExchange 公钥。 */
XRT_API bool xrtTls12ClientKeyExchangeParse(
	xbytesview Body,
	xbytesview* pPublicKey
);



/* 严格解析 OCSP CertificateStatus 正文。 */
XRT_API bool xrtTlsCertificateStatusParse(
	xbytesview Body,
	xtlscertificatestatusmessage* pStatus
);



/* 严格解析 TLS 1.3 CompressedCertificate 正文。 */
XRT_API bool xrtTlsCompressedCertificateParse(
	xbytesview Body,
	xtlscompressedcertificate* pCertificate
);

#endif



#if defined(XRT_FEATURE_TLS_HELLO_WRITE)

/* 初始化一个空的调用方缓冲 writer。 */
XRT_API bool xrtTlsWriterInit(
	xtlswriter* pWriter,
	void* pData,
	size_t iCapacity
);



/* 清空 writer 的逻辑内容，不擦除调用方缓冲。 */
XRT_API bool xrtTlsWriterReset(xtlswriter* pWriter);



/* 返回 writer 已完成区域的借用视图。 */
XRT_API xbytesview xrtTlsWriterData(const xtlswriter* pWriter);



/* 失败原子地追加一个原始扩展，负载允许与 writer 缓冲重叠。 */
XRT_API bool xrtTlsWriterExtension(
	xtlswriter* pWriter,
	xtlsextensiontype Type,
	xbytesview Data
);



/* 追加只包含一个 host_name 的 SNI 扩展。 */
XRT_API bool xrtTlsWriterHostName(
	xtlswriter* pWriter,
	xbytesview Host
);



/* 追加完整 ALPN 协议列表；协议名称按输入顺序保留。 */
XRT_API bool xrtTlsWriterProtocols(
	xtlswriter* pWriter,
	const xbytesview* pProtocols,
	size_t iCount
);



#if defined(XRT_FEATURE_TLS_PSK_WRITE)

/* 追加非空且不重复的 PSK 密钥交换模式列表。 */
XRT_API bool xrtTlsWriterPskModes(
	xtlswriter* pWriter,
	const uint8* pModes,
	size_t iCount
);



/* 追加 ClientHello PSK identities 与等量 binders，调用方保证它是末项。 */
XRT_API bool xrtTlsWriterClientPsks(
	xtlswriter* pWriter,
	const xtlspsk* pPsks,
	size_t iCount
);



/* 追加 ServerHello 选择的单一 PSK identity 索引。 */
XRT_API bool xrtTlsWriterServerPsk(
	xtlswriter* pWriter,
	uint16 iSelected
);

#endif



/* 追加带 16 位长度前缀的标识列表扩展，适用于组和签名方案。 */
XRT_API bool xrtTlsWriterIds(
	xtlswriter* pWriter,
	xtlsextensiontype Type,
	const uint16* pValues,
	size_t iCount
);



/* 追加 ClientHello supported_versions 扩展。 */
XRT_API bool xrtTlsWriterClientVersions(
	xtlswriter* pWriter,
	const uint16* pVersions,
	size_t iCount
);



/* 追加 ServerHello 选择单一版本的 supported_versions 扩展。 */
XRT_API bool xrtTlsWriterServerVersion(
	xtlswriter* pWriter,
	uint16 iVersion
);



/* 追加 ClientHello key_share 扩展，允许写出空列表以请求 Retry。 */
XRT_API bool xrtTlsWriterClientKeyShares(
	xtlswriter* pWriter,
	const xtlskeyshare* pShares,
	size_t iCount
);



/* 追加普通 ServerHello 的单个 key_share 扩展。 */
XRT_API bool xrtTlsWriterServerKeyShare(
	xtlswriter* pWriter,
	const xtlskeyshare* pShare
);



/* 追加 HelloRetryRequest 选择组形式的 key_share 扩展。 */
XRT_API bool xrtTlsWriterRetryGroup(
	xtlswriter* pWriter,
	uint16 iGroup
);



/* 追加 HelloRetryRequest 或 ClientHello 使用的非空 cookie 扩展。 */
XRT_API bool xrtTlsWriterRetryCookie(
	xtlswriter* pWriter,
	xbytesview Cookie
);



/* 返回编码 ClientHello 正文所需的精确长度。 */
XRT_API size_t xrtTlsClientHelloSize(const xtlsclienthello* pHello);



/* 失败原子地编码 ClientHello 正文；输入字段不得与输出区域重叠。 */
XRT_API bool xrtTlsClientHelloEncode(
	const xtlsclienthello* pHello,
	void* pOutput,
	size_t iOutputSize
);



/* 返回编码 ServerHello 或 HelloRetryRequest 正文所需的精确长度。 */
XRT_API size_t xrtTlsServerHelloSize(const xtlsserverhello* pHello);



/* 失败原子地编码 ServerHello 正文；输入字段不得与输出区域重叠。 */
XRT_API bool xrtTlsServerHelloEncode(
	const xtlsserverhello* pHello,
	void* pOutput,
	size_t iOutputSize
);

#endif



#if defined(XRT_FEATURE_TLS_MESSAGES_WRITE)

/* 返回编码 Certificate 正文所需长度，非法输入返回零。 */
XRT_API size_t xrtTlsCertificateSize(
	xtlsversion Version,
	xbytesview RequestContext,
	const xtlscertificateentry* pEntries,
	size_t iCount
);



/* 失败原子地编码完整 Certificate 正文。 */
XRT_API bool xrtTlsCertificateEncode(
	xtlsversion Version,
	xbytesview RequestContext,
	const xtlscertificateentry* pEntries,
	size_t iCount,
	void* pOutput,
	size_t iOutputSize
);



/* 返回编码 EncryptedExtensions 正文所需长度。 */
XRT_API size_t xrtTlsEncryptedExtensionsSize(xbytesview Extensions);



/* 失败原子地编码 EncryptedExtensions 正文。 */
XRT_API bool xrtTlsEncryptedExtensionsEncode(
	xbytesview Extensions,
	void* pOutput,
	size_t iOutputSize
);



/* 返回编码 CertificateVerify 正文所需长度。 */
XRT_API size_t xrtTlsCertificateVerifySize(
	const xtlscertificateverify* pVerify
);



/* 失败原子地编码 CertificateVerify 正文。 */
XRT_API bool xrtTlsCertificateVerifyEncode(
	const xtlscertificateverify* pVerify,
	void* pOutput,
	size_t iOutputSize
);



/* 编码非空 Finished 验证数据正文。 */
XRT_API bool xrtTlsFinishedEncode(
	xbytesview VerifyData,
	void* pOutput,
	size_t iOutputSize
);



/* 编码 TLS 1.3 单字节 KeyUpdate 请求。 */
XRT_API bool xrtTlsKeyUpdateEncode(
	xtlskeyupdate Request,
	void* pOutput,
	size_t iOutputSize
);



/* 返回编码 NewSessionTicket 正文所需长度。 */
XRT_API size_t xrtTlsSessionTicketSize(
	const xtlssessionticket* pTicket
);



/* 失败原子地编码版本对应的 NewSessionTicket 正文。 */
XRT_API bool xrtTlsSessionTicketEncode(
	const xtlssessionticket* pTicket,
	void* pOutput,
	size_t iOutputSize
);

#endif



#if defined(XRT_FEATURE_TLS_AUTH_MESSAGES_WRITE)

/* 返回编码证书颁发者名称向量所需长度。 */
XRT_API size_t xrtTlsAuthoritiesSize(
	const xbytesview* pNames,
	size_t iCount
);



/* 失败原子地编码带 16 位总长的证书颁发者名称向量。 */
XRT_API bool xrtTlsAuthoritiesEncode(
	const xbytesview* pNames,
	size_t iCount,
	void* pOutput,
	size_t iOutputSize
);



/* 返回编码 TLS 1.2 CertificateRequest 正文所需长度。 */
XRT_API size_t xrtTls12CertificateRequestSize(
	const xtls12certificaterequest* pRequest
);



/* 失败原子地编码 TLS 1.2 CertificateRequest 正文。 */
XRT_API bool xrtTls12CertificateRequestEncode(
	const xtls12certificaterequest* pRequest,
	void* pOutput,
	size_t iOutputSize
);



/* 返回编码 TLS 1.3 CertificateRequest 正文所需长度。 */
XRT_API size_t xrtTls13CertificateRequestSize(
	xbytesview RequestContext,
	xbytesview Extensions
);



/* 失败原子地编码 TLS 1.3 CertificateRequest 正文。 */
XRT_API bool xrtTls13CertificateRequestEncode(
	xbytesview RequestContext,
	xbytesview Extensions,
	void* pOutput,
	size_t iOutputSize
);



/* 返回编码 TLS 1.2 ECDHE ServerKeyExchange 正文所需长度。 */
XRT_API size_t xrtTls12ServerKeyExchangeSize(
	uint16 iGroup,
	xbytesview PublicKey,
	const xtlscertificateverify* pVerify
);



/* 失败原子地编码 TLS 1.2 ECDHE ServerKeyExchange 正文。 */
XRT_API bool xrtTls12ServerKeyExchangeEncode(
	uint16 iGroup,
	xbytesview PublicKey,
	const xtlscertificateverify* pVerify,
	void* pOutput,
	size_t iOutputSize
);



/* 返回编码 TLS 1.2 ECDHE ClientKeyExchange 正文所需长度。 */
XRT_API size_t xrtTls12ClientKeyExchangeSize(xbytesview PublicKey);



/* 编码 TLS 1.2 ECDHE ClientKeyExchange，允许公钥与输出重叠。 */
XRT_API bool xrtTls12ClientKeyExchangeEncode(
	xbytesview PublicKey,
	void* pOutput,
	size_t iOutputSize
);



/* 返回编码 OCSP CertificateStatus 正文所需长度。 */
XRT_API size_t xrtTlsCertificateStatusSize(
	const xtlscertificatestatusmessage* pStatus
);



/* 编码 OCSP CertificateStatus，允许响应与输出重叠。 */
XRT_API bool xrtTlsCertificateStatusEncode(
	const xtlscertificatestatusmessage* pStatus,
	void* pOutput,
	size_t iOutputSize
);



/* 返回编码 CompressedCertificate 正文所需长度。 */
XRT_API size_t xrtTlsCompressedCertificateSize(
	const xtlscompressedcertificate* pCertificate
);



/* 编码 CompressedCertificate，允许压缩数据与输出重叠。 */
XRT_API bool xrtTlsCompressedCertificateEncode(
	const xtlscompressedcertificate* pCertificate,
	void* pOutput,
	size_t iOutputSize
);

#endif



/* 返回给定负载所需的完整记录长度，负载越界时返回零并设置错误。 */
XRT_API size_t xrtTlsRecordSize(size_t iPayloadSize);



/*
	解析输入开头的一条完整记录。
	输入不足返回 XTLS_AGAIN，Required 返回继续解析所需的总字节数；
	只有返回 XTLS_OK 时才写入 Record。
*/
XRT_API xtlsresult xrtTlsRecordParse(
	xbytesview Input,
	xtlsrecord* pRecord,
	size_t* pRequired
);



/* 把一条记录编码到调用方缓冲；输入与输出允许重叠。 */
XRT_API bool xrtTlsRecordEncode(
	xtlsrecordtype Type,
	uint16 iLegacyVersion,
	xbytesview Payload,
	ptr pOutput,
	size_t iOutputSize
);



/* 解析恰好一个两字节 Alert 负载。 */
XRT_API bool xrtTlsAlertParse(
	xbytesview Payload,
	xtlsalertlevel* pLevel,
	xtlsalert* pAlert
);



/* 编码一个两字节 Alert 负载。 */
XRT_API bool xrtTlsAlertEncode(
	xtlsalertlevel Level,
	xtlsalert Alert,
	void* pOutput,
	size_t iOutputSize
);



XRT_EXTERN_C_END

#endif

#endif
