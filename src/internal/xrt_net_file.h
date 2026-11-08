#ifndef XRT_INTERNAL_NET_FILE_H
#define XRT_INTERNAL_NET_FILE_H
#include <xrt/net_file.h>

#if defined(XRT_FEATURE_NET_FILE)
/* Trusted native submission primitive. The caller must keep File, storage and
 * Completion alive until the one real terminal event. Public borrowed callers
 * additionally seal the port; the closed owned family registers these duties
 * in the Engine runtime directory instead. Never clear a port exposure flag. */
uint64 __xrtNetFileSubmit(xnetworker* Worker, xfile File, uint64 Offset,
    void* Data, size_t Size, xnetcompletion* Completion, bool Write);
#endif
#endif
