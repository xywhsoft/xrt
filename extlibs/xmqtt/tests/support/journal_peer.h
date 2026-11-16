#ifndef XMQTT_TEST_JOURNAL_PEER_H
#define XMQTT_TEST_JOURNAL_PEER_H
#include "../test.h"
#include <xmqtt/journal.h>
#include <xrt/vfs.h>
#include <xrt/crc32.h>
enum {JP_CAP=65536};
typedef struct jpdrive {
    unsigned char Bytes[JP_CAP],Stable[JP_CAP];size_t Size,StableSize,Chunk,Partial;
    unsigned Calls,Fail,Flushes,KillAt,Closes;bool FlushOnFailure,Zero,Reenter;
    xfile Native;xmqttjournal* Journal;
} jpdrive;
static inline void jpStatus(xmqttjournalstatus Got,xmqttjournalstatus Want,cstr Why)
{if(Got!=Want)fprintf(stderr,"journal status%d expected%d: %s error%s\n",Got,Want,Why,xrtGetError()?xrtErrorMessage(xrtGetError()):"none");testRequire(Got==Want,Why);}
static inline xmqttjournalconfig jpConfig(void)
{xmqttjournalconfig C;testRequire(xmqttJournalConfigInit(&C),"journal config");C.Binding=XRT_BYTES_LITERAL("b\0principal/slot");C.Generation=13;C.MaxTransactionBytes=1024;C.MaxMemoryBytes=16384;C.MaxFileBytes=JP_CAP;return C;}
static inline bool jpTick(jpdrive* D)
{
    ++D->Calls;
    if(D->Reenter&&D->Journal){xmqttjournalinfo I;xmqttjournalrecord R;uint64 Seq=88;D->Reenter=false;
        testRequire(!xmqttJournalInfo(D->Journal,&I)&&!xmqttJournalDestroy(D->Journal),"backend reentrant info/destroy rejected");
        jpStatus(xmqttJournalNext(D->Journal,&R),XMQTT_JOURNAL_STATE,"backend reentrant next");jpStatus(xmqttJournalAppend(D->Journal,XRT_BYTES_LITERAL("r"),&Seq),XMQTT_JOURNAL_STATE,"backend reentrant append");testRequire(Seq==88,"reentry output unchanged");xrtClearError();}
    return !D->Fail||D->Calls!=D->Fail;
}
static inline void jpBarrier(jpdrive* D)
{if(D->KillAt==D->Calls){printf("[BARRIER] %u\n",D->Calls);fflush(stdout);testRequire(getchar()=='x',"parent barrier acknowledgement or process kill");}}
static inline bool jpRead(void* State,uint64 At,void* B,size_t N,size_t* Done)
{
    jpdrive* D=State;if(D->Native)return xrtReadAt(D->Native,At,B,N,Done);
    if(At>=D->Size)N=0;else if(N>D->Size-(size_t)At)N=D->Size-(size_t)At;
    if(D->Chunk&&N>D->Chunk){N=D->Chunk;}if(N)memcpy(B,D->Bytes+(size_t)At,N);*Done=N;return true;
}
static inline bool jpWrite(void* State,uint64 At,const void* B,size_t N,size_t* Done)
{
    jpdrive* D=State;bool OK=jpTick(D);if(D->Zero){*Done=0;return true;}
    if(!OK&&N>D->Partial){N=D->Partial;}if(D->Chunk&&N>D->Chunk)N=D->Chunk;
    if(D->Native){bool Result=xrtWriteAt(D->Native,At,B,N,Done);jpBarrier(D);return OK&&Result;}
    testRequire(At<=JP_CAP&&N<=JP_CAP-At,"bounded backend write");if(N)memcpy(D->Bytes+(size_t)At,B,N);*Done=N;if(At+N>D->Size)D->Size=(size_t)At+N;jpBarrier(D);return OK;
}
static inline bool jpStat(void* State,xfileinfo* I)
{jpdrive* D=State;memset(I,0,sizeof(*I));I->Type=XFILE_TYPE_FILE;I->Available=XFILE_INFO_SIZE;return D->Native?xrtFileSize(D->Native,&I->Size):(I->Size=D->Size,true);}
static inline bool jpResize(void* State,uint64 N)
{jpdrive* D=State;if(!jpTick(D))return false;if(D->Native)return xrtFileResize(D->Native,N);testRequire(N<=JP_CAP,"bounded resize");if(N>D->Size)memset(D->Bytes+D->Size,0,(size_t)N-D->Size);D->Size=(size_t)N;return true;}
static inline bool jpFlush(void* State)
{jpdrive* D=State;bool OK=jpTick(D);++D->Flushes;if(OK||D->FlushOnFailure){if(D->Native){if(!xrtFlush(D->Native))return false;}else {memcpy(D->Stable,D->Bytes,D->Size);D->StableSize=D->Size;}}jpBarrier(D);return OK;}
static inline void jpClose(void* State){++((jpdrive*)State)->Closes;}
static const xvfsfileops_v1 jpOps={sizeof(xvfsfileops_v1),XRT_VFS_FILE_OPS_VERSION,
    XVFS_FILE_READ_AT|XVFS_FILE_WRITE_AT|XVFS_FILE_STAT|XVFS_FILE_RESIZE|XVFS_FILE_FLUSH,
    NULL,NULL,jpRead,jpWrite,NULL,jpStat,jpResize,jpFlush,jpClose};
static inline void jpRetain(void* C){(void)C;}
static inline void jpRelease(void* C){(void)C;}
static inline xvfslookup jpOpen(void* C,xvfscase Case,xstrview Path,const xfileoptions* O,xvfsfile_v1* F)
{(void)Case;(void)Path;*F=(xvfsfile_v1){&jpOps,C,O->Flags,0};return XVFS_LOOKUP_OPENED;}
static const xvfsprovider_v1 jpProvider={sizeof(xvfsprovider_v1),XRT_VFS_PROVIDER_VERSION,0,jpRetain,jpRelease,jpOpen,NULL,NULL,NULL};
typedef struct jpfile {xvfs Vfs;xvfsmount Mount;xfile File;} jpfile;
static inline jpfile jpFile(jpdrive* D)
{jpfile F={0};F.Vfs=xrtVfsCreate();testRequire(F.Vfs!=NULL,"public VFS create");F.Mount=xrtVfsMount(F.Vfs,"/",0,XVFS_CASE_SENSITIVE,&jpProvider,D,0);testRequire(F.Mount!=NULL,"public VFS mount");xfileoptions O;xrtFileOptionsInit(&O);O.Flags=XFILE_READ|XFILE_WRITE;F.File=xrtVfsOpen(F.Vfs,"/journal",&O);testRequire(F.File!=NULL,"public VFS synchronous absolute file");return F;}
static inline void jpFileClose(jpfile F)
{testRequire(xrtClose(F.File),"borrowed file still closeable");xrtVfsMountDestroy(F.Mount);xrtVfsDestroy(F.Vfs);}
static inline xmqttjournal* jpCreate(jpfile F,const xmqttjournalconfig* C)
{xmqttjournal* J=NULL;jpStatus(xmqttJournalCreate(F.File,C,&J),XMQTT_JOURNAL_DONE,"create journal through public XRT");return J;}
static inline unsigned jpReplay(xmqttjournal* J)
{unsigned Count=0;xmqttjournalrecord R={0};xmqttjournalstatus S;while((S=xmqttJournalNext(J,&R))==XMQTT_JOURNAL_DONE){++Count;testRequire(R.Sequence==Count,"contiguous replay sequence");}jpStatus(S,XMQTT_JOURNAL_EMPTY,"replay complete");return Count;}
static inline xmqttjournal* jpReopen(jpfile F,const xmqttjournalconfig* C)
{xmqttjournal* J=NULL;jpStatus(xmqttJournalOpen(F.File,C,&J),XMQTT_JOURNAL_DONE,"open journal");return J;}
static inline xmqttjournalinfo jpInfo(xmqttjournal* J)
{xmqttjournalinfo I;testRequire(xmqttJournalInfo(J,&I),"journal bounded ownership/effect info");return I;}
static inline void jpAppend(xmqttjournal* J,const char* S,uint64 Want)
{uint64 Seq=0;jpStatus(xmqttJournalAppend(J,(xbytesview){(const unsigned char*)S,strlen(S)},&Seq),XMQTT_JOURNAL_DONE,"append+data sync+both witnesses+publication sync");testRequire(Seq==Want,"committed output sequence");}
#endif
