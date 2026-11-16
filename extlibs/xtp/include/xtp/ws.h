#ifndef XTP_WS_H
#define XTP_WS_H
#include <xtp/transport.h>
#if defined(XTP_FEATURE_XTP_WS)
#include <xrt/websocket_runtime.h>
/* Host maps MessageBegin/Data/End into this cursor; no native event takeover.
 * On PAUSED/AGAIN retain unconsumed Data and defer MessageEnd/next message.
 * Never recursively feed XTP in a writer. Compression is disabled by default. */
typedef struct xtpwsinput {xtpsession* Session;uint64_t Binding;uint32_t Lane;bool Active;} xtpwsinput;
XRT_EXTERN_C_BEGIN
XTP_API xtpresult xtpWsBegin(xtpwsinput* Input,xwsopcode Opcode);
XTP_API xtpresult xtpWsData(xtpwsinput* Input,xbytesview Data,uint64_t NowUs,xtpreadprogress* Progress);
XTP_API xtpresult xtpWsEnd(xtpwsinput* Input,uint64_t NowUs,xtpreadprogress* Progress);
/* xtpDrain writer; User is borrowed OPEN xwsconn, subprotocol must be xtp.v3.
 * Complete Binary RECORD only. Source is copied by native xws before return. */
XTP_API xtpresult xtpWsWrite(void* Connection,const xtpoutput* Output,size_t* Accepted);
XRT_EXTERN_C_END
#endif
#endif
