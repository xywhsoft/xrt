#ifndef XRT_INTERNAL_CANCEL_H
#define XRT_INTERNAL_CANCEL_H
#include "xrt_internal.h"
#if defined(XRT_FEATURE_CANCEL)
/* Collector-only: the same exclusive freeze has admitted this observer-free
 * token. Publish last-producer cancellation without locks, allocation or user
 * notification. Never use this for an ordinary cancellation request. */
void __xrtCancelOwnershipCloseUnobserved(xcancel* pCancel);
/* With a real plan pin retained after Clear: has the registration owner
 * actually consumed Unwatch? Dispatch retirement remains a separate duty. */
bool __xrtCancelWatchOwnerReleased(const xcancelwatch* pWatch);
#endif
#endif
