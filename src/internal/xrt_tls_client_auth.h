#ifndef XRT_INTERNAL_TLS_CLIENT_AUTH_H
#define XRT_INTERNAL_TLS_CLIENT_AUTH_H

#include "xrt_tls_client.h"
#include "xrt_tls_identity.h"

#if defined(XRT_FEATURE_TLS_CLIENT_AUTH)

typedef struct xtlsclientauthcertificate {
	xbytesview Issuer;
	uint16 Signature;
	uint16 Alternate;
	bool Anchor;
} xtlsclientauthcertificate;

struct xtlsclientauthstate {
	xtlsidentity* Identity;
	xtlscertificateentry* Entries;
	xtlsclientauthcertificate* Certificates;
	xtlsclientauthconfigv1 Config;
	xx509cert Leaf;
	size_t CertificateCount;
	size_t Certificate12;
	size_t Certificate13;
	size_t SignatureCapacity;
	bytes Transcript;
	size_t TranscriptSize;
	size_t TranscriptCapacity;
	xtlssignature Signature;
	bool Selected;
	bool Queued;
};

void __xrtTlsClientAuthClear(xtlsclientauthstate* pAuth);
const xtlsclientauthstate* __xrtTlsClientAuthStateOf(const xtlssession* pSession);
xtlsresult __xrtTlsClientAuthFailure(xtlssession* pSession);
bool __xrtTlsClientAuthOwnershipLeaf(const xtlsclientauthstate* pAuth);
bool __xrtTlsClientAuthSelect12(xtlsclientstate* pState, const xtls12certificaterequest* pRequest);
bool __xrtTlsClientAuthSelect13(xtlsclientstate* pState, const xtls13certificaterequest* pRequest);
bool __xrtTlsClientAuthRequire(const xtlsclientstate* pState);
bool __xrtTlsClientAuthTranscriptStart12(
	xtlsclientstate* pState, xbytesview ServerHello
);
bool __xrtTlsClientAuthTranscriptAppend12(
	xtlsclientstate* pState, xbytesview Message
);
bool __xrtTlsClientAuthTranscriptReserve12(xtlsclientauthstate* pAuth, size_t iSize);
void __xrtTlsClientAuthTranscriptClear(xtlsclientauthstate* pAuth);
size_t __xrtTlsClientAuthCertificateSize(const xtlsclientstate* pState);
bool __xrtTlsClientAuthCertificateEncode(
	const xtlsclientstate* pState, void* pOutput, size_t iCapacity
);
bool __xrtTlsClientAuthVerifyEncode(
	const xtlsclientstate* pState, xbytesview Content,
	void* pOutput, size_t iCapacity, size_t* pSize
);
bool __xrtTlsClientAuthRecordsSize13(
	const xtlsrecordkey* pKey, size_t iPlain, size_t* pSize
);
xtlsresult __xrtTlsClientAuthQueue13(
	xtlssession* pSession, xbytesview Plain
);
xtlsresult __xrtTlsClientAuth12Writable(xtlssession* pSession, const xtlsclientstate* pState);
xtlsresult __xrtTlsClientAuth12Flight(xtlssession* pSession, xtlsclientstate* pState,
	const xtlshandshake* pMessage);

#endif
#endif
