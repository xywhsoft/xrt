#ifndef XTP_XRUDP_H
#define XTP_XRUDP_H
#include <xtp/transport.h>
#if defined(XTP_FEATURE_XTP_XRUDP)
#include <xrudp/session.h>
typedef struct xtpxrudpwriter {
    xrudpsession* Session;uint32_t StreamID,ChannelID;uint64_t NowUs;
} xtpxrudpwriter;
XRT_EXTERN_C_BEGIN
/* Borrow an authenticated/promoted xrudp session; never add TLS or another
 * fragmenter. STREAM maps to raw BYTES StreamID (0=>1); unreliable RECORD maps
 * to configured ChannelID and must contain one whole NOTIFY. Refresh NowUs
 * before each Drain. Current DatagramSendLimit is checked on every offer.
 * LIMIT is permanent for that payload: Drain drops only the unsent NOTIFY,
 * completes it LOCAL_ERROR/NOT_SENT, and reports Dropped; no stream fallback.
 * Host supplies readable stream bytes / whole datagrams to the matching XTP
 * lane, and calls native Poll after work and at native Due. Own no events/keys. */
XTP_API xtpresult xtpXrudpWrite(void* Writer,const xtpoutput* Output,size_t* Accepted);
XRT_EXTERN_C_END
#endif
#endif
