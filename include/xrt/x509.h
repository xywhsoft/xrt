#ifndef XRT_X509_H
#define XRT_X509_H

#include <xrt/asn1.h>
#include <xrt/time.h>

#if defined(XRT_FEATURE_X509_STORE)
	#include <xrt/pem.h>
#endif



#if defined(XRT_FEATURE_X509_PARSE) && \
	(!defined(XRT_FEATURE_ASN1_DER) || !defined(XRT_FEATURE_TIME))
	#error "XRT X.509 parsing requires ASN.1 DER and time support"
#endif

#if defined(XRT_FEATURE_X509_PROFILE) && !defined(XRT_FEATURE_X509_PARSE)
	#error "XRT X.509 profile support requires XRT_FEATURE_X509_PARSE"
#endif

#if defined(XRT_FEATURE_X509_CRL) && !defined(XRT_FEATURE_X509_PARSE)
	#error "XRT X.509 CRL support requires XRT_FEATURE_X509_PARSE"
#endif

#if defined(XRT_FEATURE_X509_DISTRIBUTION) && \
	!defined(XRT_FEATURE_X509_PROFILE)
	#error "XRT X.509 distribution points require certificate profile support"
#endif

#if defined(XRT_FEATURE_X509_CRL_PROFILE) && \
	(!defined(XRT_FEATURE_X509_CRL) || \
	 !defined(XRT_FEATURE_X509_DISTRIBUTION))
	#error "XRT X.509 CRL profile support requires CRL and distribution support"
#endif

#if defined(XRT_FEATURE_X509_CRL_VERIFY) && \
	(!defined(XRT_FEATURE_X509_CRL) || !defined(XRT_FEATURE_X509_VERIFY))
	#error "XRT X.509 CRL verification requires CRL and signature verification support"
#endif

#if defined(XRT_FEATURE_X509_CRL_POLICY) && \
	(!defined(XRT_FEATURE_X509_CRL_PROFILE) || \
	 !defined(XRT_FEATURE_X509_CRL_VERIFY) || \
	 !defined(XRT_FEATURE_X509_NAME))
	#error "XRT X.509 CRL policy requires CRL profile, verification and Name support"
#endif

#if defined(XRT_FEATURE_X509_NAME) && \
	(!defined(XRT_FEATURE_X509_PARSE) || !defined(XRT_FEATURE_UNICODE))
	#error "XRT X.509 Name support requires parsing and Unicode support"
#endif

#if defined(XRT_FEATURE_X509_NAME_CONSTRAINTS) && \
	(!defined(XRT_FEATURE_X509_PROFILE) || \
	 !defined(XRT_FEATURE_X509_NAME))
	#error "XRT X.509 NameConstraints require profile and Name support"
#endif

#if defined(XRT_FEATURE_X509_SIGNATURE) && !defined(XRT_FEATURE_X509_PARSE)
	#error "XRT X.509 signature support requires XRT_FEATURE_X509_PARSE"
#endif

#if defined(XRT_FEATURE_X509_DIGEST) && \
	(!defined(XRT_FEATURE_X509_SIGNATURE) || \
	 !defined(XRT_FEATURE_CRYPTO_SHA1) || \
	 !defined(XRT_FEATURE_CRYPTO_SHA224) || \
	 !defined(XRT_FEATURE_CRYPTO_SHA256) || \
	 !defined(XRT_FEATURE_CRYPTO_SHA512))
	#error "XRT X.509 digest support requires signature and SHA-1/SHA-2 support"
#endif

#if defined(XRT_FEATURE_X509_VERIFY) && !defined(XRT_FEATURE_X509_SIGNATURE)
	#error "XRT X.509 verification requires XRT_FEATURE_X509_SIGNATURE"
#endif

#if defined(XRT_FEATURE_X509_VERIFY_RSA) && \
	(!defined(XRT_FEATURE_X509_VERIFY) || \
	 !defined(XRT_FEATURE_X509_DIGEST) || \
	 !defined(XRT_FEATURE_CRYPTO_RSA_PSS) || \
	 !defined(XRT_FEATURE_CRYPTO_RSA_PKCS1))
	#error "XRT X.509 RSA verification dependencies are incomplete"
#endif

#if defined(XRT_FEATURE_X509_VERIFY_ECDSA) && \
	(!defined(XRT_FEATURE_X509_VERIFY) || \
	 !defined(XRT_FEATURE_X509_DIGEST) || \
	 !defined(XRT_FEATURE_CRYPTO_ECDSA_P256_DER) || \
	 !defined(XRT_FEATURE_CRYPTO_ECDSA_P384_DER))
	#error "XRT X.509 ECDSA verification dependencies are incomplete"
#endif

#if defined(XRT_FEATURE_X509_VERIFY_ED25519) && \
	(!defined(XRT_FEATURE_X509_VERIFY) || \
	 !defined(XRT_FEATURE_CRYPTO_ED25519_VERIFY))
	#error "XRT X.509 Ed25519 verification dependencies are incomplete"
#endif

#if defined(XRT_FEATURE_X509_PATH) && \
	(!defined(XRT_FEATURE_X509_NAME_CONSTRAINTS) || \
	 !defined(XRT_FEATURE_X509_VERIFY))
	#error "XRT X.509 path support requires NameConstraints and verification support"
#endif

#if defined(XRT_FEATURE_X509_PATH_BUILD) && !defined(XRT_FEATURE_X509_PATH)
	#error "XRT X.509 path building requires XRT_FEATURE_X509_PATH"
#endif

#if defined(XRT_FEATURE_X509_STORE) && \
	(!defined(XRT_FEATURE_X509_PATH_BUILD) || !defined(XRT_FEATURE_PEM))
	#error "XRT X.509 store support requires path building and PEM support"
#endif

#if defined(XRT_FEATURE_X509_STORE_FILE) && \
	(!defined(XRT_FEATURE_X509_STORE) || !defined(XRT_FEATURE_FILE_WHOLE))
	#error "XRT X.509 file store support requires store and whole-file support"
#endif

#if defined(XRT_FEATURE_X509_STORE_SYSTEM) && !defined(XRT_FEATURE_X509_STORE)
	#error "XRT X.509 system store support requires XRT_FEATURE_X509_STORE"
#endif

#if defined(XRT_FEATURE_X509_STORE_SYSTEM) && \
	!defined(_WIN32) && !defined(_WIN64) && !defined(__APPLE__) && \
	(!defined(XRT_FEATURE_X509_STORE_FILE) || !defined(XRT_FEATURE_DIR))
	#error "XRT Unix system store support requires file-store and directory support"
#endif

#if defined(XRT_FEATURE_X509_IDENTITY) && \
	(!defined(XRT_FEATURE_X509_PROFILE) || !defined(XRT_FEATURE_NET))
	#error "XRT X.509 identity support requires X.509 profile and network address support"
#endif



#if defined(XRT_FEATURE_X509_PARSE)

/* X.509 版本使用规范文档中的自然编号。 */
typedef enum xx509version {
	X509_VERSION_1 = 1,
	X509_VERSION_2,
	X509_VERSION_3
} xx509version;



/* X.509 游标把正常结束和结构错误分开表达。 */
typedef enum xx509result {
	X509_ERROR = -1,
	X509_DONE = 0,
	X509_VALUE = 1
} xx509result;



/* X.509 公钥类型独立于当前加密后端支持范围。 */
typedef enum xx509keytype {
	X509_KEY_UNKNOWN = 0,
	X509_KEY_RSA,
	X509_KEY_RSA_PSS,
	X509_KEY_EC,
	X509_KEY_ED25519,
	X509_KEY_ED448,
	X509_KEY_X25519,
	X509_KEY_X448
} xx509keytype;



/* 已知命名曲线；未知曲线仍通过 Algorithm 和 Key 原始视图暴露。 */
typedef enum xx509curve {
	X509_CURVE_UNKNOWN = 0,
	X509_CURVE_P256,
	X509_CURVE_P384,
	X509_CURVE_P521
} xx509curve;



/* X.509 模块稳定错误码。 */
typedef enum xx509error {
	X509_ERROR_DER = 1,
	X509_ERROR_CERTIFICATE,
	X509_ERROR_VERSION,
	X509_ERROR_SERIAL,
	X509_ERROR_ALGORITHM,
	X509_ERROR_NAME,
	X509_ERROR_TIME,
	X509_ERROR_PUBLIC_KEY,
	X509_ERROR_EXTENSION,
	X509_ERROR_DUPLICATE_EXTENSION,
	X509_ERROR_SIGNATURE,
	X509_ERROR_NOT_FOUND,
	X509_ERROR_GENERAL_NAME,
	X509_ERROR_OID_LIST,
	X509_ERROR_KEY_USAGE,
	X509_ERROR_BASIC_CONSTRAINTS,
	X509_ERROR_EXTENDED_KEY_USAGE,
	X509_ERROR_KEY_IDENTIFIER,
	X509_ERROR_DNS_NAME,
	X509_ERROR_IDENTITY,
	X509_ERROR_SIGNATURE_ALGORITHM,
	X509_ERROR_TRUST_ANCHOR,
	X509_ERROR_ISSUER,
	X509_ERROR_PATH,
	X509_ERROR_PATH_CONSTRAINT,
	X509_ERROR_CRITICAL_EXTENSION,
	X509_ERROR_PURPOSE,
	X509_ERROR_PATH_BUILD,
	X509_ERROR_TRUST_STORE,
	X509_ERROR_TRUST_STORE_FILE,
	X509_ERROR_TRUST_STORE_SYSTEM,
	X509_ERROR_CRL,
	X509_ERROR_CRL_ENTRY,
	X509_ERROR_CRL_NUMBER,
	X509_ERROR_CRL_ISSUING_POINT,
	X509_ERROR_CRL_REASON,
	X509_ERROR_CRL_INVALIDITY_DATE,
	X509_ERROR_CRL_CERTIFICATE_ISSUER,
	X509_ERROR_DISTRIBUTION_POINT,
	X509_ERROR_CRL_POLICY,
	X509_ERROR_CRL_SCOPE,
	X509_ERROR_CRL_DELTA,
	X509_ERROR_REVOCATION,
	X509_ERROR_NAME_CONSTRAINTS
} xx509error;



/* AlgorithmIdentifier 的全部视图都借用原 DER；Parameters 保存完整 TLV。 */
typedef struct xx509algorithm {
	xbytesview Raw;
	xbytesview Oid;
	xbytesview Parameters;
	bool HasParameters;
} xx509algorithm;



/* 证书视图零分配地借用调用方 DER；Serial 保留规范 INTEGER 的完整内容。 */
typedef struct xx509cert {
	xbytesview Raw;
	xbytesview Tbs;
	xx509version Version;
	xbytesview Serial;
	xx509algorithm TbsSignature;
	xbytesview Issuer;
	xtime NotBefore;
	xtime NotAfter;
	xbytesview Subject;
	xbytesview SubjectPublicKeyInfo;
	xbytesview IssuerUniqueId;
	uint8 IssuerUniqueIdUnusedBits;
	xbytesview SubjectUniqueId;
	uint8 SubjectUniqueIdUnusedBits;
	xbytesview Extensions;
	xx509algorithm SignatureAlgorithm;
	xbytesview Signature;
} xx509cert;



/* Name 游标逐个返回 AttributeTypeAndValue，并保留所属 RDN 编号。 */
typedef struct xx509namecursor {
	xdercursor Rdns;
	xdercursor Attributes;
	size_t Rdn;
	bool Active;
} xx509namecursor;



/* Name 属性保留值的 ASN.1 标签和借用内容，不强制转换字符集。 */
typedef struct xx509nameattr {
	xbytesview Raw;
	xbytesview Oid;
	xasn1tag ValueTag;
	xbytesview Value;
	size_t Rdn;
} xx509nameattr;



/* Extension 游标遍历证书、CRL 或其他协议对象中的扩展 SEQUENCE。 */
typedef struct xx509extcursor {
	xdercursor Items;
} xx509extcursor;



/* Extension.Value 是 extnValue OCTET STRING 内借用的 DER 内容。 */
typedef struct xx509ext {
	xbytesview Raw;
	xbytesview Oid;
	bool Critical;
	xbytesview Value;
} xx509ext;



/* 公钥视图统一保留算法和 BIT STRING 内容，RSA 额外公开模数与指数。 */
typedef struct xx509pubkey {
	xx509keytype Type;
	xx509curve Curve;
	xx509algorithm Algorithm;
	xbytesview Key;
	xbytesview Modulus;
	xbytesview Exponent;
} xx509pubkey;

#endif



#if defined(XRT_FEATURE_X509_CRL)

/* CRL 版本使用规范文档中的自然编号；省略 version 表示 v1。 */
typedef enum xx509crlversion {
	X509_CRL_VERSION_1 = 1,
	X509_CRL_VERSION_2
} xx509crlversion;



/* CRL 视图零分配地借用调用方 DER，并保留全部可扩展协议字段。 */
typedef struct xx509crl {
	xbytesview Raw;
	xbytesview Tbs;
	xx509crlversion Version;
	xx509algorithm TbsSignature;
	xbytesview Issuer;
	xtime ThisUpdate;
	bool HasNextUpdate;
	xtime NextUpdate;
	xbytesview Revoked;
	xbytesview Extensions;
	xx509algorithm SignatureAlgorithm;
	xbytesview Signature;
} xx509crl;



/* CRL 条目游标保留列表版本，以正确解释 v2 条目扩展。 */
typedef struct xx509crlcursor {
	xdercursor Items;
	xx509crlversion Version;
} xx509crlcursor;



/* CRL 条目保留完整序列号、撤销时间和可选扩展列表。 */
typedef struct xx509crlentry {
	xbytesview Raw;
	xbytesview Serial;
	xtime RevokedAt;
	xbytesview Extensions;
} xx509crlentry;

#endif



#if defined(XRT_FEATURE_X509_NAME)

/* 按 RFC 5280 和 RFC 4518 比较两个完整 Distinguished Name。 */
XRT_API xx509result xrtX509NameEqual(
	xbytesview Left,
	xbytesview Right
);



/* 判断完整名称是否位于指定 Distinguished Name 子树内。 */
XRT_API xx509result xrtX509NameWithin(
	xbytesview Name,
	xbytesview Base
);

#endif



#if defined(XRT_FEATURE_X509_SIGNATURE)

/* 签名摘要是协议标识，不表示当前构建一定包含对应密码后端。 */
typedef enum xx509hash {
	X509_HASH_NONE = 0,
	X509_HASH_SHA1,
	X509_HASH_SHA224,
	X509_HASH_SHA256,
	X509_HASH_SHA384,
	X509_HASH_SHA512
} xx509hash;



/* 签名方案与公钥类型分离，ECDSA 的曲线由签名者公钥决定。 */
typedef enum xx509signaturetype {
	X509_SIGNATURE_RSA_PKCS1 = 1,
	X509_SIGNATURE_RSA_PSS,
	X509_SIGNATURE_ECDSA,
	X509_SIGNATURE_ED25519,
	X509_SIGNATURE_ED448
} xx509signaturetype;



/* RSA-PSS 额外公开 MGF1 摘要、盐长和尾字段，其余方案保留默认零值。 */
typedef struct xx509signature {
	xx509signaturetype Type;
	xx509hash Hash;
	xx509hash MaskHash;
	size_t SaltSize;
	uint32 Trailer;
} xx509signature;

#endif



#if defined(XRT_FEATURE_X509_PROFILE)

/* GeneralName 使用 RFC 5280 CHOICE 的上下文标签编号。 */
typedef enum xx509gennametype {
	X509_NAME_OTHER = 0,
	X509_NAME_EMAIL = 1,
	X509_NAME_DNS = 2,
	X509_NAME_X400 = 3,
	X509_NAME_DIRECTORY = 4,
	X509_NAME_EDI = 5,
	X509_NAME_URI = 6,
	X509_NAME_IP = 7,
	X509_NAME_REGISTERED_ID = 8
} xx509gennametype;



/* KeyUsage 位值与 RFC 5280 位编号一一对应。 */
typedef enum xx509keyusage {
	X509_USAGE_DIGITAL_SIGNATURE = UINT16_C(0x0001),
	X509_USAGE_CONTENT_COMMITMENT = UINT16_C(0x0002),
	X509_USAGE_KEY_ENCIPHERMENT = UINT16_C(0x0004),
	X509_USAGE_DATA_ENCIPHERMENT = UINT16_C(0x0008),
	X509_USAGE_KEY_AGREEMENT = UINT16_C(0x0010),
	X509_USAGE_CERT_SIGN = UINT16_C(0x0020),
	X509_USAGE_CRL_SIGN = UINT16_C(0x0040),
	X509_USAGE_ENCIPHER_ONLY = UINT16_C(0x0080),
	X509_USAGE_DECIPHER_ONLY = UINT16_C(0x0100)
} xx509keyusage;



/* GeneralNames 游标借用一项完整 DER SEQUENCE。 */
typedef struct xx509gencursor {
	xdercursor Items;
} xx509gencursor;



/* GeneralName.Value 借用上下文标签内容，Raw 保留完整 TLV。 */
typedef struct xx509genname {
	xx509gennametype Type;
	xbytesview Raw;
	xbytesview Value;
} xx509genname;



/* OID 序列游标用于 EKU 和其他 SEQUENCE OF OBJECT IDENTIFIER。 */
typedef struct xx509oidcursor {
	xdercursor Items;
} xx509oidcursor;



/* BasicConstraints 同时区分 pathLenConstraint 不存在和数值为零。 */
typedef struct xx509basicconstraints {
	bool CA;
	bool HasPathLimit;
	uint32 PathLimit;
} xx509basicconstraints;



/* AuthorityKeyIdentifier 保留借用的 keyIdentifier、发行者游标和完整序列号。 */
typedef struct xx509authoritykeyid {
	bool HasKeyId;
	xbytesview KeyId;
	bool HasIssuer;
	xx509gencursor Issuer;
	bool HasSerial;
	xbytesview Serial;
} xx509authoritykeyid;

#endif



#if defined(XRT_FEATURE_X509_DISTRIBUTION)

/* ReasonFlags 位值按 BIT STRING 位编号表达，与 CRLReason 枚举数值相互独立。 */
typedef enum xx509crlreasonflag {
	X509_CRL_REASON_FLAG_UNUSED = UINT16_C(0x0001),
	X509_CRL_REASON_FLAG_KEY_COMPROMISE = UINT16_C(0x0002),
	X509_CRL_REASON_FLAG_CA_COMPROMISE = UINT16_C(0x0004),
	X509_CRL_REASON_FLAG_AFFILIATION_CHANGED = UINT16_C(0x0008),
	X509_CRL_REASON_FLAG_SUPERSEDED = UINT16_C(0x0010),
	X509_CRL_REASON_FLAG_CESSATION = UINT16_C(0x0020),
	X509_CRL_REASON_FLAG_CERTIFICATE_HOLD = UINT16_C(0x0040),
	X509_CRL_REASON_FLAG_PRIVILEGE_WITHDRAWN = UINT16_C(0x0080),
	X509_CRL_REASON_FLAG_AA_COMPROMISE = UINT16_C(0x0100),
	X509_CRL_REASON_FLAG_ALL = UINT16_C(0x01FE)
} xx509crlreasonflag;



/* DistributionPointName 区分完整 GeneralNames 和相对 CRL 发行者的单个 RDN。 */
typedef enum xx509distributionnametype {
	X509_DISTRIBUTION_FULL_NAME = 0,
	X509_DISTRIBUTION_RELATIVE_NAME = 1
} xx509distributionnametype;



/* 分发点名称保留完整上下文 TLV；FullNames 仅在完整名称模式下有效。 */
typedef struct xx509distributionname {
	xx509distributionnametype Type;
	xbytesview Raw;
	xbytesview Value;
	xx509gencursor FullNames;
} xx509distributionname;



/* 分发点列表游标借用完整 CRLDistributionPoints DER。 */
typedef struct xx509distributioncursor {
	xdercursor Items;
} xx509distributioncursor;



/* DistributionPoint 保留三项可选字段和完整 DER 视图。 */
typedef struct xx509distributionpoint {
	xbytesview Raw;
	bool HasName;
	xx509distributionname Name;
	bool HasReasons;
	uint16 Reasons;
	bool HasIssuer;
	xx509gencursor Issuer;
} xx509distributionpoint;

#endif



#if defined(XRT_FEATURE_X509_CRL_PROFILE)

/* CRLReason 保留 RFC 5280 的协议数值；7 没有定义。 */
typedef enum xx509crlreason {
	X509_CRL_REASON_UNSPECIFIED = 0,
	X509_CRL_REASON_KEY_COMPROMISE = 1,
	X509_CRL_REASON_CA_COMPROMISE = 2,
	X509_CRL_REASON_AFFILIATION_CHANGED = 3,
	X509_CRL_REASON_SUPERSEDED = 4,
	X509_CRL_REASON_CESSATION = 5,
	X509_CRL_REASON_CERTIFICATE_HOLD = 6,
	X509_CRL_REASON_REMOVE = 8,
	X509_CRL_REASON_PRIVILEGE_WITHDRAWN = 9,
	X509_CRL_REASON_AA_COMPROMISE = 10
} xx509crlreason;



/* IssuingDistributionPoint 区分所有 DEFAULT、OPTIONAL 和作用域字段。 */
typedef struct xx509issuingpoint {
	bool HasDistributionPoint;
	xx509distributionname DistributionPoint;
	bool OnlyUserCertificates;
	bool OnlyCaCertificates;
	bool HasReasons;
	uint16 Reasons;
	bool Indirect;
	bool OnlyAttributeCertificates;
} xx509issuingpoint;

#endif



#if defined(XRT_FEATURE_X509_NAME_CONSTRAINTS)

/* GeneralSubtrees 游标借用 permitted 或 excluded 的隐式 SEQUENCE 内容。 */
typedef struct xx509subtreecursor {
	xdercursor Items;
} xx509subtreecursor;



/* GeneralSubtree 保留名称基点和任意精度非负 distance 借用视图。 */
typedef struct xx509subtree {
	xbytesview Raw;
	xx509genname Base;
	bool HasMinimum;
	xbytesview Minimum;
	bool HasMaximum;
	xbytesview Maximum;
} xx509subtree;



/* NameConstraints 明确区分 permitted/excluded 不存在与非空列表。 */
typedef struct xx509nameconstraints {
	bool HasPermitted;
	xx509subtreecursor Permitted;
	bool HasExcluded;
	xx509subtreecursor Excluded;
} xx509nameconstraints;

#endif



#if defined(XRT_FEATURE_X509_CRL_POLICY)

/* CRL policy 标志只控制规范要求的字段是否必须显式存在。 */
typedef enum xx509crlpolicyflag {
	X509_CRL_REQUIRE_NEXT_UPDATE = UINT32_C(0x00000001),
	X509_CRL_REQUIRE_NUMBER = UINT32_C(0x00000002),
	X509_CRL_REQUIRE_AUTHORITY_KEY_ID = UINT32_C(0x00000004),
	X509_CRL_REQUIRE_KEY_IDENTIFIER = UINT32_C(0x00000008),
	X509_CRL_REQUIRE_KEY_USAGE = UINT32_C(0x00000010)
} xx509crlpolicyflag;



/* 单项查询区分完整 CRL 的有效、撤销与 delta CRL 的移除记录。 */
typedef enum xx509revocationstate {
	X509_REVOCATION_GOOD = 0,
	X509_REVOCATION_REVOKED,
	X509_REVOCATION_REMOVED
} xx509revocationstate;



/* 未内建处理的 critical CRL/条目扩展由调用方显式决定是否接受。 */
typedef xx509result (*xx509crlcriticalproc)(
	const xx509crl* pCrl,
	const xx509crlentry* pEntry,
	const xx509ext* pExtension,
	ptr pUserData
);



/* CRL 策略集中表达确定时间、时钟容差和 critical 扩展处理。 */
typedef struct xx509crlconfig {
	xtime Time;
	uint64 Skew;
	uint32 Flags;
	xx509crlcriticalproc Critical;
	ptr UserData;
} xx509crlconfig;



/* 已验证 CRL 借用原 CRL、签发者证书和全部 profile 子视图。 */
typedef struct xx509crlvalid {
	const xx509crl* Crl;
	const xx509cert* Issuer;
	bool HasAuthorityKeyId;
	xbytesview AuthorityKeyIdDer;
	bool Delta;
	bool HasNumber;
	xbytesview Number;
	xbytesview BaseNumber;
	bool HasIssuingPoint;
	xbytesview IssuingPointDer;
	xx509issuingpoint IssuingPoint;
} xx509crlvalid;



/* 吊销结果同时公开覆盖原因、命中条目和可选扩展值。 */
typedef struct xx509revocation {
	xx509revocationstate State;
	uint16 CoveredReasons;
	xx509crlentry Entry;
	bool HasReason;
	xx509crlreason Reason;
	bool HasInvalidityDate;
	xtime InvalidityDate;
} xx509revocation;



/* 多份分段 CRL 的状态归并器只保存原因掩码和最终借用结果。 */
typedef struct xx509revocationcheck {
	uint16 CoveredReasons;
	bool Determined;
	xx509revocation Revocation;
} xx509revocationcheck;



/* CRL set 复制两个已验证借用视图，不复制其底层 DER。 */
typedef struct xx509crlset {
	xx509crlvalid Base;
	xx509crlvalid Delta;
} xx509crlset;

#endif



#if defined(XRT_FEATURE_X509_PATH)

/* 路径策略标志只控制目标证书用途是否必须显式出现。 */
typedef enum xx509pathflag {
	X509_PATH_REQUIRE_KEY_USAGE = UINT32_C(0x00000001),
	X509_PATH_REQUIRE_PURPOSE = UINT32_C(0x00000002)
} xx509pathflag;



/* 未内建处理的 critical 扩展由调用方返回 VALUE、DONE 或 ERROR。 */
typedef xx509result (*xx509criticalproc)(
	const xx509cert* pCertificate,
	const xx509ext* pExtension,
	size_t iDepth,
	ptr pUserData
);



/* 信任锚是受信任名称与公钥，不把自签名证书错误地并入待验证路径。 */
typedef struct xx509anchor {
	xbytesview Name;
	xx509pubkey PublicKey;
	xbytesview Certificate;
	bool HasNameConstraints;
	xx509nameconstraints NameConstraints;
} xx509anchor;



/* 路径策略把确定时间、目标用途和自定义 critical 扩展处理集中表达。 */
typedef struct xx509pathconfig {
	xtime Time;
	uint32 Flags;
	uint16 KeyUsage;
	xbytesview Purpose;
	xx509criticalproc Critical;
	ptr UserData;
} xx509pathconfig;

#endif



#if defined(XRT_FEATURE_X509_PATH_BUILD)

/* 建链源把调用方已解析的候选发行者和独立信任锚组成借用视图。 */
typedef struct xx509pathsource {
	const xx509cert* const* Issuers;
	size_t IssuerCount;
	const xx509anchor* Anchors;
	size_t AnchorCount;
} xx509pathsource;



/* 建链结果只在成功时发布路径长度和命中的借用信任锚。 */
typedef struct xx509pathresult {
	size_t Count;
	const xx509anchor* Anchor;
} xx509pathresult;

#endif



#if defined(XRT_FEATURE_X509_STORE)

/* 信任库拥有导入的 DER 和锚对象；公开类型保持不透明。 */
typedef struct xx509store xx509store;

#endif



XRT_EXTERN_C_BEGIN



#if defined(XRT_FEATURE_X509_PARSE)

/* 严格解析一张完整 DER 证书；成功结果借用输入，失败时输出保持不变。 */
XRT_API bool xrtX509Parse(
	const void* pDer,
	size_t iSize,
	xx509cert* pCert
);



/* 判断指定绝对时间是否位于证书闭区间内；参数合法时不会设置错误。 */
XRT_API bool xrtX509ValidAt(const xx509cert* pCert, xtime iTime);



/* 严格解析一个独立 AlgorithmIdentifier DER 值。 */
XRT_API bool xrtX509AlgorithmParse(
	xbytesview Der,
	xx509algorithm* pAlgorithm
);



/* 严格解析时间内容，并兼容证书中可无歧义解释的时间标签选择。 */
XRT_API bool xrtX509TimeParse(
	xbytesview Der,
	xtime* pTime
);



/* 从完整 Name DER 初始化借用式属性游标。 */
XRT_API bool xrtX509NameInit(
	xbytesview Name,
	xx509namecursor* pCursor
);



/* 读取下一个 Name 属性；失败时游标和输出保持不变。 */
XRT_API xx509result xrtX509NameRead(
	xx509namecursor* pCursor,
	xx509nameattr* pAttribute
);



/* 查找第一个 OID 完全相同的 Name 属性。 */
XRT_API bool xrtX509NameFind(
	xbytesview Name,
	const void* pOid,
	size_t iOidSize,
	xx509nameattr* pAttribute
);



/* 从一项完整 Extensions DER 初始化游标；空视图表示没有扩展。 */
XRT_API bool xrtX509ExtensionListInit(
	xbytesview Extensions,
	xx509extcursor* pCursor
);



/* 从证书初始化扩展游标；没有扩展时初始化成功并立即结束。 */
XRT_API bool xrtX509ExtensionInit(
	const xx509cert* pCert,
	xx509extcursor* pCursor
);



/* 读取下一个扩展；失败时游标和输出保持不变。 */
XRT_API xx509result xrtX509ExtensionRead(
	xx509extcursor* pCursor,
	xx509ext* pExtension
);



/* 在一项完整 Extensions DER 中查找 OID 完全相同的扩展。 */
XRT_API bool xrtX509ExtensionListFind(
	xbytesview Extensions,
	const void* pOid,
	size_t iOidSize,
	xx509ext* pExtension
);



/* 查找第一个 OID 完全相同的扩展。 */
XRT_API bool xrtX509ExtensionFind(
	const xx509cert* pCert,
	const void* pOid,
	size_t iOidSize,
	xx509ext* pExtension
);



/* 解析 SubjectPublicKeyInfo，并返回支持未来算法的通用借用视图。 */
XRT_API bool xrtX509PublicKey(
	const xx509cert* pCert,
	xx509pubkey* pPublicKey
);

#endif



#if defined(XRT_FEATURE_X509_CRL)

/* 严格解析一份完整 DER CRL；成功结果借用输入，失败时输出保持不变。 */
XRT_API bool xrtX509CrlParse(
	const void* pDer,
	size_t iSize,
	xx509crl* pCrl
);



/* 判断绝对时间是否位于 CRL 发布窗口内；缺少 nextUpdate 时不设上界。 */
XRT_API bool xrtX509CrlValidAt(const xx509crl* pCrl, xtime iTime);



/* 初始化撤销条目游标；没有条目时初始化成功并立即结束。 */
XRT_API bool xrtX509CrlEntryInit(
	const xx509crl* pCrl,
	xx509crlcursor* pCursor
);



/* 读取下一项撤销条目；失败时游标和输出保持不变。 */
XRT_API xx509result xrtX509CrlEntryRead(
	xx509crlcursor* pCursor,
	xx509crlentry* pEntry
);



/* 按规范 INTEGER 内容精确查找序列号；未找到返回 X509_DONE。 */
XRT_API xx509result xrtX509CrlFind(
	const xx509crl* pCrl,
	xbytesview Serial,
	xx509crlentry* pEntry
);



/* 判断 CRL 是否列出证书序列号；命中时可选返回对应条目。 */
XRT_API xx509result xrtX509CrlRevokes(
	const xx509crl* pCrl,
	const xx509cert* pCert,
	xx509crlentry* pEntry
);

#endif



#if defined(XRT_FEATURE_X509_SIGNATURE)

/* 解析已知证书签名算法；未知 OID 返回 X509_DONE，格式错误返回 X509_ERROR。 */
XRT_API xx509result xrtX509SignatureParse(
	const xx509algorithm* pAlgorithm,
	xx509signature* pSignature
);

#endif



#if defined(XRT_FEATURE_X509_CRL_VERIFY)

/* 使用已解析公钥只验证 CRL 密码签名，不执行 Issuer 或撤销策略。 */
XRT_API bool xrtX509CrlVerifyKey(
	const xx509crl* pCrl,
	const xx509pubkey* pPublicKey
);



/* 使用证书公钥只验证 CRL 密码签名，不执行名称、用途或作用域策略。 */
XRT_API bool xrtX509CrlVerify(
	const xx509crl* pCrl,
	const xx509cert* pIssuer
);

#endif



#if defined(XRT_FEATURE_X509_VERIFY)

/* 使用已解析方案和公钥验证一段原始内容的证书协议签名。 */
XRT_API bool xrtX509SignatureVerify(
	const xx509signature* pScheme,
	xbytesview Content,
	xbytesview Signature,
	const xx509pubkey* pPublicKey
);



/* 使用已解析公钥验证一张证书的签名，不执行任何路径策略。 */
XRT_API bool xrtX509CertificateVerifyKey(
	const xx509cert* pCertificate,
	const xx509pubkey* pPublicKey
);



/* 只验证一张证书的签名，不执行名称、用途、有效期或路径策略。 */
XRT_API bool xrtX509CertificateVerify(
	const xx509cert* pCertificate,
	const xx509cert* pIssuer
);

#endif



#if defined(XRT_FEATURE_X509_PROFILE)

/* 从一项完整 GeneralNames DER 初始化游标。 */
XRT_API bool xrtX509GeneralNameInit(
	xbytesview Names,
	xx509gencursor* pCursor
);



/* 读取下一个 GeneralName，并验证其上下文形式和基本内容。 */
XRT_API xx509result xrtX509GeneralNameRead(
	xx509gencursor* pCursor,
	xx509genname* pName
);



/* 初始化 SubjectAltName；扩展不存在时返回 X509_DONE。 */
XRT_API xx509result xrtX509SubjectAltName(
	const xx509cert* pCert,
	xx509gencursor* pCursor
);



/* 初始化 IssuerAltName；扩展不存在时返回 X509_DONE。 */
XRT_API xx509result xrtX509IssuerAltName(
	const xx509cert* pCert,
	xx509gencursor* pCursor
);



/* 读取 KeyUsage；扩展不存在时返回 X509_DONE。 */
XRT_API xx509result xrtX509KeyUsage(
	const xx509cert* pCert,
	uint16* pUsage
);



/* 读取 BasicConstraints；扩展不存在时返回 X509_DONE。 */
XRT_API xx509result xrtX509BasicConstraints(
	const xx509cert* pCert,
	xx509basicconstraints* pConstraints
);



/* 从一项完整 SEQUENCE OF OBJECT IDENTIFIER 初始化游标。 */
XRT_API bool xrtX509OidInit(
	xbytesview Oids,
	xx509oidcursor* pCursor
);



/* 读取下一个 OID 内容八位组。 */
XRT_API xx509result xrtX509OidRead(
	xx509oidcursor* pCursor,
	xbytesview* pOid
);



/* 初始化 ExtendedKeyUsage；扩展不存在时返回 X509_DONE。 */
XRT_API xx509result xrtX509ExtendedKeyUsage(
	const xx509cert* pCert,
	xx509oidcursor* pCursor
);



/* 读取 SubjectKeyIdentifier；扩展不存在时返回 X509_DONE。 */
XRT_API xx509result xrtX509SubjectKeyId(
	const xx509cert* pCert,
	xbytesview* pKeyId
);



/* 从独立 DER OCTET STRING 解析 SubjectKeyIdentifier。 */
XRT_API bool xrtX509SubjectKeyIdParse(
	xbytesview Der,
	xbytesview* pKeyId
);



/* 读取 AuthorityKeyIdentifier；扩展不存在时返回 X509_DONE。 */
XRT_API xx509result xrtX509AuthorityKeyId(
	const xx509cert* pCert,
	xx509authoritykeyid* pIdentifier
);



/* 从独立 DER SEQUENCE 解析 AuthorityKeyIdentifier。 */
XRT_API bool xrtX509AuthorityKeyIdParse(
	xbytesview Der,
	xx509authoritykeyid* pIdentifier
);

#endif



#if defined(XRT_FEATURE_X509_DISTRIBUTION)

/* 解析一项独立 DistributionPoint DER，成功结果借用输入。 */
XRT_API bool xrtX509DistributionPointParse(
	xbytesview Der,
	xx509distributionpoint* pPoint
);



/* 从完整 CRLDistributionPoints DER 初始化借用式游标。 */
XRT_API bool xrtX509DistributionInit(
	xbytesview Der,
	xx509distributioncursor* pCursor
);



/* 读取下一个 DistributionPoint；失败时游标和输出保持不变。 */
XRT_API xx509result xrtX509DistributionRead(
	xx509distributioncursor* pCursor,
	xx509distributionpoint* pPoint
);



/* 初始化证书 CRLDistributionPoints；扩展不存在时返回 X509_DONE。 */
XRT_API xx509result xrtX509CrlPoints(
	const xx509cert* pCert,
	xx509distributioncursor* pCursor
);



/* 初始化证书 non-critical FreshestCRL；扩展不存在时返回 X509_DONE。 */
XRT_API xx509result xrtX509FreshestCrl(
	const xx509cert* pCert,
	xx509distributioncursor* pCursor
);

#endif



#if defined(XRT_FEATURE_X509_CRL_PROFILE)

/* 解析 CRLNumber 或 BaseCRLNumber，返回去除正数符号零的任意精度大端视图。 */
XRT_API bool xrtX509CrlNumberParse(
	xbytesview Der,
	xbytesview* pNumber
);



/* 读取非 critical CRLNumber；扩展不存在时返回 X509_DONE。 */
XRT_API xx509result xrtX509CrlNumber(
	const xx509crl* pCrl,
	xbytesview* pNumber
);



/* 读取 critical DeltaCRLIndicator 的 BaseCRLNumber；不存在表示完整 CRL。 */
XRT_API xx509result xrtX509CrlDeltaBase(
	const xx509crl* pCrl,
	xbytesview* pNumber
);



/* 读取 CRL 的非 critical AuthorityKeyIdentifier。 */
XRT_API xx509result xrtX509CrlAuthorityKeyId(
	const xx509crl* pCrl,
	xx509authoritykeyid* pIdentifier
);



/* 解析一项独立 IssuingDistributionPoint DER。 */
XRT_API bool xrtX509IssuingPointParse(
	xbytesview Der,
	xx509issuingpoint* pPoint
);



/* 读取 CRL 的 critical IssuingDistributionPoint。 */
XRT_API xx509result xrtX509CrlIssuingPoint(
	const xx509crl* pCrl,
	xx509issuingpoint* pPoint
);



/* 初始化完整 CRL 的 non-critical FreshestCRL；delta CRL 中出现时拒绝。 */
XRT_API xx509result xrtX509CrlFreshest(
	const xx509crl* pCrl,
	xx509distributioncursor* pCursor
);



/* 解析一项独立 CRLReason DER ENUMERATED。 */
XRT_API bool xrtX509CrlReasonParse(
	xbytesview Der,
	xx509crlreason* pReason
);



/* 读取撤销条目的 non-critical Reason Code。 */
XRT_API xx509result xrtX509CrlEntryReason(
	const xx509crlentry* pEntry,
	xx509crlreason* pReason
);



/* 解析一项独立 InvalidityDate DER GeneralizedTime。 */
XRT_API bool xrtX509CrlInvalidityDateParse(
	xbytesview Der,
	xtime* pTime
);



/* 读取撤销条目的 non-critical Invalidity Date。 */
XRT_API xx509result xrtX509CrlEntryInvalidityDate(
	const xx509crlentry* pEntry,
	xtime* pTime
);



/* 读取撤销条目的 critical Certificate Issuer GeneralNames。 */
XRT_API xx509result xrtX509CrlEntryIssuer(
	const xx509crlentry* pEntry,
	xx509gencursor* pIssuer
);

#endif



#if defined(XRT_FEATURE_X509_NAME_CONSTRAINTS)

/* 解析一项独立 NameConstraints DER，成功结果借用输入。 */
XRT_API bool xrtX509NameConstraintsParse(
	xbytesview Der,
	xx509nameconstraints* pConstraints
);



/* 读取证书的 critical NameConstraints；扩展不存在时返回 X509_DONE。 */
XRT_API xx509result xrtX509NameConstraints(
	const xx509cert* pCertificate,
	xx509nameconstraints* pConstraints
);



/* 读取下一项 GeneralSubtree；失败时游标和输出保持不变。 */
XRT_API xx509result xrtX509SubtreeRead(
	xx509subtreecursor* pCursor,
	xx509subtree* pSubtree
);



/* 按名称类型判断一个 GeneralName 是否位于约束基点内。 */
XRT_API xx509result xrtX509GeneralNameWithin(
	const xx509genname* pName,
	const xx509genname* pBase
);



/* 检查证书 Subject 与 SAN 是否满足一组 NameConstraints。 */
XRT_API bool xrtX509NameConstraintsCheck(
	const xx509nameconstraints* pConstraints,
	const xx509cert* pCertificate
);

#endif



#if defined(XRT_FEATURE_X509_CRL_POLICY)

/* 初始化当前时间下的 RFC 5280 严格 CRL 策略。 */
XRT_API void xrtX509CrlConfigInit(xx509crlconfig* pConfig);



/* 验证 CRL 的签名、签发者、时间、profile 和全部 critical 扩展。 */
XRT_API bool xrtX509CrlValidate(
	const xx509crl* pCrl,
	const xx509cert* pIssuer,
	const xx509crlconfig* pConfig,
	xx509crlvalid* pValid
);



/* 使用已验证 CRL 查询证书；不适用或 delta 中无更新时返回 X509_DONE。 */
XRT_API xx509result xrtX509CrlCheck(
	const xx509crlvalid* pValid,
	const xx509cert* pCertificate,
	xx509revocation* pRevocation
);



/* 验证并查询一张证书，适合不需要复用 CRL 验证结果的常见路径。 */
XRT_API xx509result xrtX509CrlStatus(
	const xx509crl* pCrl,
	const xx509cert* pIssuer,
	const xx509cert* pCertificate,
	const xx509crlconfig* pConfig,
	xx509revocation* pRevocation
);



/* 组合相同签发者和作用域的 complete/delta CRL 借用视图。 */
XRT_API bool xrtX509CrlSetInit(
	xx509crlset* pSet,
	const xx509crlvalid* pBase,
	const xx509crlvalid* pDelta
);



/* 按 delta 优先、base 回退的规则查询组合 CRL。 */
XRT_API xx509result xrtX509CrlSetCheck(
	const xx509crlset* pSet,
	const xx509cert* pCertificate,
	xx509revocation* pRevocation
);



/* 初始化多份分段 CRL 的零分配状态归并器。 */
XRT_API void xrtX509RevocationInit(xx509revocationcheck* pCheck);



/* 加入一份 CRL 查询结果；覆盖完整或确认撤销时返回 X509_VALUE。 */
XRT_API xx509result xrtX509RevocationUpdate(
	xx509revocationcheck* pCheck,
	const xx509revocation* pRevocation
);



/* 读取已确定的最终状态；覆盖仍不完整时返回 X509_DONE。 */
XRT_API xx509result xrtX509RevocationResult(
	const xx509revocationcheck* pCheck,
	xx509revocation* pRevocation
);

#endif



#if defined(XRT_FEATURE_X509_PATH)

/* 从一张受信任证书提取名称和公钥；不校验该证书的时间、扩展或自签名。 */
XRT_API bool xrtX509Anchor(
	const xx509cert* pCertificate,
	xx509anchor* pAnchor
);



/* 按发行者名称及可用 AKI/SKI 信息判断候选发行者，不验证签名。 */
XRT_API xx509result xrtX509IssuerMatch(
	const xx509cert* pCertificate,
	const xx509cert* pIssuer
);



/* 验证目标在首、中间 CA 向后的有序路径；信任锚不属于路径数组。 */
XRT_API bool xrtX509PathValidate(
	const xx509cert* const* ppCertificates,
	size_t iCount,
	const xx509anchor* pAnchor,
	const xx509pathconfig* pConfig
);

#endif



#if defined(XRT_FEATURE_X509_PATH_BUILD)

/* 在候选发行者和信任锚中回溯构建并验证路径；结果仅在成功时发布。 */
XRT_API bool xrtX509PathBuild(
	const xx509cert* pTarget,
	const xx509pathsource* pSource,
	const xx509pathconfig* pConfig,
	const xx509cert** ppPath,
	size_t iCapacity,
	xx509pathresult* pResult
);

#endif



#if defined(XRT_FEATURE_X509_STORE)

/* 创建空信任库。 */
XRT_API xx509store* xrtX509StoreCreate(void);



/* 释放信任库拥有的证书、索引和对象。 */
XRT_API void xrtX509StoreFree(xx509store* pStore);



/* 返回信任库中的唯一锚数量；空指针是参数错误。 */
XRT_API size_t xrtX509StoreCount(const xx509store* pStore);



/* 复制并导入一张 DER 证书；重复证书返回 X509_DONE。 */
XRT_API xx509result xrtX509StoreAdd(
	xx509store* pStore,
	const void* pDer,
	size_t iSize
);



/* 事务式导入全部 CERTIFICATE PEM 块；pAdded 可选。 */
XRT_API bool xrtX509StoreAddPem(
	xx509store* pStore,
	cstr sText,
	size_t iSize,
	size_t* pAdded
);



/* 按索引借用已解析证书；后续写入信任库会使结构指针失效。 */
XRT_API const xx509cert* xrtX509StoreCertificate(
	const xx509store* pStore,
	size_t iIndex
);



/* 按索引借用独立信任锚；后续写入信任库会使结构指针失效。 */
XRT_API const xx509anchor* xrtX509StoreAnchor(
	const xx509store* pStore,
	size_t iIndex
);



/* 把外部候选发行者与信任库锚组成可直接建链的借用源。 */
XRT_API bool xrtX509StoreSource(
	const xx509store* pStore,
	const xx509cert* const* ppIssuers,
	size_t iIssuerCount,
	xx509pathsource* pSource
);

#endif



#if defined(XRT_FEATURE_X509_STORE_FILE)

/* 自动识别 DER 或 PEM 文件并事务式导入；pAdded 可选。 */
XRT_API bool xrtX509StoreAddFile(
	xx509store* pStore,
	cstr sPath,
	size_t* pAdded
);

#endif



#if defined(XRT_FEATURE_X509_STORE_SYSTEM)

/* 事务式导入当前平台的系统信任锚；pAdded 可选。 */
XRT_API bool xrtX509StoreAddSystem(
	xx509store* pStore,
	size_t* pAdded
);



/* 创建并装载一份独立的系统信任库快照；调用方负责释放。 */
XRT_API xx509store* xrtX509StoreSystem(void);

#endif



#if defined(XRT_FEATURE_X509_IDENTITY)

/* 按 RFC 9525 比较一个证书 DNS pattern 与引用 DNS 名；不读取证书。 */
XRT_API xx509result xrtX509MatchDns(
	xstrview Pattern,
	xstrview Host
);



/* 在 SAN 的 DNS-ID 或 IP-ID 中匹配引用主机；pName 为空时只返回结果。 */
XRT_API xx509result xrtX509MatchHost(
	const xx509cert* pCert,
	xstrview Host,
	xx509genname* pName
);

#endif



XRT_EXTERN_C_END

#endif
