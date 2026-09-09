#ifndef XLLM_XRT_H
#define XLLM_XRT_H

/* XRT roots used by xllm transport, session persistence, and memory storage. */
#define XRT_MODULE_JSON_READ
#define XRT_MODULE_FILE_WHOLE
#define XRT_MODULE_DIR
#define XRT_MODULE_NET_TCP_DIAL_SYNC
#define XRT_MODULE_TLS_STREAM_DIAL_FUTURE
#define XRT_MODULE_TLS_STREAM_FUTURE
#define XRT_MODULE_TLS_VERIFY
#define XRT_MODULE_X509_STORE_SYSTEM
#define XRT_MODULE_HTTP1_BODY
#define XRT_MODULE_THREAD
#define XRT_MODULE_MUTEX
#define XRT_MODULE_CANCEL
#define XRT_MODULE_TIME

#include <xrt.h>

#endif
