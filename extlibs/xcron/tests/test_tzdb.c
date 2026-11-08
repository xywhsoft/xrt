#include "test.h"
#include "tzif_fixture.h"
#include "../src/tzdb/tzdb_internal.h"
#include "../src/timezone/timezone_internal.h"
static const uint8 TzdbUTC[54]={[0]='T',[1]='Z',[2]='i',[3]='f',[39]=1,[43]=4,[50]='U',[51]='T',[52]='C'};
static xcrontzdb* tzdbCreate(size_t Bytes)
{xcrontzdb* D=NULL;xcrontzdbconfig C={1024,4,Bytes};testRequire(xcronTzdbCreate(&C,&D,NULL),"tzdb create");return D;}
static xcrontzdbentry tzdbEntry(cstr Name,xbytesview Data)
{return (xcrontzdbentry){{Name,strlen(Name)},Data};}
static xcrontzdbsnapshot* tzdbAcquire(xcrontzdb* D)
{xcrontzdbsnapshot* S=NULL;testRequire(xcronTzdbAcquire(D,&S,NULL),"acquire named snapshot");return S;}
static xcronzone* tzdbFind(xcrontzdbsnapshot* S,cstr Name)
{xcronzone* Z=NULL;testRequire(xcronTzdbFind(S,(xstrview){Name,strlen(Name)},&Z,NULL)==XCRON_TZDB_FOUND,"find retained named zone");return Z;}
static xcrontzdbstats tzdbStats(xcrontzdb* D)
{xcrontzdbstats S;testRequire(xcronTzdbInspect(D,&S,NULL),"named database stats");return S;}
static int32 tzdbOffset(xcronzone* Z)
{xcronzonestate S;testRequire(xcronZoneAt(Z,XRT_TIME_UNIX_EPOCH,&S,NULL),"named zone actual state");return S.Offset;}
static void tzdbEnd(xcrontzdb* D)
{testRequire(xcronTzdbClose(D,NULL) && xcronTzdbDestroy(D,NULL),"close and physically destroy database");}
static void testTzdbVersions(void)
{
    xcrontzdb* D=tzdbCreate(1024*1024);xcrontzdbsnapshot* Missing=NULL;xcrontzdbissue I;
    testRequire(!xcronTzdbAcquire(D,&Missing,&I) && I.Code==XCRON_TZDB_EMPTY && !Missing,"no invented initial database");xrtClearError();
    xcrontzdbentry E[]={tzdbEntry("Etc/UTC",(xbytesview){TzdbUTC,sizeof(TzdbUTC)}),tzdbEntry("UTC",(xbytesview){TzdbUTC,sizeof(TzdbUTC)})};uint64 R=0;
    testRequire(xcronTzdbLoad(D,XRT_STR_LITERAL("test-a"),E,2,0,&R,NULL) && R==1,"initial named publication");
    xcrontzdbsnapshot* Old=tzdbAcquire(D);xcronzone* UTC=tzdbFind(Old,"UTC");testRequire(!tzdbOffset(UTC),"first rule retained");
    tzifblob B;tzifkind K[]={{3600,false,"PLUS"}};tzifBuild(&B,0,NULL,NULL,0,K,1,"");E[1]=tzdbEntry("UTC",(xbytesview){B.Data,B.Size});
    uint64 Sentinel=777;testRequire(!xcronTzdbLoad(D,XRT_STR_LITERAL("test-b"),E,2,0,&Sentinel,&I) && I.Code==XCRON_TZDB_STALE && Sentinel==777,"stale CAS changes neither version nor output");xrtClearError();
    testRequire(xcronTzdbLoad(D,XRT_STR_LITERAL("test-b"),E,2,1,&R,NULL) && R==2,"complete atomic replacement");memset(B.Data,0,B.Size);
    xcrontzdbsnapshot* New=tzdbAcquire(D);xcronzone* Plus=tzdbFind(New,"UTC");testRequire(tzdbOffset(Plus)==3600 && !tzdbOffset(UTC),"old and new names resolve distinct immutable rules");
    xcronzone* None=Plus;testRequire(xcronTzdbFind(New,XRT_STR_LITERAL("utc"),&None,NULL)==XCRON_TZDB_NONE && None==Plus,"exact names, no case fallback or output write");
    xcrontzdbsnapshotinfo Info;testRequire(xcronTzdbSnapshotInfo(Old,&Info,NULL) && Info.Revision==1 && Info.Names==2 && Info.Version.Size==6 && !memcmp(Info.Version.Data,"test-a",6),"old snapshot owns its name/version manifest");
    testRequire(xcronTzdbSnapshotRetain(Old),"explicit snapshot handoff reference");xcronTzdbSnapshotRelease(Old);
    testRequire(!xcronTzdbDestroy(D,&I) && I.Code==XCRON_TZDB_BUSY,"destroy needs explicit close");xrtClearError();tzdbEnd(D);
    testRequire(tzdbOffset(Plus)==3600 && !tzdbOffset(UTC) && xcronTzdbSnapshotInfo(Old,&Info,NULL),"snapshots and zones outlive manager destruction");
    xcronTzdbSnapshotRelease(Old);xcronTzdbSnapshotRelease(New);testRequire(!tzdbOffset(UTC),"zone can also outlive all snapshots");xcronZoneRelease(UTC);xcronZoneRelease(Plus);
}
#include "tzdb_adversarial_cases.h"
int main(void)
{testTzdbVersions();testTzdbArguments();testTzdbQuota();testTzdbAllocationPrefixes();testTzdbOrderAndWarm();
#if defined(XRT_FEATURE_THREAD)
testTzdbConcurrentPublication();testTzdbReaders();
#endif
puts("[PASS] named TZif snapshots: exact lookup/atomic reload/CAS/aggregate quota/OOM/zero-heap/old owners/concurrent publication");return 0;}
