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
#include <xmqtt/reconnect.h>
#endif
