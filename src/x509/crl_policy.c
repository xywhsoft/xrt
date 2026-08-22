#include "../internal/xrt_x509.h"



#if defined(XRT_FEATURE_X509_CRL_POLICY)

/* 判断两个借用字节视图是否完全相同。 */
static bool __xrtX509CrlPolicyEqual(xbytesview Left, xbytesview Right)
{
	return (Left.Size == Right.Size) &&
		((Left.Size == 0) || (memcmp(Left.Data, Right.Data, Left.Size) == 0));
}



/* 判断扩展 OID 是否等于已知内容八位组。 */
static bool __xrtX509CrlPolicyOid(
	const xx509ext* pExtension,
	const uint8* pOid,
	size_t iOidSize
)
{
	return (pExtension->Oid.Size == iOidSize) &&
		(memcmp(pExtension->Oid.Data, pOid, iOidSize) == 0);
}



/* 设置 CRL policy 错误并保留底层原因。 */
static bool __xrtX509CrlPolicyError(
	xx509error Code,
	cstr sMessage,
	const xerror* pCause
)
{
	__xrtX509Error(
		XERR_PROTOCOL, Code, "x509-crl-policy", sMessage, SIZE_MAX, pCause
	);
	return false;
}



/* 比较任意精度非负大端整数，返回 -1、0 或 1。 */
static int __xrtX509CrlPolicyNumberCompare(
	xbytesview Left,
	xbytesview Right
)
{
	while ( (Left.Size > 1u) && (Left.Data[0] == 0) ) {
		Left.Data++;
		Left.Size--;
	}
	while ( (Right.Size > 1u) && (Right.Data[0] == 0) ) {
		Right.Data++;
		Right.Size--;
	}
	if ( Left.Size < Right.Size ) {
		return -1;
	}
	if ( Left.Size > Right.Size ) {
		return 1;
	}
	if ( Left.Size != 0 ) {
		int iOrder = memcmp(Left.Data, Right.Data, Left.Size);

		if ( iOrder < 0 ) {
			return -1;
		}
		if ( iOrder > 0 ) {
			return 1;
		}
	}
	return 0;
}



/* 判断策略时间是否在带对称时钟容差的 CRL 窗口中。 */
static bool __xrtX509CrlPolicyTime(
	const xx509crl* pCrl,
	const xx509crlconfig* pConfig
)
{
	bool bAfterStart = (pConfig->Time >= pCrl->ThisUpdate) || xrtTimeNear(
		pConfig->Time, pCrl->ThisUpdate, pConfig->Skew
	);
	bool bBeforeEnd = !pCrl->HasNextUpdate ||
		(pConfig->Time <= pCrl->NextUpdate) || xrtTimeNear(
			pConfig->Time, pCrl->NextUpdate, pConfig->Skew
		);

	return bAfterStart && bBeforeEnd;
}



/* 检查公开策略字段和未知标志。 */
static bool __xrtX509CrlPolicyConfigValid(const xx509crlconfig* pConfig)
{
	const uint32 iKnown = X509_CRL_REQUIRE_NEXT_UPDATE |
		X509_CRL_REQUIRE_NUMBER | X509_CRL_REQUIRE_AUTHORITY_KEY_ID |
		X509_CRL_REQUIRE_KEY_IDENTIFIER | X509_CRL_REQUIRE_KEY_USAGE;

	return (pConfig != NULL) && ((pConfig->Flags & ~iKnown) == 0) &&
		(((pConfig->Flags & X509_CRL_REQUIRE_KEY_IDENTIFIER) == 0) ||
		 ((pConfig->Flags & X509_CRL_REQUIRE_AUTHORITY_KEY_ID) != 0));
}



/* 在 GeneralNames 中查找与指定 Name 相同的 directoryName。 */
static xx509result __xrtX509CrlPolicyDirectory(
	xx509gencursor Names,
	xbytesview Name,
	bool bExact
)
{
	while ( true ) {
		xx509genname Candidate;
		xx509result Result = xrtX509GeneralNameRead(&Names, &Candidate);

		if ( Result != X509_VALUE ) {
			return Result;
		}
		if ( Candidate.Type == X509_NAME_DIRECTORY ) {
			if ( bExact ) {
				if ( __xrtX509CrlPolicyEqual(Candidate.Value, Name) ) {
					return X509_VALUE;
				}
			} else {
				xx509result Equal = xrtX509NameEqual(Candidate.Value, Name);

				if ( Equal != X509_DONE ) {
					return Equal;
				}
			}
		}
	}
}



/* 判断两个签发者证书是否保存同一实际公钥。 */
static xx509result __xrtX509CrlPolicyPublicKey(
	const xx509cert* pLeft,
	const xx509cert* pRight
)
{
	xx509pubkey Left;
	xx509pubkey Right;

	if ( !xrtX509PublicKey(pLeft, &Left) ||
		!xrtX509PublicKey(pRight, &Right) ) {
		return X509_ERROR;
	}
	if ( (Left.Type != Right.Type) || (Left.Curve != Right.Curve) ) {
		return X509_DONE;
	}
	return __xrtX509CrlPolicyEqual(Left.Key, Right.Key) ?
		X509_VALUE : X509_DONE;
}



/* 验证 CRL 签发者证书的 KeyUsage。 */
static bool __xrtX509CrlPolicyKeyUsage(
	const xx509cert* pIssuer,
	const xx509crlconfig* pConfig
)
{
	uint16 iUsage;
	xx509result Result = xrtX509KeyUsage(pIssuer, &iUsage);

	if ( Result == X509_ERROR ) {
		return false;
	}
	if ( Result == X509_DONE ) {
		if ( (pConfig->Flags & X509_CRL_REQUIRE_KEY_USAGE) != 0 ) {
			return __xrtX509CrlPolicyError(
				X509_ERROR_CRL_POLICY,
				"CRL issuer certificate does not contain KeyUsage", NULL
			);
		}
		return true;
	}
	if ( (iUsage & X509_USAGE_CRL_SIGN) == 0 ) {
		return __xrtX509CrlPolicyError(
			X509_ERROR_CRL_POLICY,
			"CRL issuer certificate is not authorized for cRLSign", NULL
		);
	}
	return true;
}



/* 验证 CRL AuthorityKeyIdentifier 与签发者证书对应。 */
static bool __xrtX509CrlPolicyAuthority(
	const xx509crl* pCrl,
	const xx509cert* pIssuer,
	const xx509crlconfig* pConfig
)
{
	xx509authoritykeyid Authority;
	xx509result Result = xrtX509CrlAuthorityKeyId(pCrl, &Authority);

	if ( Result == X509_ERROR ) {
		return false;
	}
	if ( Result == X509_DONE ) {
		if ( (pConfig->Flags & X509_CRL_REQUIRE_AUTHORITY_KEY_ID) != 0 ) {
			return __xrtX509CrlPolicyError(
				X509_ERROR_CRL_POLICY,
				"CRL does not contain AuthorityKeyIdentifier", NULL
			);
		}
		return true;
	}
	if ( ((pConfig->Flags & X509_CRL_REQUIRE_KEY_IDENTIFIER) != 0) &&
		!Authority.HasKeyId ) {
		return __xrtX509CrlPolicyError(
			X509_ERROR_CRL_POLICY,
			"CRL AuthorityKeyIdentifier does not use keyIdentifier", NULL
		);
	}
	if ( Authority.HasKeyId ) {
		xbytesview SubjectKeyId;

		Result = xrtX509SubjectKeyId(pIssuer, &SubjectKeyId);
		if ( Result == X509_ERROR ) {
			return false;
		}
		if ( Result == X509_VALUE ) {
			if ( !__xrtX509CrlPolicyEqual(
				Authority.KeyId, SubjectKeyId
			) ) {
				return __xrtX509CrlPolicyError(
					X509_ERROR_CRL_POLICY,
					"CRL keyIdentifier does not match issuer SubjectKeyIdentifier",
					NULL
				);
			}
		} else if ( (pConfig->Flags & X509_CRL_REQUIRE_KEY_IDENTIFIER) != 0 ) {
			return __xrtX509CrlPolicyError(
				X509_ERROR_CRL_POLICY,
				"CRL issuer certificate does not contain SubjectKeyIdentifier",
				NULL
			);
		}
	}
	if ( Authority.HasIssuer ) {
		Result = __xrtX509CrlPolicyDirectory(
			Authority.Issuer, pIssuer->Issuer, false
		);
		if ( Result == X509_ERROR ) {
			return false;
		}
		if ( (Result != X509_VALUE) || !Authority.HasSerial ||
			!__xrtX509CrlPolicyEqual(Authority.Serial, pIssuer->Serial) ) {
			return __xrtX509CrlPolicyError(
				X509_ERROR_CRL_POLICY,
				"CRL authority issuer and serial do not identify the signer",
				NULL
			);
		}
	}
	return true;
}



/* 调用用户 critical 扩展处理器并统一其三态结果。 */
static bool __xrtX509CrlPolicyCritical(
	const xx509crl* pCrl,
	const xx509crlentry* pEntry,
	const xx509ext* pExtension,
	const xx509crlconfig* pConfig
)
{
	if ( pConfig->Critical != NULL ) {
		xx509result Result = pConfig->Critical(
			pCrl, pEntry, pExtension, pConfig->UserData
		);

		if ( Result == X509_VALUE ) {
			return true;
		}
		if ( Result == X509_ERROR ) {
			const xerror* pCause = xrtGetError();

			return __xrtX509CrlPolicyError(
				X509_ERROR_CRITICAL_EXTENSION,
				"custom critical CRL extension processing failed", pCause
			);
		}
	}
	return __xrtX509CrlPolicyError(
		X509_ERROR_CRITICAL_EXTENSION,
		pEntry == NULL ?
			"CRL contains an unsupported critical extension" :
			"CRL entry contains an unsupported critical extension",
		NULL
	);
}



/* 验证 CRL 级 critical 扩展均已由内建或用户策略处理。 */
static bool __xrtX509CrlPolicyExtensions(
	const xx509crl* pCrl,
	const xx509crlconfig* pConfig
)
{
	xx509extcursor Cursor;
	xx509ext Extension;

	if ( pCrl->Extensions.Size == 0 ) {
		return true;
	}
	if ( !xrtX509ExtensionListInit(pCrl->Extensions, &Cursor) ) {
		return false;
	}
	while ( true ) {
		xx509result Result = xrtX509ExtensionRead(&Cursor, &Extension);

		if ( Result == X509_DONE ) {
			return true;
		}
		if ( Result == X509_ERROR ) {
			return false;
		}
		if ( !Extension.Critical ) {
			continue;
		}
		if ( __xrtX509CrlPolicyOid(
			&Extension, __xrtX509OidDeltaCrl, __xrtX509OidDeltaCrlSize
		) || __xrtX509CrlPolicyOid(
			&Extension, __xrtX509OidIssuingPoint,
			__xrtX509OidIssuingPointSize
		) ) {
			continue;
		}
		if ( !__xrtX509CrlPolicyCritical(
			pCrl, NULL, &Extension, pConfig
		) ) {
			return false;
		}
	}
}



/* 判断 GeneralNames 是否包含至少一个 directoryName，并保留游标解析错误。 */
static xx509result __xrtX509CrlPolicyHasDirectory(xx509gencursor Names)
{
	while ( true ) {
		xx509genname Name;
		xx509result Result = xrtX509GeneralNameRead(&Names, &Name);

		if ( Result != X509_VALUE ) {
			return Result;
		}
		if ( Name.Type == X509_NAME_DIRECTORY ) {
			return X509_VALUE;
		}
	}
}



/* 验证每项撤销记录的已知扩展、delta 语义和 unknown critical。 */
static bool __xrtX509CrlPolicyEntries(
	const xx509crl* pCrl,
	bool bDelta,
	bool bIndirect,
	const xx509crlconfig* pConfig
)
{
	xx509crlcursor Entries;
	xx509crlentry Entry;

	if ( !xrtX509CrlEntryInit(pCrl, &Entries) ) {
		return false;
	}
	while ( true ) {
		xx509result EntryResult = xrtX509CrlEntryRead(&Entries, &Entry);
		xx509crlreason Reason;
		xtime iInvalidityDate;
		xx509gencursor Issuer;

		if ( EntryResult == X509_DONE ) {
			return true;
		}
		if ( EntryResult == X509_ERROR ) {
			return false;
		}
		EntryResult = xrtX509CrlEntryReason(&Entry, &Reason);
		if ( EntryResult == X509_ERROR ) {
			return false;
		}
		if ( (EntryResult == X509_VALUE) &&
			(Reason == X509_CRL_REASON_REMOVE) && !bDelta ) {
			return __xrtX509CrlPolicyError(
				X509_ERROR_CRL_DELTA,
				"removeFromCRL may appear only in a delta CRL", NULL
			);
		}
		if ( xrtX509CrlEntryInvalidityDate(
			&Entry, &iInvalidityDate
		) == X509_ERROR ) {
			return false;
		}
		EntryResult = xrtX509CrlEntryIssuer(&Entry, &Issuer);
		if ( EntryResult == X509_ERROR ) {
			return false;
		}
		if ( EntryResult == X509_VALUE ) {
			xx509result Directory = __xrtX509CrlPolicyHasDirectory(Issuer);

			if ( Directory == X509_ERROR ) {
				return false;
			}
			if ( !bIndirect || (Directory != X509_VALUE) ) {
				return __xrtX509CrlPolicyError(
					X509_ERROR_CRL_CERTIFICATE_ISSUER,
					"CertificateIssuer requires an indirect CRL and directoryName",
					NULL
				);
			}
		}
		if ( Entry.Extensions.Size != 0 ) {
			xx509extcursor Extensions;
			xx509ext Extension;

			if ( !xrtX509ExtensionListInit(
				Entry.Extensions, &Extensions
			) ) {
				return false;
			}
			while ( true ) {
				xx509result ExtensionResult = xrtX509ExtensionRead(
					&Extensions, &Extension
				);
				bool bKnown;

				if ( ExtensionResult == X509_DONE ) {
					break;
				}
				if ( ExtensionResult == X509_ERROR ) {
					return false;
				}
				bKnown = __xrtX509CrlPolicyOid(
					&Extension, __xrtX509OidCrlReason,
					__xrtX509OidCrlReasonSize
				) || __xrtX509CrlPolicyOid(
					&Extension, __xrtX509OidInvalidityDate,
					__xrtX509OidInvalidityDateSize
				) || __xrtX509CrlPolicyOid(
					&Extension, __xrtX509OidCertificateIssuer,
					__xrtX509OidCertificateIssuerSize
				);
				if ( Extension.Critical && !bKnown &&
					!__xrtX509CrlPolicyCritical(
						pCrl, &Entry, &Extension, pConfig
					) ) {
					return false;
				}
			}
		}
	}
}



/* 初始化当前时间下的 RFC 5280 严格 CRL 策略。 */
XRT_API void xrtX509CrlConfigInit(xx509crlconfig* pConfig)
{
	if ( pConfig == NULL ) {
		__xrtErrorSetInvalidArgument();
		return;
	}
	memset(pConfig, 0, sizeof(*pConfig));
	pConfig->Time = xrtNow();
	pConfig->Flags = X509_CRL_REQUIRE_NEXT_UPDATE |
		X509_CRL_REQUIRE_NUMBER | X509_CRL_REQUIRE_AUTHORITY_KEY_ID |
		X509_CRL_REQUIRE_KEY_IDENTIFIER | X509_CRL_REQUIRE_KEY_USAGE;
}



/* 验证 CRL 的签名、签发者、时间、profile 和全部 critical 扩展。 */
XRT_API bool xrtX509CrlValidate(
	const xx509crl* pCrl,
	const xx509cert* pIssuer,
	const xx509crlconfig* pConfig,
	xx509crlvalid* pValid
)
{
	xx509crlconfig Default;
	xx509crlvalid Valid;
	xx509result Result;
	xx509ext AuthorityExtension;
	xx509ext IssuingExtension;
	xx509distributioncursor Freshest;

	if ( (pCrl == NULL) || (pIssuer == NULL) || (pValid == NULL) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	if ( pConfig == NULL ) {
		xrtX509CrlConfigInit(&Default);
		pConfig = &Default;
	}
	if ( !__xrtX509CrlPolicyConfigValid(pConfig) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	memset(&Valid, 0, sizeof(Valid));
	Valid.Crl = pCrl;
	Valid.Issuer = pIssuer;
	Result = xrtX509NameEqual(pCrl->Issuer, pIssuer->Subject);
	if ( Result == X509_ERROR ) {
		return false;
	}
	if ( Result != X509_VALUE ) {
		return __xrtX509CrlPolicyError(
			X509_ERROR_ISSUER,
			"CRL issuer Name does not match signer certificate Subject", NULL
		);
	}
	if ( ((pConfig->Flags & X509_CRL_REQUIRE_NEXT_UPDATE) != 0) &&
		!pCrl->HasNextUpdate ) {
		return __xrtX509CrlPolicyError(
			X509_ERROR_CRL_POLICY, "CRL does not contain nextUpdate", NULL
		);
	}
	if ( !__xrtX509CrlPolicyTime(pCrl, pConfig) ) {
		return __xrtX509CrlPolicyError(
			X509_ERROR_TIME, "CRL is not current at the policy time", NULL
		);
	}
	if ( !__xrtX509CrlPolicyKeyUsage(pIssuer, pConfig) ||
		!__xrtX509CrlPolicyAuthority(pCrl, pIssuer, pConfig) ) {
		return false;
	}
	Result = __xrtX509ExtensionFindValue(
		pCrl->Extensions, __xrtX509OidAuthorityKeyId,
		__xrtX509OidAuthorityKeyIdSize, &AuthorityExtension,
		"x509-crl-policy"
	);
	if ( Result == X509_ERROR ) {
		return false;
	}
	Valid.HasAuthorityKeyId = Result == X509_VALUE;
	if ( Valid.HasAuthorityKeyId ) {
		Valid.AuthorityKeyIdDer = AuthorityExtension.Value;
	}
	Result = xrtX509CrlNumber(pCrl, &Valid.Number);
	if ( Result == X509_ERROR ) {
		return false;
	}
	Valid.HasNumber = Result == X509_VALUE;
	if ( !Valid.HasNumber &&
		((pConfig->Flags & X509_CRL_REQUIRE_NUMBER) != 0) ) {
		return __xrtX509CrlPolicyError(
			X509_ERROR_CRL_NUMBER, "CRL does not contain CRLNumber", NULL
		);
	}
	Result = xrtX509CrlDeltaBase(pCrl, &Valid.BaseNumber);
	if ( Result == X509_ERROR ) {
		return false;
	}
	Valid.Delta = Result == X509_VALUE;
	if ( Valid.Delta && (!Valid.HasNumber ||
		(__xrtX509CrlPolicyNumberCompare(
			Valid.BaseNumber, Valid.Number
		) >= 0)) ) {
		return __xrtX509CrlPolicyError(
			X509_ERROR_CRL_DELTA,
			"delta CRL requires a CRLNumber greater than BaseCRLNumber", NULL
		);
	}
	Result = xrtX509CrlIssuingPoint(pCrl, &Valid.IssuingPoint);
	if ( Result == X509_ERROR ) {
		return false;
	}
	Valid.HasIssuingPoint = Result == X509_VALUE;
	if ( Valid.HasIssuingPoint ) {
		if ( Valid.IssuingPoint.HasReasons &&
			((Valid.IssuingPoint.Reasons &
			  X509_CRL_REASON_FLAG_UNUSED) != 0) ) {
			return __xrtX509CrlPolicyError(
				X509_ERROR_CRL_SCOPE,
				"IssuingDistributionPoint uses the reserved ReasonFlags bit",
				NULL
			);
		}
		Result = __xrtX509ExtensionFindValue(
			pCrl->Extensions, __xrtX509OidIssuingPoint,
			__xrtX509OidIssuingPointSize, &IssuingExtension,
			"x509-crl-policy"
		);
		if ( Result != X509_VALUE ) {
			return false;
		}
		Valid.IssuingPointDer = IssuingExtension.Value;
	}
	Result = xrtX509CrlFreshest(pCrl, &Freshest);
	if ( Result == X509_ERROR ) {
		return false;
	}
	if ( !xrtX509CrlVerify(pCrl, pIssuer) ) {
		const xerror* pCause = xrtGetError();

		return __xrtX509CrlPolicyError(
			X509_ERROR_SIGNATURE, "CRL signature verification failed", pCause
		);
	}
	if ( !__xrtX509CrlPolicyExtensions(pCrl, pConfig) ||
		!__xrtX509CrlPolicyEntries(
			pCrl, Valid.Delta,
			Valid.HasIssuingPoint && Valid.IssuingPoint.Indirect, pConfig
		) ) {
		return false;
	}
	*pValid = Valid;
	return true;
}



/* 判断两个 DistributionPointName 是否共享完全相同的名称编码。 */
static xx509result __xrtX509CrlPolicyDistributionName(
	const xx509distributionname* pLeft,
	const xx509distributionname* pRight
)
{
	if ( pLeft->Type != pRight->Type ) {
		return X509_DONE;
	}
	if ( pLeft->Type == X509_DISTRIBUTION_RELATIVE_NAME ) {
		return __xrtX509CrlPolicyEqual(pLeft->Raw, pRight->Raw) ?
			X509_VALUE : X509_DONE;
	}
	{
		xx509gencursor Left = pLeft->FullNames;

		while ( true ) {
			xx509genname LeftName;
			xx509result Result = xrtX509GeneralNameRead(&Left, &LeftName);

			if ( Result != X509_VALUE ) {
				return Result;
			}
			{
				xx509gencursor Right = pRight->FullNames;

				while ( true ) {
					xx509genname RightName;
					xx509result RightResult = xrtX509GeneralNameRead(
						&Right, &RightName
					);

					if ( RightResult == X509_ERROR ) {
						return X509_ERROR;
					}
					if ( RightResult == X509_DONE ) {
						break;
					}
					if ( __xrtX509CrlPolicyEqual(
						LeftName.Raw, RightName.Raw
					) ) {
						return X509_VALUE;
					}
				}
			}
		}
	}
}



/* 判断 fullName 分发点是否包含 cRLIssuer 中的任一相同名称。 */
static xx509result __xrtX509CrlPolicyDistributionIssuer(
	const xx509distributionname* pDistribution,
	xx509gencursor Issuers
)
{
	if ( pDistribution->Type != X509_DISTRIBUTION_FULL_NAME ) {
		return X509_DONE;
	}
	{
		xx509gencursor Names = pDistribution->FullNames;

		while ( true ) {
			xx509genname Name;
			xx509result Result = xrtX509GeneralNameRead(&Names, &Name);

			if ( Result != X509_VALUE ) {
				return Result;
			}
			{
				xx509gencursor Candidates = Issuers;

				while ( true ) {
					xx509genname Candidate;
					xx509result CandidateResult = xrtX509GeneralNameRead(
						&Candidates, &Candidate
					);

					if ( CandidateResult == X509_ERROR ) {
						return X509_ERROR;
					}
					if ( CandidateResult == X509_DONE ) {
						break;
					}
					if ( __xrtX509CrlPolicyEqual(
						Name.Raw, Candidate.Raw
					) ) {
						return X509_VALUE;
					}
				}
			}
		}
	}
}



/* 匹配 RFC 5280 回退路径生成的 Issuer 与 IssuerAltName fullName。 */
static xx509result __xrtX509CrlPolicyDefaultDistribution(
	const xx509distributionname* pDistribution,
	const xx509cert* pCertificate
)
{
	xx509gencursor Alternatives;
	xx509result AlternativeResult;

	if ( pDistribution->Type != X509_DISTRIBUTION_FULL_NAME ) {
		return X509_DONE;
	}
	AlternativeResult = xrtX509IssuerAltName(pCertificate, &Alternatives);
	if ( AlternativeResult == X509_ERROR ) {
		return X509_ERROR;
	}
	{
		xx509gencursor Names = pDistribution->FullNames;

		while ( true ) {
			xx509genname Name;
			xx509result Result = xrtX509GeneralNameRead(&Names, &Name);

			if ( Result != X509_VALUE ) {
				return Result;
			}
			if ( Name.Type == X509_NAME_DIRECTORY ) {
				Result = xrtX509NameEqual(Name.Value, pCertificate->Issuer);
				if ( Result != X509_DONE ) {
					return Result;
				}
			}
			if ( AlternativeResult == X509_VALUE ) {
				xx509gencursor Candidates = Alternatives;

				while ( true ) {
					xx509genname Candidate;
					xx509result CandidateResult = xrtX509GeneralNameRead(
						&Candidates, &Candidate
					);

					if ( CandidateResult == X509_ERROR ) {
						return X509_ERROR;
					}
					if ( CandidateResult == X509_DONE ) {
						break;
					}
					if ( __xrtX509CrlPolicyEqual(
						Name.Raw, Candidate.Raw
					) ) {
						return X509_VALUE;
					}
				}
			}
		}
	}
}



/* 计算一张证书由当前 CRL 覆盖的原因集合。 */
static xx509result __xrtX509CrlPolicyScope(
	const xx509crlvalid* pValid,
	const xx509cert* pCertificate,
	uint16* pReasons
)
{
	const xx509crl* pCrl = pValid->Crl;
	uint16 iCrlReasons = pValid->HasIssuingPoint &&
		pValid->IssuingPoint.HasReasons ? pValid->IssuingPoint.Reasons :
		X509_CRL_REASON_FLAG_ALL;
	bool bIndirect = pValid->HasIssuingPoint &&
		pValid->IssuingPoint.Indirect;
	xx509result SameIssuer = xrtX509NameEqual(
		pCertificate->Issuer, pCrl->Issuer
	);
	xx509distributioncursor Points;
	xx509result Result;

	if ( SameIssuer == X509_ERROR ) {
		return X509_ERROR;
	}
	if ( pValid->HasIssuingPoint ) {
		const xx509issuingpoint* pPoint = &pValid->IssuingPoint;

		if ( pPoint->OnlyAttributeCertificates ) {
			return X509_DONE;
		}
		if ( pPoint->OnlyUserCertificates || pPoint->OnlyCaCertificates ) {
			xx509basicconstraints Constraints;
			bool bCa = false;

			Result = xrtX509BasicConstraints(pCertificate, &Constraints);
			if ( Result == X509_ERROR ) {
				return X509_ERROR;
			}
			if ( Result == X509_VALUE ) {
				bCa = Constraints.CA;
			}
			if ( (pPoint->OnlyUserCertificates && bCa) ||
				(pPoint->OnlyCaCertificates && !bCa) ) {
				return X509_DONE;
			}
		}
	}
	if ( !bIndirect && (SameIssuer != X509_VALUE) ) {
		return X509_DONE;
	}
	Result = xrtX509CrlPoints(pCertificate, &Points);
	if ( Result == X509_ERROR ) {
		return X509_ERROR;
	}
	if ( Result == X509_DONE ) {
		if ( SameIssuer != X509_VALUE ) {
			return X509_DONE;
		}
		if ( pValid->HasIssuingPoint &&
			pValid->IssuingPoint.HasDistributionPoint ) {
			Result = __xrtX509CrlPolicyDefaultDistribution(
				&pValid->IssuingPoint.DistributionPoint, pCertificate
			);
			if ( Result != X509_VALUE ) {
				return Result;
			}
		}
		*pReasons = iCrlReasons;
		return X509_VALUE;
	}
	{
		uint16 iCovered = 0;

		while ( true ) {
			xx509distributionpoint Point;
			xx509result ReadResult = xrtX509DistributionRead(&Points, &Point);
			bool bIssuer;
			uint16 iPointReasons;

			if ( ReadResult == X509_ERROR ) {
				return X509_ERROR;
			}
			if ( ReadResult == X509_DONE ) {
				break;
			}
			if ( Point.HasIssuer ) {
				if ( !bIndirect ) {
					continue;
				}
				ReadResult = __xrtX509CrlPolicyDirectory(
					Point.Issuer, pCrl->Issuer, true
				);
				if ( ReadResult == X509_ERROR ) {
					return X509_ERROR;
				}
				bIssuer = ReadResult == X509_VALUE;
			} else {
				bIssuer = SameIssuer == X509_VALUE;
			}
			if ( !bIssuer ) {
				continue;
			}
			if ( Point.HasReasons &&
				((Point.Reasons & X509_CRL_REASON_FLAG_UNUSED) != 0) ) {
				__xrtX509CrlPolicyError(
					X509_ERROR_CRL_SCOPE,
					"CRLDistributionPoints uses the reserved ReasonFlags bit",
					NULL
				);
				return X509_ERROR;
			}
			if ( pValid->HasIssuingPoint &&
				pValid->IssuingPoint.HasDistributionPoint ) {
				if ( Point.HasName ) {
					ReadResult = __xrtX509CrlPolicyDistributionName(
						&Point.Name,
						&pValid->IssuingPoint.DistributionPoint
					);
				} else {
					ReadResult = __xrtX509CrlPolicyDistributionIssuer(
						&pValid->IssuingPoint.DistributionPoint,
						Point.Issuer
					);
				}
				if ( ReadResult == X509_ERROR ) {
					return X509_ERROR;
				}
				if ( ReadResult != X509_VALUE ) {
					continue;
				}
			}
			iPointReasons = Point.HasReasons ? Point.Reasons :
				X509_CRL_REASON_FLAG_ALL;
			iCovered |= (uint16)(iPointReasons & iCrlReasons);
		}
		if ( iCovered == 0 ) {
			if ( SameIssuer != X509_VALUE ) {
				return X509_DONE;
			}
			if ( pValid->HasIssuingPoint &&
				pValid->IssuingPoint.HasDistributionPoint ) {
				Result = __xrtX509CrlPolicyDefaultDistribution(
					&pValid->IssuingPoint.DistributionPoint, pCertificate
				);
				if ( Result != X509_VALUE ) {
					return Result;
				}
			}
			iCovered = iCrlReasons;
		}
		*pReasons = iCovered;
		return X509_VALUE;
	}
}



/* 判断间接 CRL 当前条目发行者是否与目标证书 Issuer 相同。 */
static xx509result __xrtX509CrlPolicyEntryIssuer(
	const xx509crlvalid* pValid,
	const xx509cert* pCertificate,
	bool bExplicit,
	xx509gencursor Issuers
)
{
	if ( bExplicit ) {
		return __xrtX509CrlPolicyDirectory(
			Issuers, pCertificate->Issuer, true
		);
	}
	return xrtX509NameEqual(
		pValid->Crl->Issuer, pCertificate->Issuer
	);
}



/* 使用已验证 CRL 查询证书；不适用或 delta 中无更新时返回 X509_DONE。 */
XRT_API xx509result xrtX509CrlCheck(
	const xx509crlvalid* pValid,
	const xx509cert* pCertificate,
	xx509revocation* pRevocation
)
{
	xx509revocation Revocation;
	xx509crlcursor Entries;
	xx509crlentry Entry;
	uint16 iReasons;
	xx509result Result;
	bool bIndirect;
	bool bExplicitIssuer = false;
	xx509gencursor EffectiveIssuer = { 0 };

	if ( (pValid == NULL) || (pValid->Crl == NULL) ||
		(pValid->Issuer == NULL) || (pCertificate == NULL) ||
		(pRevocation == NULL) ) {
		__xrtErrorSetInvalidArgument();
		return X509_ERROR;
	}
	Result = __xrtX509CrlPolicyScope(pValid, pCertificate, &iReasons);
	if ( Result != X509_VALUE ) {
		return Result;
	}
	memset(&Revocation, 0, sizeof(Revocation));
	Revocation.CoveredReasons = iReasons;
	bIndirect = pValid->HasIssuingPoint && pValid->IssuingPoint.Indirect;
	if ( !xrtX509CrlEntryInit(pValid->Crl, &Entries) ) {
		return X509_ERROR;
	}
	while ( true ) {
		Result = xrtX509CrlEntryRead(&Entries, &Entry);
		if ( Result == X509_ERROR ) {
			return X509_ERROR;
		}
		if ( Result == X509_DONE ) {
			break;
		}
		if ( bIndirect ) {
			xx509gencursor Issuer;
			xx509result IssuerResult = xrtX509CrlEntryIssuer(
				&Entry, &Issuer
			);

			if ( IssuerResult == X509_ERROR ) {
				return X509_ERROR;
			}
			if ( IssuerResult == X509_VALUE ) {
				EffectiveIssuer = Issuer;
				bExplicitIssuer = true;
			}
		}
		if ( (Entry.Serial.Size != pCertificate->Serial.Size) ||
			(memcmp(
				Entry.Serial.Data, pCertificate->Serial.Data,
				Entry.Serial.Size
			) != 0) ) {
			continue;
		}
		Result = __xrtX509CrlPolicyEntryIssuer(
			pValid, pCertificate, bExplicitIssuer, EffectiveIssuer
		);
		if ( Result == X509_ERROR ) {
			return X509_ERROR;
		}
		if ( Result != X509_VALUE ) {
			continue;
		}
		Revocation.Entry = Entry;
		Result = xrtX509CrlEntryReason(&Entry, &Revocation.Reason);
		if ( Result == X509_ERROR ) {
			return X509_ERROR;
		}
		Revocation.HasReason = Result == X509_VALUE;
		Result = xrtX509CrlEntryInvalidityDate(
			&Entry, &Revocation.InvalidityDate
		);
		if ( Result == X509_ERROR ) {
			return X509_ERROR;
		}
		Revocation.HasInvalidityDate = Result == X509_VALUE;
		Revocation.State = Revocation.HasReason &&
			(Revocation.Reason == X509_CRL_REASON_REMOVE) ?
			X509_REVOCATION_REMOVED : X509_REVOCATION_REVOKED;
		*pRevocation = Revocation;
		return X509_VALUE;
	}
	if ( pValid->Delta ) {
		return X509_DONE;
	}
	Revocation.State = X509_REVOCATION_GOOD;
	*pRevocation = Revocation;
	return X509_VALUE;
}



/* 验证并查询一张证书，适合单次检查。 */
XRT_API xx509result xrtX509CrlStatus(
	const xx509crl* pCrl,
	const xx509cert* pIssuer,
	const xx509cert* pCertificate,
	const xx509crlconfig* pConfig,
	xx509revocation* pRevocation
)
{
	xx509crlvalid Valid;

	if ( !xrtX509CrlValidate(pCrl, pIssuer, pConfig, &Valid) ) {
		return X509_ERROR;
	}
	return xrtX509CrlCheck(&Valid, pCertificate, pRevocation);
}



/* 组合相同签发者和作用域的 complete/delta CRL 借用视图。 */
XRT_API bool xrtX509CrlSetInit(
	xx509crlset* pSet,
	const xx509crlvalid* pBase,
	const xx509crlvalid* pDelta
)
{
	xx509result Result;
	xx509result SameKey;

	if ( (pSet == NULL) || (pBase == NULL) || (pDelta == NULL) ||
		(pBase->Crl == NULL) || (pBase->Issuer == NULL) ||
		(pDelta->Crl == NULL) || (pDelta->Issuer == NULL) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	if ( pBase->Delta || !pDelta->Delta || !pBase->HasNumber ||
		!pDelta->HasNumber || (pDelta->BaseNumber.Size == 0) ) {
		return __xrtX509CrlPolicyError(
			X509_ERROR_CRL_DELTA,
			"CRL set requires numbered complete and delta CRLs", NULL
		);
	}
	Result = xrtX509NameEqual(pBase->Crl->Issuer, pDelta->Crl->Issuer);
	if ( Result == X509_ERROR ) {
		return false;
	}
	SameKey = __xrtX509CrlPolicyPublicKey(pBase->Issuer, pDelta->Issuer);
	if ( SameKey == X509_ERROR ) {
		return false;
	}
	if ( (Result != X509_VALUE) || (SameKey != X509_VALUE) ||
		(pBase->HasAuthorityKeyId != pDelta->HasAuthorityKeyId) ||
		(pBase->HasAuthorityKeyId && !__xrtX509CrlPolicyEqual(
			pBase->AuthorityKeyIdDer, pDelta->AuthorityKeyIdDer
		)) || (pBase->HasIssuingPoint != pDelta->HasIssuingPoint) ||
		(pBase->HasIssuingPoint && !__xrtX509CrlPolicyEqual(
			pBase->IssuingPointDer, pDelta->IssuingPointDer
		)) || (__xrtX509CrlPolicyNumberCompare(
		pBase->Number, pDelta->BaseNumber
	) < 0) || (__xrtX509CrlPolicyNumberCompare(
		pBase->Number, pDelta->Number
	) >= 0) ) {
		return __xrtX509CrlPolicyError(
			X509_ERROR_CRL_DELTA,
			"complete and delta CRLs have incompatible issuer, scope or numbers",
			NULL
		);
	}
	pSet->Base = *pBase;
	pSet->Delta = *pDelta;
	return true;
}



/* 按 delta 优先、base 回退的规则查询组合 CRL。 */
XRT_API xx509result xrtX509CrlSetCheck(
	const xx509crlset* pSet,
	const xx509cert* pCertificate,
	xx509revocation* pRevocation
)
{
	xx509revocation Revocation;
	xx509result Result;

	if ( (pSet == NULL) || (pCertificate == NULL) ||
		(pRevocation == NULL) ) {
		__xrtErrorSetInvalidArgument();
		return X509_ERROR;
	}
	Result = xrtX509CrlCheck(&pSet->Delta, pCertificate, &Revocation);
	if ( Result == X509_ERROR ) {
		return X509_ERROR;
	}
	if ( Result == X509_VALUE ) {
		if ( Revocation.State == X509_REVOCATION_REMOVED ) {
			Revocation.State = X509_REVOCATION_GOOD;
		}
		*pRevocation = Revocation;
		return X509_VALUE;
	}
	return xrtX509CrlCheck(
		&pSet->Base, pCertificate, pRevocation
	);
}



/* 初始化多份分段 CRL 的零分配状态归并器。 */
XRT_API void xrtX509RevocationInit(xx509revocationcheck* pCheck)
{
	if ( pCheck == NULL ) {
		__xrtErrorSetInvalidArgument();
		return;
	}
	memset(pCheck, 0, sizeof(*pCheck));
}



/* 加入一份 CRL 查询结果，并只在最终状态确定后发布 VALUE。 */
XRT_API xx509result xrtX509RevocationUpdate(
	xx509revocationcheck* pCheck,
	const xx509revocation* pRevocation
)
{
	if ( (pCheck == NULL) || (pRevocation == NULL) ||
		((pRevocation->CoveredReasons & ~X509_CRL_REASON_FLAG_ALL) != 0) ||
		(pRevocation->CoveredReasons == 0) ||
		(pRevocation->State < X509_REVOCATION_GOOD) ||
		(pRevocation->State > X509_REVOCATION_REMOVED) ) {
		__xrtErrorSetInvalidArgument();
		return X509_ERROR;
	}
	if ( pCheck->Determined ) {
		return X509_VALUE;
	}
	if ( pRevocation->State == X509_REVOCATION_REMOVED ) {
		__xrtX509CrlPolicyError(
			X509_ERROR_REVOCATION,
			"removeFromCRL requires complete/delta combination before aggregation",
			NULL
		);
		return X509_ERROR;
	}
	pCheck->CoveredReasons |= pRevocation->CoveredReasons;
	if ( pRevocation->State == X509_REVOCATION_REVOKED ) {
		pCheck->Revocation = *pRevocation;
		pCheck->Revocation.CoveredReasons = pCheck->CoveredReasons;
		pCheck->Determined = true;
		return X509_VALUE;
	}
	if ( pCheck->CoveredReasons == X509_CRL_REASON_FLAG_ALL ) {
		memset(&pCheck->Revocation, 0, sizeof(pCheck->Revocation));
		pCheck->Revocation.State = X509_REVOCATION_GOOD;
		pCheck->Revocation.CoveredReasons = pCheck->CoveredReasons;
		pCheck->Determined = true;
		return X509_VALUE;
	}
	return X509_DONE;
}



/* 读取原因范围已经完整或已确认撤销的最终状态。 */
XRT_API xx509result xrtX509RevocationResult(
	const xx509revocationcheck* pCheck,
	xx509revocation* pRevocation
)
{
	if ( (pCheck == NULL) || (pRevocation == NULL) ) {
		__xrtErrorSetInvalidArgument();
		return X509_ERROR;
	}
	if ( !pCheck->Determined ) {
		return X509_DONE;
	}
	*pRevocation = pCheck->Revocation;
	return X509_VALUE;
}

#endif
