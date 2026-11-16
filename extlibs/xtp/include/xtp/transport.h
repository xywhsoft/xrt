#ifndef XTP_TRANSPORT_H
#define XTP_TRANSPORT_H
#include <xtp/session.h>
#if defined(XTP_FEATURE_XTP_TRANSPORT)
/* Writer borrows Output only until return; copy/accept before reporting bytes.
 * No reentry into Session. On an error, report any truly accepted prefix too.
 * Host retains transport ownership, events, input buffers and writable timers. */
typedef xtpresult (*xtpwriteproc)(void* User,const xtpoutput* Output,size_t* Accepted);
typedef struct xtpwritebudget {size_t Bytes,Frames,ChunkBytes;} xtpwritebudget;
typedef struct xtpwriteprogress {size_t Bytes,Frames,Calls,Dropped;} xtpwriteprogress;
XRT_EXTERN_C_BEGIN
/* NULL budget:256KiB,64 frames,16KiB stream offers. RECORD is never split and
 * must fit Bytes; no waiting to form batches. AGAIN means budget/backpressure.
 * Permanent LIMIT/DEADLINE on an unsent unreliable RECORD retires only that
 * NOTIFY as LOCAL_ERROR/TIMEOUT + NOT_SENT, returns DROPPED, never SENT.
 * Other errors retain data; host reports transport failure via Unbind.
 * Now is a fresh monotonic timestamp for this bounded synchronous drain turn. */
XTP_API xtpresult xtpDrain(xtpsession* Session,uint32_t LaneID,xtpwriteproc Write,void* User,
    const xtpwritebudget* Budget,uint64_t NowUs,xtpwriteprogress* Progress);
XRT_EXTERN_C_END
#endif
#endif
