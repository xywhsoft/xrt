#ifndef XREDIS_SUBSCRIBER_NOTIFY_H
#define XREDIS_SUBSCRIBER_NOTIFY_H
#include <xredis/subscriber.h>
#if defined(XREDIS_FEATURE_XREDIS_SUBSCRIBER_NOTIFY) && !defined(XREDIS_FEATURE_XREDIS_SUBSCRIBER)
#error "Subscriber notification requires the native subscriber"
#endif
#if defined(XREDIS_FEATURE_XREDIS_SUBSCRIBER_NOTIFY)
XRT_EXTERN_C_BEGIN
/* Nonblocking shared readiness observer, not a receive reservation. ACCEPTED
 * returns one owned Future reference. Wait/Watch it, destroy that reference,
 * then recheck the delivery queue. Its only terminal value is RESOLVED/NULL;
 * a competing consumer can have drained the queue by the time it is observed.
 * One pending epoch is shared by all observers; rearming an empty queue creates
 * at most one replacement outside Gate. Concurrent epoch construction returns
 * OVERLOADED, and allocation failure returns ERROR; both preserve Future.
 * CLOSED means physical close AND an empty queue; queued messages remain
 * readable after close. Physical close wakes every pending epoch. Retaining an
 * observer does not keep Client/Engine alive after close. FutureCancel is not
 * acted on by this producer; use your own cancel token with Core's cancelable
 * wait, or detach your Watch. Never block an Engine worker in a wait/callback.
 * Pending Watch callbacks run on the owning IO worker. Newly constructed ready
 * epochs resolve before publication, so no user callback runs under Gate or on
 * a constructor thread. Ready WatchAdd follows Core's immediate-ready contract.
 * Drain-before-arm races cannot strand available messages. Success transfers
 * no message and retains no caller storage. No notification is allocated on
 * the producer path. Outputs must be disjoint, caller-owned writable objects. */
XREDIS_API xredisadmission xredisSubscriberReadable(xredisclient* Client,xfuture** Future);
/* Transfer up to Capacity owned reply references in one Gate acquisition.
 * Only the actual prefix is written; each reference must later be released.
 * Successful transfer sets Taken >0. Empty returns false and preserves all
 * output bytes and ambient error, including after physical close. Invalid
 * arguments report an error and change no outputs/queue. Capacity is positive,
 * Replies and Taken are disjoint and cannot overlap Client/mode storage.
 * Shares FIFO delivery with ClientTakePush; neither path reserves messages for
 * a particular readiness observer. No allocation, retain, payload copy or scan. */
XREDIS_API bool xredisSubscriberTakeBatch(xredisclient* Client,xredisreply** Replies,size_t Capacity,size_t* Taken);
XRT_EXTERN_C_END
#endif
#endif
