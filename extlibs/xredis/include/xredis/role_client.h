#ifndef XREDIS_ROLE_CLIENT_H
#define XREDIS_ROLE_CLIENT_H
#include <xredis/client.h>
#include <xredis/role.h>
#if defined(XREDIS_FEATURE_XREDIS_ROLE_CLIENT) && (!defined(XREDIS_FEATURE_XREDIS_CLIENT) || !defined(XREDIS_FEATURE_XREDIS_ROLE))
#error "Managed role verification requires native Client and ROLE inspection"
#endif
#if defined(XREDIS_FEATURE_XREDIS_ROLE_CLIENT)
/* Choose zero or ONE requirement in ClientInitConfig.Flags. ROLE follows
 * negotiation/auth/name/database setup and must validate and match before
 * Ready or any queued business command. All phases share the original setup
 * deadline and MaxWireBytes/Stream.WriteLimit; the role command adds14 bytes.
 * Reply limits are the existing Replies.Reader MaxValues/MaxFrameBytes bounds.
 * No Role reply is retained in Ready; Hello stays the negotiated HELLO map.
 * Mismatch returns READY_ROLE_MISMATCH at INIT_ROLE; other setup failures keep
 * their existing typed statuses and redacted diagnostics. Config/result/native
 * object layouts and all old enum values stay unchanged.
 * REQUIRE_REPLICA verifies identity, not freshness or replication consistency.
 * This point-in-time check must be repeated on each new connection. Sentinel
 * discovery, endpoint replacement and Cluster routing are separate work. */
#define XREDIS_INIT_REQUIRE_MASTER 0x10u
#define XREDIS_INIT_REQUIRE_REPLICA 0x20u
#define XREDIS_INIT_REQUIRE_SENTINEL 0x40u
#endif
#endif
