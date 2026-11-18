#ifndef XMQTT_JOURNAL_FILE_H
#define XMQTT_JOURNAL_FILE_H
#include <xmqtt/journal.h>
#if defined(XMQTT_FEATURE_XMQTT_JOURNAL_FILE) && \
    (!defined(XMQTT_FEATURE_XMQTT_JOURNAL) || !defined(XRT_FEATURE_FILE_ROOT) || !defined(XRT_FEATURE_FILE_LOCK))
#error "The locked journal requires the MQTT journal, native root and file locks"
#endif
#if defined(XMQTT_FEATURE_XMQTT_JOURNAL_FILE)
XRT_EXTERN_C_BEGIN
/* Open an existing regular file relative to the borrowed Root. Names and
 * root lifetime follow xrtRootFileOpen. The final component must be a regular
 * file in a trusted, stable namespace. Provision the name durably beforehand.
 * The new native handle locks before any journal data read, replay or write.
 * BUSY leaves Journal and the committed data unchanged; native contention
 * may notify an error handler, while the ambient error is restored.
 * Success transfers the locked handle to Journal and downstream owners.
 * Physical Destroy releases it. A close error leaves a poisoned Journal
 * that can be destroyed again to finish disposal. The Root is borrowed
 * only during construction. Windows and Linux with native lock support. */
XMQTT_API xmqttjournalstatus xmqttJournalOpenLocked(xroot Root,cstr Path,
    const xmqttjournalconfig* Config,xmqttjournal** Journal);
/* Initialize a pre-existing empty file under the same ownership lock.
 * A failed initialization can leave partial bytes for explicit recovery. */
XMQTT_API xmqttjournalstatus xmqttJournalCreateLocked(xroot Root,cstr Path,
    const xmqttjournalconfig* Config,xmqttjournal** Journal);
XRT_EXTERN_C_END
#endif
#endif
