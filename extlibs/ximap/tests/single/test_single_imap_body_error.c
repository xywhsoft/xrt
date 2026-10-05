#ifdef XIMAP_MODULE_XIMAP
#undef XIMAP_MODULE_XIMAP
#endif
#define XIMAP_MODULE_IMAP_BODY
#define XRT_MODULE_MEMORY_DEBUG
#define XIMAP_IMPLEMENTATION
#include "../../include/ximap/features.h"
#include "../../../xmail/include/xmail/features.h"
#ifndef XRT_IMPLEMENTATION
#define XRT_IMPLEMENTATION
#endif
#include "../../../../single/xrt.h"
#define XMAIL_IMPLEMENTATION
#include "../../../../single/extlibs/xmail.h"
#include "../../../../single/extlibs/ximap.h"
#if !defined(XIMAP_FEATURE_IMAP_BODY) || !defined(XRT_FEATURE_MEMORY_DEBUG)
#error "BODY diagnostic test dependency closure is incomplete"
#endif
#if defined(XRT_FEATURE_NET) || defined(XIMAP_FEATURE_IMAP_CLIENT)
#error "BODY diagnostic test unexpectedly retained networking"
#endif
#include "../test_imap_body_error.c"
