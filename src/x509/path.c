#include "../internal/xrt_x509.h"



#if defined(XRT_FEATURE_X509_PATH)

static const uint8 __xrtX509AnyPurpose[] = { 0x55, 0x1D, 0x25, 0x00 };



/* 比较两个借用字节视图。 */
static bool __xrtX509PathEqual(xbytesview Left, xbytesview Right)
{
	return (Left.Size == Right.Size) &&
		((Left.Size == 0) || (memcmp(Left.Data, Right.Data, Left.Size) == 0));
}



/* 比较扩展 OID 内容。 */
static bool __xrtX509PathOid(
	const xx509ext* pExtension,
	const uint8* pOid,
	size_t iOidSize
)
{
	return (pExtension->Oid.Size == iOidSize) &&
		(memcmp(pExtension->Oid.Data, pOid, iOidSize) == 0);
}



/* 设置路径层错误并保留底层原因。 */
static bool __xrtX509PathError(
	xx509error Code,
	cstr sMessage,
	const xerror* pCause
)
{
	__xrtX509Error(
		XERR_PROTOCOL, Code, "x509-path", sMessage, SIZE_MAX, pCause
	);
	return false;
}



/* 判断两项路径输入是否表示同一张证书。 */
bool __xrtX509PathDuplicate(
	const xx509cert* pLeft,
	const xx509cert* pRight
)
{
	return (pLeft == pRight) || __xrtX509PathEqual(pLeft->Raw, pRight->Raw);
}



/* 验证路径策略结构的公开字段组合。 */
bool __xrtX509PathConfigValid(const xx509pathconfig* pConfig)
{
	const uint32 iKnownFlags = X509_PATH_REQUIRE_KEY_USAGE |
		X509_PATH_REQUIRE_PURPOSE | X509_PATH_ALLOW_SHA1;

	return (pConfig != NULL) &&
		((pConfig->Flags & ~iKnownFlags) == 0) &&
		((pConfig->KeyUsage & UINT16_C(0xFE00)) == 0) &&
		(((pConfig->Flags & X509_PATH_REQUIRE_KEY_USAGE) == 0) ||
		 (pConfig->KeyUsage != 0)) &&
		((pConfig->Purpose.Data != NULL) || (pConfig->Purpose.Size == 0)) &&
		(((pConfig->Flags & X509_PATH_REQUIRE_PURPOSE) == 0) ||
		 (pConfig->Purpose.Size != 0));
}



/* 初始化当前时间下默认拒绝弱签名算法的路径策略。 */
XRT_API void xrtX509PathConfigInit(xx509pathconfig* pConfig)
{
	if ( pConfig == NULL ) {
		__xrtErrorSetInvalidArgument();
		return;
	}
	memset(pConfig, 0, sizeof(*pConfig));
	pConfig->Time = xrtNow();
}



/* 在密码验签前执行路径级签名算法强度策略。 */
static bool __xrtX509PathSignature(
	const xx509cert* pCertificate,
	const xx509pathconfig* pConfig
)
{
	xx509signature Signature;
	xx509result Result = xrtX509SignatureParse(
		&pCertificate->SignatureAlgorithm,
		&Signature
	);

	if ( Result != X509_VALUE ) {
		return __xrtX509PathError(
			X509_ERROR_SIGNATURE,
			"certificate path uses an unsupported signature algorithm",
			Result == X509_ERROR ? xrtGetError() : NULL
		);
	}
	if ( (Signature.Hash == X509_HASH_SHA1) &&
		((pConfig->Flags & X509_PATH_ALLOW_SHA1) == 0) ) {
		return __xrtX509PathError(
			X509_ERROR_SIGNATURE,
			"certificate path uses SHA-1 without explicit legacy policy",
			NULL
		);
	}
	return true;
}



/* 验证一张证书中当前路径层已经理解的扩展。 */
static bool __xrtX509PathExtensions(
	const xx509cert* pCertificate,
	size_t iDepth,
	const xx509pathconfig* pConfig
)
{
	xx509extcursor Cursor;
	xx509ext Extension;

	if ( !xrtX509ExtensionInit(pCertificate, &Cursor) ) {
		return false;
	}
	while ( true ) {
		xx509result ReadResult = xrtX509ExtensionRead(&Cursor, &Extension);
		xx509result ParseResult = X509_DONE;
		bool bKnown = true;

		if ( ReadResult == X509_DONE ) {
			return true;
		}
		if ( ReadResult == X509_ERROR ) {
			return false;
		}
		if ( __xrtX509PathOid(
			&Extension, __xrtX509OidSubjectAltName,
			__xrtX509OidSubjectAltNameSize
		) ) {
			xx509gencursor Names;

			ParseResult = xrtX509SubjectAltName(pCertificate, &Names);
		} else if ( __xrtX509PathOid(
			&Extension, __xrtX509OidKeyUsage, __xrtX509OidKeyUsageSize
		) ) {
			uint16 iUsage;

			ParseResult = xrtX509KeyUsage(pCertificate, &iUsage);
		} else if ( __xrtX509PathOid(
			&Extension, __xrtX509OidBasicConstraints,
			__xrtX509OidBasicConstraintsSize
		) ) {
			xx509basicconstraints Constraints;

			ParseResult = xrtX509BasicConstraints(
				pCertificate, &Constraints
			);
		} else if ( __xrtX509PathOid(
			&Extension, __xrtX509OidExtendedKeyUsage,
			__xrtX509OidExtendedKeyUsageSize
		) ) {
			xx509oidcursor Oids;

			ParseResult = xrtX509ExtendedKeyUsage(pCertificate, &Oids);
		} else if ( __xrtX509PathOid(
			&Extension, __xrtX509OidSubjectKeyId,
			__xrtX509OidSubjectKeyIdSize
		) ) {
			xbytesview KeyId;

			ParseResult = xrtX509SubjectKeyId(pCertificate, &KeyId);
		} else if ( __xrtX509PathOid(
			&Extension, __xrtX509OidAuthorityKeyId,
			__xrtX509OidAuthorityKeyIdSize
		) ) {
			xx509authoritykeyid Identifier;

			ParseResult = xrtX509AuthorityKeyId(
				pCertificate, &Identifier
			);
		} else if ( __xrtX509PathOid(
			&Extension, __xrtX509OidNameConstraints,
			__xrtX509OidNameConstraintsSize
		) ) {
			xx509nameconstraints Constraints;

			ParseResult = xrtX509NameConstraints(
				pCertificate, &Constraints
			);
		} else {
			bKnown = false;
		}
		if ( bKnown ) {
			if ( ParseResult == X509_ERROR ) {
				return false;
			}
			if ( ParseResult != X509_VALUE ) {
				return __xrtX509PathError(
					X509_ERROR_PATH,
					"a recognized certificate extension disappeared during processing",
					NULL
				);
			}
			continue;
		}
		if ( !Extension.Critical ) {
			continue;
		}
		if ( pConfig->Critical != NULL ) {
			xx509result Result = pConfig->Critical(
				pCertificate, &Extension, iDepth, pConfig->UserData
			);

			if ( Result == X509_VALUE ) {
				continue;
			}
			if ( Result == X509_ERROR ) {
				const xerror* pCause = xrtGetError();

				return __xrtX509PathError(
					X509_ERROR_CRITICAL_EXTENSION,
					"custom critical extension processing failed",
					pCause
				);
			}
		}
		return __xrtX509PathError(
			X509_ERROR_CRITICAL_EXTENSION,
			"certificate contains an unsupported critical extension", NULL
		);
	}
}



/* 验证目标证书的可选 KeyUsage 和 ExtendedKeyUsage 策略。 */
static xx509result __xrtX509PathExtendedPurpose(
	const xx509cert* pCertificate,
	xbytesview Purpose,
	bool* pPresent
)
{
	xx509oidcursor Oids;
	xx509result Result = xrtX509ExtendedKeyUsage(pCertificate, &Oids);

	*pPresent = Result == X509_VALUE;
	if ( Result != X509_VALUE ) {
		return Result;
	}
	while ( true ) {
		xbytesview Oid;
		xx509result OidResult = xrtX509OidRead(&Oids, &Oid);

		if ( OidResult != X509_VALUE ) {
			return OidResult;
		}
		if ( __xrtX509PathEqual(Oid, Purpose) ||
			((Oid.Size == sizeof(__xrtX509AnyPurpose)) &&
			 (memcmp(Oid.Data, __xrtX509AnyPurpose, Oid.Size) == 0)) ) {
			return X509_VALUE;
		}
	}
}



/* 验证目标证书的可选 KeyUsage 和 ExtendedKeyUsage 策略。 */
static bool __xrtX509PathPurpose(
	const xx509cert* pCertificate,
	const xx509pathconfig* pConfig
)
{
	if ( (pConfig->KeyUsage != 0) ||
		((pConfig->Flags & X509_PATH_REQUIRE_KEY_USAGE) != 0) ) {
		uint16 iUsage;
		xx509result Result = xrtX509KeyUsage(pCertificate, &iUsage);

		if ( Result == X509_ERROR ) {
			return false;
		}
		if ( Result == X509_DONE ) {
			if ( (pConfig->Flags & X509_PATH_REQUIRE_KEY_USAGE) == 0 ) {
				goto Extended;
			}
			return __xrtX509PathError(
				X509_ERROR_PURPOSE,
				"target certificate does not contain required KeyUsage", NULL
			);
		}
		if ( (iUsage & pConfig->KeyUsage) != pConfig->KeyUsage ) {
			return __xrtX509PathError(
				X509_ERROR_PURPOSE,
				"target certificate KeyUsage does not allow the requested use",
				NULL
			);
		}
	}

Extended:
	if ( (pConfig->Purpose.Size != 0) ||
		((pConfig->Flags & X509_PATH_REQUIRE_PURPOSE) != 0) ) {
		bool bPresent;
		xx509result Result = __xrtX509PathExtendedPurpose(
			pCertificate, pConfig->Purpose, &bPresent
		);

		if ( Result == X509_ERROR ) {
			return false;
		}
		if ( !bPresent ) {
			if ( (pConfig->Flags & X509_PATH_REQUIRE_PURPOSE) == 0 ) {
				return true;
			}
			return __xrtX509PathError(
				X509_ERROR_PURPOSE,
				"target certificate does not contain required ExtendedKeyUsage",
				NULL
			);
		}
		if ( Result != X509_VALUE ) {
			return __xrtX509PathError(
				X509_ERROR_PURPOSE,
				"target certificate ExtendedKeyUsage does not allow the requested use",
				NULL
			);
		}
	}
	return true;
}



/* 验证一张中间证书的 CA、KeyUsage 和 pathLenConstraint。 */
static bool __xrtX509PathCa(
	const xx509cert* pCertificate,
	size_t iCaBelow,
	const xx509pathconfig* pConfig
)
{
	xx509basicconstraints Constraints;
	xx509ext Extension;
	xx509result Result = xrtX509BasicConstraints(
		pCertificate, &Constraints
	);

	if ( Result == X509_ERROR ) {
		return false;
	}
	if ( (Result != X509_VALUE) || !Constraints.CA ) {
		return __xrtX509PathError(
			X509_ERROR_PATH_CONSTRAINT,
			"intermediate certificate is not authorized as a CA", NULL
		);
	}
	if ( !xrtX509ExtensionFind(
		pCertificate, __xrtX509OidBasicConstraints,
		__xrtX509OidBasicConstraintsSize, &Extension
	) ) {
		return false;
	}
	if ( !Extension.Critical ) {
		return __xrtX509PathError(
			X509_ERROR_PATH_CONSTRAINT,
			"intermediate CA BasicConstraints is not critical", NULL
		);
	}
	{
		xx509namecursor Name;
		xx509nameattr Attribute;

		if ( !xrtX509NameInit(pCertificate->Subject, &Name) ) {
			return false;
		}
		Result = xrtX509NameRead(&Name, &Attribute);
		if ( Result == X509_ERROR ) {
			return false;
		}
		if ( Result == X509_DONE ) {
			return __xrtX509PathError(
				X509_ERROR_PATH_CONSTRAINT,
				"intermediate CA subject name is empty", NULL
			);
		}
	}
	{
		uint16 iUsage;

		Result = xrtX509KeyUsage(pCertificate, &iUsage);
		if ( Result == X509_ERROR ) {
			return false;
		}
		if ( (Result == X509_VALUE) &&
			((iUsage & X509_USAGE_CERT_SIGN) == 0) ) {
			return __xrtX509PathError(
				X509_ERROR_PATH_CONSTRAINT,
				"intermediate certificate KeyUsage forbids certificate signing",
				NULL
			);
		}
		if ( Constraints.HasPathLimit && (Result != X509_VALUE) ) {
			return __xrtX509PathError(
				X509_ERROR_PATH_CONSTRAINT,
				"CA pathLenConstraint requires KeyUsage keyCertSign", NULL
			);
		}
	}
	if ( Constraints.HasPathLimit && (iCaBelow > Constraints.PathLimit) ) {
		return __xrtX509PathError(
			X509_ERROR_PATH_CONSTRAINT,
			"intermediate certificate pathLenConstraint was exceeded", NULL
		);
	}
	if ( pConfig->Purpose.Size != 0 ) {
		bool bPresent;

		Result = __xrtX509PathExtendedPurpose(
			pCertificate, pConfig->Purpose, &bPresent
		);
		if ( Result == X509_ERROR ) {
			return false;
		}
		if ( bPresent && (Result != X509_VALUE) ) {
			return __xrtX509PathError(
				X509_ERROR_PURPOSE,
				"intermediate CA ExtendedKeyUsage forbids the requested purpose",
				NULL
			);
		}
	}
	return true;
}



/* 判断一张证书是否是可从名称约束检查中跳过的 self-issued 中间证书。 */
static xx509result __xrtX509PathSelfIssued(const xx509cert* pCertificate)
{
	return xrtX509NameEqual(pCertificate->Subject, pCertificate->Issuer);
}



/* 把一组名称约束应用到目标及全部非 self-issued 下级中间证书。 */
static bool __xrtX509PathApplyNameConstraints(
	const xx509nameconstraints* pConstraints,
	const xx509cert* const* ppCertificates,
	size_t iCount
)
{
	for ( size_t i = 0; i < iCount; i++ ) {
		if ( i != 0 ) {
			xx509result Result = __xrtX509PathSelfIssued(ppCertificates[i]);

			if ( Result == X509_ERROR ) {
				return false;
			}
			if ( Result == X509_VALUE ) {
				continue;
			}
		}
		if ( !xrtX509NameConstraintsCheck(
			pConstraints, ppCertificates[i]
		) ) {
			const xerror* pCause = xrtGetError();

			return __xrtX509PathError(
				X509_ERROR_NAME_CONSTRAINTS,
				"certificate path violates a NameConstraints policy", pCause
			);
		}
	}
	return true;
}



/* 验证信任锚和每张 CA 证书向路径下方施加的名称约束。 */
static bool __xrtX509PathNameConstraints(
	const xx509cert* const* ppCertificates,
	size_t iCount,
	const xx509anchor* pAnchor
)
{
	if ( pAnchor->HasNameConstraints &&
		!__xrtX509PathApplyNameConstraints(
			&pAnchor->NameConstraints, ppCertificates, iCount
		) ) {
		return false;
	}
	for ( size_t i = 0; i < iCount; i++ ) {
		xx509nameconstraints Constraints;
		xx509result Result = xrtX509NameConstraints(
			ppCertificates[i], &Constraints
		);

		if ( Result == X509_ERROR ) {
			return false;
		}
		if ( Result == X509_DONE ) {
			continue;
		}
		if ( i == 0 ) {
			return __xrtX509PathError(
				X509_ERROR_NAME_CONSTRAINTS,
				"NameConstraints is only valid in a CA certificate", NULL
			);
		}
		if ( !__xrtX509PathApplyNameConstraints(
			&Constraints, ppCertificates, i
		) ) {
			return false;
		}
	}
	return true;
}



/* 从一张受信任证书提取名称和公钥；不校验该证书本身。 */
XRT_API bool xrtX509Anchor(
	const xx509cert* pCertificate,
	xx509anchor* pAnchor
)
{
	xx509anchor Anchor;

	if ( (pCertificate == NULL) || (pAnchor == NULL) ||
		(pCertificate->Subject.Data == NULL) ||
		(pCertificate->Subject.Size == 0) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	memset(&Anchor, 0, sizeof(Anchor));
	Anchor.Name = pCertificate->Subject;
	Anchor.Certificate = pCertificate->Raw;
	if ( !xrtX509PublicKey(pCertificate, &Anchor.PublicKey) ) {
		return false;
	}
	{
		xx509result Result = xrtX509NameConstraints(
			pCertificate, &Anchor.NameConstraints
		);

		if ( Result == X509_ERROR ) {
			return false;
		}
		Anchor.HasNameConstraints = Result == X509_VALUE;
	}
	*pAnchor = Anchor;
	return true;
}



/* 按发行者名称及可用 AKI/SKI 信息判断候选发行者。 */
XRT_API xx509result xrtX509IssuerMatch(
	const xx509cert* pCertificate,
	const xx509cert* pIssuer
)
{
	xx509authoritykeyid Authority;
	xx509result Result;

	if ( (pCertificate == NULL) || (pIssuer == NULL) ) {
		__xrtErrorSetInvalidArgument();
		return X509_ERROR;
	}
	Result = xrtX509NameEqual(pCertificate->Issuer, pIssuer->Subject);
	if ( Result != X509_VALUE ) {
		return Result;
	}
	Result = xrtX509AuthorityKeyId(pCertificate, &Authority);
	if ( Result != X509_VALUE ) {
		return Result == X509_DONE ? X509_VALUE : X509_ERROR;
	}
	if ( Authority.HasKeyId ) {
		xbytesview SubjectKeyId;

		Result = xrtX509SubjectKeyId(pIssuer, &SubjectKeyId);
		if ( Result == X509_ERROR ) {
			return X509_ERROR;
		}
		if ( (Result == X509_VALUE) &&
			!__xrtX509PathEqual(Authority.KeyId, SubjectKeyId) ) {
			return X509_DONE;
		}
	}
	if ( Authority.HasIssuer ) {
		xx509gencursor Issuers = Authority.Issuer;
		bool bName = false;

		if ( !__xrtX509PathEqual(Authority.Serial, pIssuer->Serial) ) {
			return X509_DONE;
		}
		while ( true ) {
			xx509genname Name;
			xx509result NameResult = xrtX509GeneralNameRead(
				&Issuers, &Name
			);

			if ( NameResult == X509_ERROR ) {
				return X509_ERROR;
			}
			if ( NameResult == X509_DONE ) {
				break;
			}
			if ( Name.Type == X509_NAME_DIRECTORY ) {
				NameResult = xrtX509NameEqual(Name.Value, pIssuer->Issuer);
				if ( NameResult == X509_ERROR ) {
					return X509_ERROR;
				}
				if ( NameResult == X509_VALUE ) {
					bName = true;
				}
			}
		}
		if ( !bName ) {
			return X509_DONE;
		}
	}
	return X509_VALUE;
}



/* 验证目标在首、中间 CA 向后的有序路径。 */
XRT_API bool xrtX509PathValidate(
	const xx509cert* const* ppCertificates,
	size_t iCount,
	const xx509anchor* pAnchor,
	const xx509pathconfig* pConfig
)
{
	size_t iCaBelow = 0;

	if ( (ppCertificates == NULL) || (iCount == 0) ||
		(pAnchor == NULL) || !__xrtX509PathConfigValid(pConfig) ||
		(pAnchor->Name.Data == NULL) || (pAnchor->Name.Size == 0) ||
		((pAnchor->Certificate.Data == NULL) &&
		 (pAnchor->Certificate.Size != 0)) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	for ( size_t i = 0; i < iCount; i++ ) {
		const xx509cert* pCertificate = ppCertificates[i];

		if ( (pCertificate == NULL) || (pCertificate->Raw.Data == NULL) ||
			(pCertificate->Raw.Size == 0) ) {
			__xrtErrorSetInvalidArgument();
			return false;
		}
		for ( size_t j = 0; j < i; j++ ) {
			if ( __xrtX509PathDuplicate(pCertificate, ppCertificates[j]) ) {
				return __xrtX509PathError(
					X509_ERROR_PATH,
					"a certificate appears more than once in the path", NULL
				);
			}
		}
		if ( (pAnchor->Certificate.Size != 0) && __xrtX509PathEqual(
			pCertificate->Raw, pAnchor->Certificate
		) ) {
			return __xrtX509PathError(
				X509_ERROR_PATH,
				"the trust anchor certificate must not appear in the path", NULL
			);
		}
		if ( !xrtX509ValidAt(pCertificate, pConfig->Time) ) {
			return __xrtX509PathError(
				X509_ERROR_PATH,
				"certificate is not valid at the requested time", NULL
			);
		}
		if ( !__xrtX509PathSignature(pCertificate, pConfig) ) {
			return false;
		}
		if ( !__xrtX509PathExtensions(pCertificate, i, pConfig) ) {
			return false;
		}
	}
	if ( !__xrtX509PathPurpose(ppCertificates[0], pConfig) ) {
		return false;
	}
	for ( size_t i = 1; i < iCount; i++ ) {
		const xx509cert* pCertificate = ppCertificates[i];
		xx509result NameResult;

		if ( !__xrtX509PathCa(pCertificate, iCaBelow, pConfig) ) {
			return false;
		}
		NameResult = xrtX509NameEqual(
			pCertificate->Subject, pCertificate->Issuer
		);
		if ( NameResult == X509_ERROR ) {
			return false;
		}
		if ( NameResult == X509_DONE ) {
			iCaBelow++;
		}
	}
	if ( !__xrtX509PathNameConstraints(
		ppCertificates, iCount, pAnchor
	) ) {
		return false;
	}
	for ( size_t i = 0; i < iCount; i++ ) {
		const xx509cert* pCertificate = ppCertificates[i];
		xbytesview IssuerName;
		xx509pubkey IssuerKey;
		const xx509pubkey* pIssuerKey;
		xx509result NameResult;

		if ( i + 1u < iCount ) {
			const xx509cert* pIssuer = ppCertificates[i + 1u];

			IssuerName = pIssuer->Subject;
			if ( !xrtX509PublicKey(pIssuer, &IssuerKey) ) {
				return false;
			}
			pIssuerKey = &IssuerKey;
		} else {
			IssuerName = pAnchor->Name;
			pIssuerKey = &pAnchor->PublicKey;
		}
		NameResult = xrtX509NameEqual(pCertificate->Issuer, IssuerName);
		if ( NameResult == X509_ERROR ) {
			return false;
		}
		if ( NameResult == X509_DONE ) {
			return __xrtX509PathError(
				X509_ERROR_ISSUER,
				"certificate issuer does not match the next path name", NULL
			);
		}
		if ( !xrtX509CertificateVerifyKey(pCertificate, pIssuerKey) ) {
			return __xrtX509PathError(
				X509_ERROR_SIGNATURE,
				"certificate path signature verification failed", xrtGetError()
			);
		}
	}
	return true;
}

#endif
