#include "../internal/xrt_x509.h"



#if defined(XRT_FEATURE_X509_NAME_CONSTRAINTS)

static const uint8 __xrtX509ConstraintEmailOid[] = {
	0x2A, 0x86, 0x48, 0x86, 0xF7, 0x0D, 0x01, 0x09, 0x01
};



/* 返回去除可选前导点的约束域名并验证 DNS 标签。 */
static bool __xrtX509ConstraintDomain(
	xbytesview Value,
	bool bLeadingDot,
	xbytesview* pDomain,
	bool* pSubdomains
)
{
	xbytesview Domain = Value;
	size_t iSize;
	bool bWildcard;
	bool bSubdomains = false;

	if ( bLeadingDot && (Domain.Size != 0) &&
		(Domain.Data[0] == (uint8)'.') ) {
		Domain.Data++;
		Domain.Size--;
		bSubdomains = true;
	}
	if ( !__xrtX509DnsName(
		Domain, false, &iSize, &bWildcard
	) ) {
		return false;
	}
	Domain.Size = iSize;
	*pDomain = Domain;
	*pSubdomains = bSubdomains;
	return true;
}



/* 判断一个已验证 DNS 名是否位于域名基点内。 */
static bool __xrtX509ConstraintDomainWithin(
	xbytesview Name,
	xbytesview Base,
	bool bSubdomainsOnly
)
{
	if ( (Name.Size == Base.Size) && __xrtX509AsciiEqual(Name, Base) ) {
		return !bSubdomainsOnly;
	}
	if ( (Name.Size <= Base.Size) ||
		(Name.Data[Name.Size - Base.Size - 1u] != (uint8)'.') ) {
		return false;
	}
	return __xrtX509AsciiEqual(
		(xbytesview) {
			Name.Data + (Name.Size - Base.Size), Base.Size
		}, Base
	);
}



/* 判断一个字节是否可用于 SMTP Dot-string 的 atom。 */
static bool __xrtX509ConstraintEmailAtom(uint8 iValue)
{
	if ( ((iValue >= (uint8)'A') && (iValue <= (uint8)'Z')) ||
		((iValue >= (uint8)'a') && (iValue <= (uint8)'z')) ||
		((iValue >= (uint8)'0') && (iValue <= (uint8)'9')) ) {
		return true;
	}
	return memchr(
		"!#$%&'*+-/=?^_`{|}~", iValue, sizeof("!#$%&'*+-/=?^_`{|}~") - 1u
	) != NULL;
}



/* 解析 SMTP Mailbox 为大小写敏感 local-part 与 DNS host-part。 */
static bool __xrtX509ConstraintMailbox(
	xbytesview Value,
	xbytesview* pLocal,
	xbytesview* pDomain
)
{
	size_t iAt = SIZE_MAX;
	size_t iDomainSize;
	bool bWildcard;

	if ( (Value.Data == NULL) || (Value.Size < 3u) ) {
		return false;
	}
	if ( Value.Data[0] == (uint8)'\"' ) {
		for ( size_t i = 1u; i < Value.Size; i++ ) {
			uint8 iValue = Value.Data[i];

			if ( iValue == (uint8)'\"' ) {
				if ( (i + 1u >= Value.Size) ||
					(Value.Data[i + 1u] != (uint8)'@') ) {
					return false;
				}
				iAt = i + 1u;
				break;
			}
			if ( iValue == (uint8)'\\' ) {
				i++;
				if ( (i >= Value.Size) || (Value.Data[i] < 32u) ||
					(Value.Data[i] > 126u) ) {
					return false;
				}
				continue;
			}
			if ( !(((iValue >= 32u) && (iValue <= 33u)) ||
				((iValue >= 35u) && (iValue <= 91u)) ||
				((iValue >= 93u) && (iValue <= 126u))) ) {
				return false;
			}
		}
	} else {
		bool bAtom = false;

		for ( size_t i = 0; i < Value.Size; i++ ) {
			uint8 iValue = Value.Data[i];

			if ( iValue == (uint8)'@' ) {
				if ( !bAtom ) {
					return false;
				}
				iAt = i;
				break;
			}
			if ( iValue == (uint8)'.' ) {
				if ( !bAtom ) {
					return false;
				}
				bAtom = false;
				continue;
			}
			if ( !__xrtX509ConstraintEmailAtom(iValue) ) {
				return false;
			}
			bAtom = true;
		}
	}
	if ( (iAt == SIZE_MAX) || (iAt == 0) || (iAt > 64u) ||
		(iAt + 1u >= Value.Size) ) {
		return false;
	}
	for ( size_t i = iAt + 1u; i < Value.Size; i++ ) {
		if ( Value.Data[i] == (uint8)'@' ) {
			return false;
		}
	}
	pLocal->Data = Value.Data;
	pLocal->Size = iAt;
	pDomain->Data = Value.Data + iAt + 1u;
	pDomain->Size = Value.Size - pLocal->Size - 1u;
	if ( !__xrtX509DnsName(
		*pDomain, false, &iDomainSize, &bWildcard
	) ) {
		return false;
	}
	pDomain->Size = iDomainSize;
	return true;
}



/* 从绝对 URI 提取规范要求的 DNS authority host。 */
static bool __xrtX509ConstraintUriHost(
	xbytesview Uri,
	xbytesview* pHost
)
{
	size_t iColon = SIZE_MAX;
	size_t iAuthority;
	size_t iEnd;
	size_t iHost;
	size_t iHostEnd;
	size_t iSize;
	bool bUserInfo = false;
	bool bWildcard;

	for ( size_t i = 0; i < Uri.Size; i++ ) {
		uint8 iValue = Uri.Data[i];
		bool bScheme = ((iValue >= (uint8)'A') && (iValue <= (uint8)'Z')) ||
			((iValue >= (uint8)'a') && (iValue <= (uint8)'z')) ||
			((i != 0) && (((iValue >= (uint8)'0') &&
			 (iValue <= (uint8)'9')) || (iValue == (uint8)'+') ||
			 (iValue == (uint8)'-') || (iValue == (uint8)'.')));

		if ( iValue == (uint8)':' ) {
			iColon = i;
			break;
		}
		if ( !bScheme ) {
			return false;
		}
	}
	if ( (iColon == SIZE_MAX) || (iColon == 0) ||
		(iColon + 2u >= Uri.Size) ||
		(Uri.Data[iColon + 1u] != (uint8)'/') ||
		(Uri.Data[iColon + 2u] != (uint8)'/') ) {
		return false;
	}
	iAuthority = iColon + 3u;
	iEnd = Uri.Size;
	for ( size_t i = iAuthority; i < Uri.Size; i++ ) {
		if ( (Uri.Data[i] == (uint8)'/') ||
			(Uri.Data[i] == (uint8)'?') ||
			(Uri.Data[i] == (uint8)'#') ) {
			iEnd = i;
			break;
		}
	}
	if ( iAuthority == iEnd ) {
		return false;
	}
	iHost = iAuthority;
	for ( size_t i = iAuthority; i < iEnd; i++ ) {
		if ( Uri.Data[i] == (uint8)'@' ) {
			if ( bUserInfo ) {
				return false;
			}
			bUserInfo = true;
			iHost = i + 1u;
		}
	}
	if ( (iHost == iEnd) || (Uri.Data[iHost] == (uint8)'[') ) {
		return false;
	}
	iHostEnd = iEnd;
	for ( size_t i = iHost; i < iEnd; i++ ) {
		if ( Uri.Data[i] != (uint8)':' ) {
			continue;
		}
		if ( iHostEnd != iEnd ) {
			return false;
		}
		iHostEnd = i;
	}
	if ( iHostEnd != iEnd ) {
		if ( (iHostEnd == iHost) || (iHostEnd + 1u == iEnd) ) {
			return false;
		}
		for ( size_t i = iHostEnd + 1u; i < iEnd; i++ ) {
			if ( (Uri.Data[i] < (uint8)'0') ||
				(Uri.Data[i] > (uint8)'9') ) {
				return false;
			}
		}
	}
	pHost->Data = Uri.Data + iHost;
	pHost->Size = iHostEnd - iHost;
	if ( !__xrtX509DnsName(
		*pHost, false, &iSize, &bWildcard
	) ) {
		return false;
	}
	pHost->Size = iSize;
	return true;
}



/* 验证一个约束基点具有本实现可处理的 RFC 5280 形式。 */
bool __xrtX509ConstraintBaseValid(
	const xx509genname* pBase,
	cstr sOperation
)
{
	xbytesview Domain;
	bool bSubdomains;
	bool bValid = true;

	if ( pBase->Type == X509_NAME_DNS ) {
		bValid = __xrtX509ConstraintDomain(
			pBase->Value, false, &Domain, &bSubdomains
		);
	} else if ( pBase->Type == X509_NAME_URI ) {
		bValid = __xrtX509ConstraintDomain(
			pBase->Value, true, &Domain, &bSubdomains
		);
	} else if ( pBase->Type == X509_NAME_EMAIL ) {
		xbytesview Local;

		if ( memchr(pBase->Value.Data, '@', pBase->Value.Size) != NULL ) {
			bValid = __xrtX509ConstraintMailbox(
				pBase->Value, &Local, &Domain
			);
		} else {
			bValid = __xrtX509ConstraintDomain(
				pBase->Value, true, &Domain, &bSubdomains
			);
		}
	}
	if ( !bValid ) {
		__xrtX509NameConstraintsError(
			XERR_PROTOCOL, sOperation,
			"NameConstraints base has an invalid DNS, email or URI form",
			NULL
		);
	}
	return bValid;
}



/* 按 RFC 5280 邮件约束语义比较一项 rfc822Name。 */
static xx509result __xrtX509ConstraintEmailWithin(
	xbytesview Name,
	xbytesview Base
)
{
	xbytesview NameLocal;
	xbytesview NameDomain;
	xbytesview BaseLocal;
	xbytesview BaseDomain;
	bool bSubdomains;

	if ( !__xrtX509ConstraintMailbox(
		Name, &NameLocal, &NameDomain
	) ) {
		goto Invalid;
	}
	if ( memchr(Base.Data, '@', Base.Size) != NULL ) {
		if ( !__xrtX509ConstraintMailbox(
			Base, &BaseLocal, &BaseDomain
		) ) {
			goto Invalid;
		}
		return (NameLocal.Size == BaseLocal.Size) &&
			(memcmp(NameLocal.Data, BaseLocal.Data, NameLocal.Size) == 0) &&
			__xrtX509AsciiEqual(NameDomain, BaseDomain) ?
			X509_VALUE : X509_DONE;
	}
	if ( !__xrtX509ConstraintDomain(
		Base, true, &BaseDomain, &bSubdomains
	) ) {
		goto Invalid;
	}
	if ( bSubdomains ) {
		return __xrtX509ConstraintDomainWithin(
			NameDomain, BaseDomain, true
		) ? X509_VALUE : X509_DONE;
	}
	return __xrtX509AsciiEqual(NameDomain, BaseDomain) ?
		X509_VALUE : X509_DONE;

Invalid:
	__xrtX509NameConstraintsError(
		XERR_PROTOCOL, "x509-general-name-within",
		"rfc822Name or its constraint is malformed", NULL
	);
	return X509_ERROR;
}



/* 按名称类型判断一个 GeneralName 是否位于约束基点内。 */
XRT_API xx509result xrtX509GeneralNameWithin(
	const xx509genname* pName,
	const xx509genname* pBase
)
{
	if ( (pName == NULL) || (pBase == NULL) ||
		((pName->Value.Data == NULL) && (pName->Value.Size != 0)) ||
		((pBase->Value.Data == NULL) && (pBase->Value.Size != 0)) ) {
		__xrtErrorSetInvalidArgument();
		return X509_ERROR;
	}
	if ( pName->Type != pBase->Type ) {
		return X509_DONE;
	}
	if ( pName->Type == X509_NAME_DIRECTORY ) {
		return xrtX509NameWithin(pName->Value, pBase->Value);
	}
	if ( pName->Type == X509_NAME_DNS ) {
		xbytesview Name;
		xbytesview Base;
		bool bUnused;

		if ( !__xrtX509ConstraintDomain(
			pName->Value, false, &Name, &bUnused
		) || !__xrtX509ConstraintDomain(
			pBase->Value, false, &Base, &bUnused
		) ) {
			goto Invalid;
		}
		return __xrtX509ConstraintDomainWithin(
			Name, Base, false
		) ? X509_VALUE : X509_DONE;
	}
	if ( pName->Type == X509_NAME_EMAIL ) {
		return __xrtX509ConstraintEmailWithin(
			pName->Value, pBase->Value
		);
	}
	if ( pName->Type == X509_NAME_URI ) {
		xbytesview Host;
		xbytesview Base;
		bool bSubdomains;

		if ( !__xrtX509ConstraintUriHost(pName->Value, &Host) ||
			!__xrtX509ConstraintDomain(
				pBase->Value, true, &Base, &bSubdomains
			) ) {
			goto Invalid;
		}
		return __xrtX509ConstraintDomainWithin(
			Host, Base, bSubdomains
		) ? X509_VALUE : X509_DONE;
	}
	if ( pName->Type == X509_NAME_IP ) {
		size_t iSize = pName->Value.Size;

		if ( ((iSize != 4u) && (iSize != 16u)) ||
			(pBase->Value.Size != (iSize * 2u)) ) {
			goto Invalid;
		}
		for ( size_t i = 0; i < iSize; i++ ) {
			uint8 iMask = pBase->Value.Data[i + iSize];

			if ( (pName->Value.Data[i] & iMask) !=
				(pBase->Value.Data[i] & iMask) ) {
				return X509_DONE;
			}
		}
		return X509_VALUE;
	}
	__xrtX509NameConstraintsError(
		XERR_UNSUPPORTED, "x509-general-name-within",
		"the constrained GeneralName form has no RFC 5280 matching rules",
		NULL
	);
	return X509_ERROR;

Invalid:
	__xrtX509NameConstraintsError(
		XERR_PROTOCOL, "x509-general-name-within",
		"GeneralName or its constraint has an invalid value", NULL
	);
	return X509_ERROR;
}



/* 扫描一组子树并返回同类型基点是否存在和是否命中。 */
static bool __xrtX509ConstraintScan(
	xx509subtreecursor Cursor,
	const xx509genname* pName,
	bool* pPresent,
	bool* pMatch
)
{
	bool bPresent = false;
	bool bMatch = false;

	while ( true ) {
		xx509subtree Subtree;
		xx509result Result = xrtX509SubtreeRead(&Cursor, &Subtree);

		if ( Result == X509_DONE ) {
			break;
		}
		if ( Result == X509_ERROR ) {
			return false;
		}
		if ( Subtree.Base.Type != pName->Type ) {
			continue;
		}
		bPresent = true;
		if ( Subtree.HasMinimum || Subtree.HasMaximum ) {
			__xrtX509NameConstraintsError(
				XERR_UNSUPPORTED, "x509-name-constraints-check",
				"non-default GeneralSubtree distance is not supported",
				NULL
			);
			return false;
		}
		Result = xrtX509GeneralNameWithin(pName, &Subtree.Base);
		if ( Result == X509_ERROR ) {
			return false;
		}
		if ( Result == X509_VALUE ) {
			bMatch = true;
		}
	}
	*pPresent = bPresent;
	*pMatch = bMatch;
	return true;
}



/* 检查一个证书名称是否落入 excluded 或 permitted 子树。 */
static bool __xrtX509ConstraintName(
	const xx509nameconstraints* pConstraints,
	const xx509genname* pName
)
{
	bool bPresent;
	bool bMatch;

	if ( pConstraints->HasExcluded ) {
		if ( !__xrtX509ConstraintScan(
			pConstraints->Excluded, pName, &bPresent, &bMatch
		) ) {
			return false;
		}
		if ( bPresent && bMatch ) {
			__xrtX509NameConstraintsError(
				XERR_PROTOCOL, "x509-name-constraints-check",
				"certificate name is within an excluded subtree", NULL
			);
			return false;
		}
	}
	if ( pConstraints->HasPermitted ) {
		if ( !__xrtX509ConstraintScan(
			pConstraints->Permitted, pName, &bPresent, &bMatch
		) ) {
			return false;
		}
		if ( bPresent && !bMatch ) {
			__xrtX509NameConstraintsError(
				XERR_PROTOCOL, "x509-name-constraints-check",
				"certificate name is outside all permitted subtrees", NULL
			);
			return false;
		}
	}
	return true;
}



/* 验证调用方提供的子树游标确实包含至少一项完整子树。 */
static bool __xrtX509ConstraintCursorValid(xx509subtreecursor Cursor)
{
	xx509subtree Subtree;
	xx509result Result = xrtX509SubtreeRead(&Cursor, &Subtree);

	if ( Result != X509_VALUE ) {
		if ( Result == X509_DONE ) {
			__xrtX509NameConstraintsError(
				XERR_PROTOCOL, "x509-name-constraints-check",
				"NameConstraints contains an empty GeneralSubtrees value", NULL
			);
		}
		return false;
	}
	while ( (Result = xrtX509SubtreeRead(
		&Cursor, &Subtree
	)) == X509_VALUE ) {
	}
	return Result == X509_DONE;
}



/* 对无 SAN 的旧证书检查 Subject emailAddress 属性。 */
static bool __xrtX509ConstraintSubjectEmail(
	const xx509nameconstraints* pConstraints,
	const xx509cert* pCertificate
)
{
	xx509namecursor Cursor;
	xx509nameattr Attribute;
	xx509result Result;

	if ( !xrtX509NameInit(pCertificate->Subject, &Cursor) ) {
		return false;
	}
	while ( (Result = xrtX509NameRead(
		&Cursor, &Attribute
	)) == X509_VALUE ) {
		xx509genname Name;

		if ( (Attribute.Oid.Size != sizeof(__xrtX509ConstraintEmailOid)) ||
			(memcmp(
				Attribute.Oid.Data, __xrtX509ConstraintEmailOid,
				Attribute.Oid.Size
			) != 0) ) {
			continue;
		}
		memset(&Name, 0, sizeof(Name));
		Name.Type = X509_NAME_EMAIL;
		Name.Value = Attribute.Value;
		if ( !__xrtX509ConstraintName(pConstraints, &Name) ) {
			return false;
		}
	}
	return Result == X509_DONE;
}



/* 检查证书 Subject 与 SAN 是否满足一组 NameConstraints。 */
XRT_API bool xrtX509NameConstraintsCheck(
	const xx509nameconstraints* pConstraints,
	const xx509cert* pCertificate
)
{
	xx509namecursor Subject;
	xx509nameattr Attribute;
	xx509gencursor Names;
	xx509result Result;
	bool bHasSan;

	if ( (pConstraints == NULL) || (pCertificate == NULL) ||
		(!pConstraints->HasPermitted && !pConstraints->HasExcluded) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	if ( (pConstraints->HasPermitted &&
		 !__xrtX509ConstraintCursorValid(pConstraints->Permitted)) ||
		(pConstraints->HasExcluded &&
		 !__xrtX509ConstraintCursorValid(pConstraints->Excluded)) ) {
		return false;
	}
	if ( !xrtX509NameInit(pCertificate->Subject, &Subject) ) {
		return false;
	}
	Result = xrtX509NameRead(&Subject, &Attribute);
	if ( Result == X509_ERROR ) {
		return false;
	}
	if ( Result == X509_VALUE ) {
		xx509genname Name;

		memset(&Name, 0, sizeof(Name));
		Name.Type = X509_NAME_DIRECTORY;
		Name.Value = pCertificate->Subject;
		if ( !__xrtX509ConstraintName(pConstraints, &Name) ) {
			return false;
		}
	}
	Result = xrtX509SubjectAltName(pCertificate, &Names);
	if ( Result == X509_ERROR ) {
		return false;
	}
	bHasSan = Result == X509_VALUE;
	while ( Result == X509_VALUE ) {
		xx509genname Name;

		Result = xrtX509GeneralNameRead(&Names, &Name);
		if ( Result == X509_ERROR ) {
			return false;
		}
		if ( (Result == X509_VALUE) &&
			!__xrtX509ConstraintName(pConstraints, &Name) ) {
			return false;
		}
	}
	if ( !bHasSan && !__xrtX509ConstraintSubjectEmail(
		pConstraints, pCertificate
	) ) {
		return false;
	}
	return true;
}

#endif
