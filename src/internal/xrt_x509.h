#ifndef XRT_INTERNAL_X509_H
#define XRT_INTERNAL_X509_H

#include "xrt_internal.h"

#include <xrt/x509.h>



#if defined(XRT_FEATURE_X509_PARSE)

extern const uint8 __xrtX509OidRsa[];
extern const size_t __xrtX509OidRsaSize;
extern const uint8 __xrtX509OidRsaPss[];
extern const size_t __xrtX509OidRsaPssSize;
extern const uint8 __xrtX509OidEc[];
extern const size_t __xrtX509OidEcSize;
extern const uint8 __xrtX509OidP256[];
extern const size_t __xrtX509OidP256Size;
extern const uint8 __xrtX509OidP384[];
extern const size_t __xrtX509OidP384Size;
extern const uint8 __xrtX509OidP521[];
extern const size_t __xrtX509OidP521Size;
extern const uint8 __xrtX509OidEd25519[];
extern const size_t __xrtX509OidEd25519Size;
extern const uint8 __xrtX509OidEd448[];
extern const size_t __xrtX509OidEd448Size;
extern const uint8 __xrtX509OidX25519[];
extern const size_t __xrtX509OidX25519Size;
extern const uint8 __xrtX509OidX448[];
extern const size_t __xrtX509OidX448Size;
extern const uint8 __xrtX509OidSubjectAltName[];
extern const size_t __xrtX509OidSubjectAltNameSize;
extern const uint8 __xrtX509OidIssuerAltName[];
extern const size_t __xrtX509OidIssuerAltNameSize;
extern const uint8 __xrtX509OidKeyUsage[];
extern const size_t __xrtX509OidKeyUsageSize;
extern const uint8 __xrtX509OidBasicConstraints[];
extern const size_t __xrtX509OidBasicConstraintsSize;
extern const uint8 __xrtX509OidExtendedKeyUsage[];
extern const size_t __xrtX509OidExtendedKeyUsageSize;
extern const uint8 __xrtX509OidSubjectKeyId[];
extern const size_t __xrtX509OidSubjectKeyIdSize;
extern const uint8 __xrtX509OidAuthorityKeyId[];
extern const size_t __xrtX509OidAuthorityKeyIdSize;
extern const uint8 __xrtX509OidCrlDistribution[];
extern const size_t __xrtX509OidCrlDistributionSize;
extern const uint8 __xrtX509OidFreshestCrl[];
extern const size_t __xrtX509OidFreshestCrlSize;
extern const uint8 __xrtX509OidNameConstraints[];
extern const size_t __xrtX509OidNameConstraintsSize;



/* 设置带 DER 偏移和可选原因链的 X.509 结构化错误。 */
void __xrtX509Error(
	xerrkind Kind,
	xx509error Code,
	cstr sOperation,
	cstr sMessage,
	size_t iOffset,
	const xerror* pCause
);



/* 返回借用指针相对证书 DER 起点的安全偏移。 */
size_t __xrtX509Offset(
	const uint8* pBase,
	size_t iSize,
	const uint8* pValue
);



/* 解析已经通过 DER 校验的 AlgorithmIdentifier 值。 */
bool __xrtX509AlgorithmValue(
	const xdervalue* pValue,
	xx509algorithm* pAlgorithm,
	const uint8* pBase,
	size_t iSize,
	cstr sOperation
);



/* 解析已经通过 DER 校验的 RFC 5280 时间值。 */
bool __xrtX509TimeValue(
	const xdervalue* pValue,
	xtime* pTime,
	const uint8* pBase,
	size_t iSize,
	cstr sOperation
);



/* 解析固定为 GeneralizedTime 的 RFC 5280 协议字段。 */
bool __xrtX509GeneralizedTimeValue(
	const xdervalue* pValue,
	xtime* pTime,
	const uint8* pBase,
	size_t iSize,
	cstr sOperation
);



/* 返回已经通过 DER 校验的序列号 INTEGER 完整内容。 */
bool __xrtX509SerialValue(
	const xdervalue* pValue,
	xbytesview* pSerial,
	const uint8* pBase,
	size_t iSize,
	cstr sOperation
);



/* 验证已经通过 DER 校验的 Name 值。 */
bool __xrtX509NameValue(
	const xdervalue* pValue,
	bool bAllowEmpty,
	const uint8* pBase,
	size_t iSize,
	cstr sOperation
);



/* 解析一个已经通过 DER 校验的扩展值。 */
bool __xrtX509ExtensionValue(
	const xdervalue* pValue,
	xx509ext* pExtension,
	const uint8* pBase,
	size_t iSize,
	cstr sOperation
);



/* 验证一项非空 Extensions SEQUENCE 及重复 OID。 */
bool __xrtX509ExtensionListValue(
	const xdervalue* pValue,
	xbytesview* pExtensions,
	const uint8* pBase,
	size_t iSize,
	cstr sOperation
);



/* 从已经由上层对象验证过的 Extensions 视图初始化游标。 */
bool __xrtX509ExtensionCursorInit(
	xbytesview Extensions,
	xx509extcursor* pCursor,
	cstr sOperation
);



/* 在已经由上层对象验证过的 Extensions 中查找一项，不制造 NOT_FOUND 错误。 */
xx509result __xrtX509ExtensionFindValue(
	xbytesview Extensions,
	const void* pOid,
	size_t iOidSize,
	xx509ext* pExtension,
	cstr sOperation
);



/* 从一项完整 DER 中读取唯一顶层值，并转换为指定 X.509 错误。 */
bool __xrtX509RootValue(
	xbytesview Der,
	xdervalue* pValue,
	xx509error Code,
	cstr sOperation,
	cstr sMessage
);



/* 验证并发布一个 SubjectPublicKeyInfo。 */
bool __xrtX509PublicKeyValue(
	const xdervalue* pValue,
	xx509pubkey* pPublicKey,
	const uint8* pBase,
	size_t iSize,
	cstr sOperation
);

#endif



#if defined(XRT_FEATURE_X509_PROFILE)

/* 解析普通或 NameConstraints 基点使用的 GeneralName。 */
bool __xrtX509GeneralNameValue(
	const xdervalue* pValue,
	bool bConstraint,
	xx509genname* pName,
	cstr sOperation
);



/* 校验 DNS 名称并返回去除一个根点后的长度和通配符状态。 */
bool __xrtX509DnsName(
	xbytesview Name,
	bool bPattern,
	size_t* pSize,
	bool* pWildcard
);



/* 按 ASCII 大小写不敏感规则比较两个等长字节视图。 */
bool __xrtX509AsciiEqual(xbytesview Left, xbytesview Right);

/* 从已验证的 GeneralNames 内容初始化并完整校验游标。 */
bool __xrtX509GeneralNamesContent(
	xbytesview Content,
	xx509gencursor* pCursor,
	cstr sOperation
);

#endif



#if defined(XRT_FEATURE_X509_NAME_CONSTRAINTS)

/* 设置 NameConstraints 层统一错误。 */
void __xrtX509NameConstraintsError(
	xerrkind Kind,
	cstr sOperation,
	cstr sMessage,
	const xerror* pCause
);



/* 验证一个约束基点具有本实现可处理的 RFC 5280 形式。 */
bool __xrtX509ConstraintBaseValid(
	const xx509genname* pBase,
	cstr sOperation
);

#endif



#if defined(XRT_FEATURE_X509_DISTRIBUTION)

/* 解析 DistributionPoint 或 IDP 共用的显式 DistributionPointName 字段。 */
bool __xrtX509DistributionNameValue(
	const xdervalue* pField,
	xx509distributionname* pName,
	cstr sOperation
);



/* 解析上下文隐式 ReasonFlags，并转换为稳定的主机端位值。 */
bool __xrtX509ReasonFlagsValue(
	const xdervalue* pField,
	uint32 iTag,
	uint16* pReasons,
	cstr sOperation
);

#endif



#if defined(XRT_FEATURE_X509_CRL_PROFILE)

extern const uint8 __xrtX509OidCrlNumber[];
extern const size_t __xrtX509OidCrlNumberSize;
extern const uint8 __xrtX509OidCrlReason[];
extern const size_t __xrtX509OidCrlReasonSize;
extern const uint8 __xrtX509OidInvalidityDate[];
extern const size_t __xrtX509OidInvalidityDateSize;
extern const uint8 __xrtX509OidDeltaCrl[];
extern const size_t __xrtX509OidDeltaCrlSize;
extern const uint8 __xrtX509OidIssuingPoint[];
extern const size_t __xrtX509OidIssuingPointSize;
extern const uint8 __xrtX509OidCertificateIssuer[];
extern const size_t __xrtX509OidCertificateIssuerSize;

#endif



#if defined(XRT_FEATURE_X509_PATH)

/* 验证路径策略结构的公开字段组合。 */
bool __xrtX509PathConfigValid(const xx509pathconfig* pConfig);



/* 判断两项路径输入是否表示同一张证书。 */
bool __xrtX509PathDuplicate(
	const xx509cert* pLeft,
	const xx509cert* pRight
);

#endif



#if defined(XRT_FEATURE_X509_STORE)

/* 回滚到既有锚数量，供事务式平台装载层使用。 */
void __xrtX509StoreTruncate(xx509store* pStore, size_t iCount);

#endif



#if defined(XRT_FEATURE_X509_STORE_SYSTEM)

/* 设置带平台错误码和可选原因链的系统信任库错误。 */
void __xrtX509StoreSystemError(
	xerrkind Kind,
	int32 iSystemCode,
	cstr sMessage,
	const xerror* pCause
);



/* 包装当前错误为系统信任库错误。 */
void __xrtX509StoreSystemFailure(cstr sMessage);



/* 仅允许跳过系统库中由 X.509 严格解析或算法策略拒绝的单张证书。 */
bool __xrtX509StoreSystemCanSkip(const xerror* pError);



/* 由当前平台后端把系统锚导入指定信任库。 */
bool __xrtX509StoreSystemLoad(xx509store* pStore);

#endif



#if defined(XRT_FEATURE_X509_DIGEST)

#define XRT_X509_DIGEST_MAX_SIZE 64u

/* 计算证书签名方案指定的摘要并返回实际字节数。 */
bool __xrtX509Digest(
	xx509hash Hash,
	xbytesview Content,
	uint8 pDigest[XRT_X509_DIGEST_MAX_SIZE],
	size_t* pDigestSize
);



/* 把 X.509 摘要标识转换为密码底座的同义标识。 */
bool __xrtX509CryptoHash(
	xx509hash Hash,
	xcryptohash* pCryptoHash
);

#endif



#if defined(XRT_FEATURE_X509_VERIFY_RSA)

/* 使用 RSA-PKCS#1 或 RSA-PSS 后端验证证书协议签名。 */
bool __xrtX509VerifyRsa(
	const xx509signature* pScheme,
	xbytesview Content,
	xbytesview Signature,
	const xx509pubkey* pPublicKey
);

#endif



#if defined(XRT_FEATURE_X509_VERIFY_ECDSA)

/* 使用公钥命名曲线选择严格 DER ECDSA 验签后端。 */
bool __xrtX509VerifyEcdsa(
	const xx509signature* pScheme,
	xbytesview Content,
	xbytesview Signature,
	const xx509pubkey* pPublicKey
);

#endif



#if defined(XRT_FEATURE_X509_VERIFY_ED25519)

/* 使用纯 Ed25519 模式验证证书协议签名。 */
bool __xrtX509VerifyEd25519(
	const xx509signature* pScheme,
	xbytesview Content,
	xbytesview Signature,
	const xx509pubkey* pPublicKey
);

#endif

#endif
