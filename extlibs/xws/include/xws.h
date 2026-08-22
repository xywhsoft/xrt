#ifndef XWS_H
#define XWS_H

#include <xws/features.h>
#include <xhttp.h>
#include <xrt/websocket_runtime.h>

#if defined(XWS_FEATURE_WEBSOCKET_GROUP) || \
	defined(XWS_FEATURE_WEBSOCKET_GROUP_FUTURE)
	#include <xrt/websocket_group.h>
#endif

#if defined(XWS_FEATURE_WEBSOCKET_SERVER) || \
	defined(XWS_FEATURE_WEBSOCKET_CLIENT) || \
	defined(XWS_FEATURE_WEBSOCKET_SERVER_TLS) || \
	defined(XWS_FEATURE_WEBSOCKET_CLIENT_HTTPS) || \
	defined(XWS_FEATURE_WEBSOCKET_SERVER_DEFLATE) || \
	defined(XWS_FEATURE_WEBSOCKET_CLIENT_DEFLATE)
	#include <xrt/websocket_http.h>
#endif

#if defined(XWS_FEATURE_WEBSOCKET_HTTP_FUTURE) || \
	defined(XWS_FEATURE_WEBSOCKET_CLIENT_FUTURE) || \
	defined(XWS_FEATURE_WEBSOCKET_SERVER_FUTURE)
	#include <xrt/websocket_http_future.h>
#endif

#if defined(XWS_FEATURE_WEBSOCKET_SERVER_ROUTER)
	#include <xrt/websocket_server_router.h>
#endif

#endif
