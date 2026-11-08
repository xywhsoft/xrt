#ifndef XRT_TLS_CLIENT_AUTH_H
#define XRT_TLS_CLIENT_AUTH_H

#include <xrt/tls_client.h>
#include <xrt/tls_identity.h>

#if defined(XRT_FEATURE_TLS_CLIENT_AUTH) && \
	(!defined(XRT_FEATURE_TLS_CLIENT_VERIFY) || \
	 !defined(XRT_FEATURE_TLS_IDENTITY) || \
	 !defined(XRT_FEATURE_TLS_MESSAGES_WRITE) || \
	 !defined(XRT_FEATURE_X509_PROFILE) || !defined(XRT_FEATURE_X509_NAME))
	#error "XRT_FEATURE_TLS_CLIENT_AUTH requires verified client, identity, message writers and X.509 profile/name"
#endif

#if defined(XRT_FEATURE_TLS_CLIENT_AUTH)

#define XTLS_CLIENT_AUTH_CERTIFICATES_DEFAULT 16u
#define XTLS_CLIENT_AUTH_CERTIFICATES_MAX 256u
#define XTLS_CLIENT_AUTH_CERTIFICATE_BYTES_DEFAULT (64u * 1024u)
#define XTLS_CLIENT_AUTH_TRANSCRIPT_BYTES_DEFAULT (1024u * 1024u)
#define XTLS_CLIENT_AUTH_BYTES_MAX (16u * 1024u * 1024u)
#define XTLS_CLIENT_AUTH_AUTHORITIES_MAX 256u
#define XTLS_CLIENT_AUTH_OID_FILTERS_MAX 64u
#define XTLS_CLIENT_AUTH_OID_VALUES_MAX 128u

/* Borrowed for Create only. The session retains Identity and snapshots limits.
 * RequireCertificate rejects absent/incompatible certificate requests instead
 * of completing a handshake without the configured local identity. */
typedef struct xtlsclientauthconfigv1 {
	size_t Size;
	const xtlsidentity* Identity;
	size_t MaxCertificates;
	size_t MaxCertificateBytes;
	size_t MaxTranscriptBytes;
	bool RequireCertificate;
} xtlsclientauthconfigv1;

XRT_EXTERN_C_BEGIN

XRT_API void xrtTlsClientAuthConfigInitV1(xtlsclientauthconfigv1* pConfig);

/* Full TLS 1.2/1.3 certificate handshakes. The old client config ABI is unchanged.
 * This first auth contract rejects Resume/ResumeOnly and disables ticket
 * retention until local-identity binding of resumed credentials is available.
 * Signers run synchronously on the session owner and must not reenter/destroy
 * that session. A NULL session config has the existing client defaults. */
XRT_API xtlssession* xrtTlsClientCreateAuthV1(
	const xtlsclientconfig* pSession,
	const xtlsclientauthconfigv1* pAuth,
	xnetbufpool* pPool
);

/* True after a nonempty local Certificate/proof flight is queued. This is not
 * transport delivery or proof of the server's authentication decision. */
XRT_API bool xrtTlsClientCertificateQueuedV1(const xtlssession* pSession);

XRT_EXTERN_C_END
#endif
#endif
