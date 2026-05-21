#ifndef XRT_EXTLIB_XMAIL_H
#define XRT_EXTLIB_XMAIL_H

#include "../../singlehead/xrt.h"

#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-function"
#endif
#include "../xmail_mime/xmail_mime.h"
#include "../xsmtp/xsmtp.h"
#include "../xpop3/xpop3.h"
#include "../ximap/ximap.h"
#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif

#include "xmail_xlang.h"

#endif
