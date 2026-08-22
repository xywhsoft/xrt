#ifndef XRT_INTERNAL_TLS_IDENTITY_H
#define XRT_INTERNAL_TLS_IDENTITY_H

#include "xrt_tls.h"
#include <xrt/tls_identity.h>
#include <xrt/crypto.h>



#if defined(XRT_FEATURE_TLS_IDENTITY)

typedef bool (*__xrttlsidentitysupportsproc)(
	const xtlsidentity* pIdentity,
	xtlsversion Version,
	xtlssignature Signature
);



typedef bool (*__xrttlsidentitysignproc)(
	const xtlsidentity* pIdentity,
	xtlsversion Version,
	xtlssignature Signature,
	xbytesview Message,
	void* pOutput,
	size_t iCapacity,
	size_t* pSize
);



/* 身份使用一块紧凑存储保存对象、证书视图、证书 DER 和可选私钥。 */
struct xtlsidentity {
	volatile int32 RefCount;
	size_t AllocationSize;
	size_t CertificateCount;
	size_t ExtraOffset;
	xtlsidentitytype Type;
	xx509cert Leaf;
	xx509pubkey PublicKey;
	xbytesview* Certificates;
	__xrttlsidentitysupportsproc Supports;
	__xrttlsidentitysignproc Sign;
	xtlsidentitysupportsproc ExternalSupports;
	xtlsidentitysignproc ExternalSign;
	xtlsidentityreleaseproc ExternalRelease;
	ptr ExternalContext;
};



/* 创建证书快照和精确大小的后端私有尾部，失败时不发布半成品。 */
xtlsidentity* __xrtTlsIdentityNew(
	const xbytesview* pCertificates,
	size_t iCertificateCount,
	xtlsidentitytype Type,
	size_t iExtraSize,
	__xrttlsidentitysupportsproc pSupports,
	__xrttlsidentitysignproc pSign,
	ptr* pExtra
);



/* 返回仅供内置后端使用的对齐私有尾部。 */
ptr __xrtTlsIdentityExtra(const xtlsidentity* pIdentity);



/* 设置 TLS 身份层错误并返回 false。 */
bool __xrtTlsIdentityError(
	xerrkind Kind,
	cstr sOperation,
	cstr sMessage
);



/* 包装当前错误为 TLS 身份失败原因并返回 false。 */
bool __xrtTlsIdentityCause(cstr sOperation, cstr sMessage);



#if defined(XRT_FEATURE_TLS_IDENTITY_EC)

/* 严格读取原始标量、SEC1 或 PKCS#8 EC 私钥，返回可选 SEC1 公钥视图。 */
bool __xrtTlsIdentityEcPrivate(
	xbytesview PrivateKey,
	xx509curve Curve,
	void* pScalar,
	size_t iScalarSize,
	xbytesview* pEncodedPublic
);



/* 按 TLS 版本应用 ECDSA 线路方案的曲线匹配规则。 */
bool __xrtTlsIdentityEcSupports(
	xtlsidentitytype Type,
	xtlsversion Version,
	xtlssignature Signature
);



/* 按 ECDSA 线路方案计算 SHA-256、SHA-384 或 SHA-512 摘要。 */
bool __xrtTlsIdentityEcHash(
	xtlssignature Signature,
	xbytesview Message,
	void* pHash,
	xcryptohash* pAlgorithm
);

#endif

#endif

#endif
