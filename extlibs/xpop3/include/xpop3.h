#ifndef XPOP3_H
#define XPOP3_H

#include <xpop3/features.h>
#include <xmail.h>
#include <xrt/pop3.h>

#if defined(XPOP3_FEATURE_POP3_CLIENT) || \
	defined(XPOP3_FEATURE_POP3_CLIENT_TLS)
	#include <xrt/pop3_client.h>
#endif

#if defined(XPOP3_FEATURE_POP3_AUTH)
	#include <xrt/pop3_auth.h>
#endif

#if defined(XPOP3_FEATURE_POP3_MESSAGE)
	#include <xrt/pop3_message.h>
#endif

#endif
