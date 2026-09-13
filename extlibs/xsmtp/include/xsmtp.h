#ifndef XSMTP_H
#define XSMTP_H

#include <xsmtp/features.h>
#include <xmail.h>
#include <xrt/smtp.h>

#if defined(XSMTP_FEATURE_SMTP_CLIENT) || \
	defined(XSMTP_FEATURE_SMTP_CLIENT_TLS)
	#include <xrt/smtp_client.h>
#endif

#if defined(XSMTP_FEATURE_SMTP_AUTH)
	#include <xrt/smtp_auth.h>
#endif

#if defined(XSMTP_FEATURE_SMTP_SUBMIT)
	#include <xrt/smtp_submit.h>
#endif

#endif
