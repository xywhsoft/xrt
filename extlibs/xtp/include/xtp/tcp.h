#ifndef XTP_TCP_H
#define XTP_TCP_H
#include <xtp/transport.h>
#if defined(XTP_FEATURE_XTP_TCP)
#include <xrt/tcp.h>
XRT_EXTERN_C_BEGIN
/* xtpDrain writer; User is a borrowed OPEN xnetstream, copies on acceptance.
 * Only STREAM. Does not replace native events or retain/take its reference. */
XTP_API xtpresult xtpTcpWrite(void* Stream,const xtpoutput* Output,size_t* Accepted);
XRT_EXTERN_C_END
#endif
#endif
