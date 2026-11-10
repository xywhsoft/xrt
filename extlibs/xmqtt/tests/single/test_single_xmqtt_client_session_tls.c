#ifndef XMQTT_MODULE_XMQTT_CLIENT_SESSION_TLS
#define XMQTT_MODULE_XMQTT_CLIENT_SESSION_TLS
#endif
#ifndef XRT_MODULE_TLS_SERVER
#define XRT_MODULE_TLS_SERVER
#endif
#ifndef XRT_MODULE_TLS_IDENTITY_RSA
#define XRT_MODULE_TLS_IDENTITY_RSA
#endif
#ifndef XRT_MODULE_MEMORY_STATS
#define XRT_MODULE_MEMORY_STATS
#endif
#include <xmqtt/features.h>
#define XRT_IMPLEMENTATION
#include "../../../../single/xrt.h"
static xatomic32 stCopyCount;
static void* stCopy(void* To,const void* From,size_t Bytes)
{if(Bytes==257)xrtAtomic32FetchAdd(&stCopyCount,1,XMEMORY_RELAXED);return memcpy(To,From,Bytes);}
#define memcpy stCopy
#define XMQTT_IMPLEMENTATION
#include "../../../../single/extlibs/xmqtt.h"
#undef memcpy
#define XMQTT_SESSION_TLS_COPY_TEST
#include "../test_client_session_tls.c"
