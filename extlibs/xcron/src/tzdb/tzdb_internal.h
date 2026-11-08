#ifndef XCRON_TZDB_INTERNAL_H
#define XCRON_TZDB_INTERNAL_H
#include <xcron/tzdb.h>
#include <xrt/atomic.h>
#include <xrt/memory.h>
#include <xrt/sync.h>
#if defined(XCRON_FEATURE_XCRON_TZDB)
#define XCRON_TZDB_MAGIC UINT32_C(0x58434431)
#define XCRON_TZDB_SNAPSHOT_MAGIC UINT32_C(0x58434453)
typedef struct xcrontzdbrow {xstrview Name;xcronzone* Zone;uint32 Index;} xcrontzdbrow;
struct xcrontzdbsnapshot {uint32 Magic;xatomic32 Refs;size_t ArenaBytes,Bytes,Count;uint64 Revision;xstrview Version;xcrontzdbrow Rows[];};
struct xcrontzdb {uint32 Magic;xcrontzdbconfig Config;xallocationbudget* Budget;xmutex Mutex;xcrontzdbsnapshot* Current;uint64 Revision;uint32 Loading;xatomic32 Closed;};
/* Product-private cold reader. Allocates only with xrtMalloc; success returns a
 * borrowed view and optional buffer released by the common builder. Failure
 * closes/frees its own partial resources and fills typed Work without publishing
 * an application diagnostic. No callback occurs under the publication lock. */
typedef bool (*xcrontzdbread)(ptr Context,xstrview Name,xbytesview* Data,ptr* Owned,xcrontzdbissue* Work);
typedef struct xcrontzdbsource {const xcrontzdbentry* Entries;const xstrview* Names;size_t Count;xcrontzdbread Read;ptr Context;} xcrontzdbsource;
bool xcronTzdbLoadSource(xcrontzdb* DB,xstrview Version,const xcrontzdbsource* Source,uint64 Expected,uint64* Revision,xcrontzdbissue* Issue);
#endif
#endif
