#ifdef XIMAP_MODULE_XIMAP
#undef XIMAP_MODULE_XIMAP
#endif
#define XIMAP_MODULE_IMAP_BODY
#define XRT_MODULE_MEMORY_DEBUG
#define XIMAP_IMPLEMENTATION
#include "../../single/ximap.h"
#if defined(XRT_FEATURE_NET) || defined(XIMAP_FEATURE_IMAP_CLIENT)
#error "IMAP semantic tests must retain the minimal parsing closure"
#endif
#include "../test_imap_body_semantics.c"
