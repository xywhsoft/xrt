#ifndef XMQTT_H
#define XMQTT_H
#include <xmqtt/features.h>
#if defined(XMQTT_FEATURE_XMQTT) && !defined(XMQTT_FEATURE_XMQTT_WIRE)
#error "XMQTT_FEATURE_XMQTT requires XMQTT_FEATURE_XMQTT_WIRE"
#endif
#if defined(XMQTT_FEATURE_XMQTT) && !defined(XMQTT_FEATURE_XMQTT_PROPERTIES)
#error "XMQTT_FEATURE_XMQTT requires XMQTT_FEATURE_XMQTT_PROPERTIES"
#endif
#include <xmqtt/wire.h>
#include <xmqtt/properties.h>
#if defined(XMQTT_FEATURE_XMQTT) && !defined(XMQTT_FEATURE_XMQTT_CODEC5)
#error "XMQTT_FEATURE_XMQTT requires XMQTT_FEATURE_XMQTT_CODEC5"
#endif
#include <xmqtt/packet.h>
#include <xmqtt/codec5.h>
#if defined(XMQTT_FEATURE_XMQTT) && !defined(XMQTT_FEATURE_XMQTT_CODEC311)
#error "XMQTT_FEATURE_XMQTT requires XMQTT_FEATURE_XMQTT_CODEC311"
#endif
#include <xmqtt/codec311.h>
#if defined(XMQTT_FEATURE_XMQTT) && !defined(XMQTT_FEATURE_XMQTT_NEGOTIATION)
#error "XMQTT_FEATURE_XMQTT requires XMQTT_FEATURE_XMQTT_NEGOTIATION"
#endif
#include <xmqtt/negotiation.h>
#include <xmqtt/session.h>
#include <xmqtt/session_image.h>
#include <xmqtt/journal.h>
#include <xmqtt/journal_file.h>
#include <xmqtt/journal_slot.h>
#include <xmqtt/journal_worker.h>
#include <xmqtt/store_write.h>
#include <xmqtt/store_inbox.h>
#include <xmqtt/client_store.h>
#include <xmqtt/client_store_ws.h>
#include <xmqtt/client_store_tls.h>
#if defined(XMQTT_FEATURE_XMQTT) && !defined(XMQTT_FEATURE_XMQTT_STORE_WRITE)
#error "MQTT root requires semantic store writer"
#endif
#if defined(XMQTT_FEATURE_XMQTT) && !defined(XMQTT_FEATURE_XMQTT_JOURNAL_WORKER)
#error "MQTT root requires journal worker"
#endif
#include <xmqtt/store_delta.h>
#include <xmqtt/store_state.h>
#include <xmqtt/store_compact.h>
#include <xmqtt/store_compact_online.h>
#include <xmqtt/store_session.h>
#if defined(XMQTT_FEATURE_XMQTT) && !defined(XMQTT_FEATURE_XMQTT_STORE_SESSION)
#error "MQTT root requires store Session import"
#endif
#if defined(XMQTT_FEATURE_XMQTT) && !defined(XMQTT_FEATURE_XMQTT_STORE_STATE)
#error "MQTT root requires store recovery"
#endif
#if defined(XMQTT_FEATURE_XMQTT) && !defined(XMQTT_FEATURE_XMQTT_STORE_DELTA)
#error "MQTT root requires typed store deltas"
#endif
#if defined(XMQTT_FEATURE_XMQTT) && !defined(XMQTT_FEATURE_XMQTT_JOURNAL)
#error "MQTT root requires journal"
#endif
#if defined(XMQTT_FEATURE_XMQTT) && !defined(XMQTT_FEATURE_XMQTT_SESSION_IMAGE)
#error "MQTT root requires Session images"
#endif
#if defined(XMQTT_FEATURE_XMQTT) && !defined(XMQTT_FEATURE_XMQTT_CLIENT)
#error "XMQTT_FEATURE_XMQTT requires XMQTT_FEATURE_XMQTT_CLIENT"
#endif
#include <xmqtt/client.h>
#include <xmqtt/client_auth.h>
#include <xmqtt/client_dial.h>
#include <xmqtt/client_tls.h>
#include <xmqtt/client_tls_resume.h>
#include <xmqtt/client_inbox.h>
#include <xmqtt/client_session.h>
#include <xmqtt/client_session_dial.h>
#include <xmqtt/client_session_tls.h>
#if defined(XMQTT_FEATURE_XMQTT) && !defined(XMQTT_FEATURE_XMQTT_CLIENT_WS_TLS)
#error "MQTT root requires WS/WSS client"
#endif
#if defined(XMQTT_FEATURE_XMQTT) && !defined(XMQTT_FEATURE_XMQTT_CLIENT_WS_TLS_RESUME)
#error "MQTT root requires WSS credential handoff"
#endif
#include <xmqtt/client_ws.h>
#if defined(XMQTT_FEATURE_XMQTT) && !defined(XMQTT_FEATURE_XMQTT_CLIENT_WS_CONNECT_TLS)
#error "MQTT root requires native WS/WSS setup"
#endif
#include <xmqtt/client_ws_connect.h>
#if defined(XMQTT_FEATURE_XMQTT) && !defined(XMQTT_FEATURE_XMQTT_CLIENT_WS_URI_TLS)
#error "MQTT root requires WS/WSS URI setup"
#endif
#include <xmqtt/client_ws_uri.h>
#include <xmqtt/reconnect.h>
#include <xmqtt/reconnect_store.h>
#include <xmqtt/reconnect_subscriptions.h>
#endif
