#include "../internal/xrt_tls.h"



#if defined(XRT_FEATURE_TLS_POLICY)

/* 默认策略数组是只读常量，初始化策略不会分配或复制内存。 */
static const xtlsversion __xrtTlsPolicyVersions[] = {
	XTLS_VERSION_13,
	XTLS_VERSION_12
};

static const xtlscipher __xrtTlsPolicyCiphers[] = {
	XTLS_AES_128_GCM_SHA256,
	XTLS_CHACHA20_POLY1305_SHA256,
	XTLS_AES_256_GCM_SHA384,
	XTLS_ECDHE_ECDSA_AES_128_GCM_SHA256,
	XTLS_ECDHE_RSA_AES_128_GCM_SHA256,
	XTLS_ECDHE_ECDSA_CHACHA20_POLY1305_SHA256,
	XTLS_ECDHE_RSA_CHACHA20_POLY1305_SHA256,
	XTLS_ECDHE_ECDSA_AES_256_GCM_SHA384,
	XTLS_ECDHE_RSA_AES_256_GCM_SHA384
};

static const uint16 __xrtTlsPolicyGroups[] = {
	XTLS_GROUP_X25519,
	XTLS_GROUP_SECP256R1,
	XTLS_GROUP_SECP384R1,
	XTLS_GROUP_X448
};

static const xtlssignature __xrtTlsPolicySignatures[] = {
	XTLS_SIGNATURE_ED25519,
	XTLS_SIGNATURE_ECDSA_SECP256R1_SHA256,
	XTLS_SIGNATURE_RSA_PSS_RSAE_SHA256,
	XTLS_SIGNATURE_RSA_PSS_PSS_SHA256,
	XTLS_SIGNATURE_ECDSA_SECP384R1_SHA384,
	XTLS_SIGNATURE_RSA_PSS_RSAE_SHA384,
	XTLS_SIGNATURE_RSA_PSS_PSS_SHA384,
	XTLS_SIGNATURE_ED448,
	XTLS_SIGNATURE_ECDSA_SECP521R1_SHA512,
	XTLS_SIGNATURE_RSA_PSS_RSAE_SHA512,
	XTLS_SIGNATURE_RSA_PSS_PSS_SHA512,
	XTLS_SIGNATURE_RSA_PKCS1_SHA256,
	XTLS_SIGNATURE_RSA_PKCS1_SHA384,
	XTLS_SIGNATURE_RSA_PKCS1_SHA512
};



/* 设置 TLS 策略配置错误并返回 false。 */
static bool __xrtTlsPolicyError(xerrkind Kind, cstr sMessage)
{
	xtlserror Code = (Kind == XERR_ARGUMENT) ?
		XTLS_ERROR_ARGUMENT : XTLS_ERROR_NEGOTIATION;

	__xrtTlsError(
		Kind, Code,
		"validate-tls-policy", sMessage, SIZE_MAX
	);
	return false;
}



/* 判断策略版本列表是否包含指定版本。 */
static bool __xrtTlsPolicyHasVersion(
	const xtlspolicy* pPolicy,
	xtlsversion Version
)
{
	for ( size_t i = 0; i < pPolicy->VersionCount; i++ ) {
		if ( pPolicy->Versions[i] == Version ) {
			return true;
		}
	}
	return false;
}



/* 验证有序版本偏好及其唯一性。 */
static bool __xrtTlsPolicyVersionsValid(const xtlspolicy* pPolicy)
{
	if ( (pPolicy->Versions == NULL) || (pPolicy->VersionCount == 0) ) {
		return __xrtTlsPolicyError(
			XERR_VALUE, "TLS policy requires at least one version"
		);
	}
	if ( pPolicy->VersionCount >
		(sizeof(__xrtTlsPolicyVersions) /
		 sizeof(__xrtTlsPolicyVersions[0])) ) {
		return __xrtTlsPolicyError(
			XERR_RANGE, "TLS policy contains too many versions"
		);
	}
	for ( size_t i = 0; i < pPolicy->VersionCount; i++ ) {
		if ( !__xrtTlsVersionSupported(pPolicy->Versions[i]) ) {
			return __xrtTlsPolicyError(
				XERR_VALUE, "TLS policy contains an unsupported version"
			);
		}
		for ( size_t j = 0; j < i; j++ ) {
			if ( pPolicy->Versions[i] == pPolicy->Versions[j] ) {
				return __xrtTlsPolicyError(
					XERR_VALUE, "TLS policy contains a duplicate version"
				);
			}
		}
	}
	return true;
}



/* 验证套件偏好、版本覆盖和唯一性。 */
static bool __xrtTlsPolicyCiphersValid(const xtlspolicy* pPolicy)
{
	if ( (pPolicy->Ciphers == NULL) || (pPolicy->CipherCount == 0) ) {
		return __xrtTlsPolicyError(
			XERR_VALUE, "TLS policy requires at least one cipher"
		);
	}
	if ( pPolicy->CipherCount >
		(sizeof(__xrtTlsPolicyCiphers) /
		 sizeof(__xrtTlsPolicyCiphers[0])) ) {
		return __xrtTlsPolicyError(
			XERR_RANGE, "TLS policy contains too many ciphers"
		);
	}
	for ( size_t i = 0; i < pPolicy->CipherCount; i++ ) {
		const xtlscipherinfo* pInfo = xrtTlsCipherInfo(pPolicy->Ciphers[i]);

		if ( pInfo == NULL ) {
			return __xrtTlsPolicyError(
				XERR_VALUE, "TLS policy contains an unsupported cipher"
			);
		}
		if ( !__xrtTlsPolicyHasVersion(pPolicy, pInfo->Version) ) {
			return __xrtTlsPolicyError(
				XERR_VALUE, "TLS policy cipher has no enabled version"
			);
		}
		for ( size_t j = 0; j < i; j++ ) {
			if ( pPolicy->Ciphers[i] == pPolicy->Ciphers[j] ) {
				return __xrtTlsPolicyError(
					XERR_VALUE, "TLS policy contains a duplicate cipher"
				);
			}
		}
	}

	for ( size_t i = 0; i < pPolicy->VersionCount; i++ ) {
		bool bFound = false;

		for ( size_t j = 0; j < pPolicy->CipherCount; j++ ) {
			const xtlscipherinfo* pInfo = xrtTlsCipherInfo(pPolicy->Ciphers[j]);

			if ( (pInfo != NULL) &&
				(pInfo->Version == pPolicy->Versions[i]) ) {
				bFound = true;
				break;
			}
		}
		if ( !bFound ) {
			return __xrtTlsPolicyError(
				XERR_VALUE, "TLS policy version has no enabled cipher"
			);
		}
	}
	return true;
}



/* 验证命名组偏好；空列表保留给恢复或外部密钥交换策略。 */
static bool __xrtTlsPolicyGroupsValid(const xtlspolicy* pPolicy)
{
	if ( (pPolicy->Groups == NULL) && (pPolicy->GroupCount != 0) ) {
		return __xrtTlsPolicyError(
			XERR_ARGUMENT, "TLS policy group list is null"
		);
	}
	if ( pPolicy->GroupCount >
		(sizeof(__xrtTlsPolicyGroups) /
		 sizeof(__xrtTlsPolicyGroups[0])) ) {
		return __xrtTlsPolicyError(
			XERR_RANGE, "TLS policy contains too many groups"
		);
	}
	for ( size_t i = 0; i < pPolicy->GroupCount; i++ ) {
		if ( xrtTlsGroupInfo(pPolicy->Groups[i]) == NULL ) {
			return __xrtTlsPolicyError(
				XERR_VALUE, "TLS policy contains an unsupported group"
			);
		}
		for ( size_t j = 0; j < i; j++ ) {
			if ( pPolicy->Groups[i] == pPolicy->Groups[j] ) {
				return __xrtTlsPolicyError(
					XERR_VALUE, "TLS policy contains a duplicate group"
				);
			}
		}
	}
	return true;
}



/* 判断签名方案是否能用于策略启用的至少一个版本。 */
static bool __xrtTlsPolicySignatureVersion(
	const xtlspolicy* pPolicy,
	const xtlssignatureinfo* pInfo
)
{
	for ( size_t i = 0; i < pPolicy->VersionCount; i++ ) {
		if ( (pPolicy->Versions[i] >= pInfo->Minimum) &&
			(pPolicy->Versions[i] <= pInfo->Maximum) ) {
			return true;
		}
	}
	return false;
}



/* 验证签名偏好；空列表保留给恢复和外部认证策略。 */
static bool __xrtTlsPolicySignaturesValid(const xtlspolicy* pPolicy)
{
	if ( (pPolicy->Signatures == NULL) &&
		(pPolicy->SignatureCount != 0) ) {
		return __xrtTlsPolicyError(
			XERR_ARGUMENT, "TLS policy signature list is null"
		);
	}
	if ( pPolicy->SignatureCount >
		(sizeof(__xrtTlsPolicySignatures) /
		 sizeof(__xrtTlsPolicySignatures[0])) ) {
		return __xrtTlsPolicyError(
			XERR_RANGE, "TLS policy contains too many signatures"
		);
	}
	for ( size_t i = 0; i < pPolicy->SignatureCount; i++ ) {
		const xtlssignatureinfo* pInfo = xrtTlsSignatureInfo(
			pPolicy->Signatures[i]
		);

		if ( pInfo == NULL ) {
			return __xrtTlsPolicyError(
				XERR_VALUE, "TLS policy contains an unsupported signature"
			);
		}
		if ( !__xrtTlsPolicySignatureVersion(pPolicy, pInfo) ) {
			return __xrtTlsPolicyError(
				XERR_VALUE, "TLS policy signature has no enabled version"
			);
		}
		for ( size_t j = 0; j < i; j++ ) {
			if ( pPolicy->Signatures[i] == pPolicy->Signatures[j] ) {
				return __xrtTlsPolicyError(
					XERR_VALUE, "TLS policy contains a duplicate signature"
				);
			}
		}
	}
	return true;
}



/* 初始化一套覆盖内建现代协议能力的确定性偏好。 */
XRT_API void xrtTlsPolicyInit(xtlspolicy* pPolicy)
{
	if ( pPolicy == NULL ) {
		__xrtTlsPolicyError(XERR_ARGUMENT, "TLS policy is null");
		return;
	}
	pPolicy->Versions = __xrtTlsPolicyVersions;
	pPolicy->VersionCount = sizeof(__xrtTlsPolicyVersions) /
		sizeof(__xrtTlsPolicyVersions[0]);
	pPolicy->Ciphers = __xrtTlsPolicyCiphers;
	pPolicy->CipherCount = sizeof(__xrtTlsPolicyCiphers) /
		sizeof(__xrtTlsPolicyCiphers[0]);
	pPolicy->Groups = __xrtTlsPolicyGroups;
	pPolicy->GroupCount = sizeof(__xrtTlsPolicyGroups) /
		sizeof(__xrtTlsPolicyGroups[0]);
	pPolicy->Signatures = __xrtTlsPolicySignatures;
	pPolicy->SignatureCount = sizeof(__xrtTlsPolicySignatures) /
		sizeof(__xrtTlsPolicySignatures[0]);
	pPolicy->KeySharePolicy = XTLS_KEY_SHARE_PREFER_READY;
}



/* 验证一套完整策略而不分配内存或修改输入。 */
XRT_API bool xrtTlsPolicyValid(const xtlspolicy* pPolicy)
{
	if ( pPolicy == NULL ) {
		return __xrtTlsPolicyError(
			XERR_ARGUMENT, "TLS policy is null"
		);
	}
	if ( (pPolicy->KeySharePolicy != XTLS_KEY_SHARE_PREFER_GROUP) &&
		(pPolicy->KeySharePolicy != XTLS_KEY_SHARE_PREFER_READY) ) {
		return __xrtTlsPolicyError(
			XERR_VALUE, "TLS policy key-share mode is invalid"
		);
	}
	return __xrtTlsPolicyVersionsValid(pPolicy) &&
		__xrtTlsPolicyCiphersValid(pPolicy) &&
		__xrtTlsPolicyGroupsValid(pPolicy) &&
		__xrtTlsPolicySignaturesValid(pPolicy);
}

#endif
