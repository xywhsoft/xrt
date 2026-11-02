#include "../internal/xrt_tls_client_auth.h"

#if defined(XRT_FEATURE_TLS_CLIENT_AUTH)

static bool __xrtTlsClientAuthViewEqual(xbytesview Left, xbytesview Right)
{
	return Left.Size == Right.Size && (!Left.Size || !memcmp(Left.Data, Right.Data, Left.Size));
}

static bool __xrtTlsClientAuthSelectionError(cstr Message)
{
	const xerror* cause = xrtGetError();
	xerrkind kind = cause && xrtErrorIs(cause, XERR_MEMORY) ? XERR_MEMORY : XERR_PROTOCOL;
	__xrtTlsErrorCause(kind, XTLS_ERROR_EXTENSION, "select-client-certificate", Message, SIZE_MAX, cause);
	return false;
}

static bool __xrtTlsClientAuthAuthorities(const xtlsclientauthstate* pAuth, xbytesview Data, bool* pMatch)
{
	*pMatch = true;
	if (!Data.Size) return true;
	xtlsauthoritycursor cursor;
	if (!xrtTlsAuthorities(Data, &cursor)) return false;
	if (!cursor.Data.Size) return true;
	*pMatch = false;
	size_t count = 0;
	xbytesview name;
	xtlsitemresult result;
	while ((result = xrtTlsAuthoritiesRead(&cursor, &name)) == XTLS_ITEM_VALUE) {
		if (++count > XTLS_CLIENT_AUTH_AUTHORITIES_MAX)
			return __xrtTlsClientError(XERR_RANGE, XTLS_ERROR_LIMIT, "select-client-certificate",
				"TLS client certificate authority count exceeds its limit");
		xx509namecursor check;
		xx509nameattr attribute;
		if (!xrtX509NameInit(name, &check)) return __xrtTlsClientAuthSelectionError("TLS certificate authority name is invalid");
		xx509result valid;
		while ((valid = xrtX509NameRead(&check, &attribute)) == X509_VALUE) {}
		if (valid == X509_ERROR) return __xrtTlsClientAuthSelectionError("TLS certificate authority name is invalid");
		if (!*pMatch) {
			for (size_t i = 0; i < pAuth->CertificateCount; i++) {
				xx509result equal = xrtX509NameEqual(pAuth->Certificates[i].Issuer, name);
				if (equal == X509_ERROR) return __xrtTlsClientAuthSelectionError("TLS certificate authority comparison failed");
				if (equal == X509_VALUE) { *pMatch = true; break; }
			}
		}
	}
	return result == XTLS_ITEM_DONE;
}

static bool __xrtTlsClientAuthUsageFilter(const xx509cert* pLeaf, xbytesview Values, bool* pMatch)
{
	uint16 have = 0, want = 0;
	xx509result found = xrtX509KeyUsage(pLeaf, &have);
	if (found == X509_ERROR) return false;
	*pMatch = found == X509_VALUE;
	if (!Values.Size) return true;
	xdercursor cursor;
	xdervalue value;
	xbytesview bits;
	uint8 unused;
	if (!xrtDerInit(&cursor, Values.Data, Values.Size) ||
		xrtDerRead(&cursor, &value) != XDER_VALUE || !xrtDerDone(&cursor) ||
		!xrtDerBitString(&value, &bits, &unused) || !bits.Size || bits.Size > 2u ||
		bits.Size * 8u - unused > 9u)
		return __xrtTlsClientAuthSelectionError("TLS key-usage filter is not a valid KeyUsage value");
	for (size_t i = 0; i < bits.Size * 8u - unused; i++)
		if (bits.Data[i / 8u] & (uint8)(0x80u >> (i % 8u))) want |= (uint16)(1u << i);
	if (!want) return __xrtTlsClientAuthSelectionError("TLS key-usage filter is empty");
	*pMatch = *pMatch && (have & want) == want;
	return true;
}

static bool __xrtTlsClientAuthPurposeFilter(const xx509cert* pLeaf, xbytesview Values, bool* pMatch)
{
	static const uint8 AnyUsage[] = { 0x55u, 0x1Du, 0x25u, 0u };
	xx509oidcursor have;
	xx509result found = xrtX509ExtendedKeyUsage(pLeaf, &have);
	if (found == X509_ERROR) return false;
	*pMatch = found == X509_VALUE;
	if (!Values.Size) return true;
	xx509oidcursor want;
	if (!xrtX509OidInit(Values, &want))
		return __xrtTlsClientAuthSelectionError("TLS extended-key-usage filter is invalid");
	xbytesview oid;
	xx509result result;
	size_t count = 0;
	while ((result = xrtX509OidRead(&want, &oid)) == X509_VALUE) {
		if (++count > XTLS_CLIENT_AUTH_OID_VALUES_MAX)
			return __xrtTlsClientError(XERR_RANGE, XTLS_ERROR_LIMIT, "select-client-certificate",
				"TLS extended-key-usage filter has too many values");
		if (oid.Size == sizeof(AnyUsage) && !memcmp(oid.Data, AnyUsage, sizeof(AnyUsage)))
			return __xrtTlsClientAuthSelectionError("TLS extended-key-usage filter contains anyExtendedKeyUsage");
		if (*pMatch) {
			xx509oidcursor candidate = have;
			xbytesview value;
			bool match = false;
			xx509result item;
			while ((item = xrtX509OidRead(&candidate, &value)) == X509_VALUE)
				if (__xrtTlsClientAuthViewEqual(value, oid)) match = true;
			if (item == X509_ERROR) return false;
			if (!match) *pMatch = false;
		}
	}
	return result != X509_ERROR;
}

/* Recognize the two RFC 8446 matching rules. Unknown OIDs are skipped, never
 * treated as equality filters. All OIDs/framing/duplicates remain checked. */
static bool __xrtTlsClientAuthFilters(const xtlsclientauthstate* pAuth, xbytesview Data, bool* pMatch)
{
	*pMatch = true;
	if (!Data.Size) return true;
	if (Data.Size < 2u || __xrtTlsRead16(Data.Data) != Data.Size - 2u)
		return __xrtTlsClientAuthSelectionError("TLS OID filter vector length is invalid");
	xbytesview seen[XTLS_CLIENT_AUTH_OID_FILTERS_MAX];
	size_t count = 0, offset = 2u;
	while (offset < Data.Size) {
		if (count == XTLS_CLIENT_AUTH_OID_FILTERS_MAX)
			return __xrtTlsClientError(XERR_RANGE, XTLS_ERROR_LIMIT, "select-client-certificate",
				"TLS OID filter count exceeds its limit");
		size_t oid_size = Data.Data[offset++];
		if (!oid_size || oid_size > Data.Size - offset)
			return __xrtTlsClientAuthSelectionError("TLS OID filter identifier is truncated");
		xdercursor cursor;
		xdervalue value;
		xbytesview oid;
		if (!xrtDerInit(&cursor, Data.Data + offset, oid_size) ||
			xrtDerRead(&cursor, &value) != XDER_VALUE || !xrtDerDone(&cursor) || !xrtDerOid(&value, &oid))
			return __xrtTlsClientAuthSelectionError("TLS OID filter identifier is not one DER OID");
		offset += oid_size;
		for (size_t i = 0; i < count; i++)
			if (__xrtTlsClientAuthViewEqual(oid, seen[i]))
				return __xrtTlsClientAuthSelectionError("TLS OID filter identifier is repeated");
		seen[count++] = oid;
		if (Data.Size - offset < 2u)
			return __xrtTlsClientAuthSelectionError("TLS OID filter values are truncated");
		size_t size = __xrtTlsRead16(Data.Data + offset);
		offset += 2u;
		if (size > Data.Size - offset)
			return __xrtTlsClientAuthSelectionError("TLS OID filter values are truncated");
		xbytesview values = { Data.Data + offset, size };
		bool match = true;
		if (oid.Size == 3u && oid.Data[0] == 0x55u && oid.Data[1] == 0x1Du) {
			if (oid.Data[2] == 15u && !__xrtTlsClientAuthUsageFilter(&pAuth->Leaf, values, &match)) return false;
			if (oid.Data[2] == 37u && !__xrtTlsClientAuthPurposeFilter(&pAuth->Leaf, values, &match)) return false;
		}
		*pMatch = *pMatch && match;
		offset += size;
	}
	return true;
}

static bool __xrtTlsClientAuthSelectCommon(xtlsclientstate* pState,
	const xtlsids* pSchemes, const xtlsids* pCertificateSchemes, xbytesview Authorities,
	xbytesview Filters, bool bType)
{
	xtlsclientauthstate* auth = pState->Auth;
	if (!auth) return true;
	xerror* previous = xrtTakeError();
	bool authority, filters, chain = true;
	bool result = false;
	auth->Selected = false;
	if (!__xrtTlsClientAuthAuthorities(auth, Authorities, &authority) ||
		!__xrtTlsClientAuthFilters(auth, Filters, &filters)) goto done;
	for (size_t i = 0; i < auth->CertificateCount; i++) {
		const xtlsclientauthcertificate* cert = &auth->Certificates[i];
		if (cert->Anchor) continue;
		if ((!cert->Signature || !xrtTlsIdsContain(pCertificateSchemes, cert->Signature)) &&
			(!cert->Alternate || !xrtTlsIdsContain(pCertificateSchemes, cert->Alternate))) chain = false;
	}
	if (bType && authority && filters && chain &&
		(pState->Version != XTLS_VERSION_13 || auth->Leaf.Version == X509_VERSION_3)) {
		for (size_t i = 0; i < pState->SignatureCount; i++) {
			xtlssignature scheme = (xtlssignature)pState->Signatures[i];
			if (!xrtTlsIdsContain(pSchemes, (uint16)scheme)) continue;
			bool can = xrtTlsIdentityCanSign(auth->Identity, pState->Version, scheme);
			if (xrtGetError()) goto done;
			if (can) { auth->Signature = scheme; auth->Selected = true; break; }
		}
	}
	result = auth->Selected || !auth->Config.RequireCertificate;
	if (!result) (void)__xrtTlsClientError(XERR_PERMISSION, XTLS_ERROR_IDENTITY,
		"select-client-certificate", "TLS certificate request has no compatible configured identity");
 done:
	if (result) {
		xrtClearError();
		if (previous) __xrtErrorSetOwned(previous);
	} else xrtErrorFree(previous);
	return result;
}

bool __xrtTlsClientAuthSelect12(xtlsclientstate* pState, const xtls12certificaterequest* pRequest)
{
	if (!pState->Auth) return true;
	xtlsidentitytype type = xrtTlsIdentityType(pState->Auth->Identity);
	uint8 wanted = type == XTLS_IDENTITY_RSA || type == XTLS_IDENTITY_RSA_PSS ?
		XTLS_CERTIFICATE_RSA_SIGN : XTLS_CERTIFICATE_ECDSA_SIGN;
	bool found = false;
	for (size_t i = 0; i < pRequest->CertificateTypes.Size; i++)
		if (pRequest->CertificateTypes.Data[i] == wanted) found = true;
	return __xrtTlsClientAuthSelectCommon(pState, &pRequest->Signatures, &pRequest->Signatures,
		pRequest->AuthorityData, (xbytesview) { NULL, 0 }, found);
}

bool __xrtTlsClientAuthSelect13(xtlsclientstate* pState, const xtls13certificaterequest* pRequest)
{
	if (!pState->Auth) return true;
	xtlsextension extension;
	xtlsitemresult found = xrtTlsExtensionsFind(pRequest->Extensions, XTLS_EXTENSION_OID_FILTERS, &extension);
	if (found == XTLS_ITEM_ERROR) return false;
	const xtlsids* certificates = pRequest->CertificateSignatures.Data.Size ?
		&pRequest->CertificateSignatures : &pRequest->Signatures;
	return __xrtTlsClientAuthSelectCommon(pState, &pRequest->Signatures, certificates,
		pRequest->AuthorityData, found == XTLS_ITEM_VALUE ? extension.Data : (xbytesview) { NULL, 0 }, true);
}
#endif
