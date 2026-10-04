#ifdef XIMAP_MODULE_XIMAP
#undef XIMAP_MODULE_XIMAP
#endif
#define XIMAP_MODULE_IMAP_COMPRESS
#define XIMAP_MODULE_IMAP_APPEND
#define XIMAP_MODULE_IMAP_CLIENT_TLS
#define XRT_MODULE_MEMORY_DEBUG
#define XRT_MODULE_TLS_IDENTITY_RSA
#define XRT_MODULE_PEM
#define XIMAP_IMPLEMENTATION
#include "../../single/ximap.h"
#if !defined(XMAIL_FEATURE_MAIL_NET_DEFLATE) || !defined(XRT_FEATURE_MEMORY_DEBUG)
#error "IMAP COMPRESS fault test dependency closure is incomplete"
#endif
#include "../test_imap_compress_fault_runtime.c"
