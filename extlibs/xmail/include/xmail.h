#ifndef XMAIL_H
#define XMAIL_H

#include <xmail/features.h>
#include <xrt.h>
#include <xrt/mail.h>

#if defined(XMAIL_FEATURE_MAIL_NET_DEFLATE) && \
	(!defined(XMAIL_FEATURE_MAIL_NET) || \
	 !defined(XRT_FEATURE_DEFLATE) || \
	 !defined(XRT_FEATURE_INFLATE))
	#error "XMAIL_FEATURE_MAIL_NET_DEFLATE requires mail net, Deflate and Inflate"
#endif

#if defined(XMAIL_FEATURE_MAIL) && \
	(!defined(XMAIL_FEATURE_MAIL_CODEC) || \
	!defined(XMAIL_FEATURE_MAIL_CHARSET) || \
	!defined(XMAIL_FEATURE_MAIL_HEADER) || \
	!defined(XMAIL_FEATURE_MAIL_WORD) || \
	!defined(XMAIL_FEATURE_MAIL_ADDRESS) || \
	!defined(XMAIL_FEATURE_MAIL_DATE) || \
	!defined(XMAIL_FEATURE_MAIL_ID) || \
	!defined(XMAIL_FEATURE_MAIL_PARAM) || \
	!defined(XMAIL_FEATURE_MAIL_MULTIPART) || \
	!defined(XMAIL_FEATURE_MAIL_MESSAGE) || \
	!defined(XMAIL_FEATURE_MAIL_TREE) || \
	!defined(XMAIL_FEATURE_MAIL_BUILD) || \
	!defined(XMAIL_FEATURE_MAIL_COMPOSE) || \
	!defined(XMAIL_FEATURE_MAIL_WIRE) || \
	!defined(XMAIL_FEATURE_MAIL_NET) || \
	!defined(XMAIL_FEATURE_MAIL_NET_TLS) || \
	!defined(XMAIL_FEATURE_MAIL_NET_DEFLATE))
	#error "XMAIL_FEATURE_MAIL requires every public mail capability"
#endif
#if defined(XMAIL_FEATURE_MAIL_CODEC)
	#include <xrt/mail_codec.h>
#endif

#if defined(XMAIL_FEATURE_MAIL_CHARSET)
	#include <xrt/mail_charset.h>
#endif

#if defined(XMAIL_FEATURE_MAIL_HEADER)
	#include <xrt/mail_header.h>
#endif

#if defined(XMAIL_FEATURE_MAIL_WORD)
	#include <xrt/mail_word.h>
#endif

#if defined(XMAIL_FEATURE_MAIL_ADDRESS)
	#include <xrt/mail_address.h>
#endif

#if defined(XMAIL_FEATURE_MAIL_DATE)
	#include <xrt/mail_date.h>
#endif

#if defined(XMAIL_FEATURE_MAIL_ID)
	#include <xrt/mail_id.h>
#endif

#if defined(XMAIL_FEATURE_MAIL_PARAM)
	#include <xrt/mail_param.h>
#endif

#if defined(XMAIL_FEATURE_MAIL_MULTIPART)
	#include <xrt/mail_multipart.h>
#endif

#if defined(XMAIL_FEATURE_MAIL_MESSAGE)
	#include <xrt/mail_message.h>
#endif

#if defined(XMAIL_FEATURE_MAIL_TREE)
	#include <xrt/mail_tree.h>
#endif

#if defined(XMAIL_FEATURE_MAIL_BUILD)
	#include <xrt/mail_build.h>
#endif

#if defined(XMAIL_FEATURE_MAIL_COMPOSE)
	#include <xrt/mail_compose.h>
#endif

#if defined(XMAIL_FEATURE_MAIL_WIRE)
	#include <xrt/mail_wire.h>
#endif

#if defined(XMAIL_FEATURE_MAIL_NET) || defined(XMAIL_FEATURE_MAIL_NET_TLS)
	#include <xrt/mail_net.h>
#endif

#endif
