#ifndef XIMAP_H
#define XIMAP_H

#include <ximap/features.h>
#include <xmail.h>
#include <xrt/imap.h>
#include <xrt/imap_data.h>
#include <xrt/imap_body.h>

#if defined(XIMAP_FEATURE_IMAP_CLIENT) || \
	defined(XIMAP_FEATURE_IMAP_CLIENT_TLS)
	#include <xrt/imap_client.h>
#endif

#if defined(XIMAP_FEATURE_IMAP_AUTH)
	#include <xrt/imap_auth.h>
#endif

#if defined(XIMAP_FEATURE_IMAP_COMMAND)
	#include <xrt/imap_command.h>
#endif

#if defined(XIMAP_FEATURE_IMAP_MESSAGE)
	#include <xrt/imap_message.h>
#endif

#if defined(XIMAP_FEATURE_IMAP_APPEND)
	#include <xrt/imap_append.h>
#endif

#if defined(XIMAP_FEATURE_IMAP_COMPRESS)
	#include <xrt/imap_compress.h>
#endif

#endif
