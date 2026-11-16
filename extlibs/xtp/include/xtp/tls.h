#ifndef XTP_TLS_H
#define XTP_TLS_H
#include <xtp/transport.h>
#if defined(XTP_FEATURE_XTP_TLS)
#include <xrt/tls_stream.h>
XRT_EXTERN_C_BEGIN
/* Static lifetime arrays: TLS1.3 only, ChaCha preferred, optional AES128-GCM;
 * X25519, Ed25519/ECDSA-P256 signatures. No 0-RTT path. Set client/server ALPN
 * to xtp/3; normal XRT verifier/name checks and server identity are mandatory.
 * TLS policy restricts handshake signatures; chain verification stays in XRT. */
XTP_API void xtpTlsPolicyInit(xtlspolicy* Policy,bool ChaChaOnly);
/* Checks OPEN, TLS1.3, allowed cipher, ALPN xtp/3 before accepting XTP bytes.
 * User is a borrowed xtlsstream on its owning Worker. Successful short writes
 * report Written exactly. No event replacement, retain, or cleartext fallback. */
XTP_API xtpresult xtpTlsWrite(void* Stream,const xtpoutput* Output,size_t* Accepted);
XRT_EXTERN_C_END
#endif
#endif
