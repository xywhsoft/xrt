#ifndef XRT_NET_FILE_FUTURE_H
#define XRT_NET_FILE_FUTURE_H
#include <xrt/net_file.h>
#include <xrt/future_bridge.h>
#include <xrt/buffer.h>

#if defined(XRT_FEATURE_NET_FILE_FUTURE) && (!defined(XRT_FEATURE_NET_FILE) || \
    !defined(XRT_FEATURE_FUTURE_BRIDGE) || !defined(XRT_FEATURE_BUFFER))
#error "native owned files require net_file, future_bridge and buffer"
#endif

#if defined(XRT_FEATURE_NET_FILE_FUTURE)
typedef struct xnetfile xnetfile;
/* One immutable input-owner reference. Bytes remain stable through completion;
 * descriptors/code outlive graph pins. Success consumes one owner, failure none.
 * This is a closed resident contract, not a public arbitrary callback adapter. */
typedef struct xnetfiledataownershipv1 {
    size_t size;
    void (*Drop)(ptr Data);
    const xrtownershipops* Ops;
} xnetfiledataownershipv1;

XRT_EXTERN_C_BEGIN
/* Query the actual started worker's FILE_IO capability before creating or
 * truncating. No fallback backend, helper thread or task-pool dispatch.
 * Explicit Close is required; Destroy drops a reference and never blocks.
 * The Engine owns a real runtime registration until physical close. */
XRT_API xnetfile* xrtNetFileCreate(xnetengine* Engine, uint32 Worker,
    cstr Path, const xfileoptions* Options);
XRT_API xnetfile* xrtNetFileRef(xnetfile* File);
XRT_API void xrtNetFileDestroy(xnetfile* File);
/* Read returns an owned xbuffer payload (borrow with xrtFutureValue), including
 * empty EOF. Write returns an unowned uintptr_t byte count. No shared cursor.
 * Zero length still checks state/access/offset; UINT64/size overflow rejects.
 * Pre-cancelled calls perform no I/O. Accepted cancel requests retain all OS
 * duties until the unique real terminal event; completed I/O may win the race. */
XRT_API xfuture* xrtNetFileReadFuture(xnetfile* File, uint64 Offset,
    size_t Size, xcancel* Cancel);
XRT_API xfuture* xrtNetFileWriteFuture(xnetfile* File, uint64 Offset,
    xbytesview Input, xcancel* Cancel);
XRT_API xfuture* xrtNetFileWriteRefPolicyV1(xnetfile* File, uint64 Offset,
    xbytesview Input, ptr Owner, const xnetfiledataownershipv1* Policy, xcancel* Cancel);
/* Reject new submissions, request cancellation of accepted I/O and observe the
 * same preallocated close Future on every call. Physical close waits for real
 * terminal events; cleanup failure rejects that Future, never aborts or hides.
 * On scheduling refusal, NULL reports error and a later Close can retry. */
XRT_API xfuture* xrtNetFileCloseFuture(xnetfile* File);
XRT_API xrtownershipref xrtNetFileOwnership(const xnetfile* File);
XRT_API const xnetruntimeownershipv1* xrtNetFileRuntimeOwnershipV1Get(void);
XRT_API const xfutureproducerownershipv1* xrtNetFileFutureProducerPolicyV1Get(void);
XRT_API const xfutureproducerownershipv1* xrtNetFileCloseProducerPolicyV1Get(void);
XRT_API const xfuturepayloadownershipv1* xrtNetFileFutureBytesPayloadPolicyV1Get(void);
XRT_API const xcancelwatchownershipv1* xrtNetFileFutureCancelPolicyV1Get(void);
XRT_API const xrtownershipquiescencev1* xrtNetFileOwnershipQuiescenceV1Get(void);
XRT_API const xrtownershipquiescencev1* xrtNetFileFutureOwnershipQuiescenceV1Get(void);
/* Exact native identities are checked before any client input policy or Ops.
 * Unknown input policies refuse unchanged. Preparation observes native close;
 * the execution-resource owner requests Close before retiring its Engine. */
XRT_API const xrtownershipadapterv1* xrtNetFileOwnershipAdapterV1(xrtownershipref Ref,
    const xrtownershippreparationv1** Preparation);
XRT_API const xrtownershipadapterv1* xrtNetFileFutureOwnershipAdapterV1(xrtownershipref Ref,
    const xnetfiledataownershipv1* const* Policies, size_t Count,
    const xrtownershippreparationv1** Preparation);
XRT_EXTERN_C_END
#endif
#endif
