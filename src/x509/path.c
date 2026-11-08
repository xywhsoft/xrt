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



/* 普通验证拒绝只生成值；错误对象由旧 bool 适配入口按需创建。 */
static xrt_x509_decision __xrtX509PathReject(xx509error Code, cstr Message)
{
	return __xrtX509Decision(X509_CHECK_REJECTED, Code, Message);
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
static xrt_x509_decision __xrtX509PathSignature(
	const xx509cert* pCertificate,
	const xx509pathconfig* pConfig
)
{
	xx509signature Signature;
	xx509result Result = xrtX509SignatureParse(
		&pCertificate->SignatureAlgorithm,
		&Signature
	);

	if ( Result == X509_ERROR ) {
		return __xrtX509CheckError();
	}
	if ( Result != X509_VALUE ) {
		return __xrtX509Decision(X509_CHECK_UNSUPPORTED, X509_ERROR_SIGNATURE_ALGORITHM,
			"certificate path uses an unsupported signature algorithm");
	}
	if ( (Signature.Hash == X509_HASH_SHA1) &&
		((pConfig->Flags & X509_PATH_ALLOW_SHA1) == 0) ) {
		return __xrtX509PathReject(
			X509_ERROR_SIGNATURE_ALGORITHM,
			"certificate path uses SHA-1 without explicit legacy policy"
		);
	}
	return __xrtX509CheckValid();
}



/* 验证一张证书中当前路径层已经理解的扩展。 */
static xrt_x509_decision __xrtX509PathExtensions(
	const xx509cert* pCertificate,
	size_t iDepth,
	const xx509pathconfig* pConfig
)
{
	xx509extcursor Cursor;
	xx509ext Extension;

	if ( !xrtX509ExtensionInit(pCertificate, &Cursor) ) {
		return __xrtX509CheckError();
	}
	while ( true ) {
		xx509result ReadResult = xrtX509ExtensionRead(&Cursor, &Extension);
		xx509result ParseResult = X509_DONE;
		bool bKnown = true;

		if ( ReadResult == X509_DONE ) {
			return __xrtX509CheckValid();
		}
		if ( ReadResult == X509_ERROR ) {
			return __xrtX509CheckError();
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
				return __xrtX509CheckError();
			}
			if ( ParseResult != X509_VALUE ) {
				__xrtX509Error(XERR_STATE, X509_ERROR_PATH, "x509-path",
					"a recognized certificate extension disappeared during processing", SIZE_MAX, NULL);
				return __xrtX509CheckError();
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
				/* Explicit callback failure is not a validation rejection. */
				return __xrtX509CheckError();
			}
			if ( Result != X509_DONE ) {
				__xrtErrorSetInvalidArgument();
				return __xrtX509CheckError();
			}
		}
		return __xrtX509PathReject(
			X509_ERROR_CRITICAL_EXTENSION,
			"certificate contains an unsupported critical extension"
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
static xrt_x509_decision __xrtX509PathPurpose(
	const xx509cert* pCertificate,
	const xx509pathconfig* pConfig
)
{
	if ( (pConfig->KeyUsage != 0) ||
		((pConfig->Flags & X509_PATH_REQUIRE_KEY_USAGE) != 0) ) {
		uint16 iUsage;
		xx509result Result = xrtX509KeyUsage(pCertificate, &iUsage);

		if ( Result == X509_ERROR ) {
			return __xrtX509CheckError();
		}
		if ( Result == X509_DONE ) {
			if ( (pConfig->Flags & X509_PATH_REQUIRE_KEY_USAGE) == 0 ) {
				goto Extended;
			}
			return __xrtX509PathReject(
				X509_ERROR_KEY_USAGE,
				"target certificate does not contain required KeyUsage"
			);
		}
		if ( (iUsage & pConfig->KeyUsage) != pConfig->KeyUsage ) {
			return __xrtX509PathReject(
				X509_ERROR_KEY_USAGE,
				"target certificate KeyUsage does not allow the requested use"
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
			return __xrtX509CheckError();
		}
		if ( !bPresent ) {
			if ( (pConfig->Flags & X509_PATH_REQUIRE_PURPOSE) == 0 ) {
				return __xrtX509CheckValid();
			}
			return __xrtX509PathReject(
				X509_ERROR_PURPOSE,
				"target certificate does not contain required ExtendedKeyUsage"
			);
		}
		if ( Result != X509_VALUE ) {
			return __xrtX509PathReject(
				X509_ERROR_PURPOSE,
				"target certificate ExtendedKeyUsage does not allow the requested use"
			);
		}
	}
	return __xrtX509CheckValid();
}



/* 验证一张中间证书的 CA、KeyUsage 和 pathLenConstraint。 */
static xrt_x509_decision __xrtX509PathCa(
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
		return __xrtX509CheckError();
	}
	if ( (Result != X509_VALUE) || !Constraints.CA ) {
		return __xrtX509PathReject(
			X509_ERROR_PATH_CONSTRAINT,
			"intermediate certificate is not authorized as a CA"
		);
	}
	if ( !xrtX509ExtensionFind(
		pCertificate, __xrtX509OidBasicConstraints,
		__xrtX509OidBasicConstraintsSize, &Extension
	) ) {
		return __xrtX509CheckError();
	}
	if ( !Extension.Critical ) {
		return __xrtX509PathReject(
			X509_ERROR_PATH_CONSTRAINT,
			"intermediate CA BasicConstraints is not critical"
		);
	}
	{
		xx509namecursor Name;
		xx509nameattr Attribute;

		if ( !xrtX509NameInit(pCertificate->Subject, &Name) ) {
			return __xrtX509CheckError();
		}
		Result = xrtX509NameRead(&Name, &Attribute);
		if ( Result == X509_ERROR ) {
			return __xrtX509CheckError();
		}
		if ( Result == X509_DONE ) {
			return __xrtX509PathReject(
				X509_ERROR_PATH_CONSTRAINT,
				"intermediate CA subject name is empty"
			);
		}
	}
	{
		uint16 iUsage;

		Result = xrtX509KeyUsage(pCertificate, &iUsage);
		if ( Result == X509_ERROR ) {
			return __xrtX509CheckError();
		}
		if ( (Result == X509_VALUE) &&
			((iUsage & X509_USAGE_CERT_SIGN) == 0) ) {
			return __xrtX509PathReject(
				X509_ERROR_PATH_CONSTRAINT,
				"intermediate certificate KeyUsage forbids certificate signing"
			);
		}
		if ( Constraints.HasPathLimit && (Result != X509_VALUE) ) {
			return __xrtX509PathReject(
				X509_ERROR_PATH_CONSTRAINT,
				"CA pathLenConstraint requires KeyUsage keyCertSign"
			);
		}
	}
	if ( Constraints.HasPathLimit && (iCaBelow > Constraints.PathLimit) ) {
		return __xrtX509PathReject(
			X509_ERROR_PATH_CONSTRAINT,
			"intermediate certificate pathLenConstraint was exceeded"
		);
	}
	if ( pConfig->Purpose.Size != 0 ) {
		bool bPresent;

		Result = __xrtX509PathExtendedPurpose(
			pCertificate, pConfig->Purpose, &bPresent
		);
		if ( Result == X509_ERROR ) {
			return __xrtX509CheckError();
		}
		if ( bPresent && (Result != X509_VALUE) ) {
			return __xrtX509PathReject(
				X509_ERROR_PURPOSE,
				"intermediate CA ExtendedKeyUsage forbids the requested purpose"
			);
		}
	}
	return __xrtX509CheckValid();
}



/* 判断一张证书是否是可从名称约束检查中跳过的 self-issued 中间证书。 */
static xx509result __xrtX509PathSelfIssued(const xx509cert* pCertificate)
{
	return xrtX509NameEqual(pCertificate->Subject, pCertificate->Issuer);
}



/* 把一组名称约束应用到目标及全部非 self-issued 下级中间证书。 */
static xrt_x509_decision __xrtX509PathApplyNameConstraints(
	const xx509nameconstraints* pConstraints,
	const xx509cert* const* ppCertificates,
	size_t iCount
)
{
	for ( size_t i = 0; i < iCount; i++ ) {
		xrt_x509_decision Decision;
		if ( i != 0 ) {
			xx509result Result = __xrtX509PathSelfIssued(ppCertificates[i]);

			if ( Result == X509_ERROR ) {
				return __xrtX509CheckError();
			}
			if ( Result == X509_VALUE ) {
				continue;
			}
		}
		Decision = __xrtX509NameConstraintsEvaluate(
			pConstraints, ppCertificates[i]
		);
		if ( Decision.Result != X509_CHECK_VALID ) {
			Decision.Depth = i;
			return Decision;
		}
	}
	return __xrtX509CheckValid();
}



/* 验证信任锚和每张 CA 证书向路径下方施加的名称约束。 */
static xrt_x509_decision __xrtX509PathNameConstraints(
	const xx509cert* const* ppCertificates,
	size_t iCount,
	const xx509anchor* pAnchor
)
{
	xrt_x509_decision Decision;
	if ( pAnchor->HasNameConstraints ) {
		Decision = __xrtX509PathApplyNameConstraints(
			&pAnchor->NameConstraints, ppCertificates, iCount
		);
		if ( Decision.Result != X509_CHECK_VALID ) {
			return Decision;
		}
	}
	for ( size_t i = 0; i < iCount; i++ ) {
		xx509nameconstraints Constraints;
		xx509result Result = xrtX509NameConstraints(
			ppCertificates[i], &Constraints
		);

		if ( Result == X509_ERROR ) {
			return __xrtX509CheckError();
		}
		if ( Result == X509_DONE ) {
			continue;
		}
		if ( i == 0 ) {
			Decision = __xrtX509PathReject(X509_ERROR_NAME_CONSTRAINTS,
				"NameConstraints is only valid in a CA certificate");
			Decision.Depth = i;
			return Decision;
		}
		Decision = __xrtX509PathApplyNameConstraints(
			&Constraints, ppCertificates, i
		);
		if ( Decision.Result != X509_CHECK_VALID ) {
			return Decision;
		}
	}
	return __xrtX509CheckValid();
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
xrt_x509_decision __xrtX509PathEvaluate(
	const xx509cert* const* ppCertificates,
	size_t iCount,
	const xx509anchor* pAnchor,
	const xx509pathconfig* pConfig
)
{
	size_t iCaBelow = 0;
	xrt_x509_decision Decision;

	if ( (ppCertificates == NULL) || (iCount == 0) ||
		(pAnchor == NULL) || !__xrtX509PathConfigValid(pConfig) ||
		(pAnchor->Name.Data == NULL) || (pAnchor->Name.Size == 0) ||
		((pAnchor->Certificate.Data == NULL) &&
		 (pAnchor->Certificate.Size != 0)) ) {
		__xrtErrorSetInvalidArgument();
		return __xrtX509CheckError();
	}
	for ( size_t i = 0; i < iCount; i++ ) {
		const xx509cert* pCertificate = ppCertificates[i];

		if ( (pCertificate == NULL) || (pCertificate->Raw.Data == NULL) ||
			(pCertificate->Raw.Size == 0) ) {
			__xrtErrorSetInvalidArgument();
			return __xrtX509CheckError();
		}
		for ( size_t j = 0; j < i; j++ ) {
			if ( __xrtX509PathDuplicate(pCertificate, ppCertificates[j]) ) {
				Decision = __xrtX509PathReject(
					X509_ERROR_PATH,
					"a certificate appears more than once in the path"
				);
				Decision.Depth = i;
				return Decision;
			}
		}
		if ( (pAnchor->Certificate.Size != 0) && __xrtX509PathEqual(
			pCertificate->Raw, pAnchor->Certificate
		) ) {
			Decision = __xrtX509PathReject(
				X509_ERROR_PATH,
				"the trust anchor certificate must not appear in the path"
			);
			Decision.Depth = i;
			return Decision;
		}
		if ( !xrtX509ValidAt(pCertificate, pConfig->Time) ) {
			Decision = __xrtX509PathReject(
				X509_ERROR_TIME,
				"certificate is not valid at the requested time"
			);
			Decision.Depth = i;
			return Decision;
		}
		Decision = __xrtX509PathSignature(pCertificate, pConfig);
		if ( Decision.Result != X509_CHECK_VALID ) {
			Decision.Depth = i;
			return Decision;
		}
		Decision = __xrtX509PathExtensions(pCertificate, i, pConfig);
		if ( Decision.Result != X509_CHECK_VALID ) {
			Decision.Depth = i;
			return Decision;
		}
	}
	Decision = __xrtX509PathPurpose(ppCertificates[0], pConfig);
	if ( Decision.Result != X509_CHECK_VALID ) {
		Decision.Depth = 0;
		return Decision;
	}
	for ( size_t i = 1; i < iCount; i++ ) {
		const xx509cert* pCertificate = ppCertificates[i];
		xx509result NameResult;

		Decision = __xrtX509PathCa(pCertificate, iCaBelow, pConfig);
		if ( Decision.Result != X509_CHECK_VALID ) {
			Decision.Depth = i;
			return Decision;
		}
		NameResult = xrtX509NameEqual(
			pCertificate->Subject, pCertificate->Issuer
		);
		if ( NameResult == X509_ERROR ) {
			return __xrtX509CheckError();
		}
		if ( NameResult == X509_DONE ) {
			iCaBelow++;
		}
	}
	Decision = __xrtX509PathNameConstraints(
		ppCertificates, iCount, pAnchor
	);
	if ( Decision.Result != X509_CHECK_VALID ) {
		return Decision;
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
				return __xrtX509CheckError();
			}
			pIssuerKey = &IssuerKey;
		} else {
			IssuerName = pAnchor->Name;
			pIssuerKey = &pAnchor->PublicKey;
		}
		NameResult = xrtX509NameEqual(pCertificate->Issuer, IssuerName);
		if ( NameResult == X509_ERROR ) {
			return __xrtX509CheckError();
		}
		if ( NameResult == X509_DONE ) {
			Decision = __xrtX509PathReject(
				X509_ERROR_ISSUER,
				"certificate issuer does not match the next path name"
			);
			Decision.Depth = i;
			return Decision;
		}
		Decision = __xrtX509CertificateEvaluate(pCertificate, pIssuerKey);
		if ( Decision.Result != X509_CHECK_VALID ) {
			Decision.Depth = i;
			return Decision;
		}
	}
	return __xrtX509CheckValid();
}

XRT_API xx509check xrtX509PathCheck(
	const xx509cert* const* ppCertificates, size_t iCount,
	const xx509anchor* pAnchor, const xx509pathconfig* pConfig,
	xx509pathdecision* pDecision
)
{
	xrt_x509_decision Decision;
	if ( pDecision == NULL ) {
		__xrtErrorSetInvalidArgument();
		return X509_CHECK_ERROR;
	}
	Decision = __xrtX509PathEvaluate(ppCertificates, iCount, pAnchor, pConfig);
	if ( Decision.Result != X509_CHECK_ERROR ) {
		pDecision->Reason = Decision.Reason;
		pDecision->Depth = Decision.Depth;
		pDecision->Message = Decision.Message;
	}
	return Decision.Result;
}

XRT_API bool xrtX509PathValidate(
	const xx509cert* const* ppCertificates, size_t iCount,
	const xx509anchor* pAnchor, const xx509pathconfig* pConfig
)
{
	xrt_x509_decision Decision = __xrtX509PathEvaluate(
		ppCertificates, iCount, pAnchor, pConfig);
	if ( Decision.Result == X509_CHECK_VALID ) {
		return true;
	}
	if ( Decision.Result == X509_CHECK_ERROR ) {
		return false;
	}
	/* Keep legacy path error categories; typed checks expose precise reasons. */
	if ( Decision.Reason == X509_ERROR_TIME ) {
		Decision.Reason = X509_ERROR_PATH;
	} else if ( Decision.Reason == X509_ERROR_KEY_USAGE ) {
		Decision.Reason = X509_ERROR_PURPOSE;
	} else if ( Decision.Reason == X509_ERROR_SIGNATURE_ALGORITHM ) {
		Decision.Reason = X509_ERROR_SIGNATURE;
	}
	if ( Decision.Reason == X509_ERROR_SIGNATURE ||
		Decision.Reason == X509_ERROR_NAME_CONSTRAINTS ) {
		xerror* Cause;
		if ( !__xrtX509DecisionRaise(&Decision, "x509-path") ) {
			return false;
		}
		Cause = xrtTakeError();
		__xrtX509Error(XERR_PROTOCOL, Decision.Reason, "x509-path",
			Decision.Message, SIZE_MAX, Cause);
		xrtErrorFree(Cause);
	} else {
		(void)__xrtX509DecisionRaise(&Decision, "x509-path");
	}
	return false;
}

#endif
