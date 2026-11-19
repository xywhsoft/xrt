#ifndef XMQTT_SERVER_AUTH_H
#define XMQTT_SERVER_AUTH_H
#include <xmqtt/server.h>

#if defined(XMQTT_FEATURE_XMQTT_SERVER_AUTH)
#if !defined(XMQTT_FEATURE_XMQTT_SERVER)
#error "MQTT broker enhanced authentication requires the broker"
#endif
XRT_EXTERN_C_BEGIN

typedef enum xmqttserverauthphase {
	XMQTT_SERVER_AUTH_CONNECT,
	XMQTT_SERVER_AUTH_REAUTHENTICATE,
	XMQTT_SERVER_AUTH_CONTINUE
} xmqttserverauthphase;

typedef enum xmqttserverauthdecision {
	XMQTT_SERVER_AUTH_DENY = 0,
	XMQTT_SERVER_AUTH_ACCEPT,
	XMQTT_SERVER_AUTH_CHALLENGE,
	XMQTT_SERVER_AUTH_UNSUPPORTED
} xmqttserverauthdecision;

typedef struct xmqttserverauthrequest {
	xmqttserverauthphase Phase;
	xstrview ClientId, Method;
	xbytesview Data; /* NULL Data.Data means Authentication Data was omitted. */
	const xmqttconnectpacket* Connect; /* Present only on the first CONNECT step. */
} xmqttserverauthrequest;

/* Runs on the connection worker outside broker locks. Return promptly. Request
 * views are borrowed for this call. Exchange begins NULL and is passed back for
 * each step. A nonnull Exchange is handed to Release once, on completion or
 * retirement. Reply bytes must remain valid until Release or the next Step,
 * whichever comes first. Return a null view to omit Authentication Data.
 * Step, Release and User must remain valid through broker Destroy. */
typedef xmqttserverauthdecision (*xmqttserverauthstep)(const xmqttserverauthrequest* Request,
	ptr* Exchange, xbytesview* Reply, ptr User);
typedef void (*xmqttserverauthrelease)(ptr Exchange, ptr User);

typedef struct xmqttserverauthconfig {
	size_t Size, MaxRounds, MaxExchangeBytes;
	int64 ReauthTimeout;
	xmqttserverauthstep Step;
	xmqttserverauthrelease Release;
	ptr User;
} xmqttserverauthconfig;

/* Defaults: 32 challenges, 1 MiB of aggregate exchange wire bytes and 5 s
 * for online reauthentication. Initial auth uses the original ConnectTimeout.
 * Limits are positive. MaxRounds is at most 65536; ReauthTimeout is at most
 * INT32_MAX ms. Bounds cover CONNECT, AUTH and successful CONNACK as applicable. */
XMQTT_API bool xmqttServerAuthConfigInit(xmqttserverauthconfig* Config);

/* Copies Auth. Step and Release are required. CONNECT with an Authentication
 * Method must pass both ordinary CONNECT policy and enhanced auth. Reauth uses
 * the same method and allows application traffic under the existing authority.
 * The application owns the authentication algorithm and its credentials. */
XMQTT_API bool xmqttServerCreateAuth(xnetengine* Engine, const xmqttserverconfig* Config,
	const xmqttserverauthconfig* Auth, xmqttserver** Server);

XRT_EXTERN_C_END
#endif
#endif
