#ifndef XMQTT_STORE_DELTA_H
#define XMQTT_STORE_DELTA_H
#include <xmqtt/codec5.h>
#include <xmqtt/codec311.h>
#if defined(XMQTT_FEATURE_XMQTT_STORE_DELTA) && (!defined(XMQTT_FEATURE_XMQTT_CODEC5) || !defined(XMQTT_FEATURE_XMQTT_CODEC311))
#error "MQTT store deltas require both native packet codecs"
#endif
#if defined(XMQTT_FEATURE_XMQTT_STORE_DELTA)
#define XMQTT_STORE_DELTA_SCHEMA UINT32_C(0x584d4401)
#define XMQTT_STORE_DELTA_ATTEMPTED 1u
#define XMQTT_STORE_DELTA_SENT 2u
#define XMQTT_STORE_DELTA_CLEAN_START 1u
#define XMQTT_STORE_DELTA_SESSION_PRESENT 2u
typedef enum xmqttstoredeltatype {
    XMQTT_STORE_BIND=1,XMQTT_STORE_OPEN,XMQTT_STORE_END,XMQTT_STORE_CLEAR,
    XMQTT_STORE_OUT_PUT,XMQTT_STORE_OUT_ATTEMPT,XMQTT_STORE_OUT_PUBREL,
    XMQTT_STORE_OUT_COMPLETE,XMQTT_STORE_IN_ACCEPT,XMQTT_STORE_IN_PUBREL,
    XMQTT_STORE_IN_COMPLETE,XMQTT_STORE_INBOX_ACK
} xmqttstoredeltatype;
typedef enum xmqttstoredeltastatus {
    XMQTT_STORE_DELTA_ERROR=-3,XMQTT_STORE_DELTA_MALFORMED=-2,
    XMQTT_STORE_DELTA_PROTOCOL=-1,XMQTT_STORE_DELTA_DONE=1,XMQTT_STORE_DELTA_LIMIT=2
} xmqttstoredeltastatus;
/* All unused fields MUST be zero. Epoch names a logical broker-session epoch,
 * not a connection/Engine generation. Key is a persistent nonwrapping flow or
 * delivery identity; its actual allocation/order belongs to the store applier.
 * BIND: nonempty UTF-8 ClientId in Data, WallMilliseconds; Key/id/flags/reason/
 *       Expiry zero. OPEN: clean/session-present flags, Expiry, wall; others zero.
 * END: flag1=complete client DISCONNECT, its reason (else0), effective Expiry,
 *      actual disconnect wall; other fields zero. CLEAR: reason0=admin,1=expiry,
 *      2=broker reset, wall; other fields zero; it never removes Inbox payloads.
 * OUT_PUT: Key/id, canonical QoS1/2 PUBLISH Data, attempted/sent flags (sent
 *      implies attempted); reason/Expiry/wall zero. OUT_ATTEMPT: Key/id only.
 * OUT_PUBREL: Key/id and successful PUBREC reason0/0x10 (311 only0).
 * OUT_COMPLETE: Key/id, Flags=PUBACK/PUBREC/PUBCOMP type, its valid reason;
 *      PUBREC completion must be negative. IN_ACCEPT: Key plus canonical full-
 *      topic PUBLISH Data; id matches QoS1/2,0 for QoS0. It atomically represents
 *      payload/delivery AND QoS2 identity acceptance. Other fields zero.
 * IN_PUBREL/IN_COMPLETE: Key/id only. INBOX_ACK: Key only, original acceptance
 *      Epoch; distinct from receive completion and valid after protocol reset.
 * Every record has nonzero Epoch. Data otherwise empty. No TLS/auth secrets. */
typedef struct xmqttstoredelta {
    xmqttstoredeltatype Type;uint8 Flags,Reason;uint16 Identifier;uint32 Expiry;
    uint64 Epoch,Key,WallMilliseconds;xbytesview Data;
} xmqttstoredelta;
typedef struct xmqttstoredeltaconfig {
    size_t Size;xmqttversion Version;xmqttcodecconfig Codec;size_t MaxOperations,MaxBytes;
} xmqttstoredeltaconfig;
typedef struct xmqttstoredeltadocument {
    const xmqttstoredelta* Operations;size_t Count;xbytesview Raw;
} xmqttstoredeltadocument;
XRT_EXTERN_C_BEGIN
/* Defaults: MQTT5, native default codec limits,128 operations,1MiB batch.
 * Zero limits are real refusals. Empty batches are valid when MaxBytes>=32.
 * This developing pure codec is NOT a state applier, durable Client or Inbox.
 * It validates representation/packet semantics, not authentication, stale keys,
 * legal cross-record order, broker proof, clock order or business effects. */
XMQTT_API bool xmqttStoreDeltaConfigInit(xmqttstoredeltaconfig* Config);
/* Immutable input borrows only for the call. Reusable Properties scratch is
 * provisional on any result and must not alias any input/output. One packet's
 * property descriptors are needed at a time; returned operations retain only
 * wire views, never this property scratch. Native MQTT parsers validate complete
 * PUBLISH, full topic, property/PFI limits and identity; Topic Alias is forbidden.
 * Size/Write independently preflight all records. Required/Written/wire remain
 * unchanged on any failure/LIMIT. No successful allocation. Write copies each
 * Data span once; Read borrows it. All caller writable spans are mutually
 * disjoint and disjoint from Config, source vectors and every source Data span.
 * DONE/LIMIT preserve ambient errors. Failure/refusal diagnostics may allocate
 * transiently, including a native decoder's discarded capacity diagnostic;
 * that newly published diagnostic can notify its configured handler. Quiet
 * restoration never re-notifies the old ambient error. Native error-slot
 * exchange failure returns ERROR before writer output/reader Document publish. */
XMQTT_API xmqttstoredeltastatus xmqttStoreDeltaSize(const xmqttstoredeltaconfig* Config,
    const xmqttstoredelta* Operations,size_t Count,xmqttproperty* Properties,size_t PropertyCapacity,size_t* Required);
XMQTT_API xmqttstoredeltastatus xmqttStoreDeltaWrite(const xmqttstoredeltaconfig* Config,
    const xmqttstoredelta* Operations,size_t Count,xmqttproperty* Properties,size_t PropertyCapacity,
    ptr Output,size_t Capacity,size_t* Written);
/* One EXACT complete version1 transaction, no incremental prefix retention.
 * Document publishes only on DONE; operation/property scratch is provisional on
 * errors. Keep Input and operation scratch unchanged while using Document.
 * Body bytes need the enclosing journal's CRC/security; no redundant checksum
 * is added here. Set journal PayloadSchema=XMQTT_STORE_DELTA_SCHEMA and require
 * matching MQTT version, binding and generation. All records in this encoded
 * batch form ONE journal commit, allowing later bounded group-commit policy.
 * No file sync, queue/timer, Session mutation, ACK or delivery side effect. */
XMQTT_API xmqttstoredeltastatus xmqttStoreDeltaRead(const xmqttstoredeltaconfig* Config,xbytesview Input,
    xmqttstoredelta* Operations,size_t Capacity,xmqttproperty* Properties,size_t PropertyCapacity,
    xmqttstoredeltadocument* Document);
XRT_EXTERN_C_END
#endif
#endif
